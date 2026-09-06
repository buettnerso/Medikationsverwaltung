// ============================================================================
// Datei: ReportService.h
// Zweck: Deklariert die Exportfunktionen für DrugAccount-Berichte und die studienübergreifende Übersicht.
//
// Verantwortlichkeiten:
// - Die Berichte werden aus dem aktuellen Studienmodell erzeugt und als Excel-Dateien ausgegeben.
//
// Hinweis: Kommentare erläutern Architektur und nicht offensichtliche Logik.
// Triviale Sprachkonstrukte werden bewusst nicht zeilenweise kommentiert.
// ============================================================================
#pragma once

#include "Models.h"
#include <filesystem>
#include <string>
#include <vector>

namespace med
{
    /// Ergebnis eines Berichtsexports. Enthält Pfad und Anzahl tatsächlich geschriebener Datenzeilen.
    struct ReportFiles
    {
        std::filesystem::path excelPath;
        int dataRows{ 0 };
    };

    /// Erstellt Ausgabedateien aus dem bereits geladenen Domänenmodell.
    /// Die Klasse verändert die Studien-Arbeitsmappe nicht.
    class ReportService
    {
    public:
        /// Erstellt einen DrugAccount-Gesamtbericht für ein IMP.
        static ReportFiles ExportDrugAccountOverall(
            const StudyData& study,
            const ImpData& imp,
            const std::filesystem::path& reportRoot,
            const std::filesystem::path& defaultTemplate);

        /// Erstellt einen auf ein IMP und einen Patienten gefilterten DrugAccount-Bericht.
        static ReportFiles ExportDrugAccountPatient(
            const StudyData& study,
            const ImpData& imp,
            const std::wstring& patientId,
            const std::filesystem::path& reportRoot,
            const std::filesystem::path& defaultTemplate);

        /// Exportiert die studienübergreifende Bestellübersicht als CSV.
        static std::filesystem::path ExportStudySummary(const std::vector<StudyData>& studies, const std::filesystem::path& reportRoot);
    };
}
