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
    using Day = std::chrono::sys_days;

    enum class CellType
    {
        Empty,
        Text,
        Number,
        Boolean
    };

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

    struct SheetTable
    {
        std::wstring sheetName;
        int firstRow{ 1 };
        int firstColumn{ 1 };
        std::vector<std::wstring> headers;
        std::vector<std::vector<CellValue>> rows;
        // Ursprüngliche Excel-Zeilennummer zu jeder Datenzeile. Das ist wichtig,
        // wenn Tabellen in der UI gefiltert oder sortiert werden und anschließend
        // genau die ausgewählte Quellzeile bearbeitet werden soll.
        std::vector<int> rowNumbers;
    };

    struct VisitRowDefinition
    {
        int excelRow{};
        std::vector<std::pair<std::wstring, std::wstring>> metadata;
    };

    struct VisitRecord
    {
        int excelRow{};
        int excelColumn{};
        std::wstring patientId;
        std::vector<std::pair<std::wstring, std::wstring>> metadata;
        Day date{};
        bool hasDate{ false };
    };

    struct DocumentEntry
    {
        std::wstring title;
        std::wstring category;
        std::filesystem::path path;
    };

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

    enum class OrderState
    {
        Ok,
        Soon,
        Due,
        Pending,
        NotRequired,
        Unknown
    };

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
        std::wstring packageContent;                       // optional, z. B. "84"
        std::wstring doseStrength;                         // optional, z. B. "40 mg"
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

    struct StudyData
    {
        std::filesystem::path excelPath;
        std::filesystem::path studyDirectory;
        std::filesystem::file_time_type lastWriteTime{};
        StudyConfig config;

        std::wstring studyName;
        std::wstring workbookStudyName;
        std::wstring euctNumber;
        std::wstring loadWarning;
        bool hasStudyProfile{ false };
        bool profileColumnLayout{ false };
        SheetTable profileTable;
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
        std::vector<DocumentEntry> documents;

        bool pendingOrder{ false };
        std::optional<Day> pendingOrderDate;
        OrderPlan plan;
    };
}
