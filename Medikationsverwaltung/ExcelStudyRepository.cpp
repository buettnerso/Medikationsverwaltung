#include "pch.h"
#include "ExcelStudyRepository.h"

#include "DateUtils.h"
#include "AuditLogger.h"
#include "ExcelCom.h"
#include "OrderPlanner.h"
#include "Settings.h"

#include <algorithm>
#include <cwctype>
#include <fstream>
#include <memory>
#include <set>
#include <sstream>

namespace
{
    bool IsXlsx(const std::filesystem::path& path)
    {
        auto ext = path.extension().wstring();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
        return ext == L".xlsx" || ext == L".xlsm";
    }

    std::wstring ToText(const med::CellValue& c, const std::wstring& header = L"")
    {
        return med::date::CellToDisplay(c, med::date::HeaderLooksLikeDate(header));
    }

    double ToNumber(const med::CellValue& c, double fallback = 0.0)
    {
        if (c.type == med::CellType::Number) return c.number;
        if (c.type == med::CellType::Text)
        {
            try
            {
                auto s = c.text;
                std::replace(s.begin(), s.end(), L',', L'.');
                return std::stod(s);
            }
            catch (...) {}
        }
        return fallback;
    }

    bool ToBool(const med::CellValue& c, bool fallback = false)
    {
        if (c.type == med::CellType::Boolean) return c.boolean;
        if (c.type == med::CellType::Number) return c.number != 0.0;
        const auto n = med::date::Normalize(ToText(c));
        if (n == L"ja" || n == L"yes" || n == L"true" || n == L"1" || n == L"x") return true;
        if (n == L"nein" || n == L"no" || n == L"false" || n == L"0") return false;
        return fallback;
    }

    int FindHeaderLike(const med::SheetTable& table, const std::vector<std::wstring>& candidates)
    {
        for (size_t i = 0; i < table.headers.size(); ++i)
        {
            const auto normalized = med::date::Normalize(table.headers[i]);
            for (const auto& candidate : candidates)
            {
                const auto c = med::date::Normalize(candidate);
                if (!c.empty() && (normalized == c || normalized.find(c) != std::wstring::npos))
                    return static_cast<int>(i);
            }
        }
        return -1;
    }

    int FindHeaderExactOrLike(const med::SheetTable& table, const std::wstring& preferred, const std::vector<std::wstring>& fallbacks)
    {
        if (!med::date::Trim(preferred).empty())
        {
            const auto exact = med::date::Normalize(preferred);
            for (size_t i = 0; i < table.headers.size(); ++i)
                if (med::date::Normalize(table.headers[i]) == exact) return static_cast<int>(i);
            for (size_t i = 0; i < table.headers.size(); ++i)
                if (med::date::Normalize(table.headers[i]).find(exact) != std::wstring::npos) return static_cast<int>(i);
        }
        return FindHeaderLike(table, fallbacks);
    }

    std::wstring Timestamp()
    {
        SYSTEMTIME st{};
        ::GetLocalTime(&st);
        wchar_t buffer[64]{};
        swprintf_s(buffer, L"%04u%02u%02u_%02u%02u%02u", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
        return buffer;
    }

    const std::vector<med::CellValue>* FindSourceRow(const med::SheetTable& table, int excelRow)
    {
        for (size_t i = 0; i < table.rows.size(); ++i)
        {
            const int sourceRow = i < table.rowNumbers.size()
                ? table.rowNumbers[i]
                : table.firstRow + 1 + static_cast<int>(i);
            if (sourceRow == excelRow) return &table.rows[i];
        }
        return nullptr;
    }

    std::wstring NormalizeAuditInput(const std::wstring& header, const std::wstring& value)
    {
        const auto trimmed = med::date::Trim(value);
        if (trimmed.empty()) return L"";

        if (med::date::HeaderLooksLikeDate(header))
        {
            if (const auto parsed = med::date::ParseGermanDate(trimmed))
                return med::date::FormatGermanDate(*parsed);
        }
        return trimmed;
    }

    std::vector<med::AuditChange> BuildAuditChanges(
        const med::SheetTable& table,
        int excelRow,
        const std::map<std::wstring, std::wstring>& values)
    {
        std::vector<med::AuditChange> changes;
        const auto* oldRow = FindSourceRow(table, excelRow);

        for (size_t c = 0; c < table.headers.size(); ++c)
        {
            const auto& header = table.headers[c];
            const auto valueIt = values.find(header);
            if (valueIt == values.end()) continue;

            const std::wstring oldValue =
                oldRow && c < oldRow->size()
                ? med::date::Trim(ToText((*oldRow)[c], header))
                : L"";
            const std::wstring newValue = NormalizeAuditInput(header, valueIt->second);

            if (oldValue != newValue)
                changes.push_back({ header, oldValue, newValue });
        }
        return changes;
    }

    void WriteAudit(
        const med::StudyData& study,
        const med::ImpData* imp,
        const std::wstring& action,
        const std::wstring& sheet,
        int excelRow,
        std::vector<med::AuditChange> changes,
        const std::wstring& operationId = L"",
        const std::wstring& details = L"")
    {
        med::AuditEntry entry;
        entry.operationId = operationId;
        entry.imp = imp ? imp->name : L"";
        entry.action = action;
        entry.sheet = sheet;
        entry.excelRow = excelRow;
        entry.details = details;
        entry.changes = std::move(changes);
        med::AuditLogger::Write(study, entry);
    }

    bool AnyNonEmpty(const std::vector<med::CellValue>& row)
    {
        return std::any_of(row.begin(), row.end(), [](const med::CellValue& c) { return !c.IsEmpty(); });
    }

    std::wstring ReadRowText(const med::SheetTable& table, const std::vector<med::CellValue>& row,
        const std::vector<std::wstring>& names, const std::wstring& fallback = L"")
    {
        const int c = FindHeaderLike(table, names);
        return c >= 0 && static_cast<size_t>(c) < row.size() ? med::date::Trim(ToText(row[static_cast<size_t>(c)], table.headers[static_cast<size_t>(c)])) : fallback;
    }

    double ReadRowNumber(const med::SheetTable& table, const std::vector<med::CellValue>& row,
        const std::vector<std::wstring>& names, double fallback = 0.0)
    {
        const int c = FindHeaderLike(table, names);
        return c >= 0 && static_cast<size_t>(c) < row.size() ? ToNumber(row[static_cast<size_t>(c)], fallback) : fallback;
    }

    bool ReadRowBool(const med::SheetTable& table, const std::vector<med::CellValue>& row,
        const std::vector<std::wstring>& names, bool fallback)
    {
        const int c = FindHeaderLike(table, names);
        return c >= 0 && static_cast<size_t>(c) < row.size() ? ToBool(row[static_cast<size_t>(c)], fallback) : fallback;
    }

    bool LooksLikeRowStudyProfile(const med::SheetTable& table)
    {
        return FindHeaderLike(table, { L"IMP-Name", L"IMP Name", L"Medikament", L"Prüfpräparat" }) >= 0 &&
            (FindHeaderLike(table, { L"Warenart", L"Studienware", L"Bestellpflicht", L"Applikationsform" }) >= 0 ||
             FindHeaderLike(table, { L"IMP-ID", L"IMP ID" }) >= 0);
    }

    bool LooksLikeColumnStudyProfile(const med::SheetTable& table)
    {
        if (table.headers.size() < 2) return false;
        const auto first = med::date::Normalize(table.headers.front());
        if (first != L"imps" && first != L"imp") return false;
        for (const auto& row : table.rows)
        {
            if (row.empty()) continue;
            const auto label = med::date::Normalize(ToText(row.front()));
            if (label.find(L"imp bezeichnung") != std::wstring::npos ||
                label.find(L"applikationsform") != std::wstring::npos ||
                label.find(L"bestellpflicht") != std::wstring::npos ||
                label.find(L"lieferzeit") != std::wstring::npos)
                return true;
        }
        return false;
    }

    int FindProfilePropertyRow(const med::SheetTable& table, const std::vector<std::wstring>& candidates)
    {
        for (size_t r = 0; r < table.rows.size(); ++r)
        {
            if (table.rows[r].empty()) continue;
            const auto label = med::date::Normalize(ToText(table.rows[r][0]));
            for (const auto& candidate : candidates)
            {
                const auto c = med::date::Normalize(candidate);
                if (!c.empty() && (label == c || label.find(c) != std::wstring::npos)) return static_cast<int>(r);
            }
        }
        return -1;
    }

    med::CellValue ProfileCell(const med::SheetTable& table, const std::vector<std::wstring>& names, size_t column)
    {
        const int row = FindProfilePropertyRow(table, names);
        if (row < 0 || static_cast<size_t>(row) >= table.rows.size() || column >= table.rows[static_cast<size_t>(row)].size())
            return med::CellValue::Empty();
        return table.rows[static_cast<size_t>(row)][column];
    }

    std::wstring ProfileText(const med::SheetTable& table, const std::vector<std::wstring>& names, size_t column, const std::wstring& fallback = L"")
    {
        const auto cell = ProfileCell(table, names, column);
        const auto value = med::date::Trim(ToText(cell));
        return value.empty() ? fallback : value;
    }

    double ProfileNumber(const med::SheetTable& table, const std::vector<std::wstring>& names, size_t column, double fallback)
    {
        const auto cell = ProfileCell(table, names, column);
        return cell.IsEmpty() ? fallback : ToNumber(cell, fallback);
    }

    bool ProfileBool(const med::SheetTable& table, const std::vector<std::wstring>& names, size_t column, bool fallback)
    {
        const auto cell = ProfileCell(table, names, column);
        return cell.IsEmpty() ? fallback : ToBool(cell, fallback);
    }

    std::wstring AlphaNum(std::wstring value)
    {
        value = med::date::Normalize(value);
        std::wstring result;
        for (const wchar_t ch : value) if (std::iswalnum(ch)) result.push_back(ch);
        return result;
    }

    int NameScore(const std::wstring& a, const std::wstring& b)
    {
        const auto x = AlphaNum(a), y = AlphaNum(b);
        if (x.empty() || y.empty()) return 0;
        if (x == y) return 10000;
        if (x.find(y) != std::wstring::npos || y.find(x) != std::wstring::npos)
            return 7000 + static_cast<int>(std::min(x.size(), y.size()) * 20);
        size_t common = 0;
        while (common < x.size() && common < y.size() && x[common] == y[common]) ++common;
        int score = static_cast<int>(common * 500);
        // Abkürzungen wie GILT/Gilteritinib und VEN/VENAZA sollen zuverlässig erkannt werden.
        if (common >= 3) score += 2500;
        return score;
    }

    std::wstring InferOrderHeader(const med::SheetTable& table, const std::wstring& impName)
    {
        int best = -1;
        std::wstring result;
        for (const auto& header : table.headers)
        {
            const auto n = med::date::Normalize(header);
            if (n.empty() || n.find(L"bestellung") != std::wstring::npos || n == L"order" ||
                n.find(L"bestellt am") != std::wstring::npos || n == L"am" ||
                n.find(L"erhalten") != std::wstring::npos || n.find(L"received") != std::wstring::npos ||
                n.find(L"datum") != std::wstring::npos || n.find(L"date") != std::wstring::npos ||
                n.find(L"comment") != std::wstring::npos || n.find(L"bemerk") != std::wstring::npos)
                continue;
            const int score = NameScore(impName, header);
            if (score > best) { best = score; result = header; }
        }
        return best >= 2500 ? result : L"";
    }

    std::wstring InferInventoryValue(const med::SheetTable& table, const std::wstring& filterHeader,
        const std::wstring& impName, const std::wstring& orderHeader)
    {
        const int col = FindHeaderExactOrLike(table, filterHeader, {});
        if (col < 0) return L"";
        std::set<std::wstring> unique;
        for (const auto& row : table.rows)
        {
            if (static_cast<size_t>(col) >= row.size()) continue;
            const auto value = med::date::Trim(ToText(row[static_cast<size_t>(col)], table.headers[static_cast<size_t>(col)]));
            if (!value.empty()) unique.insert(value);
        }
        int best = -1;
        std::wstring result;
        for (const auto& value : unique)
        {
            const int score = std::max(NameScore(impName, value), NameScore(orderHeader, value));
            if (score > best) { best = score; result = value; }
        }
        return best >= 2500 ? result : L"";
    }

    med::SheetTable FilterInventoryForImp(const med::SheetTable& source, const med::ImpData& imp)
    {
        if (source.headers.empty() || med::date::Trim(imp.inventoryImpHeader).empty()) return source;
        const int filterCol = FindHeaderExactOrLike(source, imp.inventoryImpHeader, {});
        if (filterCol < 0) return source;

        med::SheetTable filtered = source;
        filtered.rows.clear();
        filtered.rowNumbers.clear();
        const auto wanted = med::date::Normalize(imp.inventoryImpValue.empty() ? imp.name : imp.inventoryImpValue);
        for (size_t r = 0; r < source.rows.size(); ++r)
        {
            const auto& row = source.rows[r];
            if (static_cast<size_t>(filterCol) >= row.size()) continue;
            const auto actual = med::date::Normalize(ToText(row[static_cast<size_t>(filterCol)], source.headers[static_cast<size_t>(filterCol)]));
            if (actual == wanted)
            {
                filtered.rows.push_back(row);
                filtered.rowNumbers.push_back(r < source.rowNumbers.size() ? source.rowNumbers[r] : source.firstRow + 1 + static_cast<int>(r));
            }
        }
        return filtered;
    }

    int StatePriority(med::OrderState state)
    {
        switch (state)
        {
        case med::OrderState::Due: return 0;
        case med::OrderState::Soon: return 1;
        case med::OrderState::Pending: return 2;
        case med::OrderState::Ok: return 3;
        case med::OrderState::NotRequired: return 4;
        default: return 5;
        }
    }
}

namespace med
{
    int ExcelStudyRepository::FindHeader(const SheetTable& table, const std::wstring& keyContains)
    {
        const auto key = date::Normalize(keyContains);
        for (size_t i = 0; i < table.headers.size(); ++i)
        {
            if (date::Normalize(table.headers[i]).find(key) != std::wstring::npos)
                return static_cast<int>(i);
        }
        return -1;
    }

