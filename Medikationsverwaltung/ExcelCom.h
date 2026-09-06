// ============================================================================
// Datei: ExcelCom.h
// Zweck: Deklariert eine kleine RAII-Schicht über die Microsoft-Excel-COM-Automation.
//
// Verantwortlichkeiten:
// - Kapselt VARIANT/IDispatch-Aufrufe, Arbeitsmappen und die Excel.Application-Instanz.
// - Verhindert, dass COM-Details in Repository- und GUI-Code verteilt werden.
//
// Hinweis: Kommentare erläutern Architektur und nicht offensichtliche Logik.
// Triviale Sprachkonstrukte werden bewusst nicht zeilenweise kommentiert.
// ============================================================================
#pragma once

#include "Models.h"

#include <Windows.h>
#include <wrl/client.h>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace med::excel
{
    /// RAII-Wrapper um VARIANT. Kümmert sich um Initialisierung, Kopieren und Freigabe
    /// und bietet typsichere Konvertierungen für die restliche Anwendung.
    class Variant
    {
    public:
        Variant();
        explicit Variant(const std::wstring& value);
        explicit Variant(const wchar_t* value);
        explicit Variant(double value);
        explicit Variant(int value);
        explicit Variant(bool value);
        /// Erzeugt explizit einen COM-Datumswert (VT_DATE) aus einer Excel-Seriennummer.
        static Variant Date(double value);
        Variant(const Variant& other);
        Variant(Variant&& other) noexcept;
        Variant& operator=(const Variant& other);
        Variant& operator=(Variant&& other) noexcept;
        ~Variant();

        static Variant Adopt(VARIANT&& value);
        const VARIANT& Get() const { return m_value; }
        VARIANT& Get() { return m_value; }
        Microsoft::WRL::ComPtr<IDispatch> AsDispatch() const;
        std::wstring AsString() const;
        double AsDouble(double fallback = 0.0) const;
        int AsInt(int fallback = 0) const;
        bool AsBool(bool fallback = false) const;
        /// Übersetzt den COM-Wert in das Excel-unabhängige interne CellValue-Modell.
        CellValue ToCellValue() const;

    private:
        VARIANT m_value{};
    };

    /// Dünner Late-Binding-Wrapper um IDispatch.
    /// Get/Put/Call entsprechen Eigenschaften lesen, Eigenschaften schreiben und Methoden aufrufen.
    class Dispatch
    {
    public:
        Dispatch() = default;
        explicit Dispatch(Microsoft::WRL::ComPtr<IDispatch> value) : m_value(std::move(value)) {}
        explicit operator bool() const { return m_value != nullptr; }

        Variant Get(const wchar_t* name, const std::vector<Variant>& args = {}) const;
        void Put(const wchar_t* name, const Variant& value) const;
        Variant Call(const wchar_t* name, const std::vector<Variant>& args = {}) const;
        IDispatch* Raw() const { return m_value.Get(); }

    private:
        Variant Invoke(const wchar_t* name, WORD flags, const std::vector<Variant>& args, const Variant* putValue = nullptr) const;
        Microsoft::WRL::ComPtr<IDispatch> m_value;
    };

    /// Besitzender Wrapper einer geöffneten Excel-Arbeitsmappe.
    /// Die Klasse stellt nur die Operationen bereit, die die Medikationsverwaltung benötigt.
    class Workbook
    {
    public:
        Workbook() = default;
        explicit Workbook(Dispatch workbook) : m_workbook(std::move(workbook)) {}
        Workbook(const Workbook&) = delete;
        Workbook& operator=(const Workbook&) = delete;
        Workbook(Workbook&&) noexcept = default;
        Workbook& operator=(Workbook&&) noexcept = default;
        ~Workbook();

        bool HasSheet(const std::wstring& sheetName) const;
        std::wstring FirstSheetName() const;
        /// Liest den belegten Bereich eines Arbeitsblatts in eine SheetTable ein.
        SheetTable ReadSheet(const std::wstring& sheetName) const;
        CellValue GetCell(const std::wstring& sheetName, int row, int column) const;
        /// Schreibt einen einzelnen Zellwert. dateFormat kennzeichnet fachliche Datumsfelder.
        void SetCell(const std::wstring& sheetName, int row, int column, const CellValue& value, bool dateFormat = false);
        void ClearRange(const std::wstring& sheetName, int firstRow, int firstColumn, int lastRow, int lastColumn);
        void CopyFormats(const std::wstring& sheetName, int sourceRow, int targetRow, int firstColumn, int lastColumn);
        void ConfigurePrint(const std::wstring& sheetName, int firstRow, int firstColumn, int lastRow, int lastColumn, bool landscape, int titleRows);
        void ConfigurePageHeader(const std::wstring& sheetName, const std::wstring& leftHeader, const std::wstring& centerHeader, const std::wstring& rightHeader);
        void ExportSheetPdf(const std::wstring& sheetName, const std::filesystem::path& pdfPath);
        void Save();
        /// Schließt die Arbeitsmappe explizit; der Destruktor schließt notfalls ohne Speichern.
        void Close(bool saveChanges = false);
        bool IsReadOnly() const;

    private:
        Dispatch Worksheet(const std::wstring& sheetName) const;
        Dispatch m_workbook;
        bool m_closed{ false };
    };

    /// Lebenszeit-Wrapper für eine unsichtbare Excel.Application-COM-Instanz.
    /// Eine Instanz kann nacheinander mehrere Arbeitsmappen öffnen, was Voll-Reloads beschleunigt.
    class Application
    {
    public:
        Application();
        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;
        ~Application();

        Workbook Open(const std::filesystem::path& path, bool readOnly) const;

    private:
        Dispatch m_excel;
    };
}
