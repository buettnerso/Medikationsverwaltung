#include "pch.h"
#include "ReportService.h"
#include "AuditLogger.h"

#include "DateUtils.h"
#include "ExcelCom.h"
#include "OrderPlanner.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace
{
    std::string Utf8(const std::wstring& value)
    {
        if (value.empty()) return {};
        const int needed = ::WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
        std::string result(static_cast<size_t>(needed), '\0');
        ::WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), needed, nullptr, nullptr);
        return result;
    }

    std::string Csv(const std::wstring& value)
    {
        auto s = Utf8(value);
        const bool quote = s.find(';') != std::string::npos || s.find('"') != std::string::npos || s.find('\n') != std::string::npos;
        size_t pos = 0;
        while ((pos = s.find('"', pos)) != std::string::npos) { s.insert(pos, 1, '"'); pos += 2; }
        return quote ? '"' + s + '"' : s;
    }

    std::wstring Timestamp()
    {
        SYSTEMTIME st{};
        ::GetLocalTime(&st);
        wchar_t buffer[64]{};
        swprintf_s(buffer, L"%04u%02u%02u_%02u%02u%02u", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
        return buffer;
    }

    std::wstring TodayText()
    {
        SYSTEMTIME st{};
        ::GetLocalTime(&st);
        wchar_t buffer[32]{};
        swprintf_s(buffer, L"%02u.%02u.%04u", st.wDay, st.wMonth, st.wYear);
        return buffer;
    }

    std::wstring SafeFilePart(std::wstring value)
    {
        for (auto& ch : value)
            if (ch == L'/' || ch == L'\\' || ch == L':' || ch == L'*' || ch == L'?' || ch == L'"' || ch == L'<' || ch == L'>' || ch == L'|') ch = L'_';
        return value.empty() ? L"Unbenannt" : value;
    }

    std::filesystem::path Prepare(const std::filesystem::path& root)
    {
        std::filesystem::create_directories(root);
        return root;
    }

    void WriteBom(std::ofstream& out)
    {
        const unsigned char bom[] = { 0xEF, 0xBB, 0xBF };
        out.write(reinterpret_cast<const char*>(bom), 3);
    }

    int FindHeaderLike(const med::SheetTable& table, const std::vector<std::wstring>& candidates)
    {
        // Exakte Treffer haben Vorrang. Das verhindert z. B., dass "Return Comment"
        // als allgemeines "Comment" erkannt wird.
        for (size_t i = 0; i < table.headers.size(); ++i)
        {
            const auto header = med::date::Normalize(table.headers[i]);
            for (const auto& candidate : candidates)
                if (header == med::date::Normalize(candidate)) return static_cast<int>(i);
        }
        for (size_t i = 0; i < table.headers.size(); ++i)
        {
            const auto header = med::date::Normalize(table.headers[i]);
            for (const auto& candidate : candidates)
            {
                const auto c = med::date::Normalize(candidate);
                if (!c.empty() && header.find(c) != std::wstring::npos) return static_cast<int>(i);
            }
        }
        return -1;
    }

    int FindSecondGenericComment(const med::SheetTable& table, int firstCommentColumn)
    {
        for (size_t i = 0; i < table.headers.size(); ++i)
        {
            if (static_cast<int>(i) == firstCommentColumn) continue;
            const auto normalized = med::date::Normalize(table.headers[i]);
            if (normalized == L"comment" || normalized == L"kommentar" || normalized == L"bemerkung")
                return static_cast<int>(i);
        }
        return -1;
    }

    bool RowHasData(const std::vector<med::CellValue>& row)
    {
        return std::any_of(row.begin(), row.end(), [](const med::CellValue& value) { return !value.IsEmpty(); });
    }

    med::CellValue CellAt(const std::vector<med::CellValue>& row, int column)
    {
        if (column < 0 || static_cast<size_t>(column) >= row.size()) return med::CellValue::Empty();
        return row[static_cast<size_t>(column)];
    }

    // Berichtsdaten werden absichtlich als sichtbarer Datumstext geschrieben.
    // Damit ist die Darstellung unabhängig vom lokalen Excel-Datumsformat und von
    // der internen Serienzahl des Quelldokuments immer eindeutig dd.mm.yyyy.
    med::CellValue ReportDateCell(const med::CellValue& source)
    {
        if (source.IsEmpty()) return med::CellValue::Empty();
        if (const auto day = med::date::CellToDate(source))
            return med::CellValue::Text(med::date::FormatGermanDate(*day));

        const auto display = med::date::Trim(med::date::CellToDisplay(source));
        return display.empty() ? med::CellValue::Empty() : med::CellValue::Text(display);
    }

    std::filesystem::path ResolveTemplate(
        const med::StudyData& study,
        const med::ImpData& imp,
        const std::filesystem::path& defaultTemplate,
        bool patientReport)
    {
        const std::wstring fileName = patientReport ? L"DrugAccount_Patient.xlsx" : L"DrugAccount_Gesamt.xlsx";

        // Eine explizit konfigurierte Vorlage wird nur verwendet, wenn sie selbst
        // bereits den passenden Berichtstyp bezeichnet. Bei einem konfigurierten
        // Ordner wird dort nach der typbezogenen Vorlage gesucht.
        if (!study.config.drugAccountTemplate.empty())
        {
            auto configured = std::filesystem::path(study.config.drugAccountTemplate);
            if (configured.is_relative()) configured = study.studyDirectory / configured;
            if (std::filesystem::is_directory(configured))
            {
                const auto candidate = configured / fileName;
                if (std::filesystem::exists(candidate)) return candidate;
            }
            else if (std::filesystem::exists(configured))
            {
                const auto normalizedName = med::date::Normalize(configured.filename().wstring());
                const auto typeToken = med::date::Normalize(patientReport ? L"Patient" : L"Gesamt");
                if (normalizedName.find(typeToken) != std::wstring::npos) return configured;
            }
        }

        const auto impSpecific = study.studyDirectory / L"Documents" / L"DrugAccount" / SafeFilePart(imp.id) / fileName;
        if (std::filesystem::exists(impSpecific)) return impSpecific;

        const auto studySpecific = study.studyDirectory / L"Documents" / L"DrugAccount" / fileName;
        if (std::filesystem::exists(studySpecific)) return studySpecific;

        if (std::filesystem::exists(defaultTemplate)) return defaultTemplate;
        throw std::runtime_error(patientReport
            ? "DrugAccount-Patientenvorlage wurde nicht gefunden."
            : "DrugAccount-Gesamtvorlage wurde nicht gefunden.");
    }

    void EnsureRows(med::excel::Workbook& workbook, const std::wstring& sheet, int templateLastRow, int requiredLastRow, int lastColumn)
    {
        for (int row = templateLastRow + 1; row <= requiredLastRow; ++row)
            workbook.CopyFormats(sheet, templateLastRow, row, 1, lastColumn);
    }

    std::wstring ResolveSheetName(med::excel::Workbook& workbook, bool patientReport)
    {
        if (patientReport)
        {
            if (workbook.HasSheet(L"DrugAccount_Patient")) return L"DrugAccount_Patient";
            if (workbook.HasSheet(L"Tabelle2")) return L"Tabelle2"; // Kompatibilität für eigene Altvorlagen
        }
        else
        {
            if (workbook.HasSheet(L"DrugAccount_Gesamt")) return L"DrugAccount_Gesamt";
            if (workbook.HasSheet(L"Tabelle1")) return L"Tabelle1";
        }
        throw std::runtime_error(patientReport
            ? "Die Patientenvorlage enthält kein Arbeitsblatt 'DrugAccount_Patient'."
            : "Die Gesamtvorlage enthält kein Arbeitsblatt 'DrugAccount_Gesamt'.");
    }

    int PopulateOverallSheet(
        med::excel::Workbook& workbook,
        const std::wstring& sheet,
        const med::StudyData& study,
        const med::ImpData& imp)
    {
        constexpr int dataStartRow = 6;
        constexpr int templateLastRow = 35;
        constexpr int lastColumn = 11;
        const auto& inventory = imp.inventory;

        if (inventory.headers.empty())
            throw std::runtime_error("Für das ausgewählte IMP wurden keine DrugInventory-/Inventardaten geladen.");

        const int deliveryCol = FindHeaderLike(inventory, {
            L"DeliveryDate", L"Delivery Date", L"Datum Erhalt", L"Datum erhalten am",
            L"ReceivedDate", L"Received Date", L"Lieferdatum", L"Wareneingang am" });
        const int chargeCol = FindHeaderLike(inventory, {
            L"Charge No", L"Charge No.", L"Charg.No", L"ChargNo", L"Chargen-Nr.", L"Chargennummer",
            L"Batch", L"Lot", L"Charge" });
        const int kitCol = FindHeaderLike(inventory, {
            L"Vial No", L"Vial No.", L"Kit No", L"Kit No.", L"KitNo",
            L"Box-Nr", L"Box-Nr.", L"Box Nr", L"Box No", L"Box Number",
            L"Bottle No", L"Pack No", L"Package No", L"Serial", L"Container",
            L"Vial", L"Kit", L"Box", L"Pack" });
        const int expiryCol = FindHeaderLike(inventory, {
            L"Expiry-Date", L"Expiry Date", L"ExpiryDate", L"Expiration Date",
            L"Verfallsdatum", L"Verfalldatum", L"Verfall" });
        const int dispensingCol = FindHeaderLike(inventory, {
            L"DispensingDate", L"Dispensing Date", L"Datum Ausgabe", L"Datum ausgegeben am",
            L"Ausgaben am", L"Dispensed", L"Ausgabedatum", L"Abgabedatum" });
        const int patientCol = FindHeaderLike(inventory, {
            L"Pat.ID", L"Pat ID", L"Pat.-ID", L"PatID", L"PatientID", L"Patient ID",
            L"Patient", L"Patienten-ID", L"Patienten ID", L"Patientennummer", L"Patienten-Nr.", L"Pat.Nr." });
        const int commentCol = FindHeaderLike(inventory, {
            L"Dispensing Comment", L"Comment Dispensing", L"Kommentar Ausgabe", L"Bemerkung Ausgabe",
            L"Kommentar Abgabe", L"Bemerkung Abgabe", L"Comment", L"Bemerkung", L"Kommentar" });
        const int returnDateCol = FindHeaderLike(inventory, {
            L"Date returned", L"ReturnDate", L"Return Date", L"ReturnedDate", L"Returned Date",
            L"Datum zurück gegeben am", L"Datum zurückgegeben am", L"Datum zurück gegeben",
            L"Datum zurück erhalten", L"Datum zurück erhalten am", L"Rückgabedatum",
            L"Datum Rückgabe", L"Zurückgegeben am", L"Rueckgabedatum", L"Rueckgabe", L"Rückgabe" });
        const int quantityUnusedCol = FindHeaderLike(inventory, {
            L"Number of unused", L"Number unused", L"Unused Quantity", L"Quantity Unused",
            L"Quantity Returned", L"Returned Quantity", L"Anzahl unbenutzt",
            L"Anzahl übrig gebliebener", L"Anzahl übrig geblieben", L"Anzahl übrig gebliebene",
            L"Übrig geblieben", L"Restanzahl", L"Restmenge", L"Menge Rückgabe", L"Anzahl Rückgabe" });
        const int destructionDateCol = FindHeaderLike(inventory, {
            L"Destruction Date", L"DestructionDate", L"Date destroyed", L"DestroyedDate",
            L"Vernichtungsdatum", L"Datum Vernichtung", L"Datum der Vernichtung", L"Vernichtet am" });
        int returnCommentCol = FindHeaderLike(inventory, {
            L"Return Comment", L"ReturnComment", L"Destruction Comment", L"DestructionComment",
            L"Rückgabe Kommentar", L"Kommentar Rückgabe", L"Bemerkung Rückgabe",
            L"Vernichtung Kommentar", L"Kommentar Vernichtung", L"Bemerkung Vernichtung",
            L"Kommentar Rückgabe/Vernichtung", L"Bemerkung Rückgabe/Vernichtung", L"Comment Return" });
        if (returnCommentCol < 0)
            returnCommentCol = FindSecondGenericComment(inventory, commentCol);

        std::vector<const std::vector<med::CellValue>*> rows;
        for (const auto& row : inventory.rows)
            if (RowHasData(row)) rows.push_back(&row);
        if (rows.empty())
            throw std::runtime_error("Im Inventar des ausgewählten IMP wurden keine Datensätze gefunden. Es wird kein leerer DrugAccount erzeugt.");

        const int requiredLastRow = std::max(templateLastRow, dataStartRow + static_cast<int>(rows.size()) - 1);
        EnsureRows(workbook, sheet, templateLastRow, requiredLastRow, lastColumn);
        workbook.ClearRange(sheet, dataStartRow, 1, requiredLastRow, lastColumn);

        workbook.SetCell(sheet, 1, 2, med::CellValue::Text(study.studyName));
        workbook.SetCell(sheet, 1, 10, med::CellValue::Text(TodayText()));
        workbook.SetCell(sheet, 2, 2, med::CellValue::Text(med::date::Trim(study.euctNumber).empty() ? L"-" : med::date::Trim(study.euctNumber)));
        workbook.SetCell(sheet, 2, 6, med::CellValue::Text(imp.name));

        int outputRow = dataStartRow;
        for (const auto* row : rows)
        {
            // Gesamtbericht: kompletter Lebenszyklus eines IMP-Inventardatensatzes.
            workbook.SetCell(sheet, outputRow, 1, ReportDateCell(CellAt(*row, deliveryCol)));
            workbook.SetCell(sheet, outputRow, 2, CellAt(*row, chargeCol));
            workbook.SetCell(sheet, outputRow, 3, CellAt(*row, kitCol));
            workbook.SetCell(sheet, outputRow, 4, ReportDateCell(CellAt(*row, expiryCol)));
            workbook.SetCell(sheet, outputRow, 5, ReportDateCell(CellAt(*row, dispensingCol)));
            workbook.SetCell(sheet, outputRow, 6, CellAt(*row, patientCol));
            workbook.SetCell(sheet, outputRow, 7, CellAt(*row, commentCol));
            workbook.SetCell(sheet, outputRow, 8, ReportDateCell(CellAt(*row, returnDateCol)));
            workbook.SetCell(sheet, outputRow, 9, CellAt(*row, quantityUnusedCol));
            workbook.SetCell(sheet, outputRow, 10, ReportDateCell(CellAt(*row, destructionDateCol)));
            workbook.SetCell(sheet, outputRow, 11, CellAt(*row, returnCommentCol));
            ++outputRow;
        }
        return static_cast<int>(rows.size());
    }

    int PopulatePatientSheet(
        med::excel::Workbook& workbook,
        const std::wstring& sheet,
        const med::StudyData& study,
        const med::ImpData& imp,
        const std::wstring& patientId)
    {
        constexpr int dataStartRow = 6;
        constexpr int templateLastRow = 35;
        constexpr int lastColumn = 11;
        const auto& inventory = imp.inventory;

        if (inventory.headers.empty())
            throw std::runtime_error("Für das ausgewählte IMP wurden keine Inventardaten geladen.");

        const int patientCol = FindHeaderLike(inventory, {
            L"Pat.ID", L"Pat ID", L"Pat.-ID", L"PatID", L"PatientID", L"Patient ID",
            L"Patient", L"Patienten-ID", L"Patienten ID", L"Patientennummer", L"Patienten-Nr.", L"Pat.Nr." });
        if (patientCol < 0) throw std::runtime_error("Im Inventar wurde keine Patientenspalte erkannt.");

        const int chargeCol = FindHeaderLike(inventory, {
            L"Charge No", L"Charge No.", L"Charg.No", L"ChargNo", L"Chargen-Nr.", L"Chargennummer",
            L"Batch", L"Lot", L"Charge" });
        const int kitCol = FindHeaderLike(inventory, {
            L"Vial No", L"Vial No.", L"Kit No", L"Kit No.", L"KitNo",
            L"Box-Nr", L"Box-Nr.", L"Box Nr", L"Box No", L"Box Number",
            L"Bottle No", L"Pack No", L"Package No", L"Serial", L"Container",
            L"Vial", L"Kit", L"Box", L"Pack" });
        const int expiryCol = FindHeaderLike(inventory, {
            L"Expiry-Date", L"Expiry Date", L"ExpiryDate", L"Expiration Date",
            L"Verfallsdatum", L"Verfalldatum", L"Verfall" });
        const int dispensingCol = FindHeaderLike(inventory, {
            L"DispensingDate", L"Dispensing Date", L"Datum Ausgabe", L"Datum ausgegeben am",
            L"Ausgaben am", L"Dispensed", L"Ausgabedatum", L"Abgabedatum" });
        const int commentCol = FindHeaderLike(inventory, {
            L"Dispensing Comment", L"Comment Dispensing", L"Kommentar Ausgabe", L"Bemerkung Ausgabe",
            L"Kommentar Abgabe", L"Bemerkung Abgabe", L"Comment", L"Bemerkung", L"Kommentar" });
        const int returnDateCol = FindHeaderLike(inventory, {
            L"Date returned", L"ReturnDate", L"Return Date", L"ReturnedDate", L"Returned Date",
            L"Datum zurück gegeben am", L"Datum zurückgegeben am", L"Datum zurück gegeben",
            L"Datum zurück erhalten", L"Datum zurück erhalten am", L"Rückgabedatum",
            L"Datum Rückgabe", L"Zurückgegeben am", L"Rueckgabedatum", L"Rueckgabe", L"Rückgabe" });
        const int quantityUnusedCol = FindHeaderLike(inventory, {
            L"Number of unused", L"Number unused", L"Unused Quantity", L"Quantity Unused",
            L"Quantity Returned", L"Returned Quantity", L"Anzahl unbenutzt",
            L"Anzahl übrig gebliebener", L"Anzahl übrig geblieben", L"Anzahl übrig gebliebene",
            L"Übrig geblieben", L"Restanzahl", L"Restmenge", L"Menge Rückgabe", L"Anzahl Rückgabe" });
        const int destructionDateCol = FindHeaderLike(inventory, {
            L"Destruction Date", L"DestructionDate", L"Date destroyed", L"DestroyedDate",
            L"Vernichtungsdatum", L"Datum Vernichtung", L"Datum der Vernichtung", L"Vernichtet am" });
        int returnCommentCol = FindHeaderLike(inventory, {
            L"Return Comment", L"ReturnComment", L"Destruction Comment", L"DestructionComment",
            L"Rückgabe Kommentar", L"Kommentar Rückgabe", L"Bemerkung Rückgabe",
            L"Vernichtung Kommentar", L"Kommentar Vernichtung", L"Bemerkung Vernichtung",
            L"Kommentar Rückgabe/Vernichtung", L"Bemerkung Rückgabe/Vernichtung", L"Comment Return" });
        if (returnCommentCol < 0)
            returnCommentCol = FindSecondGenericComment(inventory, commentCol);

        std::vector<const std::vector<med::CellValue>*> rows;
        for (const auto& row : inventory.rows)
        {
            if (static_cast<size_t>(patientCol) >= row.size()) continue;
            const auto actual = med::date::Normalize(med::date::CellToDisplay(row[static_cast<size_t>(patientCol)]));
            if (actual == med::date::Normalize(patientId) && RowHasData(row)) rows.push_back(&row);
        }
        if (rows.empty())
            throw std::runtime_error("Für den ausgewählten Patienten wurden im Inventar dieses IMP keine DrugAccount-Datensätze gefunden. Es wird kein leerer Bericht erzeugt.");

        const int requiredLastRow = std::max(templateLastRow, dataStartRow + static_cast<int>(rows.size()) - 1);
        EnsureRows(workbook, sheet, templateLastRow, requiredLastRow, lastColumn);
        workbook.ClearRange(sheet, dataStartRow, 1, requiredLastRow, lastColumn);

        workbook.SetCell(sheet, 1, 2, med::CellValue::Text(study.studyName));
        workbook.SetCell(sheet, 1, 10, med::CellValue::Text(TodayText()));
        workbook.SetCell(sheet, 2, 2, med::CellValue::Text(med::date::Trim(study.euctNumber).empty() ? L"-" : med::date::Trim(study.euctNumber)));
        workbook.SetCell(sheet, 2, 6, med::CellValue::Text(imp.name));
        workbook.SetCell(sheet, 2, 10, med::CellValue::Text(patientId));

        int outputRow = dataStartRow;
        for (const auto* row : rows)
        {
            // Patientenbericht: Dispensing + Return/Destruction, Signaturfelder bleiben leer.
            workbook.SetCell(sheet, outputRow, 1, CellAt(*row, chargeCol));
            workbook.SetCell(sheet, outputRow, 2, CellAt(*row, kitCol));
            workbook.SetCell(sheet, outputRow, 3, ReportDateCell(CellAt(*row, expiryCol)));
            workbook.SetCell(sheet, outputRow, 4, ReportDateCell(CellAt(*row, dispensingCol)));
            workbook.SetCell(sheet, outputRow, 5, CellAt(*row, commentCol));
            // Spalte 6 = Signature: bewusst leer.
            workbook.SetCell(sheet, outputRow, 7, ReportDateCell(CellAt(*row, returnDateCol)));
            workbook.SetCell(sheet, outputRow, 8, CellAt(*row, quantityUnusedCol));
            workbook.SetCell(sheet, outputRow, 9, ReportDateCell(CellAt(*row, destructionDateCol)));
            workbook.SetCell(sheet, outputRow, 10, CellAt(*row, returnCommentCol));
            // Spalte 11 = Signature: bewusst leer.
            ++outputRow;
        }
        return static_cast<int>(rows.size());
    }

    bool OutputRowHasData(med::excel::Workbook& workbook, const std::wstring& sheet, int row, int lastColumn)
    {
        for (int column = 1; column <= lastColumn; ++column)
            if (!workbook.GetCell(sheet, row, column).IsEmpty()) return true;
        return false;
    }

    med::ReportFiles CreateExcelReport(
        const med::StudyData& study,
        const med::ImpData& imp,
        const std::filesystem::path& reportRoot,
        const std::filesystem::path& defaultTemplate,
        const std::wstring& suffix,
        const std::wstring* patientId)
    {
        const bool patientReport = patientId != nullptr;
        const auto sourceTemplate = ResolveTemplate(study, imp, defaultTemplate, patientReport);
        const auto dir = Prepare(reportRoot);
        const auto baseName = L"DrugAccount_" + SafeFilePart(study.studyName) + L"_" + SafeFilePart(imp.name) + L"_" + suffix + L"_" + Timestamp();
        const auto xlsxPath = dir / (baseName + L".xlsx");
        std::filesystem::copy_file(sourceTemplate, xlsxPath, std::filesystem::copy_options::overwrite_existing);

        med::excel::Application excel;
        auto workbook = excel.Open(xlsxPath, false);
        const auto sheet = ResolveSheetName(workbook, patientReport);

        int count = patientReport
            ? PopulatePatientSheet(workbook, sheet, study, imp, *patientId)
            : PopulateOverallSheet(workbook, sheet, study, imp);

        constexpr int verifyStartRow = 6;
        constexpr int verifyColumns = 11;
        if (count <= 0 || !OutputRowHasData(workbook, sheet, verifyStartRow, verifyColumns) ||
            !OutputRowHasData(workbook, sheet, verifyStartRow + count - 1, verifyColumns))
            throw std::runtime_error("Die Berichtsdaten konnten nicht verifiziert in die DrugAccount-Vorlage geschrieben werden. Der Export wurde abgebrochen, damit kein leerer Bericht entsteht.");

        workbook.Save();
        workbook.Close(false);
        return { xlsxPath, count };
    }
}