    int ExcelStudyRepository::LastNonEmptyDataRow(const SheetTable& table)
    {
        for (int i = static_cast<int>(table.rows.size()) - 1; i >= 0; --i)
            if (AnyNonEmpty(table.rows[static_cast<size_t>(i)])) return i;
        return -1;
    }

    bool ExcelStudyRepository::ColumnLooksNumeric(const SheetTable& table, int columnIndex)
    {
        if (columnIndex < 0) return false;
        for (const auto& row : table.rows)
        {
            if (static_cast<size_t>(columnIndex) < row.size() && !row[static_cast<size_t>(columnIndex)].IsEmpty())
                return row[static_cast<size_t>(columnIndex)].type == CellType::Number;
        }
        return false;
    }

    CellValue ExcelStudyRepository::InputToCell(const std::wstring& header, const std::wstring& text, const SheetTable& table, int columnIndex)
    {
        const auto trimmed = date::Trim(text);
        if (trimmed.empty()) return CellValue::Empty();

        if (date::HeaderLooksLikeDate(header))
        {
            if (const auto d = date::ParseGermanDate(trimmed))
                return CellValue::Number(date::ToExcelSerial(*d));
            throw std::runtime_error("Ungültiges Datum. Bitte TT.MM.JJJJ verwenden.");
        }

        if (ColumnLooksNumeric(table, columnIndex))
        {
            try
            {
                auto s = trimmed;
                std::replace(s.begin(), s.end(), L',', L'.');
                return CellValue::Number(std::stod(s));
            }
            catch (...)
            {
                throw std::runtime_error("Numerisches Feld enthält keinen gültigen Zahlenwert.");
            }
        }
        return CellValue::Text(trimmed);
    }

    std::wstring ExcelStudyRepository::NextOrderNumber(const SheetTable& orders)
    {
        int maxNumber = 0;
        const int col = FindHeaderLike(orders, { L"Bestellung", L"Order", L"Bestellnummer" });
        if (col >= 0)
        {
            for (const auto& row : orders.rows)
            {
                if (static_cast<size_t>(col) >= row.size()) continue;
                auto s = ToText(row[static_cast<size_t>(col)]);
                std::wstring digits;
                for (wchar_t ch : s) if (std::iswdigit(ch)) digits.push_back(ch);
                if (!digits.empty())
                {
                    try { maxNumber = std::max(maxNumber, std::stoi(digits)); } catch (...) {}
                }
            }
        }
        wchar_t buffer[16]{};
        swprintf_s(buffer, L"#%02d", maxNumber + 1);
        return buffer;
    }

    std::wstring ExcelStudyRepository::NextVialPrefix(const SheetTable& inventory)
    {
        const int col = FindHeaderLike(inventory, { L"Vial No", L"Kit No", L"Box-Nr", L"Box Nr", L"Box No", L"Box Number", L"Pack No", L"Serial", L"Container", L"Vial", L"Kit", L"Box", L"Pack" });
        int maxBatch = 0;
        if (col >= 0)
        {
            for (const auto& row : inventory.rows)
            {
                if (static_cast<size_t>(col) >= row.size()) continue;
                auto value = ToText(row[static_cast<size_t>(col)]);
                if (value.size() >= 2 && (value[0] == L'B' || value[0] == L'b'))
                {
                    size_t i = 1;
                    std::wstring digits;
                    while (i < value.size() && std::iswdigit(value[i])) digits.push_back(value[i++]);
                    if (!digits.empty())
                    {
                        try { maxBatch = std::max(maxBatch, std::stoi(digits)); } catch (...) {}
                    }
                }
            }
        }
        return L"B" + std::to_wstring(maxBatch + 1);
    }

