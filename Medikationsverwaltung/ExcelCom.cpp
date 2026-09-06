// ============================================================================
// Datei: ExcelCom.cpp
// Zweck: Implementiert die COM-Automation für Microsoft Excel.
//
// Verantwortlichkeiten:
// - Öffnet und schließt Arbeitsmappen, liest Tabellenbereiche und schreibt Zellwerte.
// - Kapselt Datumswerte, Druckeinstellungen und Fehlerausgaben aus IDispatch-Aufrufen.
//
// Hinweis: Kommentare erläutern Architektur und nicht offensichtliche Logik.
// Triviale Sprachkonstrukte werden bewusst nicht zeilenweise kommentiert.
// ============================================================================
#include "pch.h"
#include "ExcelCom.h"

#include <algorithm>
#include <combaseapi.h>
#include <oleauto.h>
#include <sstream>

namespace
{
    // -------------------------------------------------------------------------
    // COM-/Fehler-Hilfsfunktionen
    // -------------------------------------------------------------------------
    // Excel wird über Late Binding (IDispatch) angesprochen. Dadurch ist keine
    // generierte Excel-Typbibliothek im Projekt erforderlich.
    std::string Narrow(const std::wstring& value)
    {
        if (value.empty()) return {};
        const int needed = ::WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
        std::string result(static_cast<size_t>(needed), '\0');
        ::WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), needed, nullptr, nullptr);
        return result;
    }

    std::wstring DescriptionFromExcepInfo(EXCEPINFO& ex)
    {
        std::wstring text;
        if (ex.bstrDescription) text = ex.bstrDescription;
        else if (ex.bstrSource) text = ex.bstrSource;
        if (ex.bstrSource) ::SysFreeString(ex.bstrSource);
        if (ex.bstrDescription) ::SysFreeString(ex.bstrDescription);
        if (ex.bstrHelpFile) ::SysFreeString(ex.bstrHelpFile);
        ex = {};
        return text;
    }

    med::excel::Variant ResultVariant(VARIANT& raw)
    {
        return med::excel::Variant::Adopt(std::move(raw));
    }
}

namespace med::excel
{

    namespace
    {
        std::wstring ColumnName(int column)
        {
            if (column <= 0) return L"A";
            std::wstring result;
            while (column > 0)
            {
                --column;
                result.insert(result.begin(), static_cast<wchar_t>(L'A' + (column % 26)));
                column /= 26;
            }
            return result;
        }

        std::wstring RangeAddress(int firstRow, int firstColumn, int lastRow, int lastColumn)
        {
            return ColumnName(firstColumn) + std::to_wstring(firstRow) + L":" +
                ColumnName(lastColumn) + std::to_wstring(lastRow);
        }
    }
    // -------------------------------------------------------------------------
    // Variant: RAII für COM-Werte
    // -------------------------------------------------------------------------
    // VariantInit/VariantClear werden zentral gekapselt, damit kein aufrufender Code
    // die fehleranfällige Lebenszeit eines rohen VARIANT verwalten muss.
    Variant::Variant()
    {
        ::VariantInit(&m_value);
    }

    Variant::Variant(const std::wstring& value) : Variant()
    {
        m_value.vt = VT_BSTR;
        m_value.bstrVal = ::SysAllocStringLen(value.data(), static_cast<UINT>(value.size()));
    }

    Variant::Variant(const wchar_t* value) : Variant(std::wstring(value ? value : L"")) {}

    Variant::Variant(double value) : Variant()
    {
        m_value.vt = VT_R8;
        m_value.dblVal = value;
    }

    Variant::Variant(int value) : Variant()
    {
        m_value.vt = VT_I4;
        m_value.lVal = value;
    }

    Variant::Variant(bool value) : Variant()
    {
        m_value.vt = VT_BOOL;
        m_value.boolVal = value ? VARIANT_TRUE : VARIANT_FALSE;
    }

    Variant Variant::Date(double value)
    {
        Variant result;
        result.m_value.vt = VT_DATE;
        result.m_value.date = value;
        return result;
    }

    Variant::Variant(const Variant& other) : Variant()
    {
        ::VariantCopy(&m_value, const_cast<VARIANT*>(&other.m_value));
    }

    Variant::Variant(Variant&& other) noexcept : m_value(other.m_value)
    {
        ::VariantInit(&other.m_value);
    }

