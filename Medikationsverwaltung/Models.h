// ============================================================================
// Datei: Models.h
// Zweck: Definiert die zentralen Datenmodelle, die zwischen Excel-Zugriff, Geschäftslogik und GUI ausgetauscht werden.
//
// Verantwortlichkeiten:
// - Beschreibt Zellen, Tabellen, Studien, IMPs, Visiten, Dokumente und Bestellplanung.
// - Enthält keine Datei- oder GUI-Logik; die Strukturen dienen als gemeinsames Domänenmodell.
//
// Hinweis: Kommentare erläutern Architektur und nicht offensichtliche Logik.
// Triviale Sprachkonstrukte werden bewusst nicht zeilenweise kommentiert.
// ============================================================================
#pragma once

#include <chrono>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace med
{
    /// Einheitlicher Datumstyp des Projekts: Kalendertag ohne Uhrzeit/Zeitzone.
    using Day = std::chrono::sys_days;

    /// Vereinfachtes Zelltypsystem, damit die Geschäftslogik nicht von COM-VARIANT abhängt.
    enum class CellType
    {
        Empty,
        Text,
        Number,
        Boolean
    };

    /// Excel-unabhängige Repräsentation eines einzelnen Zellwerts.
    /// Text, Zahl und Boolean bleiben typisiert; eine leere Zelle ist explizit darstellbar.
    struct CellValue
    {
        CellType type{ CellType::Empty };
        std::wstring text{};
        double number{};
        bool boolean{};

        static CellValue Empty() { return {}; }
        static CellValue Text(std::wstring value)
        {
            CellValue c;
            c.type = CellType::Text;
            c.text = std::move(value);
            return c;
        }
        static CellValue Number(double value)
        {
            CellValue c;
            c.type = CellType::Number;
            c.number = value;
            return c;
        }
        static CellValue Boolean(bool value)
        {
            CellValue c;
            c.type = CellType::Boolean;
            c.boolean = value;
            return c;
        }

        bool IsEmpty() const
        {
            return type == CellType::Empty || (type == CellType::Text && text.empty());
        }
    };

    /// In-Memory-Abbild eines Excel-Tabellenbereichs.
    /// headers und rows werden für Anzeige, Filterung und spätere Rückschreiboperationen verwendet.
    struct SheetTable
    {
        std::wstring sheetName;
        /// Excel-Zeile/-Spalte der erkannten Kopfzeile bzw. des ersten Tabellenbereichs (1-basiert).
        int firstRow{ 1 };
        int firstColumn{ 1 };
        std::vector<std::wstring> headers;
        std::vector<std::vector<CellValue>> rows;
        // Ursprüngliche Excel-Zeilennummer zu jeder Datenzeile. Das ist wichtig,
        // wenn Tabellen in der UI gefiltert oder sortiert werden und anschließend
        // genau die ausgewählte Quellzeile bearbeitet werden soll.
        std::vector<int> rowNumbers;
    };

    /// Beschreibt eine Visitenzeile unabhängig von einem einzelnen Patienten.
    struct VisitRowDefinition
    {
        int excelRow{};
        std::vector<std::pair<std::wstring, std::wstring>> metadata;
    };

    /// Konkreter Termin eines Patienten. excelRow/excelColumn zeigen auf die Quellzelle,
    /// damit Änderungen wieder exakt an dieselbe Stelle geschrieben werden können.
    struct VisitRecord
    {
        int excelRow{};
        int excelColumn{};
        std::wstring patientId;
        std::vector<std::pair<std::wstring, std::wstring>> metadata;
        Day date{};
        bool hasDate{ false };
    };

    /// Datei oder Link aus dem Documents-Unterordner einer Studie.
    struct DocumentEntry
    {
        std::wstring title;
        std::wstring category;
        std::filesystem::path path;
    };

    /// Optionale Konfiguration aus study.config.ini.
    /// Sie überschreibt nur technische Standardnamen; die sichtbare Studienidentität bleibt
    /// der Name des Studienordners.
    struct StudyConfig
    {
        std::wstring studyNameOverride;
        std::wstring medicationOverride;
        std::wstring workbookFile;
        std::wstring drugAccountTemplate;
        std::wstring stockSheet{ L"Meldebestand" };
        std::wstring orderSheet{ L"Bestellübersicht" };
        std::wstring inventorySheet{ L"DrugInventory" };
        std::wstring visitSheet{ L"PatientenVisiten" };
        double consumptionPerVisit{ 1.0 };
        int patientColumnsStart{ 0 }; // 1-basiert; 0 = automatisch erkennen
    };

    /// Fachlicher Zustand der Bestellplanung, unabhängig von der späteren Farb-/Textdarstellung.
    enum class OrderState
    {
        Ok,
        Soon,
        Due,
        Pending,
        NotRequired,
        Unknown
    };

    /// Ergebnis der Bestellberechnung für ein IMP. Optionalen Datumswerten fehlt ein Wert,
    /// wenn aus den vorhandenen Daten kein belastbarer Zeitpunkt abgeleitet werden kann.
    struct OrderPlan
    {
        OrderState state{ OrderState::Unknown };
        std::optional<Day> recommendedFrom;
        std::optional<Day> latestOrderDate;
        std::optional<Day> criticalVisitDate;
        int futureVisits{ 0 };
        int visitsDuringLeadTime{ 0 };
        int projectedVisits{ 0 };
        bool estimated{ false };
        std::wstring explanation;
    };

    // Ein verwaltetes Prüf-/Begleitmedikament bzw. Produkt innerhalb einer Studie.
    // Die Daten stammen bevorzugt aus dem ersten Tabellenblatt "Studienprofil".
    struct ImpData
    {
        std::wstring id;
        std::wstring name;
        std::wstring goodsType{ L"Studienware" };          // z. B. Studienware / Handelsware
        std::wstring applicationForm;                      // Tablette, Vial, Spritze, Infusion ...
        bool orderRequired{ true };                        // Handelsware kann z. B. false sein
        double minimumStock{ 0.0 };
        int leadTimeDays{ 0 };
        double consumptionPerVisit{ 1.0 };
        int bufferDays{ 7 };
        int forecastIntervalDays{ 0 };                   // optionales Intervall nur für ausdrücklich markierte Prognosen

        std::wstring orderSheet;
        std::wstring orderQuantityHeader;                 // genaue bzw. erkennbare Bestellspalte
        std::wstring inventorySheet;
        std::wstring inventoryImpHeader;                  // optional bei gemeinsamem Inventarblatt
        std::wstring inventoryImpValue;                   // Filterwert für dieses IMP
        std::wstring visitFilter;                         // optional: nur Visiten mit passender Metadatenangabe
        std::wstring orderProcess;                        // Freitext, z. B. Sponsorportal / E-Mail / Apotheke
        std::wstring documentCategory;                    // Unterordner/Kategorie für Dokumente

        double stock{ 0.0 };
        SheetTable orders;
        SheetTable inventory;
        bool pendingOrder{ false };
        std::optional<Day> pendingOrderDate;
        OrderPlan plan;
    };

    /// Vollständig geladene Studie. Enthält Dateimetadaten, IMPs, Tabellen, Visiten, Dokumente
    /// und abgeleitete Zustände. Dieses Objekt ist die zentrale Datenbasis der GUI.
    struct StudyData
    {
        std::filesystem::path excelPath;
        std::filesystem::path studyDirectory;
        /// Änderungszeit beim Einlesen; wird vor Schreibvorgängen zur Konflikterkennung geprüft.
        std::filesystem::file_time_type lastWriteTime{};
        StudyConfig config;

        std::wstring studyName;
        std::wstring workbookStudyName;
        std::wstring euctNumber;
        std::wstring loadWarning;
        bool hasStudyProfile{ false };
        bool profileColumnLayout{ false };
        SheetTable profileTable;
        /// Alle in dieser Studie verwalteten IMPs/Produkte; bei Legacy-Dateien typischerweise eins.
        std::vector<ImpData> imps;

        // Legacy-/Kompatibilitätsfelder für bestehende Ein-IMP-Dateien und ältere UI-Pfade.
        std::wstring medication;
        double stock{ 0.0 };
        double minimumStock{ 0.0 };
        int leadTimeDays{ 0 };
        int bufferDays{ 7 };
        std::wstring excelSignal;

        SheetTable stockTable;
        SheetTable orders;
        SheetTable inventory;
        SheetTable visitTable;

        std::vector<std::wstring> patientIds;
        std::vector<std::wstring> visitMetadataHeaders;
        std::vector<VisitRowDefinition> visitRows;
        std::vector<VisitRecord> visits;
        /// Rekursiv erkannte Dokumente und Links aus dem Documents-Unterordner.
        std::vector<DocumentEntry> documents;

        bool pendingOrder{ false };
        std::optional<Day> pendingOrderDate;
        OrderPlan plan;
    };
}