    void ExcelStudyRepository::LoadVisitModel(StudyData& study)
    {
        study.patientIds.clear();
        study.visitMetadataHeaders.clear();
        study.visitRows.clear();
        study.visits.clear();

        const auto& table = study.visitTable;
        if (table.headers.empty()) return;

        int patientStart = study.config.patientColumnsStart;
        if (patientStart <= 0)
        {
            // Automatisch die erste Spalte erkennen, deren Daten überwiegend Datumswerte sind.
            // Dadurch dürfen vor den Patienten beliebig viele Metadatenfelder wie Visite, ECP,
            // Zyklus, Dosisstufe usw. stehen.
            patientStart = 2;
            for (size_t c = 1; c < table.headers.size(); ++c)
            {
                int dateCount = 0;
                int nonEmpty = 0;
                for (size_t r = 0; r < table.rows.size() && r < 30; ++r)
                {
                    if (c >= table.rows[r].size() || table.rows[r][c].IsEmpty()) continue;
                    ++nonEmpty;
                    if (date::CellToDate(table.rows[r][c])) ++dateCount;
                }
                if (dateCount > 0 && dateCount * 2 >= std::max(1, nonEmpty))
                {
                    patientStart = static_cast<int>(c) + 1; // 1-basiert
                    break;
                }
            }
        }
        patientStart = std::clamp(patientStart, 2, static_cast<int>(table.headers.size()) + 1);
        const int metadataCount = patientStart - 1;

        for (int c = 0; c < metadataCount && c < static_cast<int>(table.headers.size()); ++c)
            study.visitMetadataHeaders.push_back(table.headers[static_cast<size_t>(c)]);

        for (size_t c = static_cast<size_t>(metadataCount); c < table.headers.size(); ++c)
        {
            auto patient = date::Trim(table.headers[c]);
            if (!patient.empty()) study.patientIds.push_back(patient);
        }

        const int lastRow = LastNonEmptyDataRow(table);
        if (lastRow < 0) return;
        for (int r = 0; r <= lastRow; ++r)
        {
            const auto& row = table.rows[static_cast<size_t>(r)];
            VisitRowDefinition def;
            def.excelRow = table.firstRow + 1 + r;
            bool hasMetadata = false;
            for (int c = 0; c < metadataCount; ++c)
            {
                const std::wstring header = c < static_cast<int>(table.headers.size()) ? table.headers[static_cast<size_t>(c)] : L"Feld";
                const std::wstring value = c < static_cast<int>(row.size()) ? ToText(row[static_cast<size_t>(c)], header) : L"";
                if (!value.empty()) hasMetadata = true;
                def.metadata.emplace_back(header, value);
            }
            if (!hasMetadata) continue;
            study.visitRows.push_back(def);

            for (size_t c = static_cast<size_t>(metadataCount); c < table.headers.size() && c < row.size(); ++c)
            {
                const auto patient = date::Trim(table.headers[c]);
                if (patient.empty()) continue;
                if (const auto d = date::CellToDate(row[c]))
                {
                    VisitRecord record;
                    record.excelRow = def.excelRow;
                    record.excelColumn = table.firstColumn + static_cast<int>(c);
                    record.patientId = patient;
                    record.metadata = def.metadata;
                    record.date = *d;
                    record.hasDate = true;
                    study.visits.push_back(std::move(record));
                }
            }
        }
    }