    Variant& Variant::operator=(const Variant& other)
    {
        if (this != &other)
        {
            ::VariantClear(&m_value);
            ::VariantInit(&m_value);
            ::VariantCopy(&m_value, const_cast<VARIANT*>(&other.m_value));
        }
        return *this;
    }

    Variant& Variant::operator=(Variant&& other) noexcept
    {
        if (this != &other)
        {
            ::VariantClear(&m_value);
            m_value = other.m_value;
            ::VariantInit(&other.m_value);
        }
        return *this;
    }

    Variant::~Variant()
    {
        ::VariantClear(&m_value);
    }

    Variant Variant::Adopt(VARIANT&& value)
    {
        Variant result;
        ::VariantClear(&result.m_value);
        result.m_value = value;
        ::VariantInit(&value);
        return result;
    }

    Microsoft::WRL::ComPtr<IDispatch> Variant::AsDispatch() const
    {
        Microsoft::WRL::ComPtr<IDispatch> result;
        if (m_value.vt == VT_DISPATCH && m_value.pdispVal)
            result = m_value.pdispVal;
        else if (m_value.vt == VT_UNKNOWN && m_value.punkVal)
            m_value.punkVal->QueryInterface(IID_PPV_ARGS(&result));
        return result;
    }

    std::wstring Variant::AsString() const
    {
        if (m_value.vt == VT_BSTR && m_value.bstrVal)
            return std::wstring(m_value.bstrVal, ::SysStringLen(m_value.bstrVal));
        if (m_value.vt == VT_EMPTY || m_value.vt == VT_NULL) return L"";

        VARIANT converted{};
        ::VariantInit(&converted);
        if (SUCCEEDED(::VariantChangeType(&converted, const_cast<VARIANT*>(&m_value), 0, VT_BSTR)))
        {
            std::wstring result = converted.bstrVal ? std::wstring(converted.bstrVal, ::SysStringLen(converted.bstrVal)) : L"";
            ::VariantClear(&converted);
            return result;
        }
        return L"";
    }

    double Variant::AsDouble(double fallback) const
    {
        if (m_value.vt == VT_R8) return m_value.dblVal;
        if (m_value.vt == VT_R4) return m_value.fltVal;
        if (m_value.vt == VT_I4 || m_value.vt == VT_INT) return static_cast<double>(m_value.lVal);
        if (m_value.vt == VT_I2) return static_cast<double>(m_value.iVal);
        VARIANT converted{};
        ::VariantInit(&converted);
        if (SUCCEEDED(::VariantChangeType(&converted, const_cast<VARIANT*>(&m_value), 0, VT_R8)))
        {
            const double result = converted.dblVal;
            ::VariantClear(&converted);
            return result;
        }
        return fallback;
    }

    int Variant::AsInt(int fallback) const
    {
        return static_cast<int>(AsDouble(static_cast<double>(fallback)));
    }

    bool Variant::AsBool(bool fallback) const
    {
        if (m_value.vt == VT_BOOL) return m_value.boolVal == VARIANT_TRUE;
        VARIANT converted{};
        ::VariantInit(&converted);
        if (SUCCEEDED(::VariantChangeType(&converted, const_cast<VARIANT*>(&m_value), 0, VT_BOOL)))
        {
            const bool result = converted.boolVal == VARIANT_TRUE;
            ::VariantClear(&converted);
            return result;
        }
        return fallback;
    }

    CellValue Variant::ToCellValue() const
    {
        switch (m_value.vt)
        {
        case VT_EMPTY:
        case VT_NULL:
            return CellValue::Empty();
        case VT_BSTR:
            return CellValue::Text(AsString());
        case VT_BOOL:
            return CellValue::Boolean(AsBool());
        case VT_R8:
        case VT_R4:
        case VT_I2:
        case VT_I4:
        case VT_INT:
        case VT_UI1:
        case VT_UI2:
        case VT_UI4:
            return CellValue::Number(AsDouble());
        case VT_DATE:
            return CellValue::Number(m_value.date);
        default:
            return CellValue::Text(AsString());
        }
    }