namespace med
{
    ReportFiles ReportService::ExportDrugAccountOverall(const StudyData& study, const ImpData& imp,
        const std::filesystem::path& reportRoot, const std::filesystem::path& defaultTemplate)
    {
        AuditLogger::EnsureWritable();
        auto result = CreateExcelReport(study, imp, reportRoot, defaultTemplate, L"Gesamt", nullptr);

        AuditEntry entry;
        entry.imp = imp.name;
        entry.action = L"REPORT_EXPORT_OVERALL";
        entry.sheet = imp.inventory.sheetName;
        entry.details = L"DrugAccount Gesamt wurde aus dem aktuellen DrugInventory erzeugt.";
        entry.changes = {
            { L"Ausgabedatei", L"", result.excelPath.wstring() },
            { L"Datenzeilen", L"", std::to_wstring(result.dataRows) }
        };
        AuditLogger::Write(study, entry);
        return result;
    }

    ReportFiles ReportService::ExportDrugAccountPatient(const StudyData& study, const ImpData& imp, const std::wstring& patientId,
        const std::filesystem::path& reportRoot, const std::filesystem::path& defaultTemplate)
    {
        AuditLogger::EnsureWritable();
        auto result = CreateExcelReport(study, imp, reportRoot, defaultTemplate, L"Patient_" + SafeFilePart(patientId), &patientId);

        AuditEntry entry;
        entry.imp = imp.name;
        entry.action = L"REPORT_EXPORT_PATIENT";
        entry.sheet = imp.inventory.sheetName;
        entry.details = L"DrugAccount pro Patient wurde aus dem aktuellen DrugInventory erzeugt.";
        entry.changes = {
            { L"Patient", L"", patientId },
            { L"Ausgabedatei", L"", result.excelPath.wstring() },
            { L"Datenzeilen", L"", std::to_wstring(result.dataRows) }
        };
        AuditLogger::Write(study, entry);
        return result;
    }