    void ExcelStudyRepository::LoadDocuments(StudyData& study)
    {
        study.documents.clear();
        const auto root = study.studyDirectory / L"Documents";
        if (!std::filesystem::exists(root)) return;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root))
        {
            if (!entry.is_regular_file()) continue;
            DocumentEntry document;
            document.path = entry.path();
            document.title = entry.path().stem().wstring();
            const auto relative = std::filesystem::relative(entry.path().parent_path(), root);
            document.category = relative.empty() || relative == L"." ? L"Allgemein" : relative.wstring();
            study.documents.push_back(std::move(document));
        }
        std::sort(study.documents.begin(), study.documents.end(), [](const auto& a, const auto& b)
        {
            if (a.category != b.category) return a.category < b.category;
            return a.title < b.title;
        });
    }

    void ExcelStudyRepository::DetectPendingOrder(StudyData&, ImpData& imp)
    {
        imp.pendingOrder = false;
        imp.pendingOrderDate.reset();
        if (imp.orders.headers.empty() || !imp.orderRequired) return;

        const int quantityCol = FindHeaderExactOrLike(imp.orders, imp.orderQuantityHeader, { imp.name });
        const int orderCol = FindHeaderLike(imp.orders, { L"Bestellung", L"Order", L"Bestellnummer" });
        const int dateCol = FindHeaderLike(imp.orders, { L"Bestelldatum", L"bestellt am", L"OrderDate", L"Am" });
        const int receivedCol = FindHeaderLike(imp.orders, { L"Erhalten am", L"Received", L"Wareneingang" });
        if (receivedCol < 0) return;

        for (int r = LastNonEmptyDataRow(imp.orders); r >= 0; --r)
        {
            const auto& row = imp.orders.rows[static_cast<size_t>(r)];
            bool relevant = false;
            if (quantityCol >= 0 && static_cast<size_t>(quantityCol) < row.size()) relevant = !row[static_cast<size_t>(quantityCol)].IsEmpty();
            else if (orderCol >= 0 && static_cast<size_t>(orderCol) < row.size()) relevant = !row[static_cast<size_t>(orderCol)].IsEmpty();
            else relevant = AnyNonEmpty(row);
            if (!relevant) continue;

            if (static_cast<size_t>(receivedCol) >= row.size() || row[static_cast<size_t>(receivedCol)].IsEmpty())
            {
                imp.pendingOrder = true;
                if (dateCol >= 0 && static_cast<size_t>(dateCol) < row.size()) imp.pendingOrderDate = date::CellToDate(row[static_cast<size_t>(dateCol)]);
            }
            break;
        }
    }

    void ExcelStudyRepository::ComputeStock(ImpData& imp)
    {
        if (imp.inventory.headers.empty()) return; // Profil-Bestand ggf. beibehalten

        const int receivedQtyCol = FindHeaderLike(imp.inventory, { L"Quantity Received", L"Anzahl erhalten", L"Menge erhalten", L"Received Quantity" });
        const int dispensedQtyCol = FindHeaderLike(imp.inventory, { L"Quantity Dispensed", L"Anzahl ausgegeben", L"Menge ausgegeben", L"Dispensed Quantity" });
        if (receivedQtyCol >= 0)
        {
            double received = 0.0, dispensed = 0.0;
            for (const auto& row : imp.inventory.rows)
            {
                if (static_cast<size_t>(receivedQtyCol) < row.size()) received += ToNumber(row[static_cast<size_t>(receivedQtyCol)], 0.0);
                if (dispensedQtyCol >= 0 && static_cast<size_t>(dispensedQtyCol) < row.size()) dispensed += ToNumber(row[static_cast<size_t>(dispensedQtyCol)], 0.0);
            }
            imp.stock = std::max(0.0, received - dispensed);
            return;
        }

        const int itemCol = FindHeaderLike(imp.inventory, { L"Vial No", L"Kit No", L"KitNo", L"Box-Nr", L"Box Nr", L"Box No", L"Box Number", L"Serial", L"Pack No", L"Package No", L"Container", L"Vial", L"Kit", L"Box", L"Pack" });
        const int dispensingCol = FindHeaderLike(imp.inventory, { L"DispensingDate", L"Dispensing Date", L"Datum Ausgabe", L"Ausgaben am", L"Dispensed" });
        const int patientCol = FindHeaderLike(imp.inventory, { L"Pat.ID", L"PatID", L"PatientID", L"Patient" });
        if (itemCol < 0) return;

        int received = 0, dispensed = 0;
        for (const auto& row : imp.inventory.rows)
        {
            if (static_cast<size_t>(itemCol) < row.size() && !row[static_cast<size_t>(itemCol)].IsEmpty()) ++received;
            const bool hasDispensing = dispensingCol >= 0 && static_cast<size_t>(dispensingCol) < row.size() && !row[static_cast<size_t>(dispensingCol)].IsEmpty();
            const bool hasPatient = patientCol >= 0 && static_cast<size_t>(patientCol) < row.size() && !row[static_cast<size_t>(patientCol)].IsEmpty();
            if (hasDispensing || hasPatient) ++dispensed;
        }
        imp.stock = static_cast<double>(std::max(0, received - dispensed));
    }

    void ExcelStudyRepository::SyncLegacyFields(StudyData& study)
    {
        study.medication.clear();
        for (const auto& imp : study.imps)
        {
            if (!study.medication.empty()) study.medication += L", ";
            study.medication += imp.name;
        }

        if (!study.imps.empty())
        {
            const auto& first = study.imps.front();
            study.stock = first.stock;
            study.minimumStock = first.minimumStock;
            study.leadTimeDays = first.leadTimeDays;
            study.bufferDays = first.bufferDays;
            study.orders = first.orders;
            study.inventory = first.inventory;
            study.pendingOrder = first.pendingOrder;
            study.pendingOrderDate = first.pendingOrderDate;
        }

        const ImpData* worst = nullptr;
        for (const auto& imp : study.imps)
        {
            if (!imp.orderRequired) continue;
            if (!worst || StatePriority(imp.plan.state) < StatePriority(worst->plan.state)) worst = &imp;
        }
        if (worst) study.plan = worst->plan;
        else
        {
            study.plan.state = OrderState::NotRequired;
            study.plan.explanation = L"Für diese Studie ist aktuell kein bestellpflichtiges IMP definiert.";
        }
    }

    StudyData ExcelStudyRepository::LoadOne(const std::filesystem::path& excelPath) const
    {
        excel::Application excel;
        return LoadOneWithApplication(excelPath, excel);
    }

    StudyData ExcelStudyRepository::LoadOneWithApplication(const std::filesystem::path& excelPath, excel::Application& excel) const
    {
        StudyData study;
        study.excelPath = excelPath;
        study.studyDirectory = excelPath.parent_path();
        study.lastWriteTime = std::filesystem::last_write_time(excelPath);
        study.config = LoadStudyConfig(study.studyDirectory);

        auto workbook = excel.Open(excelPath, true);

        // Mehrere IMPs verwenden häufig dieselben Tabellenblätter (z. B. ein gemeinsames
        // DrugInventory und eine gemeinsame Bestellübersicht). Diese Blätter wurden
        // bislang pro IMP erneut über COM gelesen. Ein kleiner Cache hält jedes Blatt
        // innerhalb dieses Ladevorgangs nur einmal im Speicher.
        std::map<std::wstring, bool> hasSheetCache;
        std::map<std::wstring, SheetTable> sheetCache;
        auto hasSheetCached = [&](const std::wstring& name)
        {
            if (name.empty()) return false;
            if (auto it = hasSheetCache.find(name); it != hasSheetCache.end()) return it->second;
            const bool exists = workbook.HasSheet(name);
            hasSheetCache.emplace(name, exists);
            return exists;
        };
        auto readSheetCached = [&](const std::wstring& name) -> const SheetTable&
        {
            auto it = sheetCache.find(name);
            if (it == sheetCache.end())
                it = sheetCache.emplace(name, workbook.ReadSheet(name)).first;
            return it->second;
        };

        const auto firstSheet = workbook.FirstSheetName();
        if (!firstSheet.empty())
        {
            hasSheetCache[firstSheet] = true;
            study.profileTable = readSheetCached(firstSheet);
        }
        const bool rowProfile = LooksLikeRowStudyProfile(study.profileTable);
        const bool columnProfile = LooksLikeColumnStudyProfile(study.profileTable);
        study.hasStudyProfile = rowProfile || columnProfile;
        study.profileColumnLayout = columnProfile;

        if (hasSheetCached(study.config.stockSheet)) study.stockTable = readSheetCached(study.config.stockSheet);
        if (hasSheetCached(study.config.visitSheet)) study.visitTable = readSheetCached(study.config.visitSheet);

        // Studienidentität: immer der Unterordnername. Excel-Stammdaten dienen als
        // beschreibende Metadaten und Plausibilitätsprüfung.
        study.studyName = date::Trim(study.studyDirectory.filename().wstring());
        if (study.studyName.empty()) study.studyName = excelPath.stem().wstring();

        if (!firstSheet.empty())
        {
            // Die beiden Stammdaten stehen im spaltenorientierten Profil bewusst oberhalb
            // der eigentlichen Tabellenkopfzeile und würden durch ReadSheet sonst nicht
            // als Datenzeilen auftauchen.
            for (int r = 1; r <= 12; ++r)
            {
                const auto label = date::Normalize(ToText(workbook.GetCell(firstSheet, r, 1)));
                const auto value = date::Trim(ToText(workbook.GetCell(firstSheet, r, 2)));
                if (label.find(L"studienbezeichnung") != std::wstring::npos || label == L"studie" || label == L"study")
                {
                    if (!value.empty()) study.workbookStudyName = value;
                }
                if (label.find(L"euct") != std::wstring::npos) study.euctNumber = value;
            }
        }

        // Alle Zeilen des Meldebestands als Legacy-/Fallback-IMP erfassen. Damit wird
        // eine Multi-IMP-Studie nicht mehr auf die erste Zeile reduziert.
        std::vector<ImpData> stockImps;
        if (!study.stockTable.headers.empty())
        {
            for (const auto& row : study.stockTable.rows)
            {
                if (!AnyNonEmpty(row)) continue;
                ImpData imp;
                imp.name = ReadRowText(study.stockTable, row, { L"Medikament", L"Medication", L"Drug", L"IMP" });
                if (imp.name.empty()) continue;
                const auto excelStudy = ReadRowText(study.stockTable, row, { L"Studie", L"Study" });
                if (study.workbookStudyName.empty() && !excelStudy.empty()) study.workbookStudyName = excelStudy;
                imp.id = imp.name;
                imp.goodsType = L"Studienware";
                imp.orderRequired = true;
                imp.stock = ReadRowNumber(study.stockTable, row, { L"Bestand", L"Stock" }, 0.0);
                imp.minimumStock = ReadRowNumber(study.stockTable, row, { L"Mindestbestand", L"Mindesbestand", L"Minimum" }, 0.0);
                imp.leadTimeDays = static_cast<int>(ReadRowNumber(study.stockTable, row, { L"Lieferzeit", L"LeadTime" }, 0.0));
                imp.bufferDays = static_cast<int>(ReadRowNumber(study.stockTable, row, { L"Pufferzeit", L"Anzahl Tage zwischen den Gaben" }, 7.0));
                imp.consumptionPerVisit = study.config.consumptionPerVisit;
                imp.orderSheet = study.config.orderSheet;
                imp.inventorySheet = study.config.inventorySheet;
                imp.documentCategory = L"Bestellung";
                if (study.excelSignal.empty()) study.excelSignal = ReadRowText(study.stockTable, row, { L"Bestellsignal", L"Signal" });
                stockImps.push_back(std::move(imp));
            }
        }

        if (rowProfile)
        {
            size_t profileIndex = 0;
            for (const auto& row : study.profileTable.rows)
            {
                if (!AnyNonEmpty(row)) continue;
                ImpData imp = profileIndex < stockImps.size() ? stockImps[profileIndex] : ImpData{};
                const auto profileName = ReadRowText(study.profileTable, row, { L"IMP-Name", L"IMP Name", L"Medikament", L"Prüfpräparat" });
                if (!profileName.empty()) imp.name = profileName;
                if (imp.name.empty()) { ++profileIndex; continue; }
                imp.id = ReadRowText(study.profileTable, row, { L"IMP-ID", L"IMP ID" }, imp.id.empty() ? imp.name : imp.id);
                imp.goodsType = ReadRowText(study.profileTable, row, { L"Warenart", L"Typ" }, imp.goodsType.empty() ? L"Studienware" : imp.goodsType);
                imp.applicationForm = ReadRowText(study.profileTable, row, { L"Applikationsform", L"Darreichungsform" }, imp.applicationForm);
                const bool defaultOrder = date::Normalize(imp.goodsType).find(L"handelsware") == std::wstring::npos;
                imp.orderRequired = ReadRowBool(study.profileTable, row, { L"Bestellpflicht", L"Bestellen", L"Bestellpflichtig" }, defaultOrder);
                imp.minimumStock = ReadRowNumber(study.profileTable, row, { L"Mindestbestand", L"Mindesbestand", L"Minimum" }, imp.minimumStock);
                imp.leadTimeDays = static_cast<int>(ReadRowNumber(study.profileTable, row, { L"Lieferzeit", L"LeadTime" }, imp.leadTimeDays));
                imp.consumptionPerVisit = ReadRowNumber(study.profileTable, row, { L"Verbrauch pro Visite", L"Verbrauch pro Gabe", L"Consumption" }, imp.consumptionPerVisit);
                imp.bufferDays = static_cast<int>(ReadRowNumber(study.profileTable, row, { L"Pufferzeit", L"Bestellpuffer", L"Zeit zwischen den Gaben" }, imp.bufferDays));
                imp.forecastIntervalDays = static_cast<int>(ReadRowNumber(study.profileTable, row, { L"Prognoseintervall", L"Visitenintervall", L"Forecast Interval" }, imp.forecastIntervalDays));
                imp.stock = ReadRowNumber(study.profileTable, row, { L"Bestand", L"Startbestand" }, imp.stock);
                imp.orderSheet = ReadRowText(study.profileTable, row, { L"Bestellblatt", L"Arbeitsblattbezeichnung", L"OrderSheet" }, imp.orderSheet.empty() ? study.config.orderSheet : imp.orderSheet);
                imp.orderQuantityHeader = ReadRowText(study.profileTable, row, { L"Bestellspalte", L"Spaltenbezeichnung", L"OrderColumn" }, imp.orderQuantityHeader);
                imp.inventorySheet = ReadRowText(study.profileTable, row, { L"Inventarblatt", L"InventorySheet" }, imp.inventorySheet.empty() ? study.config.inventorySheet : imp.inventorySheet);
                imp.inventoryImpHeader = ReadRowText(study.profileTable, row, { L"Inventar-IMP-Spalte", L"Spaltenbezeichnung für IMP", L"Inventory IMP Column", L"IMP-Spalte" }, imp.inventoryImpHeader);
                imp.inventoryImpValue = ReadRowText(study.profileTable, row, { L"Inventar-IMP-Wert", L"Inventarwert für IMP", L"Inventory IMP Value" }, imp.inventoryImpValue);
                imp.visitFilter = ReadRowText(study.profileTable, row, { L"Visitenfilter", L"VisitFilter" }, imp.visitFilter);
                imp.orderProcess = ReadRowText(study.profileTable, row, { L"Bestellverfahren", L"OrderProcess" }, imp.orderProcess);
                imp.documentCategory = ReadRowText(study.profileTable, row, { L"Dokumentkategorie", L"Dokumentenkategorie", L"Dokumentordner", L"DocumentCategory" }, imp.documentCategory.empty() ? L"Bestellung" : imp.documentCategory);
                const auto excelStudy = ReadRowText(study.profileTable, row, { L"Studie", L"Study" });
                if (study.workbookStudyName.empty() && !excelStudy.empty()) study.workbookStudyName = excelStudy;
                study.imps.push_back(std::move(imp));
                ++profileIndex;
            }
        }
        else if (columnProfile)
        {
            // SEQUENCE-Layout: IMPs stehen als Spalten, Eigenschaften als Zeilen.
            for (size_t c = 1; c < study.profileTable.headers.size(); ++c)
            {
                ImpData imp = (c - 1) < stockImps.size() ? stockImps[c - 1] : ImpData{};
                const auto configuredName = ProfileText(study.profileTable, { L"IMP Bezeichnung", L"IMP-Name", L"Medikament" }, c);
                if (!configuredName.empty()) imp.name = configuredName;
                if (imp.name.empty()) continue; // leere IMP3..IMP6-Spalten nicht als Phantom-IMP anlegen

                imp.id = date::Trim(study.profileTable.headers[c]);
                if (imp.id.empty()) imp.id = imp.name;
                imp.applicationForm = ProfileText(study.profileTable, { L"Applikationsform", L"Darreichungsform" }, c, imp.applicationForm);
                imp.goodsType = ProfileText(study.profileTable, { L"Studienware/Handelsware", L"Warenart" }, c, imp.goodsType.empty() ? L"Studienware" : imp.goodsType);
                const bool defaultOrder = date::Normalize(imp.goodsType).find(L"handelsware") == std::wstring::npos;
                imp.orderRequired = ProfileBool(study.profileTable, { L"Von STA Bestellpflichtig", L"Bestellpflichtig", L"Bestellpflicht" }, c, defaultOrder);
                imp.minimumStock = ProfileNumber(study.profileTable, { L"Mindestbestand", L"Mindesbestand", L"Minimum" }, c, imp.minimumStock);
                imp.leadTimeDays = static_cast<int>(ProfileNumber(study.profileTable, { L"Lieferzeit (Tage)", L"Lieferzeit", L"LeadTime" }, c, imp.leadTimeDays));
                imp.consumptionPerVisit = ProfileNumber(study.profileTable, { L"Verbrauch pro Visite", L"Verbrauch pro Gabe" }, c, imp.consumptionPerVisit);
                imp.bufferDays = static_cast<int>(ProfileNumber(study.profileTable, { L"Zeit zwischen den Gaben", L"Pufferzeit", L"Bestellpuffer" }, c, imp.bufferDays));
                imp.forecastIntervalDays = static_cast<int>(ProfileNumber(study.profileTable, { L"Prognoseintervall", L"Visitenintervall" }, c, imp.forecastIntervalDays));
                imp.orderSheet = ProfileText(study.profileTable, { L"Arbeitsblattbezeichnung", L"Bestellblatt", L"OrderSheet" }, c, imp.orderSheet.empty() ? study.config.orderSheet : imp.orderSheet);
                imp.orderQuantityHeader = ProfileText(study.profileTable, { L"Spaltenbezeichnung", L"Bestellspalte", L"OrderColumn" }, c, imp.orderQuantityHeader);
                imp.inventorySheet = ProfileText(study.profileTable, { L"Inventarblatt", L"InventorySheet" }, c, imp.inventorySheet.empty() ? study.config.inventorySheet : imp.inventorySheet);
                imp.inventoryImpHeader = ProfileText(study.profileTable, { L"Spaltenbezeichnung für IMP", L"Inventar-IMP-Spalte", L"Inventory IMP Column" }, c, imp.inventoryImpHeader);
                imp.inventoryImpValue = ProfileText(study.profileTable, { L"Inventarwert für IMP", L"Inventar-IMP-Wert", L"Inventory IMP Value" }, c, imp.inventoryImpValue);
                imp.visitFilter = ProfileText(study.profileTable, { L"Visitenfilter", L"VisitFilter" }, c, imp.visitFilter);
                imp.orderProcess = ProfileText(study.profileTable, { L"Bestellverfahren", L"OrderProcess" }, c, imp.orderProcess);
                imp.documentCategory = ProfileText(study.profileTable, { L"Dokumentenkategorie", L"Dokumentkategorie", L"DocumentCategory" }, c, imp.documentCategory.empty() ? L"Bestellung" : imp.documentCategory);
                study.imps.push_back(std::move(imp));
            }
        }

        // Wenn das Profil noch nicht vollständig gepflegt ist, sämtliche IMPs aus dem
        // Meldebestand ergänzen, die noch nicht in der Profilauflösung vorkommen.
        for (const auto& base : stockImps)
        {
            const auto exists = std::any_of(study.imps.begin(), study.imps.end(), [&](const ImpData& current)
            {
                return NameScore(current.name, base.name) >= 7000;
            });
            if (!exists) study.imps.push_back(base);
        }

        if (study.imps.empty())
        {
            if (study.stockTable.headers.empty())
                throw std::runtime_error("Weder ein lesbares Studienprofil noch ein lesbarer Meldebestand mit IMP-Daten wurde gefunden.");
            ImpData imp;
            imp.name = study.config.medicationOverride.empty() ? L"IMP1" : study.config.medicationOverride;
            imp.id = imp.name;
            imp.goodsType = L"Studienware";
            imp.orderRequired = true;
            imp.consumptionPerVisit = study.config.consumptionPerVisit;
            imp.orderSheet = study.config.orderSheet;
            imp.inventorySheet = study.config.inventorySheet;
            imp.documentCategory = L"Bestellung";
            study.imps.push_back(std::move(imp));
        }

        // Fachtabellen laden und bei gemeinsamem Inventory die IMP-Kurzbezeichnungen
        // (z. B. GILT/VEN) robust auflösen.
        for (auto& imp : study.imps)
        {
            if (imp.orderSheet.empty()) imp.orderSheet = study.config.orderSheet;
            if (imp.inventorySheet.empty()) imp.inventorySheet = study.config.inventorySheet;

            if (!imp.orderSheet.empty() && hasSheetCached(imp.orderSheet))
            {
                imp.orders = readSheetCached(imp.orderSheet);
                if (FindHeaderExactOrLike(imp.orders, imp.orderQuantityHeader, {}) < 0)
                {
                    const auto inferred = InferOrderHeader(imp.orders, imp.name);
                    if (!inferred.empty()) imp.orderQuantityHeader = inferred;
                }
            }

            if (!imp.inventorySheet.empty() && hasSheetCached(imp.inventorySheet))
            {
                const auto& rawInventory = readSheetCached(imp.inventorySheet);
                if (imp.inventoryImpHeader.empty() && study.imps.size() > 1)
                {
                    const int autoImp = FindHeaderLike(rawInventory, { L"IMP", L"Medication", L"Medikament", L"Drug", L"Product" });
                    if (autoImp >= 0) imp.inventoryImpHeader = rawInventory.headers[static_cast<size_t>(autoImp)];
                }
                if (!imp.inventoryImpHeader.empty())
                {
                    const auto inferred = InferInventoryValue(rawInventory, imp.inventoryImpHeader, imp.name, imp.orderQuantityHeader);
                    if (imp.inventoryImpValue.empty() || FindHeaderExactOrLike(rawInventory, imp.inventoryImpHeader, {}) >= 0)
                    {
                        // Einen nur aus dem langen Namen gesetzten Standardwert durch die
                        // tatsächlich vorkommende Kurzbezeichnung ersetzen, sofern ermittelbar.
                        if (!inferred.empty()) imp.inventoryImpValue = inferred;
                    }
                }
                imp.inventory = FilterInventoryForImp(rawInventory, imp);
            }
        }

        workbook.Close(false);

        if (!study.workbookStudyName.empty() && date::Normalize(study.workbookStudyName) != date::Normalize(study.studyName))
        {
            study.loadWarning = L"In der Excel ist als Studie '" + study.workbookStudyName + L"' hinterlegt; im Programm wird der Studienordner '" + study.studyName + L"' als eindeutige Identität verwendet.";
        }

        LoadVisitModel(study);

        for (auto& imp : study.imps)
        {
            ComputeStock(imp);
            DetectPendingOrder(study, imp);
            imp.plan = OrderPlanner::Calculate(study, imp, date::Today());
        }

        // Patienten auch aus sämtlichen IMP-Inventaren ergänzen.
        for (const auto& imp : study.imps)
        {
            const int inventoryPatientCol = FindHeaderLike(imp.inventory, { L"Pat.ID", L"PatID", L"PatientID", L"Patient" });
            if (inventoryPatientCol < 0) continue;
            for (const auto& row : imp.inventory.rows)
            {
                if (static_cast<size_t>(inventoryPatientCol) >= row.size()) continue;
                const auto patient = date::Trim(ToText(row[static_cast<size_t>(inventoryPatientCol)], imp.inventory.headers[static_cast<size_t>(inventoryPatientCol)]));
                if (patient.empty()) continue;
                const auto exists = std::any_of(study.patientIds.begin(), study.patientIds.end(), [&](const std::wstring& current)
                {
                    return date::Normalize(current) == date::Normalize(patient);
                });
                if (!exists) study.patientIds.push_back(patient);
            }
        }

        LoadDocuments(study);
        SyncLegacyFields(study);
        return study;
    }

    std::vector<StudyData> ExcelStudyRepository::LoadAll(const std::filesystem::path& studyFolder) const
    {
        std::vector<StudyData> studies;
        if (!std::filesystem::exists(studyFolder)) std::filesystem::create_directories(studyFolder);

        size_t candidateFiles = 0;
        std::string firstError;
        std::vector<std::filesystem::path> studyDirectories;
        std::unique_ptr<excel::Application> sharedExcel;
        for (const auto& entry : std::filesystem::directory_iterator(studyFolder))
            if (entry.is_directory()) studyDirectories.push_back(entry.path());
        std::sort(studyDirectories.begin(), studyDirectories.end());

        for (const auto& directory : studyDirectories)
        {
            const auto config = LoadStudyConfig(directory);
            std::vector<std::filesystem::path> candidates;
            if (!config.workbookFile.empty())
            {
                auto configured = std::filesystem::path(config.workbookFile);
                if (configured.is_relative()) configured = directory / configured;
                if (std::filesystem::exists(configured) && std::filesystem::is_regular_file(configured) && IsXlsx(configured)) candidates.push_back(configured);
            }
            if (candidates.empty())
            {
                for (const auto& file : std::filesystem::directory_iterator(directory))
                {
                    if (!file.is_regular_file() || !IsXlsx(file.path())) continue;
                    const auto filename = file.path().filename().wstring();
                    if (filename.rfind(L"~$", 0) == 0) continue;
                    candidates.push_back(file.path());
                }
                std::sort(candidates.begin(), candidates.end());
            }

            for (const auto& candidate : candidates)
            {
                ++candidateFiles;
                try
                {
                    // Ein Excel-Prozess für den kompletten Ordner statt ein neuer Prozess
                    // pro Studie. Das reduziert insbesondere den Start-/Aktualisieren-
                    // Overhead bei mehreren Studien deutlich.
                    if (!sharedExcel) sharedExcel = std::make_unique<excel::Application>();
                    studies.push_back(LoadOneWithApplication(candidate, *sharedExcel));
                    break;
                }
                catch (const std::exception& e) { if (firstError.empty()) firstError = e.what(); }
            }
        }

        if (candidateFiles > 0 && studies.empty() && !firstError.empty()) throw std::runtime_error(firstError);
        std::sort(studies.begin(), studies.end(), [](const StudyData& a, const StudyData& b)
        {
            const int pa = StatePriority(a.plan.state), pb = StatePriority(b.plan.state);
            return pa == pb ? a.studyName < b.studyName : pa < pb;
        });
        return studies;
    }

    void ExcelStudyRepository::CheckUnchanged(const StudyData& study)
    {
        if (!std::filesystem::exists(study.excelPath)) throw std::runtime_error("Die Studien-Excel wurde verschoben oder gelöscht.");
        if (std::filesystem::last_write_time(study.excelPath) != study.lastWriteTime)
            throw std::runtime_error("Die Excel-Datei wurde seit dem letzten Einlesen extern verändert. Bitte zuerst 'Aktualisieren' verwenden, damit keine Änderungen überschrieben werden.");
    }

    void ExcelStudyRepository::CreateBackup(const StudyData& study)
    {
        const auto backupDir = study.studyDirectory / L"Backups";
        std::filesystem::create_directories(backupDir);
        const auto target = backupDir / (study.excelPath.stem().wstring() + L"_" + Timestamp() + study.excelPath.extension().wstring());
        std::filesystem::copy_file(study.excelPath, target, std::filesystem::copy_options::overwrite_existing);
    }

    void ExcelStudyRepository::UpdateSheetRow(const StudyData& study, const SheetTable& table, int excelRow,
        const std::map<std::wstring, std::wstring>& values)
    {
        if (table.headers.empty() || table.sheetName.empty()) throw std::runtime_error("Die ausgewählte Tabelle kann nicht bearbeitet werden.");
        if (excelRow <= table.firstRow) throw std::runtime_error("Ungültige Excel-Zeilennummer für die Bearbeitung.");
        CheckUnchanged(study);
        CreateBackup(study);
        excel::Application excel;
        auto workbook = excel.Open(study.excelPath, false);
        if (workbook.IsReadOnly()) throw std::runtime_error("Die Studien-Excel ist schreibgeschützt oder bereits in Excel geöffnet.");

        for (size_t c = 0; c < table.headers.size(); ++c)
        {
            const auto& header = table.headers[c];
            const auto it = values.find(header);
            if (it == values.end()) continue;
            const auto value = InputToCell(header, it->second, table, static_cast<int>(c));
            workbook.SetCell(table.sheetName, excelRow, table.firstColumn + static_cast<int>(c), value, date::HeaderLooksLikeDate(header));
        }
        workbook.Save();
        workbook.Close(false);
    }

    void ExcelStudyRepository::UpdateOrderRow(const StudyData& study, const ImpData& imp, int excelRow,
        const std::map<std::wstring, std::wstring>& values) const
    {
        const auto changes = BuildAuditChanges(imp.orders, excelRow, values);
        AuditLogger::EnsureWritable();
        UpdateSheetRow(study, imp.orders, excelRow, values);
        WriteAudit(study, &imp, L"ORDER_UPDATE", imp.orders.sheetName, excelRow, changes);
    }

    void ExcelStudyRepository::UpdateInventoryRow(const StudyData& study, const ImpData& imp, int excelRow,
        const std::map<std::wstring, std::wstring>& values) const
    {
        const auto changes = BuildAuditChanges(imp.inventory, excelRow, values);
        AuditLogger::EnsureWritable();
        UpdateSheetRow(study, imp.inventory, excelRow, values);
        WriteAudit(study, &imp, L"INVENTORY_UPDATE", imp.inventory.sheetName, excelRow, changes);
    }

    void ExcelStudyRepository::UpdateProfileRow(const StudyData& study, int excelRow,
        const std::map<std::wstring, std::wstring>& values) const
    {
        const auto changes = BuildAuditChanges(study.profileTable, excelRow, values);
        AuditLogger::EnsureWritable();
        UpdateSheetRow(study, study.profileTable, excelRow, values);
        WriteAudit(study, nullptr, L"PROFILE_UPDATE", study.profileTable.sheetName, excelRow, changes);
    }

    void ExcelStudyRepository::UpdateProfileMetadata(const StudyData& study, const std::wstring& studyName,
        const std::wstring& euctNumber) const
    {
        if (study.profileTable.sheetName.empty()) throw std::runtime_error("Es wurde kein Studienprofil als erstes Tabellenblatt erkannt.");
        AuditLogger::EnsureWritable();
        CheckUnchanged(study);
        CreateBackup(study);
        excel::Application excel;
        auto workbook = excel.Open(study.excelPath, false);
        if (workbook.IsReadOnly()) throw std::runtime_error("Die Studien-Excel ist schreibgeschützt oder bereits in Excel geöffnet.");

        int studyRow = 1;
        int euctRow = 2;
        for (int r = 1; r <= 12; ++r)
        {
            const auto label = date::Normalize(ToText(workbook.GetCell(study.profileTable.sheetName, r, 1)));
            if (label.find(L"studienbezeichnung") != std::wstring::npos || label == L"studie" || label == L"study") studyRow = r;
            if (label.find(L"euct") != std::wstring::npos) euctRow = r;
        }
        const auto oldStudyName = date::Trim(ToText(workbook.GetCell(study.profileTable.sheetName, studyRow, 2)));
        const auto oldEuctNumber = date::Trim(ToText(workbook.GetCell(study.profileTable.sheetName, euctRow, 2)));
        const auto newStudyName = date::Trim(studyName);
        const auto newEuctNumber = date::Trim(euctNumber);

        workbook.SetCell(study.profileTable.sheetName, studyRow, 2, CellValue::Text(newStudyName));
        workbook.SetCell(study.profileTable.sheetName, euctRow, 2, CellValue::Text(newEuctNumber));
        workbook.Save();
        workbook.Close(false);

        std::vector<AuditChange> changes;
        if (oldStudyName != newStudyName) changes.push_back({ L"Studienbezeichnung", oldStudyName, newStudyName });
        if (oldEuctNumber != newEuctNumber) changes.push_back({ L"EUCT-No.", oldEuctNumber, newEuctNumber });
        WriteAudit(study, nullptr, L"STUDY_METADATA_UPDATE", study.profileTable.sheetName, 0, std::move(changes));
    }

    void ExcelStudyRepository::AppendOrder(const StudyData& study, const ImpData& imp, const std::map<std::wstring, std::wstring>& values) const
    {
        if (!imp.orderRequired) throw std::runtime_error("Dieses Produkt ist als nicht bestellpflichtig gekennzeichnet.");
        if (imp.orders.headers.empty() || imp.orderSheet.empty()) throw std::runtime_error("Für dieses IMP wurde kein lesbares Bestellblatt gefunden.");
        AuditLogger::EnsureWritable();
        CheckUnchanged(study);
        CreateBackup(study);
        excel::Application excel;
        auto workbook = excel.Open(study.excelPath, false);
        if (workbook.IsReadOnly()) throw std::runtime_error("Die Studien-Excel ist schreibgeschützt oder bereits in Excel geöffnet.");

        const int dataIndex = LastNonEmptyDataRow(imp.orders) + 1;
        const int excelRow = imp.orders.firstRow + 1 + dataIndex;
        const auto operationId = AuditLogger::NewOperationId();
        std::vector<AuditChange> auditChanges;
        for (size_t c = 0; c < imp.orders.headers.size(); ++c)
        {
            const auto& header = imp.orders.headers[c];
            const auto normalized = date::Normalize(header);
            CellValue value;
            if (normalized.find(L"bestellung") != std::wstring::npos || normalized == L"order" || normalized.find(L"bestellnummer") != std::wstring::npos)
                value = CellValue::Text(NextOrderNumber(imp.orders));
            else if (normalized.find(L"erhalten") != std::wstring::npos || normalized.find(L"received") != std::wstring::npos || normalized.find(L"wareneingang") != std::wstring::npos)
                value = CellValue::Empty();
            else
            {
                auto it = values.find(header);
                value = InputToCell(header, it == values.end() ? L"" : it->second, imp.orders, static_cast<int>(c));
            }
            workbook.SetCell(imp.orderSheet, excelRow, imp.orders.firstColumn + static_cast<int>(c), value, date::HeaderLooksLikeDate(header));
            const auto auditValue = date::Trim(ToText(value, header));
            if (!auditValue.empty()) auditChanges.push_back({ header, L"", auditValue });
        }
        workbook.Save();
        workbook.Close(false);
        WriteAudit(study, &imp, L"ORDER_CREATE", imp.orderSheet, excelRow, std::move(auditChanges), operationId);
    }

    void ExcelStudyRepository::AppendOrder(const StudyData& study, const std::map<std::wstring, std::wstring>& values) const
    {
        if (study.imps.empty()) throw std::runtime_error("Kein IMP definiert.");
        AppendOrder(study, study.imps.front(), values);
    }

    void ExcelStudyRepository::AppendInventoryRow(const StudyData& study, const ImpData& imp, const std::map<std::wstring, std::wstring>& values) const
    {
        if (imp.inventory.headers.empty() || imp.inventorySheet.empty()) throw std::runtime_error("Für dieses IMP wurde kein lesbares Inventarblatt gefunden.");
        AuditLogger::EnsureWritable();
        CheckUnchanged(study);
        CreateBackup(study);
        excel::Application excel;
        auto workbook = excel.Open(study.excelPath, false);
        if (workbook.IsReadOnly()) throw std::runtime_error("Die Studien-Excel ist schreibgeschützt oder bereits in Excel geöffnet.");

        // Bei gefilterten gemeinsamen Inventarblättern muss die tatsächliche letzte
        // Zeile des vollständigen Blatts verwendet werden, nicht nur die gefilterte Ansicht.
        auto complete = workbook.ReadSheet(imp.inventorySheet);
        const int dataIndex = LastNonEmptyDataRow(complete) + 1;
        const int excelRow = complete.firstRow + 1 + dataIndex;
        const auto operationId = AuditLogger::NewOperationId();
        std::vector<AuditChange> auditChanges;
        for (size_t c = 0; c < complete.headers.size(); ++c)
        {
            const auto& header = complete.headers[c];
            std::wstring text;
            if (!imp.inventoryImpHeader.empty() && date::Normalize(header) == date::Normalize(imp.inventoryImpHeader))
                text = imp.inventoryImpValue.empty() ? imp.name : imp.inventoryImpValue;
            else
            {
                auto it = values.find(header);
                if (it != values.end()) text = it->second;
            }
            const auto value = InputToCell(header, text, complete, static_cast<int>(c));
            workbook.SetCell(imp.inventorySheet, excelRow, complete.firstColumn + static_cast<int>(c), value, date::HeaderLooksLikeDate(header));
            const auto auditValue = date::Trim(ToText(value, header));
            if (!auditValue.empty()) auditChanges.push_back({ header, L"", auditValue });
        }
        workbook.Save();
        workbook.Close(false);
        WriteAudit(study, &imp, L"INVENTORY_CREATE", imp.inventorySheet, excelRow, std::move(auditChanges), operationId);
    }

    void ExcelStudyRepository::AppendInventoryRow(const StudyData& study, const std::map<std::wstring, std::wstring>& values) const
    {
        if (study.imps.empty()) throw std::runtime_error("Kein IMP definiert.");
        AppendInventoryRow(study, study.imps.front(), values);
    }

    void ExcelStudyRepository::AppendInventoryBatch(const StudyData& study, const ImpData& imp,
        const std::wstring& deliveryDate, const std::wstring& charge, const std::wstring& expiryDate,
        const std::vector<std::wstring>& itemIdentifiers, int receivedOrderExcelRow) const
    {
        if (itemIdentifiers.empty() || itemIdentifiers.size() > 500)
            throw std::runtime_error("Bitte zwischen 1 und 500 Box-/Kit-Nummern angeben.");

        const auto delivery = date::ParseGermanDate(deliveryDate);
        const auto expiry = date::ParseGermanDate(expiryDate);
        if (!delivery || !expiry)
            throw std::runtime_error("Liefer- und Verfallsdatum müssen im Format TT.MM.JJJJ eingegeben werden.");
        if (imp.inventorySheet.empty())
            throw std::runtime_error("Für dieses IMP ist kein Inventarblatt definiert.");

        std::vector<std::wstring> identifiers;
        identifiers.reserve(itemIdentifiers.size());
        for (const auto& raw : itemIdentifiers)
        {
            const auto value = date::Trim(raw);
            if (value.empty())
                throw std::runtime_error("Jede Packung benötigt eine Box-/Kit-Nummer. Die Nummer kann aus Buchstaben, Zahlen und Zeichen bestehen.");
            identifiers.push_back(value);
        }

        AuditLogger::EnsureWritable();
        CheckUnchanged(study);
        CreateBackup(study);
        excel::Application excel;
        auto workbook = excel.Open(study.excelPath, false);
        if (workbook.IsReadOnly())
            throw std::runtime_error("Die Studien-Excel ist schreibgeschützt oder bereits in Excel geöffnet.");

        auto complete = workbook.ReadSheet(imp.inventorySheet);
        const int deliveryCol = FindHeaderLike(complete, { L"DeliveryDate", L"Lieferdatum", L"Datum Erhalt" });
        const int chargeCol = FindHeaderLike(complete, { L"Charge No", L"Charge", L"Batch", L"Lot" });
        const int vialCol = FindHeaderLike(complete, { L"Vial No", L"Kit No", L"Box-Nr", L"Box Nr", L"Box No", L"Box Number", L"Pack No", L"Serial", L"Container", L"Vial", L"Kit", L"Box", L"Pack" });
        const int expiryCol = FindHeaderLike(complete, { L"Expiry-Date", L"Expiry", L"Verfall" });
        const int impFilterCol = imp.inventoryImpHeader.empty() ? -1 : FindHeaderExactOrLike(complete, imp.inventoryImpHeader, {});
        if (deliveryCol < 0 || chargeCol < 0 || vialCol < 0 || expiryCol < 0)
            throw std::runtime_error("Für die Wareneingangserfassung fehlen benötigte Inventarspalten (Lieferdatum, Charge, Box-/Kit-Nr. und Verfall).");

        const auto operationId = AuditLogger::NewOperationId();
        std::vector<std::pair<int, std::vector<AuditChange>>> createdRows;
        int nextData = LastNonEmptyDataRow(complete) + 1;
        for (const auto& identifier : identifiers)
        {
            const int excelRow = complete.firstRow + 1 + nextData++;

            // Nur tatsächlich zu befüllende Zellen über COM schreiben. Zuvor wurde jede
            // Spalte jeder neuen Zeile einzeln gesetzt – auch leere Spalten. Gerade bei
            // größeren Lieferungen erzeugt das unnötig viele COM-Aufrufe.
            workbook.SetCell(
                imp.inventorySheet, excelRow, complete.firstColumn + deliveryCol,
                CellValue::Number(date::ToExcelSerial(*delivery)), true);
            workbook.SetCell(
                imp.inventorySheet, excelRow, complete.firstColumn + chargeCol,
                CellValue::Text(charge), false);
            workbook.SetCell(
                imp.inventorySheet, excelRow, complete.firstColumn + vialCol,
                CellValue::Text(identifier), false);
            workbook.SetCell(
                imp.inventorySheet, excelRow, complete.firstColumn + expiryCol,
                CellValue::Number(date::ToExcelSerial(*expiry)), true);

            if (impFilterCol >= 0 && impFilterCol != deliveryCol && impFilterCol != chargeCol &&
                impFilterCol != vialCol && impFilterCol != expiryCol)
            {
                workbook.SetCell(
                    imp.inventorySheet, excelRow, complete.firstColumn + impFilterCol,
                    CellValue::Text(imp.inventoryImpValue.empty() ? imp.name : imp.inventoryImpValue), false);
            }

            std::vector<AuditChange> rowChanges;
            rowChanges.push_back({ complete.headers[static_cast<size_t>(deliveryCol)], L"", date::FormatGermanDate(*delivery) });
            rowChanges.push_back({ complete.headers[static_cast<size_t>(chargeCol)], L"", charge });
            rowChanges.push_back({ complete.headers[static_cast<size_t>(vialCol)], L"", identifier });
            rowChanges.push_back({ complete.headers[static_cast<size_t>(expiryCol)], L"", date::FormatGermanDate(*expiry) });
            if (impFilterCol >= 0 && impFilterCol != deliveryCol && impFilterCol != chargeCol &&
                impFilterCol != vialCol && impFilterCol != expiryCol)
            {
                rowChanges.push_back({
                    complete.headers[static_cast<size_t>(impFilterCol)],
                    L"",
                    imp.inventoryImpValue.empty() ? imp.name : imp.inventoryImpValue });
            }
            createdRows.emplace_back(excelRow, std::move(rowChanges));
        }

        // Optional genau die ausgewählte Bestellung als erhalten markieren.
        std::vector<AuditChange> receivedOrderChanges;
        std::wstring receivedOrderSheet;
        if (receivedOrderExcelRow > 0 && !imp.orderSheet.empty())
        {
            auto completeOrders = workbook.ReadSheet(imp.orderSheet);
            const int receivedCol = FindHeaderExactOrLike(
                completeOrders,
                L"Erhalten am",
                { L"Eingang am", L"Date received", L"Received date", L"Received on", L"Wareneingang", L"Received" });
            if (receivedCol < 0)
                throw std::runtime_error("Die ausgewählte Bestelltabelle enthält keine Spalte 'Erhalten am' / 'Received'.");

            if (receivedOrderExcelRow <= completeOrders.firstRow)
                throw std::runtime_error("Die ausgewählte Bestellung besitzt keine gültige Excel-Zeilennummer.");

            const auto oldReceived = date::Trim(ToText(
                workbook.GetCell(imp.orderSheet, receivedOrderExcelRow, completeOrders.firstColumn + receivedCol),
                completeOrders.headers[static_cast<size_t>(receivedCol)]));

            workbook.SetCell(
                imp.orderSheet,
                receivedOrderExcelRow,
                completeOrders.firstColumn + receivedCol,
                CellValue::Number(date::ToExcelSerial(*delivery)),
                true);

            receivedOrderSheet = imp.orderSheet;
            receivedOrderChanges.push_back({
                completeOrders.headers[static_cast<size_t>(receivedCol)],
                oldReceived,
                date::FormatGermanDate(*delivery) });
        }

        workbook.Save();
        workbook.Close(false);

        for (auto& [row, changes] : createdRows)
            WriteAudit(study, &imp, L"GOODS_RECEIPT", imp.inventorySheet, row, std::move(changes), operationId);

        if (!receivedOrderChanges.empty())
            WriteAudit(study, &imp, L"ORDER_RECEIVED", receivedOrderSheet, receivedOrderExcelRow,
                std::move(receivedOrderChanges), operationId, L"Wareneingang wurde der ausgewählten offenen Bestellung zugeordnet.");
    }

    void ExcelStudyRepository::SetVisitDate(const StudyData& study, int excelRow, int excelColumn, const std::wstring& dateText) const
    {
        const auto parsed = date::ParseGermanDate(dateText);
        if (!parsed) throw std::runtime_error("Bitte das Datum im Format TT.MM.JJJJ eingeben.");
        AuditLogger::EnsureWritable();
        CheckUnchanged(study);
        CreateBackup(study);
        excel::Application excel;
        auto workbook = excel.Open(study.excelPath, false);
        if (workbook.IsReadOnly()) throw std::runtime_error("Die Studien-Excel ist schreibgeschützt oder bereits in Excel geöffnet.");

        const auto oldValue = date::Trim(ToText(
            workbook.GetCell(study.config.visitSheet, excelRow, excelColumn),
            L"Datum"));
        const auto newValue = date::FormatGermanDate(*parsed);

        workbook.SetCell(study.config.visitSheet, excelRow, excelColumn, CellValue::Number(date::ToExcelSerial(*parsed)), true);
        workbook.Save();
        workbook.Close(false);

        WriteAudit(study, nullptr, L"VISIT_DATE_UPDATE", study.config.visitSheet, excelRow,
            { { L"Visiten-Datum", oldValue, newValue } }, L"", L"Excel-Spalte " + std::to_wstring(excelColumn));
    }

    void ExcelStudyRepository::AddPatient(const StudyData& study, const std::wstring& patientId) const
    {
        const auto id = date::Trim(patientId);
        if (id.empty()) throw std::runtime_error("Bitte eine Patienten-ID eingeben.");
        for (const auto& existing : study.patientIds)
            if (date::Normalize(existing) == date::Normalize(id)) throw std::runtime_error("Diese Patienten-ID existiert bereits.");

        AuditLogger::EnsureWritable();
        CheckUnchanged(study);
        CreateBackup(study);
        excel::Application excel;
        auto workbook = excel.Open(study.excelPath, false);
        if (workbook.IsReadOnly()) throw std::runtime_error("Die Studien-Excel ist schreibgeschützt oder bereits in Excel geöffnet.");

        int newColumn = study.visitTable.firstColumn + static_cast<int>(study.visitTable.headers.size());
        const int metaCount = static_cast<int>(study.visitMetadataHeaders.size());
        for (int c = metaCount; c < static_cast<int>(study.visitTable.headers.size()); ++c)
        {
            if (date::Trim(study.visitTable.headers[static_cast<size_t>(c)]).empty())
            {
                newColumn = study.visitTable.firstColumn + c;
                break;
            }
        }
        workbook.SetCell(study.config.visitSheet, study.visitTable.firstRow, newColumn, CellValue::Text(id), false);
        workbook.Save();
        workbook.Close(false);

        WriteAudit(study, nullptr, L"PATIENT_CREATE", study.config.visitSheet, study.visitTable.firstRow,
            { { L"Patienten-ID", L"", id } }, L"", L"Neue Patientenspalte " + std::to_wstring(newColumn));
    }

    void ExcelStudyRepository::AddVisitRow(const StudyData& study, const std::map<std::wstring, std::wstring>& metadata) const
    {
        if (study.visitMetadataHeaders.empty()) throw std::runtime_error("Die Visitenstruktur konnte nicht erkannt werden.");
        AuditLogger::EnsureWritable();
        CheckUnchanged(study);
        CreateBackup(study);
        excel::Application excel;
        auto workbook = excel.Open(study.excelPath, false);
        if (workbook.IsReadOnly()) throw std::runtime_error("Die Studien-Excel ist schreibgeschützt oder bereits in Excel geöffnet.");

        const int dataIndex = LastNonEmptyDataRow(study.visitTable) + 1;
        const int excelRow = study.visitTable.firstRow + 1 + dataIndex;
        std::vector<AuditChange> auditChanges;
        for (size_t c = 0; c < study.visitMetadataHeaders.size(); ++c)
        {
            const auto& header = study.visitMetadataHeaders[c];
            auto it = metadata.find(header);
            const auto text = it == metadata.end() ? L"" : date::Trim(it->second);
            if (c == 0 && text.empty()) throw std::runtime_error("Das erste Visitenfeld darf nicht leer sein.");
            CellValue value = CellValue::Text(text);
            if (ColumnLooksNumeric(study.visitTable, static_cast<int>(c)) && !text.empty())
            {
                try { value = CellValue::Number(std::stod(text)); } catch (...) {}
            }
            workbook.SetCell(study.config.visitSheet, excelRow, study.visitTable.firstColumn + static_cast<int>(c), value, false);
            if (!text.empty()) auditChanges.push_back({ header, L"", text });
        }
        workbook.Save();
        workbook.Close(false);

        WriteAudit(study, nullptr, L"VISIT_CREATE", study.config.visitSheet, excelRow, std::move(auditChanges));
    }
}