    // -------------------------------------------------------------------------
    // Late-Binding-Aufrufe über IDispatch
    // -------------------------------------------------------------------------
    // Excel-Eigenschaften und -Methoden werden anhand ihres Namens zur Laufzeit
    // aufgelöst. COM-Fehler werden als std::runtime_error weitergegeben.
    Variant Dispatch::Invoke(const wchar_t* name, WORD flags, const std::vector<Variant>& args, const Variant* putValue) const
    {
        if (!m_value) throw std::runtime_error("Ungültiges COM-Objekt.");

        DISPID dispid{};
        LPOLESTR methodName = const_cast<LPOLESTR>(name);
        HRESULT hr = m_value->GetIDsOfNames(IID_NULL, &methodName, 1, LOCALE_USER_DEFAULT, &dispid);
        if (FAILED(hr))
            throw std::runtime_error("Excel-COM: Member nicht gefunden: " + Narrow(name));

        std::vector<VARIANTARG> rawArgs;
        rawArgs.reserve(args.size() + (putValue ? 1u : 0u));

        if (putValue)
        {
            VARIANTARG copied{};
            ::VariantInit(&copied);
            ::VariantCopy(&copied, const_cast<VARIANT*>(&putValue->Get()));
            rawArgs.push_back(copied);
        }

        for (auto it = args.rbegin(); it != args.rend(); ++it)
        {
            VARIANTARG copied{};
            ::VariantInit(&copied);
            ::VariantCopy(&copied, const_cast<VARIANT*>(&it->Get()));
            rawArgs.push_back(copied);
        }

        DISPPARAMS params{};
        params.rgvarg = rawArgs.empty() ? nullptr : rawArgs.data();
        params.cArgs = static_cast<UINT>(rawArgs.size());
        DISPID namedPut = DISPID_PROPERTYPUT;
        if (putValue)
        {
            params.rgdispidNamedArgs = &namedPut;
            params.cNamedArgs = 1;
        }

        VARIANT result{};
        ::VariantInit(&result);
        EXCEPINFO ex{};
        UINT argError = 0;
        hr = m_value->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT, flags, &params, &result, &ex, &argError);

        for (auto& arg : rawArgs) ::VariantClear(&arg);

        if (FAILED(hr))
        {
            const auto description = DescriptionFromExcepInfo(ex);
            ::VariantClear(&result);
            std::wostringstream message;
            message << L"Excel-COM-Fehler bei '" << name << L"' (HRESULT 0x" << std::hex << static_cast<unsigned long>(hr) << L")";
            if (!description.empty()) message << L": " << description;
            throw std::runtime_error(Narrow(message.str()));
        }

