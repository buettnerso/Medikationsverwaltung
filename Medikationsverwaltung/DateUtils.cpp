// ============================================================================
// Datei: DateUtils.cpp
// Zweck: Implementiert robuste Datums- und Textkonvertierungen für Eingaben aus Excel und aus der GUI.
//
// Verantwortlichkeiten:
// - Akzeptiert mehrere Datumsformate und wandelt zwischen std::chrono::sys_days und Excel-Serienwerten um.
// - Normalisiert Spaltennamen, damit unterschiedliche Schreibweisen zuverlässig verglichen werden können.
//
// Hinweis: Kommentare erläutern Architektur und nicht offensichtliche Logik.
// Triviale Sprachkonstrukte werden bewusst nicht zeilenweise kommentiert.
// ============================================================================
#include "pch.h"
#include "DateUtils.h"

#include <cwctype>
#include <iomanip>
#include <sstream>

namespace med::date
{
    Day Today()
    {
        SYSTEMTIME st{};
        ::GetLocalTime(&st);
        return std::chrono::sys_days{
            std::chrono::year{ static_cast<int>(st.wYear) } /
            std::chrono::month{ st.wMonth } /
            std::chrono::day{ st.wDay }
        };
    }

    // -------------------------------------------------------------------------
    // Textnormalisierung
    // -------------------------------------------------------------------------
    // Diese Funktionen werden besonders beim Vergleich frei benannter Excel-
    // Spalten verwendet. Normalize reduziert Unterschiede durch Groß-/Kleinschreibung,
    // Leerzeichen und Satzzeichen.
    std::wstring Trim(std::wstring value)
    {
        auto isSpace = [](wchar_t c) { return std::iswspace(c) != 0; };
        while (!value.empty() && isSpace(value.front())) value.erase(value.begin());
        while (!value.empty() && isSpace(value.back())) value.pop_back();
        return value;
    }

    std::wstring Normalize(std::wstring value)
    {
        value = Trim(std::move(value));
        std::wstring result;
        result.reserve(value.size());
        for (wchar_t c : value)
        {
            if (std::iswalnum(c))
            {
                result.push_back(static_cast<wchar_t>(std::towlower(c)));
            }
        }
        return result;
    }

    std::optional<Day> ParseGermanDate(const std::wstring& value)
    {
        const auto s = Trim(value);
        if (s.empty()) return std::nullopt;

        int d = 0, m = 0, y = 0;
        wchar_t sep1 = 0, sep2 = 0;
        std::wistringstream in(s);
        if ((in >> d >> sep1 >> m >> sep2 >> y) &&
            (sep1 == L'.' || sep1 == L'/' || sep1 == L'-') &&
            (sep2 == L'.' || sep2 == L'/' || sep2 == L'-'))
        {
            const auto ymd = std::chrono::year{ y } /
                std::chrono::month{ static_cast<unsigned>(m) } /
                std::chrono::day{ static_cast<unsigned>(d) };
            if (ymd.ok()) return Day{ ymd };
        }

        // ISO-Format als zusätzliche robuste Eingabe unterstützen.
        if (s.size() >= 10 && s[4] == L'-' && s[7] == L'-')
        {
            try
            {
                y = std::stoi(s.substr(0, 4));
                m = std::stoi(s.substr(5, 2));
                d = std::stoi(s.substr(8, 2));
                const auto ymd = std::chrono::year{ y } /
                    std::chrono::month{ static_cast<unsigned>(m) } /
                    std::chrono::day{ static_cast<unsigned>(d) };
                if (ymd.ok()) return Day{ ymd };
            }
            catch (...) {}
        }

        // Manche ältere/extern bearbeitete Tabellen enthalten Excel-Seriennummern
        // versehentlich als Text. Diese werden für Datumsfelder ebenfalls erkannt.
        try
        {
            std::wstring numeric = s;
            std::replace(numeric.begin(), numeric.end(), L',', L'.');
            size_t parsed = 0;
            const double serial = std::stod(numeric, &parsed);
            if (parsed == numeric.size() && serial >= 1000.0 && serial <= 100000.0)
                return FromExcelSerial(serial);
        }
        catch (...) {}

        return std::nullopt;
    }

    std::wstring FormatGermanDate(Day value)
    {
        const std::chrono::year_month_day ymd{ value };
        std::wostringstream out;
        out << std::setfill(L'0')
            << std::setw(2) << static_cast<unsigned>(ymd.day()) << L'.'
            << std::setw(2) << static_cast<unsigned>(ymd.month()) << L'.'
            << static_cast<int>(ymd.year());
        return out.str();
    }

    Day FromExcelSerial(double serial)
    {
        // Excel/Windows-Dateisystem: 1899-12-30 ist für moderne Datumswerte
        // der praktikable Bezugspunkt und berücksichtigt den historischen 1900-Leap-Year-Bug.
        const Day base = std::chrono::sys_days{ std::chrono::year{ 1899 } / 12 / 30 };
        return base + std::chrono::days{ static_cast<long long>(serial) };
    }

    double ToExcelSerial(Day value)
    {
        const Day base = std::chrono::sys_days{ std::chrono::year{ 1899 } / 12 / 30 };
        return static_cast<double>((value - base).count());
    }

    bool HeaderLooksLikeDate(const std::wstring& header)
    {
        const auto n = Normalize(header);
        return n == L"am" ||
            n.find(L"datum") != std::wstring::npos ||
            n.find(L"date") != std::wstring::npos ||
            n.find(L"erhaltenam") != std::wstring::npos ||
            n.find(L"bestelltam") != std::wstring::npos ||
            n.find(L"zuruckgegeben") != std::wstring::npos ||
            n.find(L"zurueckgegeben") != std::wstring::npos ||
            n.find(L"zurückgegeben") != std::wstring::npos ||
            n.find(L"ruckgabe") != std::wstring::npos ||
            n.find(L"rueckgabe") != std::wstring::npos ||
            n.find(L"rückgabe") != std::wstring::npos ||
            n.find(L"vernichtung") != std::wstring::npos ||
            n.find(L"verfall") != std::wstring::npos ||
            n.find(L"expiry") != std::wstring::npos ||
            n.find(L"dispensing") != std::wstring::npos ||
            n.find(L"delivery") != std::wstring::npos ||
            n.find(L"received") != std::wstring::npos;
    }

    std::wstring CellToDisplay(const CellValue& value, bool dateLike)
    {
        switch (value.type)
        {
        case CellType::Text:
            if (dateLike)
            {
                if (const auto parsed = ParseGermanDate(value.text))
                    return FormatGermanDate(*parsed);
            }
            return value.text;
        case CellType::Boolean:
            return value.boolean ? L"Ja" : L"Nein";
        case CellType::Number:
            if (dateLike && value.number > 1000.0)
            {
                return FormatGermanDate(FromExcelSerial(value.number));
            }
            else
            {
                std::wostringstream out;
                if (std::abs(value.number - std::round(value.number)) < 0.0000001)
                    out << static_cast<long long>(std::llround(value.number));
                else
                    out << std::fixed << std::setprecision(2) << value.number;
                return out.str();
            }
        default:
            return L"";
        }
    }

    std::optional<Day> CellToDate(const CellValue& value)
    {
        if (value.type == CellType::Number && value.number > 1000.0)
            return FromExcelSerial(value.number);
        if (value.type == CellType::Text)
            return ParseGermanDate(value.text);
        return std::nullopt;
    }
}
