// ============================================================================
// Datei: DateUtils.h
// Zweck: Deklariert Hilfsfunktionen für Datumskonvertierung, Textnormalisierung und Anzeige von Excel-Zellwerten.
//
// Verantwortlichkeiten:
// - Kapselt die Behandlung deutscher Datumsangaben und Excel-Datumsserienwerte.
//
// Hinweis: Kommentare erläutern Architektur und nicht offensichtliche Logik.
// Triviale Sprachkonstrukte werden bewusst nicht zeilenweise kommentiert.
// ============================================================================
#pragma once

#include "Models.h"
#include <optional>
#include <string>

namespace med::date
{
    /// Liefert den heutigen Kalendertag ohne Uhrzeit.
    Day Today();

    /// Liest Datumsangaben aus GUI/Excel. Unterstützt u. a. TT.MM.JJJJ und ISO-Datum.
    std::optional<Day> ParseGermanDate(const std::wstring& value);

    /// Formatiert einen Kalendertag einheitlich als TT.MM.JJJJ für die Benutzeroberfläche.
    std::wstring FormatGermanDate(Day value);

    /// Konvertiert zwischen Excel-Datumsseriennummer und std::chrono::sys_days.
    Day FromExcelSerial(double serial);
    double ToExcelSerial(Day value);

    /// Wandelt einen typisierten Zellwert in einen für die GUI geeigneten Text um.
    std::wstring CellToDisplay(const CellValue& value, bool dateLike = false);
    std::optional<Day> CellToDate(const CellValue& value);

    /// Entfernt führende und nachfolgende Leerzeichen.
    std::wstring Trim(std::wstring value);

    /// Normalisiert Text für robuste Vergleiche von Spaltennamen und Konfigurationswerten.
    std::wstring Normalize(std::wstring value);
    /// Erkennt anhand des Spaltennamens, ob eine Spalte voraussichtlich Datumswerte enthält.
    bool HeaderLooksLikeDate(const std::wstring& header);
}