        return ResultVariant(result);
    }

    Variant Dispatch::Get(const wchar_t* name, const std::vector<Variant>& args) const
    {
        return Invoke(name, DISPATCH_PROPERTYGET, args);
    }

    void Dispatch::Put(const wchar_t* name, const Variant& value) const
    {
        (void)Invoke(name, DISPATCH_PROPERTYPUT, {}, &value);
    }

    Variant Dispatch::Call(const wchar_t* name, const std::vector<Variant>& args) const
    {
        return Invoke(name, DISPATCH_METHOD, args);
    }

    // -------------------------------------------------------------------------
    // Arbeitsmappe und Arbeitsblätter
    // -------------------------------------------------------------------------
    // Workbook ist bewegbar, aber nicht kopierbar. Dadurch bleibt eindeutig, welche
    // Instanz für das Schließen der zugrunde liegenden Excel-Arbeitsmappe zuständig ist.
    Workbook::~Workbook()
    {
        try { Close(false); } catch (...) {}
    }

    Dispatch Workbook::Worksheet(const std::wstring& sheetName) const
    {
        auto sheets = Dispatch(m_workbook.Get(L"Worksheets").AsDispatch());
        auto sheet = sheets.Get(L"Item", { Variant(sheetName) }).AsDispatch();
        if (!sheet) throw std::runtime_error("Arbeitsblatt nicht gefunden: " + Narrow(sheetName));
        return Dispatch(sheet);
    }

    bool Workbook::HasSheet(const std::wstring& sheetName) const
    {
        try
        {
            (void)Worksheet(sheetName);
            return true;
        }
        catch (...) { return false; }
    }

    std::wstring Workbook::FirstSheetName() const
    {
        auto sheets = Dispatch(m_workbook.Get(L"Worksheets").AsDispatch());
        auto first = Dispatch(sheets.Get(L"Item", { Variant(1) }).AsDispatch());
        return first ? first.Get(L"Name").AsString() : L"";
    }

    /// Liest den Excel-UsedRange möglichst blockweise. Das ist deutlich schneller
    /// als für jede einzelne Zelle einen separaten COM-Aufruf auszuführen.
    SheetTable Workbook::ReadSheet(const std::wstring& sheetName) const
    {
        SheetTable table;
        table.sheetName = sheetName;
        const auto sheet = Worksheet(sheetName);
        const auto used = Dispatch(sheet.Get(L"UsedRange").AsDispatch());
        if (!used) return table;

        const auto rowsObject = Dispatch(used.Get(L"Rows").AsDispatch());
        const auto colsObject = Dispatch(used.Get(L"Columns").AsDispatch());
        const int rows = std::max(1, rowsObject.Get(L"Count").AsInt(1));
        const int cols = std::max(1, colsObject.Get(L"Count").AsInt(1));
        const int usedFirstRow = used.Get(L"Row").AsInt(1);
        table.firstColumn = used.Get(L"Column").AsInt(1);

        auto values = used.Get(L"Value2");
        std::vector<std::vector<CellValue>> matrix(static_cast<size_t>(rows), std::vector<CellValue>(static_cast<size_t>(cols)));

        const VARIANT& raw = values.Get();
        if ((raw.vt & VT_ARRAY) && raw.parray)
        {
            SAFEARRAY* array = raw.parray;
            LONG rLow = 0, rHigh = -1, cLow = 0, cHigh = -1;
            ::SafeArrayGetLBound(array, 1, &rLow);
            ::SafeArrayGetUBound(array, 1, &rHigh);
            ::SafeArrayGetLBound(array, 2, &cLow);
            ::SafeArrayGetUBound(array, 2, &cHigh);

            for (LONG r = rLow; r <= rHigh && (r - rLow) < rows; ++r)
            {
                for (LONG c = cLow; c <= cHigh && (c - cLow) < cols; ++c)
                {
                    LONG idx[2] = { r, c };
                    VARIANT element{};
                    ::VariantInit(&element);
                    if (SUCCEEDED(::SafeArrayGetElement(array, idx, &element)))
                    {
                        auto cell = Variant::Adopt(std::move(element)).ToCellValue();
                        matrix[static_cast<size_t>(r - rLow)][static_cast<size_t>(c - cLow)] = std::move(cell);
                    }
                }
            }
        }
        else
        {
            matrix[0][0] = values.ToCellValue();
        }

        if (matrix.empty()) return table;

        // Nicht mehr davon ausgehen, dass die Überschrift zwingend in der ersten
        // Zeile des UsedRange liegt. Damit bleiben Tabellen lesbar, wenn z. B.
        // eine Titelzeile eingefügt oder die Spaltenanzahl geändert wurde.
        size_t headerRow = 0;
        int bestScore = -100000;
        const size_t inspectRows = std::min<size_t>(matrix.size(), 20);
        for (size_t r = 0; r < inspectRows; ++r)
        {
            int nonEmpty = 0;
            int textCount = 0;
            int numberCount = 0;
            for (const auto& cell : matrix[r])
            {
                if (cell.IsEmpty()) continue;
                ++nonEmpty;
                if (cell.type == CellType::Text) ++textCount;
                else if (cell.type == CellType::Number) ++numberCount;
            }
            if (nonEmpty == 0) continue;
            const int score = textCount * 4 + nonEmpty * 2 - numberCount * 2 - (nonEmpty == 1 ? 8 : 0);
            if (score > bestScore)
            {
                bestScore = score;
                headerRow = r;
            }
        }

        table.firstRow = usedFirstRow + static_cast<int>(headerRow);
        table.headers.reserve(matrix[headerRow].size());
        for (const auto& cell : matrix[headerRow])
        {
            if (cell.type == CellType::Text) table.headers.push_back(cell.text);
            else if (cell.type == CellType::Number)
            {
                std::wostringstream out;
                out << cell.number;
                table.headers.push_back(out.str());
            }
            else if (cell.type == CellType::Boolean) table.headers.push_back(cell.boolean ? L"TRUE" : L"FALSE");
            else table.headers.push_back(L"");
        }
        for (size_t r = headerRow + 1; r < matrix.size(); ++r)
        {
            table.rows.push_back(std::move(matrix[r]));
            table.rowNumbers.push_back(usedFirstRow + static_cast<int>(r));
        }
        return table;
    }

    CellValue Workbook::GetCell(const std::wstring& sheetName, int row, int column) const
    {
        auto sheet = Worksheet(sheetName);
        auto cells = Dispatch(sheet.Get(L"Cells").AsDispatch());
        auto cell = Dispatch(cells.Get(L"Item", { Variant(row), Variant(column) }).AsDispatch());
        if (!cell) return CellValue::Empty();
        return cell.Get(L"Value2").ToCellValue();
    }

    /// Zentraler Schreibpfad für Zellen. Fachliche Datumswerte werden als VT_DATE
    /// an Excel übergeben und anschließend mit einem lokalen Datumsformat versehen.
    void Workbook::SetCell(const std::wstring& sheetName, int row, int column, const CellValue& value, bool dateFormat)
    {
        auto sheet = Worksheet(sheetName);
        auto cells = Dispatch(sheet.Get(L"Cells").AsDispatch());
        auto cell = Dispatch(cells.Get(L"Item", { Variant(row), Variant(column) }).AsDispatch());

        // Datumswerte werden bewusst als COM VT_DATE über Range.Value geschrieben.
        // Damit entscheidet Excel selbst über die korrekte interne Datumsrepräsentation.
        // Das verhindert Fälle, in denen ein numerischer Serienwert durch lokale
        // Format-/Typkonvertierung als eine kleine Zahl (z. B. "2") stehen bleibt.
        if (dateFormat && value.type == CellType::Number)
        {
            cell.Put(L"Value", Variant::Date(value.number));
        }
        else
        {
            switch (value.type)
            {
            case CellType::Text:
                cell.Put(L"Value2", Variant(value.text));
                break;
            case CellType::Number:
                cell.Put(L"Value2", Variant(value.number));
                break;
            case CellType::Boolean:
                cell.Put(L"Value2", Variant(value.boolean));
                break;
            default:
                cell.Put(L"Value2", Variant(L""));
                break;
            }
        }

        if (dateFormat)
        {
            try
            {
                cell.Put(L"NumberFormatLocal", Variant(L"TT.MM.JJJJ"));
            }
            catch (...)
            {
                cell.Put(L"NumberFormat", Variant(L"dd.mm.yyyy"));
            }
        }
    }

    void Workbook::ClearRange(const std::wstring& sheetName, int firstRow, int firstColumn, int lastRow, int lastColumn)
    {
        auto sheet = Worksheet(sheetName);
        auto range = Dispatch(sheet.Get(L"Range", { Variant(RangeAddress(firstRow, firstColumn, lastRow, lastColumn)) }).AsDispatch());
        if (range) range.Call(L"ClearContents");
    }

    void Workbook::CopyFormats(const std::wstring& sheetName, int sourceRow, int targetRow, int firstColumn, int lastColumn)
    {
        auto sheet = Worksheet(sheetName);
        auto source = Dispatch(sheet.Get(L"Range", { Variant(RangeAddress(sourceRow, firstColumn, sourceRow, lastColumn)) }).AsDispatch());
        auto target = Dispatch(sheet.Get(L"Range", { Variant(RangeAddress(targetRow, firstColumn, targetRow, lastColumn)) }).AsDispatch());
        if (!source || !target) return;
        source.Call(L"Copy");
        // xlPasteFormats = -4122
        target.Call(L"PasteSpecial", { Variant(-4122) });
    }

    void Workbook::ConfigurePrint(const std::wstring& sheetName, int firstRow, int firstColumn, int lastRow, int lastColumn, bool landscape, int titleRows)
    {
        auto sheet = Worksheet(sheetName);
        auto pageSetup = Dispatch(sheet.Get(L"PageSetup").AsDispatch());
        if (!pageSetup) return;

        // Excel: xlPortrait = 1, xlLandscape = 2
        pageSetup.Put(L"Orientation", Variant(landscape ? 2 : 1));
        pageSetup.Put(L"Zoom", Variant(false));
        pageSetup.Put(L"FitToPagesWide", Variant(1));
        pageSetup.Put(L"FitToPagesTall", Variant(false));
        pageSetup.Put(L"CenterHorizontally", Variant(true));
        pageSetup.Put(L"PrintArea", Variant(L"$" + ColumnName(firstColumn) + L"$" + std::to_wstring(firstRow) +
            L":$" + ColumnName(lastColumn) + L"$" + std::to_wstring(lastRow)));
        if (titleRows > 0)
            pageSetup.Put(L"PrintTitleRows", Variant(L"$1:$" + std::to_wstring(titleRows)));
    }

    void Workbook::ConfigurePageHeader(const std::wstring& sheetName, const std::wstring& leftHeader, const std::wstring& centerHeader, const std::wstring& rightHeader)
    {
        auto sheet = Worksheet(sheetName);
        auto pageSetup = Dispatch(sheet.Get(L"PageSetup").AsDispatch());
        if (!pageSetup) return;

        pageSetup.Put(L"LeftHeader", Variant(leftHeader));
        pageSetup.Put(L"CenterHeader", Variant(centerHeader));
        pageSetup.Put(L"RightHeader", Variant(rightHeader));
    }

    void Workbook::ExportSheetPdf(const std::wstring& sheetName, const std::filesystem::path& pdfPath)
    {
        std::filesystem::create_directories(pdfPath.parent_path());
        auto sheet = Worksheet(sheetName);
        // xlTypePDF = 0
        sheet.Call(L"ExportAsFixedFormat", { Variant(0), Variant(pdfPath.wstring()) });
    }

    void Workbook::Save()
    {
        m_workbook.Call(L"Save");
    }

    void Workbook::Close(bool saveChanges)
    {
        if (m_closed || !m_workbook) return;
        m_workbook.Call(L"Close", { Variant(saveChanges) });
        m_closed = true;
    }

    bool Workbook::IsReadOnly() const
    {
        return m_workbook.Get(L"ReadOnly").AsBool(false);
    }

    // -------------------------------------------------------------------------
    // Excel.Application-Lebenszyklus
    // -------------------------------------------------------------------------
    // Excel läuft unsichtbar und ohne Benutzerinteraktion. Beim Destruktor wird Quit
    // aufgerufen, damit keine verwaisten EXCEL.EXE-Prozesse zurückbleiben.
    Application::Application()
    {
        CLSID clsid{};
        HRESULT hr = ::CLSIDFromProgID(L"Excel.Application", &clsid);
        if (FAILED(hr))
            throw std::runtime_error("Microsoft Excel ist nicht installiert oder die Excel-COM-Schnittstelle ist nicht registriert.");

        Microsoft::WRL::ComPtr<IDispatch> excel;
        hr = ::CoCreateInstance(clsid, nullptr, CLSCTX_LOCAL_SERVER, IID_PPV_ARGS(&excel));
        if (FAILED(hr) || !excel)
            throw std::runtime_error("Microsoft Excel konnte nicht gestartet werden.");

        m_excel = Dispatch(excel);
        m_excel.Put(L"Visible", Variant(false));
        m_excel.Put(L"DisplayAlerts", Variant(false));
        try { m_excel.Put(L"AskToUpdateLinks", Variant(false)); } catch (...) {}
        // Die Anwendung arbeitet vollständig im Hintergrund. Bildschirmaktualisierung ist
        // für diese COM-Vorgänge nicht erforderlich und verursacht bei vielen
        // Zelländerungen nur zusätzlichen Overhead.
        try { m_excel.Put(L"ScreenUpdating", Variant(false)); } catch (...) {}
        try { m_excel.Put(L"DisplayStatusBar", Variant(false)); } catch (...) {}
    }

    Application::~Application()
    {
        try
        {
            if (m_excel) m_excel.Call(L"Quit");
        }
        catch (...) {}
    }

    Workbook Application::Open(const std::filesystem::path& path, bool readOnly) const
    {
        auto workbooks = Dispatch(m_excel.Get(L"Workbooks").AsDispatch());
        // Workbooks.Open(Filename, UpdateLinks, ReadOnly)
        auto workbook = workbooks.Call(L"Open", { Variant(path.wstring()), Variant(0), Variant(readOnly) }).AsDispatch();
        if (!workbook) throw std::runtime_error("Excel-Datei konnte nicht geöffnet werden: " + Narrow(path.wstring()));
        return Workbook(Dispatch(workbook));
    }
}
