// ============================================================================
// Datei: UiHelpers.cpp
// Zweck: Implementiert kleine wiederverwendbare Bausteine für das programmgesteuert erzeugte WinUI-Layout.
//
// Verantwortlichkeiten:
// - Vereinheitlicht Schrift, Kartenrahmen, einfache Tabellen und Statusfarben.
//
// Hinweis: Kommentare erläutern Architektur und nicht offensichtliche Logik.
// Triviale Sprachkonstrukte werden bewusst nicht zeilenweise kommentiert.
// ============================================================================
#include "pch.h"
#include "UiHelpers.h"

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Media;
using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Text;

namespace med::ui
{
    // -------------------------------------------------------------------------
    // Farben und visuelle Grundelemente
    // -------------------------------------------------------------------------
    SolidColorBrush Brush(unsigned char r, unsigned char g, unsigned char b)
    {
        return SolidColorBrush(Color{ 255, r, g, b });
    }

    SolidColorBrush StatusBrush(int state)
    {
        switch (state)
        {
        case 0: return Brush(207, 250, 223); // OK
        case 1: return Brush(255, 236, 179); // soon
        case 2: return Brush(255, 205, 210); // due
        case 3: return Brush(207, 226, 255); // pending
        default: return Brush(235, 235, 235);
        }
    }

    /// Zentralisiert Basisschrift und FontWeight, damit die programmgesteuerte GUI
    /// nicht in jeder Ansicht eigene Formatierungsregeln wiederholt.
    TextBlock Text(const std::wstring& value, double fontSize, bool bold)
    {
        TextBlock t;
        t.Text(value);
        t.FontSize(fontSize);
        t.TextWrapping(TextWrapping::Wrap);
        if (bold) t.FontWeight(FontWeights::SemiBold());
        return t;
    }

    Border Card(const UIElement& child, double padding)
    {
        Border border;
        border.Background(Brush(255, 255, 255));
        border.BorderBrush(Brush(224, 229, 236));
        border.BorderThickness(Thickness{ 1,1,1,1 });
        border.CornerRadius(CornerRadius{ 8,8,8,8 });
        border.Padding(Thickness{ padding,padding,padding,padding });
        border.Child(child);
        return border;
    }

    /// Einfache Nur-Lesen-Tabelle für kleine Übersichten. Interaktive Tabellen mit
    /// Filter/Sortierung werden direkt in MainWindow aufgebaut.
    FrameworkElement Table(const std::vector<std::wstring>& headers, const std::vector<std::vector<std::wstring>>& rows, size_t maxRows)
    {
        StackPanel outer;
        outer.Spacing(0);

        auto addRow = [&](const std::vector<std::wstring>& values, bool header)
        {
            Grid grid;
            const size_t cols = std::max<size_t>(1, headers.size());
            for (size_t i = 0; i < cols; ++i)
            {
                ColumnDefinition def;
                def.Width(GridLength{ i == 0 ? 1.2 : 1.0, GridUnitType::Star });
                grid.ColumnDefinitions().Append(def);
            }
            for (size_t c = 0; c < cols; ++c)
            {
                Border cell;
                cell.BorderBrush(Brush(224, 229, 236));
                cell.BorderThickness(Thickness{ c == 0 ? 1.0 : 0.0, 0.0, 1.0, 1.0 });
                cell.Padding(Thickness{ 8,7,8,7 });
                if (header) cell.Background(Brush(244, 247, 251));
                auto text = Text(c < values.size() ? values[c] : L"", 13.0, header);
                text.VerticalAlignment(VerticalAlignment::Center);
                cell.Child(text);
                Grid::SetColumn(cell, static_cast<int>(c));
                grid.Children().Append(cell);
            }
            outer.Children().Append(grid);
        };

        addRow(headers, true);
        size_t count = 0;
        for (const auto& row : rows)
        {
            if (count++ >= maxRows) break;
            addRow(row, false);
        }
        if (rows.size() > maxRows)
        {
            auto note = Text(L"… weitere Zeilen sind in der Excel-Datei vorhanden.", 12.0, false);
            note.Margin(Thickness{ 8,8,8,8 });
            outer.Children().Append(note);
        }

        ScrollViewer scroll;
        scroll.HorizontalScrollBarVisibility(ScrollBarVisibility::Auto);
        scroll.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
        scroll.MaxHeight(520);
        scroll.Content(outer);
        return scroll;
    }

    StackPanel LabeledField(const std::wstring& label, const Control& control)
    {
        StackPanel panel;
        panel.Spacing(5);
        panel.MinWidth(170);
        panel.Children().Append(Text(label, 12.0, true));
        panel.Children().Append(control);
        return panel;
    }
}