    std::filesystem::path ReportService::ExportStudySummary(const std::vector<StudyData>& studies, const std::filesystem::path& reportRoot)
    {
        std::filesystem::create_directories(reportRoot);
        const auto path = reportRoot / (L"Studienübersicht_" + Timestamp() + L".csv");
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        WriteBom(out);
        out << "Studie;IMP;Warenart;Applikationsform;Bestellpflicht;Bestand;Mindestbestand;Lieferzeit;Status;Bestellfenster_von;Spaetestens;Prognose;Excel-Datei\r\n";
        for (const auto& s : studies)
        {
            for (const auto& imp : s.imps)
            {
                out << Csv(s.studyName) << ';' << Csv(imp.name) << ';' << Csv(imp.goodsType) << ';' << Csv(imp.applicationForm) << ';'
                    << (imp.orderRequired ? "Ja" : "Nein") << ';' << imp.stock << ';' << imp.minimumStock << ';' << imp.leadTimeDays << ';'
                    << Csv(OrderPlanner::StateText(imp.plan.state)) << ';'
                    << Csv(imp.plan.recommendedFrom ? date::FormatGermanDate(*imp.plan.recommendedFrom) : L"") << ';'
                    << Csv(imp.plan.latestOrderDate ? date::FormatGermanDate(*imp.plan.latestOrderDate) : L"") << ';'
                    << (imp.plan.estimated ? "Ja" : "Nein") << ';' << Csv(s.excelPath.wstring()) << "\r\n";
            }
        }
        return path;
    }
}
