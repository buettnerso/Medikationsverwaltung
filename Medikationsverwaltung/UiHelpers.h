// ============================================================================
// Datei: UiHelpers.h
// Zweck: Deklariert wiederverwendbare WinUI-Hilfsfunktionen für Text, Karten, Tabellen und Farbpinsel.
//
// Verantwortlichkeiten:
// - Dient ausschließlich der Darstellung und enthält keine Fach- oder Excel-Logik.
//
// Hinweis: Kommentare erläutern Architektur und nicht offensichtliche Logik.
// Triviale Sprachkonstrukte werden bewusst nicht zeilenweise kommentiert.
// ============================================================================
#pragma once

#include "pch.h"
#include <string>
#include <vector>

namespace med::ui
{
    /// Erzeugt einen einheitlich formatierten TextBlock.
    winrt::Microsoft::UI::Xaml::Controls::TextBlock Text(
        const std::wstring& value,
        double fontSize = 14.0,
        bool bold = false);

    /// Verpackt Inhalt in die im Programm verwendete Karten-/GroupBox-Optik.
    winrt::Microsoft::UI::Xaml::Controls::Border Card(
        const winrt::Microsoft::UI::Xaml::UIElement& child,
        double padding = 16.0);

    /// Erzeugt eine einfache schreibgeschützte Tabelle. Für Filter/Sortierung wird
    /// MainWindow::BuildInteractiveTable verwendet.
    winrt::Microsoft::UI::Xaml::FrameworkElement Table(
        const std::vector<std::wstring>& headers,
        const std::vector<std::vector<std::wstring>>& rows,
        size_t maxRows = 100);

    /// Kombiniert Feldbezeichnung und Eingabe-Control in einem vertikalen Block.
    winrt::Microsoft::UI::Xaml::Controls::StackPanel LabeledField(
        const std::wstring& label,
        const winrt::Microsoft::UI::Xaml::Controls::Control& control);

    /// Hilfsfunktionen für konsistente RGB- und Statusfarben.
    winrt::Microsoft::UI::Xaml::Media::SolidColorBrush Brush(unsigned char r, unsigned char g, unsigned char b);
    winrt::Microsoft::UI::Xaml::Media::SolidColorBrush StatusBrush(int state);
}
