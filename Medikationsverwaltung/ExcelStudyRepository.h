// ============================================================================
// Datei: ExcelStudyRepository.h
// Zweck: Deklariert die Persistenzschicht für studienspezifische Excel-Arbeitsmappen.
//
// Verantwortlichkeiten:
// - Liest Studien und IMPs in das Domänenmodell ein.
// - Schreibt Bestellungen, Wareneingänge, Inventaränderungen und Visiten zurück nach Excel.
// - Erzeugt vor schreibenden Änderungen Sicherungskopien und prüft konkurrierende Dateiänderungen.
//
// Hinweis: Kommentare erläutern Architektur und nicht offensichtliche Logik.
// Triviale Sprachkonstrukte werden bewusst nicht zeilenweise kommentiert.
// ============================================================================
#pragma once

#include "Models.h"
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace med::excel { class Application; }

namespace med
{
    /// Repository für die Excel-basierte Persistenz.
    /// Alle schreibenden Änderungen an Studien-Arbeitsmappen laufen über diese Klasse.
    class ExcelStudyRepository
    {
    public:
        /// Lädt alle direkten Studienunterordner und verwendet dabei eine gemeinsame Excel-Instanz.
        std::vector<StudyData> LoadAll(const std::filesystem::path& studyFolder) const;
        /// Lädt eine einzelne Arbeitsmappe vollständig in StudyData.
        StudyData LoadOne(const std::filesystem::path& excelPath) const;

        /// Ergänzt eine neue Bestellzeile für das angegebene IMP.
        void AppendOrder(const StudyData& study, const ImpData& imp, const std::map<std::wstring, std::wstring>& values) const;
        void AppendOrder(const StudyData& study, const std::map<std::wstring, std::wstring>& values) const;

        /// Ergänzt einen einzelnen Inventardatensatz. Batch-Wareneingänge nutzen AppendInventoryBatch.
        void AppendInventoryRow(const StudyData& study, const ImpData& imp, const std::map<std::wstring, std::wstring>& values) const;
        void AppendInventoryRow(const StudyData& study, const std::map<std::wstring, std::wstring>& values) const;

        /// Erfasst einen kompletten Wareneingang mit frei vorgegebenen Box-/Kit-Kennzeichnungen
        /// und kann optional eine konkrete offene Bestellung als erhalten markieren.
        void AppendInventoryBatch(const StudyData& study, const ImpData& imp,
            const std::wstring& deliveryDate,
            const std::wstring& charge,
            const std::wstring& expiryDate,
            const std::vector<std::wstring>& itemIdentifiers,
            int receivedOrderExcelRow = -1) const;

        /// Schreibt einen Visiten-/Patiententermin an die bekannte Excel-Zelle zurück.
        void SetVisitDate(const StudyData& study, int excelRow, int excelColumn, const std::wstring& dateText) const;
        /// Aktualisiert eine bestehende Bestellzeile anhand ihrer ursprünglichen Excel-Zeilennummer.
        void UpdateOrderRow(const StudyData& study, const ImpData& imp, int excelRow, const std::map<std::wstring, std::wstring>& values) const;
        /// Aktualisiert eine bestehende Inventarzeile anhand ihrer ursprünglichen Excel-Zeilennummer.
        void UpdateInventoryRow(const StudyData& study, const ImpData& imp, int excelRow, const std::map<std::wstring, std::wstring>& values) const;
        void UpdateProfileRow(const StudyData& study, int excelRow, const std::map<std::wstring, std::wstring>& values) const;
        void UpdateProfileMetadata(const StudyData& study, const std::wstring& studyName, const std::wstring& euctNumber) const;
        /// Fügt im Visitenblatt eine neue Patientenspalte hinzu.
        void AddPatient(const StudyData& study, const std::wstring& patientId) const;
        /// Fügt im Visitenblatt eine neue Visitenzeile mit den angegebenen Metadaten hinzu.
        void AddVisitRow(const StudyData& study, const std::map<std::wstring, std::wstring>& metadata) const;

    private:
        StudyData LoadOneWithApplication(const std::filesystem::path& excelPath, excel::Application& excel) const;
        static int FindHeader(const SheetTable& table, const std::wstring& keyContains);
        static int LastNonEmptyDataRow(const SheetTable& table);
        static CellValue InputToCell(const std::wstring& header, const std::wstring& text, const SheetTable& table, int columnIndex);
        static bool ColumnLooksNumeric(const SheetTable& table, int columnIndex);
        static std::wstring NextOrderNumber(const SheetTable& orders);
        static std::wstring NextVialPrefix(const SheetTable& inventory);
        static void LoadVisitModel(StudyData& study);
        static void UpdateSheetRow(const StudyData& study, const SheetTable& table, int excelRow, const std::map<std::wstring, std::wstring>& values);
        static void LoadDocuments(StudyData& study);
        static void DetectPendingOrder(StudyData& study, ImpData& imp);
        static void ComputeStock(ImpData& imp);
        static void SyncLegacyFields(StudyData& study);
        /// Verhindert stilles Überschreiben, wenn die Datei seit dem Einlesen extern verändert wurde.
        static void CheckUnchanged(const StudyData& study);
        /// Erstellt unmittelbar vor einer schreibenden Änderung eine datierte Sicherungskopie.
        static void CreateBackup(const StudyData& study);
    };
}
