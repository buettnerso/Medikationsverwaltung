#include "pch.h"
#include "MainWindow.h"
#include "resource.h"

#include "DateUtils.h"
#include "AppVersion.h"
#include "AuditLogger.h"
#include "MedicationDocumentation.h"
#include "OrderPlanner.h"
#include "ReportService.h"
#include "UiHelpers.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <memory>
#include <cwchar>
#include <cwctype>
#include <sstream>

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Media;

namespace
{
    Button MakeButton(const std::wstring& text)
    {
        Button button;
        button.Content(box_value(text));
        button.Padding(Thickness{ 14,8,14,8 });
        button.MinHeight(36);
        return button;
    }

    Button MakePrimaryButton(const std::wstring& text)
    {
        auto button = MakeButton(text);
        // Dezentes, professionelles Blau nur für die wichtigste Aktion eines Bereichs.
        button.Background(med::ui::Brush(45, 93, 140));
        button.Foreground(med::ui::Brush(255, 255, 255));
        return button;
    }

    void CopyTextToClipboard(HWND owner, const std::wstring& text)
    {
        if (!::OpenClipboard(owner))
            throw std::runtime_error("Die Zwischenablage konnte nicht geöffnet werden.");

        HGLOBAL memory = nullptr;
        try
        {
            if (!::EmptyClipboard())
                throw std::runtime_error("Die Zwischenablage konnte nicht geleert werden.");

            const SIZE_T bytes = (text.size() + 1) * sizeof(wchar_t);
            memory = ::GlobalAlloc(GMEM_MOVEABLE, bytes);
            if (!memory)
                throw std::runtime_error("Für die Zwischenablage konnte kein Speicher reserviert werden.");

            void* target = ::GlobalLock(memory);
            if (!target)
                throw std::runtime_error("Der Zwischenablagespeicher konnte nicht geöffnet werden.");

            ::CopyMemory(target, text.c_str(), bytes);
            ::GlobalUnlock(memory);

            if (!::SetClipboardData(CF_UNICODETEXT, memory))
                throw std::runtime_error("Der Text konnte nicht in die Zwischenablage kopiert werden.");

            // Nach erfolgreichem SetClipboardData gehört der Speicher Windows.
            memory = nullptr;
            ::CloseClipboard();
        }
        catch (...)
        {
            if (memory) ::GlobalFree(memory);
            ::CloseClipboard();
            throw;
        }
    }

    std::wstring ExpiryForDocumentation(const med::CellValue& value, const std::wstring& header)
    {
        if (const auto parsed = med::date::CellToDate(value))
        {
            // Wenn Excel ein vollständiges Datum enthält, wird es auch im
            // Dokumentationstext vollständig als TT.MM.JJJJ dargestellt.
            return med::date::FormatGermanDate(*parsed);
        }

        // Teilangaben wie 01/2027 sollen unverändert erhalten bleiben.
        return med::date::Trim(med::date::CellToDisplay(value, true));
    }


    // Ordnet drei Arbeitskarten abhängig von der tatsächlich verfügbaren Breite an.
    // Große Fenster: 3 Spalten. Mittlere Fenster: 2 Spalten. Kleine Fenster: 1 Spalte.
    // Die Steuerelemente selbst werden dabei NICHT skaliert; nur ihre Anordnung ändert sich.
    Grid MakeResponsiveThreeCardGrid(
        const FrameworkElement& first,
        const FrameworkElement& second,
        const FrameworkElement& third)
    {
        Grid grid;
        grid.HorizontalAlignment(HorizontalAlignment::Stretch);

        for (int i = 0; i < 3; ++i)
        {
            ColumnDefinition column;
            column.Width(GridLength{ 1, GridUnitType::Star });
            grid.ColumnDefinitions().Append(column);

            RowDefinition row;
            row.Height(GridLength{ 0, GridUnitType::Auto });
            grid.RowDefinitions().Append(row);
        }

        grid.Children().Append(first);
        grid.Children().Append(second);
        grid.Children().Append(third);

        auto arrange = [grid, first, second, third](double width)
        {
            // Die Schwellen beziehen sich auf den Inhaltsbereich nach Navigation und Seitenrändern.
            // Dadurch bleibt z. B. auf einem kleineren Notebook ausreichend Platz pro Formular.
            if (width >= 1220.0)
            {
                for (unsigned int c = 0; c < 3; ++c)
                    grid.ColumnDefinitions().GetAt(c).Width(GridLength{ 1, GridUnitType::Star });

                Grid::SetRow(first, 0);  Grid::SetColumn(first, 0);  Grid::SetColumnSpan(first, 1);
                Grid::SetRow(second, 0); Grid::SetColumn(second, 1); Grid::SetColumnSpan(second, 1);
                Grid::SetRow(third, 0);  Grid::SetColumn(third, 2);  Grid::SetColumnSpan(third, 1);

                first.Margin(Thickness{ 0,0,6,0 });
                second.Margin(Thickness{ 6,0,6,0 });
                third.Margin(Thickness{ 6,0,0,0 });
            }
            else if (width >= 760.0)
            {
                grid.ColumnDefinitions().GetAt(0).Width(GridLength{ 1, GridUnitType::Star });
                grid.ColumnDefinitions().GetAt(1).Width(GridLength{ 1, GridUnitType::Star });
                grid.ColumnDefinitions().GetAt(2).Width(GridLength{ 0, GridUnitType::Pixel });

                Grid::SetRow(first, 0);  Grid::SetColumn(first, 0);  Grid::SetColumnSpan(first, 1);
                Grid::SetRow(second, 0); Grid::SetColumn(second, 1); Grid::SetColumnSpan(second, 1);
                Grid::SetRow(third, 1);  Grid::SetColumn(third, 0);  Grid::SetColumnSpan(third, 2);

                first.Margin(Thickness{ 0,0,6,6 });
                second.Margin(Thickness{ 6,0,0,6 });
                third.Margin(Thickness{ 0,6,0,0 });
            }
            else
            {
                grid.ColumnDefinitions().GetAt(0).Width(GridLength{ 1, GridUnitType::Star });
                grid.ColumnDefinitions().GetAt(1).Width(GridLength{ 0, GridUnitType::Pixel });
                grid.ColumnDefinitions().GetAt(2).Width(GridLength{ 0, GridUnitType::Pixel });

                Grid::SetRow(first, 0);  Grid::SetColumn(first, 0);  Grid::SetColumnSpan(first, 1);
                Grid::SetRow(second, 1); Grid::SetColumn(second, 0); Grid::SetColumnSpan(second, 1);
                Grid::SetRow(third, 2);  Grid::SetColumn(third, 0);  Grid::SetColumnSpan(third, 1);

                first.Margin(Thickness{ 0,0,0,8 });
                second.Margin(Thickness{ 0,8,0,8 });
                third.Margin(Thickness{ 0,8,0,0 });
            }
        };

        grid.Loaded([grid, arrange](auto const&, auto const&)
        {
            arrange(grid.ActualWidth());
        });
        grid.SizeChanged([arrange](auto const&, SizeChangedEventArgs const& args)
        {
            arrange(static_cast<double>(args.NewSize().Width));
        });

        return grid;
    }

    // Entsprechende responsive Anordnung für zwei Karten.
    Grid MakeResponsiveTwoCardGrid(
        const FrameworkElement& first,
        const FrameworkElement& second)
    {
        Grid grid;
        grid.HorizontalAlignment(HorizontalAlignment::Stretch);

        for (int i = 0; i < 2; ++i)
        {
            ColumnDefinition column;
            column.Width(GridLength{ 1, GridUnitType::Star });
            grid.ColumnDefinitions().Append(column);

            RowDefinition row;
            row.Height(GridLength{ 0, GridUnitType::Auto });
            grid.RowDefinitions().Append(row);
        }

        grid.Children().Append(first);
        grid.Children().Append(second);

        auto arrange = [grid, first, second](double width)
        {
            if (width >= 760.0)
            {
                grid.ColumnDefinitions().GetAt(0).Width(GridLength{ 1, GridUnitType::Star });
                grid.ColumnDefinitions().GetAt(1).Width(GridLength{ 1, GridUnitType::Star });

                Grid::SetRow(first, 0); Grid::SetColumn(first, 0);
                Grid::SetRow(second, 0); Grid::SetColumn(second, 1);
                first.Margin(Thickness{ 0,0,6,0 });
                second.Margin(Thickness{ 6,0,0,0 });
            }
            else
            {
                grid.ColumnDefinitions().GetAt(0).Width(GridLength{ 1, GridUnitType::Star });
                grid.ColumnDefinitions().GetAt(1).Width(GridLength{ 0, GridUnitType::Pixel });

                Grid::SetRow(first, 0); Grid::SetColumn(first, 0);
                Grid::SetRow(second, 1); Grid::SetColumn(second, 0);
                first.Margin(Thickness{ 0,0,0,8 });
                second.Margin(Thickness{ 0,8,0,0 });
            }
        };

        grid.Loaded([grid, arrange](auto const&, auto const&)
        {
            arrange(grid.ActualWidth());
        });
        grid.SizeChanged([arrange](auto const&, SizeChangedEventArgs const& args)
        {
            arrange(static_cast<double>(args.NewSize().Width));
        });

        return grid;
    }

    std::vector<std::wstring> SplitNonEmptyLines(const std::wstring& value)
    {
        // WinUI TextBox kann Zeilenumbrüche je nach Eingabe/Paste als CRLF, LF oder
        // auch als einzelnes CR liefern. Vor dem Splitten deshalb alle Varianten
        // vereinheitlichen. Dadurch werden mehrere Box-/Kit-IDs zuverlässig als
        // einzelne Zeilen erkannt.
        std::wstring normalized;
        normalized.reserve(value.size());
        for (size_t i = 0; i < value.size(); ++i)
        {
            const wchar_t ch = value[i];
            if (ch == L'\r')
            {
                normalized.push_back(L'\n');
                if (i + 1 < value.size() && value[i + 1] == L'\n') ++i;
            }
            else if (ch == 0x2028 || ch == 0x2029)
            {
                normalized.push_back(L'\n');
            }
            else
            {
                normalized.push_back(ch);
            }
        }

        std::vector<std::wstring> result;
        std::wstringstream stream(normalized);
        std::wstring line;
        while (std::getline(stream, line, L'\n'))
        {
            line = med::date::Trim(line);
            if (!line.empty()) result.push_back(line);
        }
        return result;
    }

    bool RowHasData(const std::vector<med::CellValue>& row)
    {
        return std::any_of(row.begin(), row.end(), [](const med::CellValue& c) { return !c.IsEmpty(); });
    }

    std::wstring PlanWindow(const med::OrderPlan& plan)
    {
        std::wstring result = L"–";
        if (plan.recommendedFrom && plan.latestOrderDate)
            result = med::date::FormatGermanDate(*plan.recommendedFrom) + L" – " + med::date::FormatGermanDate(*plan.latestOrderDate);
        else if (plan.latestOrderDate)
            result = L"spätestens " + med::date::FormatGermanDate(*plan.latestOrderDate);
        if (plan.estimated && result != L"–") result += L" (Prognose)";
        return result;
    }

    int StatusIndex(med::OrderState state)
    {
        switch (state)
        {
        case med::OrderState::Ok: return 0;
        case med::OrderState::Soon: return 1;
        case med::OrderState::Due: return 2;
        case med::OrderState::Pending: return 3;
        case med::OrderState::NotRequired: return 4;
        default: return 4;
        }
    }

    std::wstring Widen(const std::string& value)
    {
        if (value.empty()) return L"";
        const int needed = ::MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
        std::wstring out(static_cast<size_t>(needed), L'\0');
        ::MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), out.data(), needed);
        return out;
    }

    void WriteUtf8File(const std::filesystem::path& path, const std::wstring& text)
    {
        std::filesystem::create_directories(path.parent_path());
        const int needed = ::WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
        std::string utf8(static_cast<size_t>(needed), '\0');
        ::WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), utf8.data(), needed, nullptr, nullptr);
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        const unsigned char bom[] = { 0xEF, 0xBB, 0xBF };
        out.write(reinterpret_cast<const char*>(bom), 3);
        out.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
    }

    std::wstring ReadTextFile(const std::filesystem::path& path)
    {
        if (!std::filesystem::exists(path)) return L"";
        std::ifstream in(path, std::ios::binary);
        std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (bytes.size() >= 3 && static_cast<unsigned char>(bytes[0]) == 0xEF && static_cast<unsigned char>(bytes[1]) == 0xBB && static_cast<unsigned char>(bytes[2]) == 0xBF)
            bytes.erase(0, 3);
        return Widen(bytes);
    }
}

namespace med
{
    MainWindow::MainWindow()
    {
        m_settings = AppSettings::Load();
        AuditLogger::SetAuditDirectory(m_settings.auditFolder);
        m_window = Window();
        m_window.Title(L"Medikationsverwaltung");

        auto native = m_window.as<::IWindowNative>();
        winrt::check_hresult(native->get_WindowHandle(&m_hwnd));

        // Kompakte, monitorabhängige Startgröße:
        // Ziel ist ungefähr das Seitenverhältnis eines A5-Blatts im Querformat,
        // ohne das Fenster auf kleineren Notebooks über den verfügbaren Arbeitsbereich
        // hinaus zu vergrößern. Schriftgrößen und Steuerelemente werden dabei nicht skaliert.
        constexpr double desiredWidthDip = 980.0;
        constexpr double desiredHeightDip = 680.0;

        const UINT dpi = std::max<UINT>(96, ::GetDpiForWindow(m_hwnd));
        const double dpiScale = static_cast<double>(dpi) / 96.0;

        int desiredWidth = static_cast<int>(std::lround(desiredWidthDip * dpiScale));
        int desiredHeight = static_cast<int>(std::lround(desiredHeightDip * dpiScale));

        HMONITOR monitor = ::MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO monitorInfo{ sizeof(MONITORINFO) };
        if (::GetMonitorInfoW(monitor, &monitorInfo))
        {
            const int workWidth = monitorInfo.rcWork.right - monitorInfo.rcWork.left;
            const int workHeight = monitorInfo.rcWork.bottom - monitorInfo.rcWork.top;

            // Auf kleinen Displays maximal rund 90 % des nutzbaren Desktopbereichs belegen.
            // Das Fenster darf anschließend vom Benutzer beliebig vergrößert/maximiert werden.
            desiredWidth = std::min(desiredWidth, static_cast<int>(workWidth * 0.90));
            desiredHeight = std::min(desiredHeight, static_cast<int>(workHeight * 0.90));

            const int x = monitorInfo.rcWork.left + (workWidth - desiredWidth) / 2;
            const int y = monitorInfo.rcWork.top + (workHeight - desiredHeight) / 2;

            ::SetWindowPos(
                m_hwnd,
                nullptr,
                x,
                y,
                desiredWidth,
                desiredHeight,
                SWP_NOZORDER | SWP_NOACTIVATE);
        }
        else
        {
            ::SetWindowPos(
                m_hwnd,
                nullptr,
                0,
                0,
                desiredWidth,
                desiredHeight,
                SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        }

        // App-Icon direkt aus den EXE-Ressourcen laden. Das ist für die
        // unpackaged WinUI-3-Anwendung robuster als ein Laufzeit-Dateipfad.
        const HINSTANCE module = ::GetModuleHandleW(nullptr);
        if (module)
        {
            const auto bigIcon = static_cast<HICON>(::LoadImageW(
                module,
                MAKEINTRESOURCEW(IDI_APP_ICON),
                IMAGE_ICON,
                32,
                32,
                LR_DEFAULTCOLOR | LR_SHARED));

            const auto smallIcon = static_cast<HICON>(::LoadImageW(
                module,
                MAKEINTRESOURCEW(IDI_APP_ICON),
                IMAGE_ICON,
                16,
                16,
                LR_DEFAULTCOLOR | LR_SHARED));

            if (bigIcon)
                ::SendMessageW(m_hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(bigIcon));
            if (smallIcon)
                ::SendMessageW(m_hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(smallIcon));
        }

        BuildNavigation();
        LoadStudies();
        RenderOverview();
    }

    void MainWindow::Activate()
    {
        m_window.Activate();
    }

    void MainWindow::BuildNavigation()
    {
        m_navigation = NavigationView();
        m_navigation.PaneTitle(L"Medikationsverwaltung");
        m_navigation.IsBackButtonVisible(NavigationViewBackButtonVisible::Collapsed);
        m_navigation.IsSettingsVisible(false);
        // Die Navigation reagiert auf die Fensterbreite:
        // - groß: vollständig aufgeklappt,
        // - mittel: kompakte Symbolleiste,
        // - klein: Overlay/Hamburger-Menü.
        // Dadurch verliert der eigentliche Arbeitsbereich auf kleinen Displays nicht 220 px.
        m_navigation.PaneDisplayMode(NavigationViewPaneDisplayMode::Auto);
        m_navigation.OpenPaneLength(220);
        m_navigation.CompactPaneLength(52);
        m_navigation.CompactModeThresholdWidth(900);
        m_navigation.ExpandedModeThresholdWidth(1250);

        // Kleine Versionsangabe im Fußbereich der Navigation.
        // Dieselbe Version wird auch in jeden Audit-Eintrag geschrieben.
        TextBlock versionFooter;
        versionFooter.Text(med::version::Display);
        versionFooter.FontSize(10.0);
        versionFooter.Foreground(ui::Brush(112, 118, 128));
        versionFooter.Margin(Thickness{ 12,6,0,10 });
        m_navigation.PaneFooter(versionFooter);

        auto add = [&](const std::wstring& title, const std::wstring& tag)
        {
            NavigationViewItem item;
            item.Content(box_value(title));
            item.Tag(box_value(tag));
            m_navigation.MenuItems().Append(item);
            return item;
        };

        auto first = add(L"Übersicht", L"overview");
        add(L"Berichte", L"reports");
        add(L"Vorlagen & Dokumente", L"documents");
        add(L"Einstellungen", L"settings");
        m_navigation.SelectedItem(first);

        m_navigation.SelectionChanged([this](NavigationView const&, NavigationViewSelectionChangedEventArgs const& args)
        {
            if (m_rendering) return;
            auto item = args.SelectedItem().try_as<NavigationViewItem>();
            if (!item) return;
            auto tag = unbox_value_or<hstring>(item.Tag(), L"overview");
            RenderSection(tag.c_str());
        });

        m_window.Content(m_navigation);
    }

    void MainWindow::LoadStudies()
    {
        try
        {
            m_studies = m_repository.LoadAll(m_settings.studyFolder);
            if (m_selectedStudy >= m_studies.size()) m_selectedStudy = 0;
        }
        catch (const std::exception& e)
        {
            m_studies.clear();
            ShowError(e);
        }
    }

    void MainWindow::ReloadKeepingSelection()
    {
        std::wstring selectedName;
        std::wstring selectedImpId;
        if (auto s = CurrentStudy())
        {
            selectedName = s->studyName;
            if (const auto* imp = SelectedImp(*s)) selectedImpId = imp->id;
        }
        LoadStudies();
        if (!selectedName.empty())
        {
            for (size_t i = 0; i < m_studies.size(); ++i)
                if (m_studies[i].studyName == selectedName) { m_selectedStudy = i; break; }
        }
        m_selectedImp = 0;
        if (auto s = CurrentStudy(); s && !selectedImpId.empty())
        {
            for (size_t i = 0; i < s->imps.size(); ++i)
                if (s->imps[i].id == selectedImpId) { m_selectedImp = i; break; }
        }
        RenderSection(m_currentSection);
    }

    void MainWindow::ReloadCurrentStudyKeepingSelection()
    {
        // Nach einer Änderung wurde bislang der komplette Studienordner neu eingelesen.
        // Bei mehreren Excel-Dateien startet das für jede Studie erneut Excel-COM und ist
        // unnötig langsam. Nach einem Schreibvorgang genügt es, ausschließlich die
        // geänderte Arbeitsmappe neu einzulesen. Ein kompletter Reload bleibt über
        // "Daten neu einlesen" / "Aktualisieren" verfügbar.
        if (m_selectedStudy >= m_studies.size())
        {
            ReloadKeepingSelection();
            return;
        }

        const auto excelPath = m_studies[m_selectedStudy].excelPath;
        std::wstring selectedImpId;
        if (const auto* imp = SelectedImp(m_studies[m_selectedStudy])) selectedImpId = imp->id;

        try
        {
            auto refreshed = m_repository.LoadOne(excelPath);
            m_studies[m_selectedStudy] = std::move(refreshed);
            m_selectedImp = 0;
            if (!selectedImpId.empty())
            {
                const auto& study = m_studies[m_selectedStudy];
                for (size_t i = 0; i < study.imps.size(); ++i)
                {
                    if (study.imps[i].id == selectedImpId)
                    {
                        m_selectedImp = i;
                        break;
                    }
                }
            }
            RenderSection(m_currentSection);
        }
        catch (...)
        {
            // Fallback: Falls sich z. B. die Studienstruktur grundlegend geändert hat,
            // bleibt der vollständige Reload als robuste Rückfallebene erhalten.
            ReloadKeepingSelection();
        }
    }

    const StudyData* MainWindow::CurrentStudy() const
    {
        return m_selectedStudy < m_studies.size() ? &m_studies[m_selectedStudy] : nullptr;
    }

    StudyData* MainWindow::CurrentStudy()
    {
        return m_selectedStudy < m_studies.size() ? &m_studies[m_selectedStudy] : nullptr;
    }

    const ImpData* MainWindow::SelectedImp(const StudyData& study) const
    {
        if (study.imps.empty()) return nullptr;
        const size_t index = std::min(m_selectedImp, study.imps.size() - 1);
        return &study.imps[index];
    }

    std::filesystem::path MainWindow::ReportRoot(const StudyData& study) const
    {
        return m_settings.ExportFolderForStudy(study.studyName, study.studyDirectory);
    }

    std::filesystem::path MainWindow::GlobalReportRoot() const
    {
        return m_settings.studyFolder / L"Exporte";
    }

    std::filesystem::path MainWindow::DefaultDrugAccountOverallTemplate() const
    {
        return m_settings.executableDirectory / L"Templates" / L"DrugAccount_Gesamt.xlsx";
    }

    std::filesystem::path MainWindow::DefaultDrugAccountPatientTemplate() const
    {
        return m_settings.executableDirectory / L"Templates" / L"DrugAccount_Patient.xlsx";
    }

    std::optional<std::filesystem::path> MainWindow::PickFolder(const std::filesystem::path& initialFolder) const
    {
        ::Microsoft::WRL::ComPtr<IFileOpenDialog> dialog;
        winrt::check_hresult(::CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(dialog.GetAddressOf())));

        FILEOPENDIALOGOPTIONS options{};
        winrt::check_hresult(dialog->GetOptions(&options));
        winrt::check_hresult(dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST));
        dialog->SetTitle(L"Ordner auswählen");

        if (!initialFolder.empty() && std::filesystem::exists(initialFolder))
        {
            ::Microsoft::WRL::ComPtr<IShellItem> initialItem;
            if (SUCCEEDED(::SHCreateItemFromParsingName(initialFolder.c_str(), nullptr, IID_PPV_ARGS(initialItem.GetAddressOf()))))
                dialog->SetFolder(initialItem.Get());
        }

        const HRESULT shown = dialog->Show(m_hwnd);
        if (shown == HRESULT_FROM_WIN32(ERROR_CANCELLED)) return std::nullopt;
        winrt::check_hresult(shown);

        ::Microsoft::WRL::ComPtr<IShellItem> selected;
        winrt::check_hresult(dialog->GetResult(selected.GetAddressOf()));
        PWSTR rawPath = nullptr;
        winrt::check_hresult(selected->GetDisplayName(SIGDN_FILESYSPATH, &rawPath));
        std::filesystem::path result(rawPath ? rawPath : L"");
        if (rawPath) ::CoTaskMemFree(rawPath);
        return result;
    }

    void MainWindow::ChooseStudyFolder()
    {
        try
        {
            const auto selected = PickFolder(m_settings.studyFolder);
            if (!selected) return;
            std::filesystem::create_directories(*selected);
            m_settings.studyFolder = std::filesystem::weakly_canonical(*selected);
            m_settings.Save();
            m_selectedStudy = 0;
            LoadStudies();
            RenderSection(m_currentSection);
        }
        catch (const std::exception& e)
        {
            ShowError(e);
        }
    }

    void MainWindow::RenderSection(const std::wstring& tag)
    {
        m_currentSection = tag;
        if (tag == L"studies") RenderStudies();
        else if (tag == L"reports") RenderGlobalReports();
        else if (tag == L"documents") RenderGlobalDocuments();
        else if (tag == L"settings") RenderSettings();
        else RenderOverview();
    }

    FrameworkElement MainWindow::BuildTopBar(const std::wstring& title, const std::wstring& subtitle)
    {
        StackPanel labels;
        labels.Spacing(3);
        labels.Children().Append(ui::Text(title, 24, true));
        if (!subtitle.empty())
        {
            auto sub = ui::Text(subtitle, 13, false);
            sub.Foreground(ui::Brush(95, 107, 125));
            labels.Children().Append(sub);
        }
        return labels;
    }

    std::vector<std::vector<std::wstring>> MainWindow::DisplayRows(const SheetTable& table, size_t maxRows) const
    {
        std::vector<std::vector<std::wstring>> result;
        for (const auto& row : table.rows)
        {
            if (!RowHasData(row)) continue;
            std::vector<std::wstring> display;
            for (size_t c = 0; c < table.headers.size(); ++c)
            {
                display.push_back(c < row.size() ? date::CellToDisplay(row[c], date::HeaderLooksLikeDate(table.headers[c])) : L"");
            }
            result.push_back(std::move(display));
            if (result.size() >= maxRows) break;
        }
        return result;
    }

    std::vector<int> MainWindow::DisplayRowKeys(const SheetTable& table, size_t maxRows) const
    {
        std::vector<int> result;
        for (size_t r = 0; r < table.rows.size(); ++r)
        {
            if (!RowHasData(table.rows[r])) continue;
            result.push_back(r < table.rowNumbers.size() ? table.rowNumbers[r] : table.firstRow + 1 + static_cast<int>(r));
            if (result.size() >= maxRows) break;
        }
        return result;
    }

    void MainWindow::RefreshCurrentView()
    {
        RenderSection(m_currentSection);
    }

    FrameworkElement MainWindow::BuildInteractiveTable(
        const std::wstring& key,
        const std::vector<std::wstring>& headers,
        const std::vector<std::vector<std::wstring>>& rows,
        const std::vector<int>& rowKeys,
        size_t maxRows,
        bool selectable)
    {
        StackPanel outer;
        outer.Spacing(0);
        outer.HorizontalAlignment(HorizontalAlignment::Stretch);
        if (headers.empty())
        {
            outer.Children().Append(ui::Text(L"Keine Tabellenspalten erkannt.", 13, false));
            return outer;
        }

        // Einheitlicher "Card-Table"-Stil:
        // - ein gemeinsamer abgerundeter Außenrahmen,
        // - ruhiger Tabellenkopf,
        // - sehr dezente Gitternetzlinien,
        // - alternierende Zeilenhintergründe für bessere Lesbarkeit.
        const auto tableBorderBrush = ui::Brush(214, 220, 229);
        const auto gridLineBrush = ui::Brush(232, 236, 242);
        const auto headerBackground = ui::Brush(246, 248, 251);
        const auto alternateRowBackground = ui::Brush(250, 251, 253);
        const auto selectedRowBackground = ui::Brush(238, 245, 255);
        const auto transparentBrush =
            SolidColorBrush(winrt::Windows::UI::Color{ 0, 0, 0, 0 });

        auto& state = m_tableStates[key];
        std::vector<size_t> visible;
        visible.reserve(rows.size());
        for (size_t r = 0; r < rows.size(); ++r)
        {
            bool keep = true;
            for (const auto& [column, filter] : state.filters)
            {
                if (date::Trim(filter).empty()) continue;
                const auto actual = column < rows[r].size() ? date::Normalize(rows[r][column]) : L"";
                if (actual.find(date::Normalize(filter)) == std::wstring::npos) { keep = false; break; }
            }
            if (keep) visible.push_back(r);
        }

        auto tryNumber = [](std::wstring value) -> std::optional<double>
        {
            value = med::date::Trim(std::move(value));
            if (value.empty()) return std::nullopt;
            std::replace(value.begin(), value.end(), L',', L'.');
            wchar_t* end = nullptr;
            const double number = std::wcstod(value.c_str(), &end);
            if (!end || end == value.c_str()) return std::nullopt;
            while (*end && std::iswspace(*end)) ++end;
            return *end == L'\0' ? std::optional<double>(number) : std::nullopt;
        };

        if (state.sortColumn >= 0 && static_cast<size_t>(state.sortColumn) < headers.size())
        {
            const size_t column = static_cast<size_t>(state.sortColumn);
            std::stable_sort(visible.begin(), visible.end(), [&](size_t a, size_t b)
            {
                const auto av = column < rows[a].size() ? rows[a][column] : L"";
                const auto bv = column < rows[b].size() ? rows[b][column] : L"";
                const auto ad = date::ParseGermanDate(av), bd = date::ParseGermanDate(bv);
                if (ad && bd) return state.sortAscending ? (*ad < *bd) : (*bd < *ad);
                const auto an = tryNumber(av), bn = tryNumber(bv);
                if (an && bn) return state.sortAscending ? (*an < *bn) : (*bn < *an);
                return state.sortAscending ? date::Normalize(av) < date::Normalize(bv) : date::Normalize(bv) < date::Normalize(av);
            });
        }

        // Kopf und Datenkörper sind getrennt, damit die Kopfzeile beim vertikalen
        // Scrollen stehen bleibt. Die Spalten werden zunächst mit Pixelbreiten
        // angelegt und anschließend aus EINEM gemeinsamen Breitenmodell synchronisiert.
        // Dadurch können Kopf und Tabelleninhalt nicht mehr gegeneinander verrutschen.
        auto addColumns = [&](Grid& grid)
        {
            if (selectable)
            {
                ColumnDefinition selectColumn;
                selectColumn.Width(GridLength{ 34, GridUnitType::Pixel });
                grid.ColumnDefinitions().Append(selectColumn);
            }

            for (size_t c = 0; c < headers.size(); ++c)
            {
                ColumnDefinition def;
                def.Width(GridLength{ 120, GridUnitType::Pixel });
                grid.ColumnDefinitions().Append(def);
            }
        };

        Grid headerGrid;
        headerGrid.HorizontalAlignment(HorizontalAlignment::Stretch);
        addColumns(headerGrid);
        RowDefinition headerRow;
        headerRow.Height(GridLength{ 0, GridUnitType::Auto });
        headerGrid.RowDefinitions().Append(headerRow);

        if (selectable)
        {
            Border selectHeader;
            selectHeader.BorderBrush(gridLineBrush);
            selectHeader.BorderThickness(Thickness{ 0,0,1,0 });
            selectHeader.Padding(Thickness{ 1,0,1,0 });

            auto tick = ui::Text(L"✓", 12, true);
            tick.HorizontalAlignment(HorizontalAlignment::Center);
            tick.VerticalAlignment(VerticalAlignment::Center);

            selectHeader.Child(tick);
            Grid::SetColumn(selectHeader, 0);
            headerGrid.Children().Append(selectHeader);
        }

        for (size_t c = 0; c < headers.size(); ++c)
        {
            Border cell;
            cell.BorderBrush(gridLineBrush);
            cell.BorderThickness(Thickness{
                0.0,
                0.0,
                (c + 1 < headers.size()) ? 1.0 : 0.0,
                0.0 });
            cell.Padding(Thickness{ 0,0,0,0 });

            std::wstring title = headers[c].empty() ? L"Spalte " + std::to_wstring(c + 1) : headers[c];
            if (state.filters.contains(c) && !date::Trim(state.filters[c]).empty()) title += L"  ●";
            if (state.sortColumn == static_cast<int>(c)) title += state.sortAscending ? L"  ↑" : L"  ↓";
            title += L"  ▾";

            Button button;
            button.Content(box_value(title));
            button.HorizontalAlignment(HorizontalAlignment::Stretch);
            button.HorizontalContentAlignment(HorizontalAlignment::Left);
            button.Padding(Thickness{ 8,2,8,2 });
            button.MinHeight(29);
            button.FontSize(13.0);
            button.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
            button.Background(transparentBrush);
            button.BorderThickness(Thickness{ 0,0,0,0 });
            button.CornerRadius(CornerRadius{ 0,0,0,0 });

            Flyout flyout;
            StackPanel menu;
            menu.Spacing(7);
            menu.MinWidth(250);
            menu.Children().Append(ui::Text(headers[c].empty() ? L"Spalte" : headers[c], 14, true));
            TextBox filterBox;
            auto it = state.filters.find(c);
            if (it != state.filters.end()) filterBox.Text(it->second);
            filterBox.PlaceholderText(L"Filter: enthält ...");
            menu.Children().Append(filterBox);

            auto apply = MakeButton(L"Filter anwenden");
            apply.Click([this, key, c, filterBox, flyout](auto&&, auto&&)
            {
                auto value = date::Trim(std::wstring(filterBox.Text().c_str()));
                if (value.empty()) m_tableStates[key].filters.erase(c);
                else m_tableStates[key].filters[c] = value;
                flyout.Hide();
                RefreshCurrentView();
            });
            menu.Children().Append(apply);

            StackPanel sortRow;
            sortRow.Orientation(Orientation::Horizontal);
            sortRow.Spacing(6);
            auto asc = MakeButton(L"↑ Aufsteigend");
            asc.Click([this, key, c, flyout](auto&&, auto&&)
            {
                auto& st = m_tableStates[key]; st.sortColumn = static_cast<int>(c); st.sortAscending = true;
                flyout.Hide(); RefreshCurrentView();
            });
            auto desc = MakeButton(L"↓ Absteigend");
            desc.Click([this, key, c, flyout](auto&&, auto&&)
            {
                auto& st = m_tableStates[key]; st.sortColumn = static_cast<int>(c); st.sortAscending = false;
                flyout.Hide(); RefreshCurrentView();
            });
            sortRow.Children().Append(asc); sortRow.Children().Append(desc);
            menu.Children().Append(sortRow);

            auto clear = MakeButton(L"Filter dieser Spalte löschen");
            clear.Click([this, key, c, flyout](auto&&, auto&&)
            {
                m_tableStates[key].filters.erase(c); flyout.Hide(); RefreshCurrentView();
            });
            menu.Children().Append(clear);
            auto reset = MakeButton(L"Alle Filter / Sortierung zurücksetzen");
            reset.Click([this, key, flyout](auto&&, auto&&)
            {
                auto& st = m_tableStates[key]; st.filters.clear(); st.sortColumn = -1; st.sortAscending = true;
                flyout.Hide(); RefreshCurrentView();
            });
            menu.Children().Append(reset);

            flyout.Content(menu);
            button.Flyout(flyout);
            cell.Child(button);
            Grid::SetColumn(cell, static_cast<int>(c + (selectable ? 1 : 0)));
            headerGrid.Children().Append(cell);
        }

        Grid bodyGrid;
        bodyGrid.HorizontalAlignment(HorizontalAlignment::Stretch);
        addColumns(bodyGrid);

        size_t shown = 0;
        for (const auto rowIndex : visible)
        {
            if (shown >= maxRows) break;
            RowDefinition dataRow;
            dataRow.Height(GridLength{ 0, GridUnitType::Auto });
            bodyGrid.RowDefinitions().Append(dataRow);
            const int gridRow = static_cast<int>(shown++);

            const int rowKey = rowIndex < rowKeys.size() ? rowKeys[rowIndex] : static_cast<int>(rowIndex);
            const bool selected = selectable && state.selectedKey == rowKey;

            const bool alternateRow = (gridRow % 2) != 0;

            if (selectable)
            {
                Border selectCell;
                selectCell.BorderBrush(gridLineBrush);
                selectCell.BorderThickness(Thickness{ 0,0,1,1 });
                selectCell.Padding(Thickness{ 0,0,0,0 });

                if (selected)
                    selectCell.Background(selectedRowBackground);
                else if (alternateRow)
                    selectCell.Background(alternateRowBackground);

                CheckBox check;
                check.IsChecked(selected);
                check.HorizontalAlignment(HorizontalAlignment::Center);
                check.VerticalAlignment(VerticalAlignment::Center);
                check.Click([this, key, rowKey](auto&& sender, auto&&)
                {
                    auto cb = sender.as<CheckBox>();
                    bool checked = false; if (auto value = cb.IsChecked()) checked = value.Value();
                    m_tableStates[key].selectedKey = checked ? rowKey : -1;
                    RefreshCurrentView();
                });
                selectCell.Child(check);
                Grid::SetRow(selectCell, gridRow);
                Grid::SetColumn(selectCell, 0);
                bodyGrid.Children().Append(selectCell);
            }

            for (size_t c = 0; c < headers.size(); ++c)
            {
                Border cell;
                cell.BorderBrush(gridLineBrush);
                cell.BorderThickness(Thickness{
                    0.0,
                    0.0,
                    (c + 1 < headers.size()) ? 1.0 : 0.0,
                    1.0 });
                cell.Padding(Thickness{ 8,4,8,4 });

                if (selected)
                    cell.Background(selectedRowBackground);
                else if (alternateRow)
                    cell.Background(alternateRowBackground);

                auto text = ui::Text(c < rows[rowIndex].size() ? rows[rowIndex][c] : L"", 13.0, false);
                text.VerticalAlignment(VerticalAlignment::Center);
                text.TextWrapping(TextWrapping::NoWrap);
                cell.Child(text);
                Grid::SetRow(cell, gridRow);
                Grid::SetColumn(cell, static_cast<int>(c + (selectable ? 1 : 0)));
                bodyGrid.Children().Append(cell);
            }
        }

        // Wieder die größere Tabellenhöhe wie vor v8.7. Die Kopfzeile bleibt
        // trotzdem fixiert; nur der Datenkörper scrollt intern.
        const double viewportHeight = 510.0;

        ScrollViewer bodyScroll;
        bodyScroll.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
        bodyScroll.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);
        bodyScroll.HorizontalAlignment(HorizontalAlignment::Stretch);
        bodyScroll.HorizontalContentAlignment(HorizontalAlignment::Stretch);
        bodyScroll.MaxHeight(viewportHeight);
        bodyScroll.Content(bodyGrid);

        const double restoreVertical = state.verticalOffset;
        bodyScroll.ViewChanged([this, key](auto const& sender, auto const&)
        {
            auto view = sender.as<ScrollViewer>();
            m_tableStates[key].verticalOffset = view.VerticalOffset();
        });
        bodyScroll.Loaded([bodyScroll, restoreVertical](auto const&, auto const&)
        {
            if (restoreVertical <= 0.5) return;
            auto vertical = box_value(restoreVertical)
                .as<winrt::Windows::Foundation::IReference<double>>();
            bodyScroll.ChangeView(nullptr, vertical, nullptr, true);
        });

        // Kopf- und Datenbereich bilden optisch eine gemeinsame Karte.
        // Die äußeren Ecken werden nur am oberen bzw. unteren Rand gerundet;
        // die inneren Zelllinien bleiben bewusst gerade.
        Border headerSurface;
        headerSurface.Background(headerBackground);
        headerSurface.BorderBrush(tableBorderBrush);
        headerSurface.BorderThickness(Thickness{ 1,1,1,1 });
        headerSurface.CornerRadius(CornerRadius{ 9,9,0,0 });
        headerSurface.Child(headerGrid);

        Border bodySurface;
        bodySurface.Background(ui::Brush(255, 255, 255));
        bodySurface.BorderBrush(tableBorderBrush);
        bodySurface.BorderThickness(Thickness{ 1,0,1,1 });
        bodySurface.CornerRadius(CornerRadius{ 0,0,9,9 });
        bodySurface.Child(bodyScroll);

        Grid tableStack;
        tableStack.HorizontalAlignment(HorizontalAlignment::Stretch);

        RowDefinition tableHeaderRow;
        tableHeaderRow.Height(GridLength{ 0, GridUnitType::Auto });
        RowDefinition tableBodyRow;
        tableBodyRow.Height(GridLength{ 0, GridUnitType::Auto });
        tableStack.RowDefinitions().Append(tableHeaderRow);
        tableStack.RowDefinitions().Append(tableBodyRow);

        Grid::SetRow(headerSurface, 0);
        Grid::SetRow(bodySurface, 1);
        tableStack.Children().Append(headerSurface);
        tableStack.Children().Append(bodySurface);

        // Hinweise stehen außerhalb der intern scrollenden Datenfläche.
        StackPanel result;
        result.Spacing(0);
        result.HorizontalAlignment(HorizontalAlignment::Stretch);
        result.Children().Append(tableStack);

        if (visible.size() > maxRows)
        {
            auto note = ui::Text(L"… weitere gefilterte Zeilen sind vorhanden.", 12, false);
            note.Margin(Thickness{ 6,4,6,4 });
            result.Children().Append(note);
        }
        if (visible.empty())
        {
            auto note = ui::Text(L"Keine Zeilen entsprechen den aktuellen Filtern.", 12, false);
            note.Margin(Thickness{ 6,6,6,6 });
            result.Children().Append(note);
        }

        // Ein äußerer horizontaler ScrollViewer wird immer verwendet. Solange die
        // Tabelle in die Fensterbreite passt, erscheint kein Scrollbalken. Bei
        // kleineren Fenstern bleibt horizontales Scrollen möglich.
        ScrollViewer horizontalScroll;
        horizontalScroll.HorizontalScrollBarVisibility(ScrollBarVisibility::Auto);
        horizontalScroll.VerticalScrollBarVisibility(ScrollBarVisibility::Disabled);
        horizontalScroll.HorizontalAlignment(HorizontalAlignment::Stretch);
        horizontalScroll.HorizontalContentAlignment(HorizontalAlignment::Left);
        horizontalScroll.Content(result);

        const double restoreHorizontal = state.horizontalOffset;
        horizontalScroll.ViewChanged([this, key](auto const& sender, auto const&)
        {
            auto view = sender.as<ScrollViewer>();
            m_tableStates[key].horizontalOffset = view.HorizontalOffset();
        });
        horizontalScroll.Loaded([horizontalScroll, restoreHorizontal](auto const&, auto const&)
        {
            if (restoreHorizontal <= 0.5) return;
            auto horizontal = box_value(restoreHorizontal)
                .as<winrt::Windows::Foundation::IReference<double>>();
            horizontalScroll.ChangeView(horizontal, nullptr, nullptr, true);
        });

        // Kopf und Datenkörper erhalten bei jeder Fensterbreite EXAKT die gleichen
        // Pixelbreiten. Die Tabelle füllt dabei weiterhin die verfügbare Breite aus.
        horizontalScroll.SizeChanged(
            [headerGrid, bodyGrid, headerSurface, bodySurface, tableStack, bodyScroll, result, headers, selectable]
            (auto const&, SizeChangedEventArgs const& args)
            {
                const double viewport = std::max(320.0, static_cast<double>(args.NewSize().Width));
                const double selectorWidth = selectable ? 34.0 : 0.0;
                constexpr double scrollbarGutter = 17.0;

                std::vector<double> minimums;
                std::vector<double> weights;
                minimums.reserve(headers.size());
                weights.reserve(headers.size());

                double minimumTotal = 0.0;
                double weightTotal = 0.0;
                for (const auto& rawHeader : headers)
                {
                    const auto header = date::Trim(rawHeader);
                    const double length = static_cast<double>(header.size());
                    const double minimum = std::clamp(88.0 + length * 2.1, 96.0, 176.0);
                    const double weight = std::clamp(0.9 + length / 18.0, 1.0, 1.9);
                    minimums.push_back(minimum);
                    weights.push_back(weight);
                    minimumTotal += minimum;
                    weightTotal += weight;
                }

                const double usable = std::max(
                    minimumTotal,
                    viewport - selectorWidth - scrollbarGutter);
                const double extra = std::max(0.0, usable - minimumTotal);

                if (selectable)
                {
                    headerGrid.ColumnDefinitions().GetAt(0).Width(
                        GridLength{ selectorWidth, GridUnitType::Pixel });
                    bodyGrid.ColumnDefinitions().GetAt(0).Width(
                        GridLength{ selectorWidth, GridUnitType::Pixel });
                }

                double dataTotal = 0.0;
                for (size_t c = 0; c < headers.size(); ++c)
                {
                    const double width = minimums[c] +
                        (weightTotal > 0.0 ? extra * (weights[c] / weightTotal) : 0.0);
                    const unsigned int index = static_cast<unsigned int>(
                        c + (selectable ? 1 : 0));

                    headerGrid.ColumnDefinitions().GetAt(index).Width(
                        GridLength{ width, GridUnitType::Pixel });
                    bodyGrid.ColumnDefinitions().GetAt(index).Width(
                        GridLength{ width, GridUnitType::Pixel });
                    dataTotal += width;
                }

                const double exactGridWidth = selectorWidth + dataTotal;
                headerGrid.Width(exactGridWidth);
                bodyGrid.Width(exactGridWidth);

                const double tableWidth = exactGridWidth + scrollbarGutter;
                headerSurface.Width(tableWidth);
                bodySurface.Width(tableWidth);
                bodyScroll.Width(tableWidth);
                tableStack.Width(tableWidth);
                result.Width(tableWidth);
            });

        return horizontalScroll;
    }

    void MainWindow::RenderOverview()
    {
        m_rendering = true;
        ScrollViewer scroll;
        scroll.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);

        const double restorePageOffset = m_overviewScrollOffset;
        scroll.ViewChanged([this](auto const& sender, auto const&)
        {
            auto view = sender.as<ScrollViewer>();
            m_overviewScrollOffset = view.VerticalOffset();
        });
        scroll.Loaded([scroll, restorePageOffset](auto const&, auto const&)
        {
            if (restorePageOffset <= 0.5) return;
            auto vertical = box_value(restorePageOffset)
                .as<winrt::Windows::Foundation::IReference<double>>();
            scroll.ChangeView(nullptr, vertical, nullptr, true);
        });

        StackPanel page;
        page.Padding(Thickness{ 18,16,18,24 });
        page.Spacing(14);
        page.Children().Append(BuildTopBar(L"Bestellübersicht aller Studien", L"Priorisiert Studien anhand von Bestand, terminierten Visiten, Lieferzeit und offenen Bestellungen."));

        if (m_studies.empty())
        {
            StackPanel empty;
            empty.Spacing(10);
            empty.Children().Append(ui::Text(L"Keine Studien gefunden", 19, true));
            empty.Children().Append(ui::Text(L"Den Studien-Datenordner kannst du unter Einstellungen festlegen. Pro Studie liegt dort ein eigener Unterordner mit der Studien-Excel."));
            auto settings = MakeButton(L"Einstellungen öffnen");
            settings.HorizontalAlignment(HorizontalAlignment::Left);
            settings.Click([this](auto&&, auto&&) { RenderSection(L"settings"); });
            empty.Children().Append(settings);
            page.Children().Append(ui::Card(empty));
            scroll.Content(page);
            m_navigation.Content(scroll);
            m_rendering = false;
            return;
        }

        struct OverviewEntry { const StudyData* study{}; const ImpData* imp{}; };
        std::vector<OverviewEntry> entries;
        for (const auto& s : m_studies)
            for (const auto& imp : s.imps)
                if (imp.orderRequired) entries.push_back({ &s, &imp });
        auto priority = [](OrderState state)
        {
            switch (state)
            {
            case OrderState::Due: return 0;
            case OrderState::Soon: return 1;
            case OrderState::Pending: return 2;
            case OrderState::Ok: return 3;
            default: return 4;
            }
        };
        std::sort(entries.begin(), entries.end(), [&](const auto& a, const auto& b)
        {
            const int pa = priority(a.imp->plan.state), pb = priority(b.imp->plan.state);
            if (pa != pb) return pa < pb;
            if (a.imp->plan.latestOrderDate && b.imp->plan.latestOrderDate && a.imp->plan.latestOrderDate != b.imp->plan.latestOrderDate)
                return *a.imp->plan.latestOrderDate < *b.imp->plan.latestOrderDate;
            if (a.study->studyName != b.study->studyName) return a.study->studyName < b.study->studyName;
            return a.imp->name < b.imp->name;
        });

        std::vector<std::wstring> headers{ L"Prio", L"Studie", L"IMP", L"Form", L"Bestand", L"Minimum", L"Lieferzeit", L"Nächstes Bestellfenster", L"Status" };
        std::vector<std::vector<std::wstring>> rows;
        for (size_t i = 0; i < entries.size(); ++i)
        {
            const auto& s = *entries[i].study;
            const auto& imp = *entries[i].imp;
            rows.push_back({ std::to_wstring(i + 1), s.studyName, imp.name, imp.applicationForm.empty() ? L"–" : imp.applicationForm,
                date::CellToDisplay(CellValue::Number(imp.stock)), date::CellToDisplay(CellValue::Number(imp.minimumStock)),
                std::to_wstring(imp.leadTimeDays) + L" T", PlanWindow(imp.plan), OrderPlanner::StateText(imp.plan.state) });
        }
        if (rows.empty()) rows.push_back({ L"–", L"–", L"Keine bestellpflichtigen IMPs", L"–", L"–", L"–", L"–", L"–", L"–" });
        page.Children().Append(BuildInteractiveTable(L"global|overview", headers, rows, {}, 100, false));

        // Studienauswahl und Aktionen dürfen auf kleinen Displays nicht gegeneinander
        // drücken. Auf breiten Fenstern stehen die Aktionen rechts; auf schmaleren
        // Fenstern wechseln sie automatisch in eine zweite Zeile.
        Grid selectorArea;
        selectorArea.HorizontalAlignment(HorizontalAlignment::Stretch);

        ColumnDefinition selectorMainColumn;
        selectorMainColumn.Width(GridLength{ 1, GridUnitType::Star });
        ColumnDefinition selectorActionColumn;
        selectorActionColumn.Width(GridLength{ 0, GridUnitType::Auto });
        selectorArea.ColumnDefinitions().Append(selectorMainColumn);
        selectorArea.ColumnDefinitions().Append(selectorActionColumn);

        for (int i = 0; i < 2; ++i)
        {
            RowDefinition row;
            row.Height(GridLength{ 0, GridUnitType::Auto });
            selectorArea.RowDefinitions().Append(row);
        }

        StackPanel selectorInfo;
        selectorInfo.Spacing(4);

        StackPanel selectorLine;
        selectorLine.Orientation(Orientation::Horizontal);
        selectorLine.Spacing(10);

        auto label = ui::Text(L"Studie auswählen:", 14, true);
        label.VerticalAlignment(VerticalAlignment::Center);
        selectorLine.Children().Append(label);

        m_studyCombo = ComboBox();
        for (const auto& study : m_studies)
        {
            std::wstring display = study.studyName;
            if (!date::Trim(study.euctNumber).empty()) display += L" [" + date::Trim(study.euctNumber) + L"]";
            m_studyCombo.Items().Append(box_value(display));
        }
        m_studyCombo.SelectedIndex(static_cast<int>(m_selectedStudy));
        m_studyCombo.MinWidth(240);
        m_studyCombo.MaxWidth(420);
        m_studyCombo.SelectionChanged([this](auto&&, auto&&)
        {
            if (m_rendering || !m_studyCombo) return;
            const int index = m_studyCombo.SelectedIndex();
            if (index >= 0 && static_cast<size_t>(index) < m_studies.size())
            {
                m_selectedStudy = static_cast<size_t>(index);
                m_selectedImp = 0;
                m_selectedStudyTab = 0;
                RenderOverview();
            }
        });
        selectorLine.Children().Append(m_studyCombo);
        selectorInfo.Children().Append(selectorLine);

        if (auto study = CurrentStudy())
        {
            auto meta = ui::Text(
                L"IMPs: " + std::to_wstring(study->imps.size()) +
                L"   |   Datei: " + study->excelPath.filename().wstring(),
                12,
                false);
            meta.Foreground(ui::Brush(95, 107, 125));
            meta.TextWrapping(TextWrapping::Wrap);
            selectorInfo.Children().Append(meta);
        }

        Grid::SetRow(selectorInfo, 0);
        Grid::SetColumn(selectorInfo, 0);
        selectorArea.Children().Append(selectorInfo);

        ScrollViewer actionScroll;
        actionScroll.HorizontalScrollBarVisibility(ScrollBarVisibility::Auto);
        actionScroll.VerticalScrollBarVisibility(ScrollBarVisibility::Disabled);
        actionScroll.HorizontalAlignment(HorizontalAlignment::Stretch);

        StackPanel studyActions;
        studyActions.Orientation(Orientation::Horizontal);
        studyActions.Spacing(7);

        if (CurrentStudy())
        {
            auto openExcel = MakeButton(L"Studien-Excel öffnen");
            openExcel.Click([this](auto&&, auto&&)
            {
                if (auto s = CurrentStudy()) OpenPath(s->excelPath);
            });

            auto openDocs = MakeButton(L"Dokumentenordner öffnen");
            openDocs.Click([this](auto&&, auto&&)
            {
                if (auto s = CurrentStudy())
                {
                    auto path = s->studyDirectory / L"Documents";
                    std::filesystem::create_directories(path);
                    OpenPath(path);
                }
            });

            auto reload = MakeButton(L"Daten neu einlesen");
            reload.Click([this](auto&&, auto&&) { ReloadKeepingSelection(); });

            studyActions.Children().Append(openExcel);
            studyActions.Children().Append(openDocs);
            studyActions.Children().Append(reload);
        }

        actionScroll.Content(studyActions);
        Grid::SetRow(actionScroll, 0);
        Grid::SetColumn(actionScroll, 1);
        selectorArea.Children().Append(actionScroll);

        auto arrangeSelector = [selectorArea, selectorInfo, actionScroll](double width)
        {
            if (width >= 1080.0)
            {
                Grid::SetRow(selectorInfo, 0);
                Grid::SetColumn(selectorInfo, 0);
                Grid::SetColumnSpan(selectorInfo, 1);

                Grid::SetRow(actionScroll, 0);
                Grid::SetColumn(actionScroll, 1);
                Grid::SetColumnSpan(actionScroll, 1);
                actionScroll.Margin(Thickness{ 14,0,0,0 });
            }
            else
            {
                Grid::SetRow(selectorInfo, 0);
                Grid::SetColumn(selectorInfo, 0);
                Grid::SetColumnSpan(selectorInfo, 2);

                Grid::SetRow(actionScroll, 1);
                Grid::SetColumn(actionScroll, 0);
                Grid::SetColumnSpan(actionScroll, 2);
                actionScroll.Margin(Thickness{ 0,8,0,0 });
            }
        };

        selectorArea.Loaded([selectorArea, arrangeSelector](auto const&, auto const&)
        {
            arrangeSelector(selectorArea.ActualWidth());
        });
        selectorArea.SizeChanged([arrangeSelector](auto const&, SizeChangedEventArgs const& args)
        {
            arrangeSelector(static_cast<double>(args.NewSize().Width));
        });

        page.Children().Append(selectorArea);

        // Klare visuelle Trennung: oberhalb studienübergreifende Übersicht/Aktionen,
        // unterhalb ausschließlich Inhalte der aktuell ausgewählten Studie.
        Border divider;
        divider.Height(1);
        divider.Background(ui::Brush(219, 225, 232));
        divider.Margin(Thickness{ 0,10,0,6 });
        page.Children().Append(divider);

        if (auto study = CurrentStudy())
        {
            Border studyHeader;
            studyHeader.Background(ui::Brush(247, 249, 252));
            studyHeader.BorderBrush(ui::Brush(224, 229, 236));
            studyHeader.BorderThickness(Thickness{ 1,1,1,1 });
            studyHeader.CornerRadius(CornerRadius{ 7,7,7,7 });
            studyHeader.Padding(Thickness{ 14,10,14,10 });
            studyHeader.Margin(Thickness{ 0,2,0,4 });

            StackPanel studyLabels;
            studyLabels.Spacing(2);
            auto studyTitle = ui::Text(study->studyName, 20, true);
            studyTitle.Foreground(ui::Brush(45, 93, 140));
            studyLabels.Children().Append(studyTitle);

            std::wstring studyMeta = L"Studienspezifischer Arbeitsbereich";
            if (!date::Trim(study->euctNumber).empty()) studyMeta += L"   |   EUCT-No.: " + date::Trim(study->euctNumber);
            studyMeta += L"   |   " + std::to_wstring(study->imps.size()) + L" IMP";
            if (study->imps.size() != 1) studyMeta += L"s";
            auto metaText = ui::Text(studyMeta, 13, false);
            metaText.Foreground(ui::Brush(95, 107, 125));
            studyLabels.Children().Append(metaText);

            studyHeader.Child(studyLabels);
            page.Children().Append(studyHeader);
        }

        page.Children().Append(BuildStudyTabs());

        scroll.Content(page);
        m_navigation.Content(scroll);
        m_rendering = false;
    }

    FrameworkElement MainWindow::BuildStudyTabs()
    {
        TabView tabs;
        tabs.IsAddTabButtonVisible(false);
        if (const auto* study = CurrentStudy())
        {
            auto add = [&](const std::wstring& header, const FrameworkElement& content)
            {
                TabViewItem item;
                item.Header(box_value(header));
                item.IsClosable(false);
                item.Content(content);
                tabs.TabItems().Append(item);
            };
            add(L"Übersicht", BuildStudyOverview(*study));
            add(L"Bestellungen", BuildOrders(*study));
            add(L"Wareneingang / Bestand", BuildInventory(*study));
            add(L"Patientenvisiten", BuildVisits(*study));
            add(L"Berichte", BuildReports(*study));
            add(L"Notizen", BuildNotes(*study));

            const int maxIndex = static_cast<int>(tabs.TabItems().Size()) - 1;
            tabs.SelectedIndex(std::clamp(m_selectedStudyTab, 0, std::max(0, maxIndex)));
            tabs.SelectionChanged([this](auto const& sender, auto const&)
            {
                if (m_rendering) return;
                auto view = sender.as<TabView>();
                if (view.SelectedIndex() >= 0) m_selectedStudyTab = view.SelectedIndex();
            });
        }
        return tabs;
    }

    FrameworkElement MainWindow::BuildStudyProfile(const StudyData& study)
    {
        StackPanel panel;
        panel.Spacing(12);
        panel.Margin(Thickness{ 0,12,0,0 });
        panel.Children().Append(ui::Text(L"Studienprofil – " + study.studyName, 18, true));

        if (!study.hasStudyProfile || study.profileTable.headers.empty())
        {
            StackPanel info;
            info.Spacing(8);
            info.Children().Append(ui::Text(L"Für diese Arbeitsmappe wurde noch kein strukturiertes Studienprofil als erstes Tabellenblatt erkannt.", 14, true));
            info.Children().Append(ui::Text(L"Die bestehende Datei bleibt im Legacy-Modus nutzbar. Für Multi-IMP-Studien empfiehlt sich das spaltenorientierte Studienprofil mit IMP1, IMP2, ... als Spalten."));
            auto open = MakeButton(L"Studien-Excel öffnen");
            auto path = study.excelPath;
            open.Click([this, path](auto&&, auto&&) { OpenPath(path); });
            info.Children().Append(open);
            panel.Children().Append(ui::Card(info));
            return panel;
        }

        StackPanel identity;
        identity.Spacing(8);
        identity.Children().Append(ui::Text(L"Studienstammdaten", 16, true));
        identity.Children().Append(ui::Text(L"Programm-Identität (Ordnername): " + study.studyName + L"   |   Datei: " + study.excelPath.filename().wstring(), 12, false));
        m_profileStudyNameInput = TextBox();
        m_profileStudyNameInput.Text(study.workbookStudyName);
        m_profileEuctInput = TextBox();
        m_profileEuctInput.Text(study.euctNumber);
        identity.Children().Append(ui::LabeledField(L"Studienbezeichnung in Excel", m_profileStudyNameInput));
        identity.Children().Append(ui::LabeledField(L"EUCT-No.", m_profileEuctInput));
        auto saveMeta = MakeButton(L"Stammdaten speichern");
        saveMeta.HorizontalAlignment(HorizontalAlignment::Left);
        saveMeta.Click([this](auto&&, auto&&)
        {
            try
            {
                if (auto s = CurrentStudy())
                {
                    m_repository.UpdateProfileMetadata(*s, m_profileStudyNameInput.Text().c_str(), m_profileEuctInput.Text().c_str());
                    ReloadCurrentStudyKeepingSelection();
                    ShowInfo(L"Die Studienstammdaten wurden in der Excel gespeichert.");
                }
            }
            catch (const std::exception& e) { ShowError(e); }
        });
        identity.Children().Append(saveMeta);
        panel.Children().Append(ui::Card(identity));

        StackPanel profile;
        profile.Spacing(8);
        profile.Children().Append(ui::Text(L"IMP-Definitionen aus dem ersten Tabellenblatt", 16, true));
        profile.Children().Append(ui::Text(L"Jede Zeile beschreibt eine Eigenschaft; IMP1, IMP2 usw. sind die verwalteten Produkte. Spalten können über das Symbol ▾ gefiltert und sortiert werden. Eine Zeile kann ausgewählt und darunter bearbeitet werden.", 12, false));
        const auto profileRows = DisplayRows(study.profileTable, 200);
        const auto profileKeys = DisplayRowKeys(study.profileTable, 200);
        const std::wstring profileKey = study.studyName + L"|profile";
        profile.Children().Append(BuildInteractiveTable(profileKey, study.profileTable.headers, profileRows, profileKeys, 200, true));

        const int selectedRow = m_tableStates[profileKey].selectedKey;
        if (selectedRow > 0)
        {
            size_t sourceIndex = study.profileTable.rows.size();
            for (size_t r = 0; r < study.profileTable.rows.size(); ++r)
            {
                const int excelRow = r < study.profileTable.rowNumbers.size() ? study.profileTable.rowNumbers[r] : study.profileTable.firstRow + 1 + static_cast<int>(r);
                if (excelRow == selectedRow) { sourceIndex = r; break; }
            }
            if (sourceIndex < study.profileTable.rows.size())
            {
                StackPanel editor;
                editor.Spacing(7);
                editor.Margin(Thickness{ 0,10,0,0 });
                editor.Children().Append(ui::Text(L"Ausgewählte Profilzeile bearbeiten – Excel-Zeile " + std::to_wstring(selectedRow), 14, true));
                m_profileRowInputs.clear();
                for (size_t c = 0; c < study.profileTable.headers.size(); ++c)
                {
                    const auto header = study.profileTable.headers[c].empty() ? L"Spalte " + std::to_wstring(c + 1) : study.profileTable.headers[c];
                    TextBox input;
                    if (c < study.profileTable.rows[sourceIndex].size())
                        input.Text(date::CellToDisplay(study.profileTable.rows[sourceIndex][c], date::HeaderLooksLikeDate(header)));
                    m_profileRowInputs[study.profileTable.headers[c]] = input;
                    editor.Children().Append(ui::LabeledField(header, input));
                }
                auto save = MakeButton(L"Profilzeile speichern");
                save.HorizontalAlignment(HorizontalAlignment::Left);
                save.Click([this, selectedRow](auto&&, auto&&)
                {
                    try
                    {
                        auto s = CurrentStudy(); if (!s) return;
                        std::map<std::wstring, std::wstring> values;
                        for (const auto& [header, input] : m_profileRowInputs) values[header] = input.Text().c_str();
                        m_repository.UpdateProfileRow(*s, selectedRow, values);
                        ReloadCurrentStudyKeepingSelection();
                        ShowInfo(L"Die ausgewählte Studienprofilzeile wurde gespeichert.");
                    }
                    catch (const std::exception& e) { ShowError(e); }
                });
                editor.Children().Append(save);
                profile.Children().Append(ui::Card(editor));
            }
        }
        panel.Children().Append(ui::Card(profile));

        StackPanel resolved;
        resolved.Spacing(8);
        resolved.Children().Append(ui::Text(L"Vom Programm aufgelöste IMP / Produkte", 16, true));
        resolved.Children().Append(ui::Text(L"Diese Ansicht zeigt die tatsächlich verwendeten Zuordnungen – einschließlich automatisch ermittelter Kurzbezeichnungen wie Gilteritinib → GILT oder VENAZA → VEN.", 12, false));
        std::vector<std::wstring> headers{ L"IMP-ID", L"IMP", L"Warenart", L"Form", L"Bestellpflicht", L"Minimum", L"Lieferzeit", L"Bestellspalte", L"Inventarfilter" };
        std::vector<std::vector<std::wstring>> rows;
        for (const auto& imp : study.imps)
        {
            rows.push_back({ imp.id, imp.name, imp.goodsType.empty() ? L"–" : imp.goodsType,
                imp.applicationForm.empty() ? L"–" : imp.applicationForm, imp.orderRequired ? L"Ja" : L"Nein",
                date::CellToDisplay(CellValue::Number(imp.minimumStock)), std::to_wstring(imp.leadTimeDays) + L" T",
                imp.orderQuantityHeader.empty() ? L"–" : imp.orderQuantityHeader,
                imp.inventoryImpHeader.empty() ? L"–" : imp.inventoryImpHeader + L" = " + (imp.inventoryImpValue.empty() ? L"?" : imp.inventoryImpValue) });
        }
        resolved.Children().Append(BuildInteractiveTable(study.studyName + L"|resolvedImps", headers, rows, {}, 100, false));
        panel.Children().Append(ui::Card(resolved));
        return panel;
    }

    FrameworkElement MainWindow::BuildStudyOverview(const StudyData& study)
    {
        StackPanel panel;
        panel.Spacing(12);
        panel.Margin(Thickness{ 0,12,0,0 });

        if (!study.loadWarning.empty())
        {
            Border warning;
            warning.Background(ui::StatusBrush(1));
            warning.CornerRadius(CornerRadius{ 6,6,6,6 });
            warning.Padding(Thickness{ 10,8,10,8 });
            warning.Child(ui::Text(study.loadWarning, 13, false));
            panel.Children().Append(warning);
        }

        StackPanel impCard;
        impCard.Spacing(8);
        impCard.Children().Append(ui::Text(L"Verwaltete IMP / Produkte – " + study.studyName, 17, true));
        impCard.Children().Append(ui::Text(L"Die Produkteigenschaften werden aus der Studien-Excel gelesen. Nur als bestellpflichtig gekennzeichnete Studienware fließt in die Bestellpriorisierung ein.", 12, false));
        std::vector<std::wstring> impHeaders{ L"IMP", L"Warenart", L"Applikationsform", L"Bestand", L"Minimum", L"Lieferzeit", L"Bestellfenster", L"Status" };
        std::vector<std::vector<std::wstring>> impRows;
        for (const auto& imp : study.imps)
        {
            impRows.push_back({ imp.name,
                imp.goodsType.empty() ? L"–" : imp.goodsType,
                imp.applicationForm.empty() ? L"–" : imp.applicationForm,
                date::CellToDisplay(CellValue::Number(imp.stock)),
                date::CellToDisplay(CellValue::Number(imp.minimumStock)),
                std::to_wstring(imp.leadTimeDays) + L" Tage",
                imp.orderRequired ? PlanWindow(imp.plan) : L"–",
                OrderPlanner::StateText(imp.plan.state) });
        }
        if (impRows.empty()) impRows.push_back({ L"Keine IMP definiert", L"–", L"–", L"–", L"–", L"–", L"–", L"Unklar" });
        impCard.Children().Append(BuildInteractiveTable(study.studyName + L"|studyOverviewImps", impHeaders, impRows, {}, 50, false));
        panel.Children().Append(ui::Card(impCard));

        StackPanel upcoming;
        upcoming.Spacing(7);
        upcoming.Children().Append(ui::Text(L"Nächste geplante Visiten", 17, true));
        auto sorted = study.visits;
        std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.date < b.date; });
        int shown = 0;
        for (const auto& visit : sorted)
        {
            if (!visit.hasDate || visit.date < date::Today())
                continue;

            // In der Übersicht werden bewusst nur die fachlich wichtigsten Angaben
            // gezeigt: Datum, Patient, Visitenbezeichnung und Visitenzusatz.
            auto getMetadataValue =
                [&visit](std::initializer_list<std::wstring> aliases) -> std::wstring
            {
                for (const auto& [metadataKey, value] : visit.metadata)
                {
                    const auto normalizedKey = date::Normalize(metadataKey);

                    for (const auto& alias : aliases)
                    {
                        if (normalizedKey == date::Normalize(alias))
                            return date::Trim(value);
                    }
                }
                return L"";
            };

            const std::wstring visitName =
                getMetadataValue({
                    L"Visiten-Bezeichnung",
                    L"Visitenbezeichnung",
                    L"Visite",
                    L"Visit"
                });

            const std::wstring visitExtra =
                getMetadataValue({
                    L"Visiten-Zusatz",
                    L"Visitenzusatz",
                    L"Zusatz",
                    L"ECP"
                });

            std::wstring line =
                date::FormatGermanDate(visit.date)
                + L"  ·  "
                + visit.patientId;

            if (!visitName.empty())
                line += L"  ·  " + visitName;

            if (!visitExtra.empty())
                line += L"  ·  " + visitExtra;

            upcoming.Children().Append(ui::Text(line, 13, false));

            if (++shown >= 8)
                break;
        }
        if (shown == 0) upcoming.Children().Append(ui::Text(L"Keine zukünftigen Visiten mit Datum hinterlegt.", 13, false));
        auto upcomingCard = ui::Card(upcoming);

        auto docsCard = BuildDocumentQuickLinks(study, L"Bestellung");
        panel.Children().Append(MakeResponsiveTwoCardGrid(upcomingCard, docsCard));

        auto forecastNote = ui::Text(L"Bestellfenster mit '(Prognose)' beruhen auf einem hinterlegten Prognoseintervall oder einem aus vorhandenen Terminen abgeleiteten typischen Abstand, wenn die terminierten Visiten noch nicht weit genug in die Zukunft reichen.", 12, false);
        forecastNote.Foreground(ui::Brush(95, 107, 125));
        panel.Children().Append(forecastNote);
        return panel;
    }

    FrameworkElement MainWindow::BuildDocumentQuickLinks(const StudyData& study, const std::wstring& categoryContains)
    {
        StackPanel panel;
        panel.Spacing(7);
        panel.Children().Append(ui::Text(L"Dokumente & Vorlagen für diesen Arbeitsschritt", 16, true));
        int count = 0;
        const auto needle = date::Normalize(categoryContains);
        for (const auto& doc : study.documents)
        {
            const auto hay = date::Normalize(doc.category + L" " + doc.title);
            if (!needle.empty() && hay.find(needle) == std::wstring::npos) continue;
            Grid row;
            ColumnDefinition c1; c1.Width(GridLength{ 1, GridUnitType::Star });
            ColumnDefinition c2; c2.Width(GridLength{ 0, GridUnitType::Auto });
            row.ColumnDefinitions().Append(c1); row.ColumnDefinitions().Append(c2);
            auto text = ui::Text(doc.category + L"  ·  " + doc.title, 13, false); text.VerticalAlignment(VerticalAlignment::Center);
            row.Children().Append(text);
            auto open = MakeButton(L"Öffnen");
            auto path = doc.path;
            open.Click([this, path](auto&&, auto&&) { OpenPath(path); });
            Grid::SetColumn(open, 1); row.Children().Append(open);
            panel.Children().Append(row);
            if (++count >= 8) break;
        }
        if (count == 0)
            panel.Children().Append(ui::Text(L"Noch keine passenden Dokumente hinterlegt. Dateien, E-Mail-Vorlagen oder .url-Links können im Studienordner unter Documents abgelegt werden.", 13, false));
        return ui::Card(panel);
    }

    FrameworkElement MainWindow::BuildOrders(const StudyData& study)
    {
        StackPanel panel;
        panel.Spacing(12); panel.Margin(Thickness{ 0,12,0,0 });
        panel.Children().Append(ui::Text(L"Bestellungen – " + study.studyName, 18, true));

        if (study.imps.empty())
        {
            panel.Children().Append(ui::Card(ui::Text(L"In dieser Studie ist noch kein IMP definiert.")));
            return panel;
        }

        m_orderImpCombo = ComboBox();
        for (const auto& item : study.imps)
        {
            std::wstring label = item.name;
            if (!item.orderRequired) label += L"  [keine Studienbestellung]";
            m_orderImpCombo.Items().Append(box_value(label));
        }
        m_orderImpCombo.SelectedIndex(static_cast<int>(std::min(m_selectedImp, study.imps.size() - 1)));
        m_orderImpCombo.SelectionChanged([this](auto&&, auto&&)
        {
            if (m_rendering || !m_orderImpCombo) return;
            const int index = m_orderImpCombo.SelectedIndex();
            if (index >= 0)
            {
                m_selectedImp = static_cast<size_t>(index);
                RenderOverview();
            }
        });
        panel.Children().Append(ui::LabeledField(L"IMP / Produkt", m_orderImpCombo));

        const auto* imp = SelectedImp(study);
        if (!imp) return panel;

        StackPanel meta;
        meta.Spacing(4);
        meta.Children().Append(ui::Text(L"Warenart: " + (imp->goodsType.empty() ? L"–" : imp->goodsType) +
            L"   |   Applikationsform: " + (imp->applicationForm.empty() ? L"–" : imp->applicationForm), 12, false));
        meta.Children().Append(ui::Text(L"Bestellblatt: " + (imp->orderSheet.empty() ? L"–" : imp->orderSheet) +
            L"   |   Bestellspalte: " + (imp->orderQuantityHeader.empty() ? L"automatisch" : imp->orderQuantityHeader), 11, false));
        if (!imp->orderProcess.empty()) meta.Children().Append(ui::Text(L"Bestellverfahren: " + imp->orderProcess, 12, false));
        panel.Children().Append(ui::Card(meta));

        if (!imp->orderRequired)
        {
            panel.Children().Append(ui::Card(ui::Text(L"Dieses Produkt ist in der Studien-Excel als nicht bestellpflichtig markiert. Es kann weiterhin bei Verabreichung/Bestand dokumentiert werden, erscheint aber nicht in der Bestellpriorisierung.")));
            return panel;
        }

        const std::wstring orderTableKey = study.studyName + L"|orders|" + imp->id;
        if (!imp->orders.headers.empty())
        {
            const auto orderRows = DisplayRows(imp->orders, 300);
            const auto orderKeys = DisplayRowKeys(imp->orders, 300);
            panel.Children().Append(BuildInteractiveTable(orderTableKey, imp->orders.headers, orderRows, orderKeys, 120, true));
        }
        else
        {
            panel.Children().Append(ui::Card(ui::Text(L"Das für dieses IMP konfigurierte Bestellblatt wurde nicht gefunden oder enthält keine erkennbare Kopfzeile.")));
        }

        // Die drei Arbeitsschritte werden responsiv angeordnet. Auf großen Fenstern
        // stehen sie nebeneinander, auf kleineren Displays werden sie in zwei bzw.
        // eine Spalte umgebrochen. Dadurch bleiben Felder und Schrift unverzerrt.

        // 1) Ausgewählte Bestellung bearbeiten
        StackPanel editor;
        editor.Spacing(6);
        editor.Children().Append(ui::Text(L"Ausgewählte Bestellung bearbeiten", 16, true));
        const int selectedRow = m_tableStates[orderTableKey].selectedKey;
        bool editorPopulated = false;
        if (selectedRow > 0 && !imp->orders.headers.empty())
        {
            size_t sourceIndex = imp->orders.rows.size();
            for (size_t r = 0; r < imp->orders.rows.size(); ++r)
            {
                const int excelRow = r < imp->orders.rowNumbers.size()
                    ? imp->orders.rowNumbers[r]
                    : imp->orders.firstRow + 1 + static_cast<int>(r);
                if (excelRow == selectedRow) { sourceIndex = r; break; }
            }

            if (sourceIndex < imp->orders.rows.size())
            {
                auto rowInfo = ui::Text(L"Ausgewählte Excel-Zeile " + std::to_wstring(selectedRow), 13, false);
                rowInfo.Foreground(ui::Brush(95, 107, 125));
                editor.Children().Append(rowInfo);

                m_orderEditInputs.clear();
                for (size_t c = 0; c < imp->orders.headers.size(); ++c)
                {
                    const auto& header = imp->orders.headers[c];
                    if (date::Trim(header).empty()) continue;
                    TextBox input;
                    if (c < imp->orders.rows[sourceIndex].size())
                        input.Text(date::CellToDisplay(imp->orders.rows[sourceIndex][c], date::HeaderLooksLikeDate(header)));
                    if (date::HeaderLooksLikeDate(header)) input.PlaceholderText(L"TT.MM.JJJJ");
                    m_orderEditInputs[header] = input;
                    editor.Children().Append(ui::LabeledField(header, input));
                }

                auto saveEdit = MakePrimaryButton(L"Änderungen speichern");
                saveEdit.HorizontalAlignment(HorizontalAlignment::Left);
                saveEdit.Click([this, selectedRow](auto&&, auto&&)
                {
                    try
                    {
                        auto s = CurrentStudy(); if (!s) return;
                        const auto* i = SelectedImp(*s); if (!i) return;
                        std::map<std::wstring, std::wstring> values;
                        for (const auto& [header, input] : m_orderEditInputs) values[header] = input.Text().c_str();
                        m_repository.UpdateOrderRow(*s, *i, selectedRow, values);
                        ReloadCurrentStudyKeepingSelection();
                        ShowInfo(L"Die ausgewählte Bestellzeile wurde aktualisiert.");
                    }
                    catch (const std::exception& e) { ShowError(e); }
                });
                editor.Children().Append(saveEdit);
                editorPopulated = true;
            }
        }
        if (!editorPopulated)
        {
            editor.Children().Append(ui::Text(
                L"Setze in der Bestelltabelle ein Häkchen bei der Bestellung, die du bearbeiten möchtest.",
                12, false));
        }
        auto editorCard = ui::Card(editor);

        // 2) Neue Bestellung erfassen
        StackPanel form;
        form.Spacing(7);
        form.Children().Append(ui::Text(L"Neue Bestellung erfassen", 16, true));
        if (!imp->orders.headers.empty())
        {
            m_orderInputs.clear();
            for (size_t c = 0; c < imp->orders.headers.size(); ++c)
            {
                const auto& header = imp->orders.headers[c];
                if (date::Trim(header).empty()) continue;
                const auto n = date::Normalize(header);
                if (n.find(L"bestellung") != std::wstring::npos || n == L"order" || n.find(L"bestellnummer") != std::wstring::npos ||
                    n.find(L"erhalten") != std::wstring::npos || n.find(L"received") != std::wstring::npos || n.find(L"wareneingang") != std::wstring::npos) continue;
                TextBox input;
                if (date::HeaderLooksLikeDate(header)) input.Text(date::FormatGermanDate(date::Today()));
                input.PlaceholderText(header);
                m_orderInputs[header] = input;
                form.Children().Append(ui::LabeledField(header, input));
            }
            auto save = MakePrimaryButton(L"Bestellung in Excel erfassen");
            save.HorizontalAlignment(HorizontalAlignment::Left);
            save.Click([this](auto&&, auto&&)
            {
                try
                {
                    auto studyNow = CurrentStudy(); if (!studyNow) return;
                    const auto* impNow = SelectedImp(*studyNow); if (!impNow) return;
                    std::map<std::wstring, std::wstring> values;
                    for (const auto& [header, input] : m_orderInputs) values[header] = input.Text().c_str();
                    m_repository.AppendOrder(*studyNow, *impNow, values);
                    ReloadCurrentStudyKeepingSelection();
                    ShowInfo(L"Die Bestellung wurde in der Studien-Excel erfasst.");
                }
                catch (const std::exception& e) { ShowError(e); }
            });
            form.Children().Append(save);
        }
        else
        {
            form.Children().Append(ui::Text(L"Für dieses IMP stehen aktuell keine Bestellspalten zur Eingabe zur Verfügung.", 13, false));
        }
        auto formCard = ui::Card(form);

        // 3) Dokumente & Vorlagen
        auto docsCard = BuildDocumentQuickLinks(study, imp->documentCategory.empty() ? L"Bestellung" : imp->documentCategory);

        panel.Children().Append(MakeResponsiveThreeCardGrid(editorCard, formCard, docsCard));
        return panel;
    }


    int MainWindow::HeaderLike(const SheetTable& table, const std::vector<std::wstring>& candidates)
    {
        for (size_t i = 0; i < table.headers.size(); ++i)
        {
            auto h = date::Normalize(table.headers[i]);
            for (const auto& candidate : candidates)
            {
                auto c = date::Normalize(candidate);
                if (h == c || h.find(c) != std::wstring::npos) return static_cast<int>(i);
            }
        }
        return -1;
    }

    FrameworkElement MainWindow::BuildInventory(const StudyData& study)
    {
        StackPanel panel;
        panel.Spacing(12); panel.Margin(Thickness{ 0,12,0,0 });
        panel.Children().Append(ui::Text(L"Wareneingang / Bestand – " + study.studyName, 18, true));

        if (study.imps.empty())
        {
            panel.Children().Append(ui::Card(ui::Text(L"In dieser Studie ist noch kein IMP definiert.")));
            return panel;
        }

        m_inventoryImpCombo = ComboBox();
        for (const auto& item : study.imps) m_inventoryImpCombo.Items().Append(box_value(item.name));
        m_inventoryImpCombo.SelectedIndex(static_cast<int>(std::min(m_selectedImp, study.imps.size() - 1)));
        m_inventoryImpCombo.SelectionChanged([this](auto&&, auto&&)
        {
            if (m_rendering || !m_inventoryImpCombo) return;
            const int index = m_inventoryImpCombo.SelectedIndex();
            if (index >= 0)
            {
                m_selectedImp = static_cast<size_t>(index);
                RenderOverview();
            }
        });
        panel.Children().Append(ui::LabeledField(L"IMP / Produkt", m_inventoryImpCombo));

        const auto* imp = SelectedImp(study);
        if (!imp) return panel;

        StackPanel status;
        status.Spacing(4);
        status.Children().Append(ui::Text(L"Aktueller Bestand: " + date::CellToDisplay(CellValue::Number(imp->stock)) +
            L"   |   Warenart: " + (imp->goodsType.empty() ? L"–" : imp->goodsType) +
            L"   |   Applikationsform: " + (imp->applicationForm.empty() ? L"–" : imp->applicationForm), 12, false));
        status.Children().Append(ui::Text(L"Inventarblatt: " + (imp->inventorySheet.empty() ? L"–" : imp->inventorySheet), 12, false));
        panel.Children().Append(ui::Card(status));

        const std::wstring inventoryTableKey = study.studyName + L"|inventory|" + imp->id;
        if (imp->inventory.headers.empty())
        {
            panel.Children().Append(ui::Card(ui::Text(L"Für dieses IMP wurde kein lesbares Inventarblatt gefunden.")));
            return panel;
        }

        const auto inventoryRows = DisplayRows(imp->inventory, 500);
        const auto inventoryKeys = DisplayRowKeys(imp->inventory, 500);
        panel.Children().Append(BuildInteractiveTable(inventoryTableKey, imp->inventory.headers, inventoryRows, inventoryKeys, 140, true));

        // Die drei Arbeitskarten werden weiter unten responsiv zusammengesetzt.
        // So bleibt die Eingabe auf kleineren Displays lesbar, statt drei schmale
        // Spalten zwanghaft nebeneinander zu pressen.

        // 1) Wareneingang erfassen
        const bool batchCapable =
            HeaderLike(imp->inventory, { L"DeliveryDate", L"Lieferdatum", L"Datum Erhalt" }) >= 0 &&
            HeaderLike(imp->inventory, { L"Charge", L"Batch", L"Lot" }) >= 0 &&
            HeaderLike(imp->inventory, { L"Vial", L"Kit", L"Box-Nr", L"Box Nr", L"Box No", L"Pack", L"Serial" }) >= 0 &&
            HeaderLike(imp->inventory, { L"Expiry", L"Verfall" }) >= 0;

        StackPanel batch;
        batch.Spacing(7);
        batch.Children().Append(ui::Text(L"Wareneingang erfassen", 16, true));

        if (batchCapable)
        {
            m_batchDelivery = TextBox();
            m_batchDelivery.Text(date::FormatGermanDate(date::Today()));
            m_batchDelivery.PlaceholderText(L"TT.MM.JJJJ");

            m_batchCharge = TextBox();
            m_batchCharge.PlaceholderText(L"z. B. 230017");

            m_batchExpiry = TextBox();
            m_batchExpiry.PlaceholderText(L"TT.MM.JJJJ");

            m_batchCount = TextBox();
            m_batchCount.Text(L"1");
            m_batchCount.PlaceholderText(L"Anzahl Packungen / Kits");

            m_batchIdentifiers = TextBox();
            m_batchIdentifiers.AcceptsReturn(true);
            m_batchIdentifiers.TextWrapping(TextWrapping::Wrap);
            m_batchIdentifiers.MinHeight(120);
            m_batchIdentifiers.PlaceholderText(
                L"Eine Box-/Kit-Nr. pro Zeile, z. B.\n100094\nAB-24/771\nKIT X-009");

            m_batchSameIdentifier = CheckBox();
            m_batchSameIdentifier.Content(box_value(L"Gleiche Box-/Kit-Nr. für alle Einheiten"));

            batch.Children().Append(ui::LabeledField(L"Lieferdatum", m_batchDelivery));
            batch.Children().Append(ui::LabeledField(L"Charge No.", m_batchCharge));
            batch.Children().Append(ui::LabeledField(L"Verfallsdatum", m_batchExpiry));
            batch.Children().Append(ui::LabeledField(L"Anzahl Einheiten", m_batchCount));
            batch.Children().Append(ui::LabeledField(L"Box-/Kit-Nummern", m_batchIdentifiers));

            auto identifierHint = ui::Text(
                L"Box-/Kit-Nummern werden unverändert als Text gespeichert. Buchstaben, Zahlen, Bindestriche und andere Zeichen sind zulässig; eine automatische Nummerierung findet nicht statt.",
                12, false);
            identifierHint.Foreground(ui::Brush(95, 107, 125));
            batch.Children().Append(identifierHint);
            batch.Children().Append(m_batchSameIdentifier);

            // Optional eine konkrete offene Bestellung mit diesem Wareneingang verknüpfen.
            // Fachliche Regel: "offen" bedeutet ausschließlich, dass die Zelle in der
            // Spalte "Erhalten am" leer ist. Ein separater Status wird nicht benötigt.
            m_batchOrderCombo = ComboBox();
            m_batchOpenOrderRows.clear();
            m_batchOrderCombo.Items().Append(box_value(L"Keine Zuordnung"));
            m_batchOpenOrderRows.push_back(-1);

            if (!imp->orders.headers.empty())
            {
                int receivedCol = -1;
                const auto exactReceived = date::Normalize(L"Erhalten am");
                for (size_t c = 0; c < imp->orders.headers.size(); ++c)
                {
                    if (date::Normalize(imp->orders.headers[c]) == exactReceived)
                    {
                        receivedCol = static_cast<int>(c);
                        break;
                    }
                }
                if (receivedCol < 0)
                {
                    receivedCol = HeaderLike(imp->orders,
                        { L"Eingang am", L"Date received", L"Received date", L"Received on", L"Wareneingang" });
                }

                const int orderCol = HeaderLike(imp->orders, { L"Bestellung", L"Order", L"Bestellnummer" });
                int orderDateCol = -1;
                for (size_t c = 0; c < imp->orders.headers.size(); ++c)
                {
                    const auto h = date::Normalize(imp->orders.headers[c]);
                    if (h == date::Normalize(L"Am") || h == date::Normalize(L"Bestellt am") ||
                        h == date::Normalize(L"Bestelldatum") || h == date::Normalize(L"Order date"))
                    {
                        orderDateCol = static_cast<int>(c);
                        break;
                    }
                }

                int quantityCol = -1;
                if (!date::Trim(imp->orderQuantityHeader).empty())
                {
                    const auto wanted = date::Normalize(imp->orderQuantityHeader);
                    for (size_t c = 0; c < imp->orders.headers.size(); ++c)
                    {
                        if (date::Normalize(imp->orders.headers[c]) == wanted)
                        {
                            quantityCol = static_cast<int>(c);
                            break;
                        }
                    }
                }
                if (quantityCol < 0) quantityCol = HeaderLike(imp->orders, { imp->name });

                if (receivedCol >= 0)
                {
                    for (size_t r = 0; r < imp->orders.rows.size(); ++r)
                    {
                        const auto& row = imp->orders.rows[r];

                        // Nur der Inhalt von "Erhalten am" entscheidet über offen/geschlossen.
                        // Auch Text mit Leerzeichen bzw. eine Formel mit leerem Ergebnis gilt als leer.
                        std::wstring receivedText;
                        if (static_cast<size_t>(receivedCol) < row.size())
                            receivedText = date::Trim(date::CellToDisplay(row[static_cast<size_t>(receivedCol)], true));
                        if (!receivedText.empty()) continue;

                        // Keine zusätzliche Filterung nach Menge: Jede Bestellung mit leerem
                        // "Erhalten am" wird angeboten. Bei Multi-IMP-Tabellen wird die Menge
                        // des aktuell gewählten IMP lediglich informativ im Label angezeigt.
                        const int excelRow = r < imp->orders.rowNumbers.size()
                            ? imp->orders.rowNumbers[r]
                            : imp->orders.firstRow + 1 + static_cast<int>(r);

                        std::wstring label = L"Excel-Zeile " + std::to_wstring(excelRow);
                        if (orderCol >= 0 && static_cast<size_t>(orderCol) < row.size() &&
                            !row[static_cast<size_t>(orderCol)].IsEmpty())
                        {
                            label = date::CellToDisplay(row[static_cast<size_t>(orderCol)]) + L"  |  " + label;
                        }
                        if (orderDateCol >= 0 && static_cast<size_t>(orderDateCol) < row.size() &&
                            !row[static_cast<size_t>(orderDateCol)].IsEmpty())
                        {
                            label += L"  |  bestellt: " +
                                date::CellToDisplay(row[static_cast<size_t>(orderDateCol)], true);
                        }
                        if (quantityCol >= 0)
                        {
                            std::wstring quantity = L"–";
                            if (static_cast<size_t>(quantityCol) < row.size() &&
                                !row[static_cast<size_t>(quantityCol)].IsEmpty())
                                quantity = date::CellToDisplay(row[static_cast<size_t>(quantityCol)]);
                            label += L"  |  " + imp->name + L": " + quantity;
                        }

                        m_batchOrderCombo.Items().Append(box_value(label));
                        m_batchOpenOrderRows.push_back(excelRow);
                    }
                }
                else
                {
                    m_batchOrderCombo.Items().Append(box_value(L"Keine Spalte 'Erhalten am' erkannt"));
                    m_batchOpenOrderRows.push_back(-1);
                }
            }

            m_batchOrderCombo.SelectedIndex(0);
            batch.Children().Append(ui::LabeledField(L"Zugehörige offene Bestellung (optional)", m_batchOrderCombo));

            auto save = MakePrimaryButton(L"Wareneingang speichern");
            save.HorizontalAlignment(HorizontalAlignment::Left);
            save.Click([this](auto&&, auto&&)
            {
                try
                {
                    auto s = CurrentStudy();
                    if (!s) return;
                    const auto* impNow = SelectedImp(*s);
                    if (!impNow) return;

                    int count = 0;
                    try
                    {
                        count = std::stoi(std::wstring(m_batchCount.Text().c_str()));
                    }
                    catch (...)
                    {
                        throw std::runtime_error("Bitte bei 'Anzahl Einheiten' eine ganze Zahl eingeben.");
                    }
                    if (count <= 0 || count > 500)
                        throw std::runtime_error("Bitte eine Anzahl zwischen 1 und 500 angeben.");

                    auto identifiers = SplitNonEmptyLines(m_batchIdentifiers.Text().c_str());
                    bool sameIdentifier = false;
                    if (auto checked = m_batchSameIdentifier.IsChecked())
                        sameIdentifier = checked.Value();

                    if (sameIdentifier)
                    {
                        if (identifiers.empty())
                            throw std::runtime_error("Bitte eine Box-/Kit-Nummer eingeben.");

                        // Wichtig: Der Wert muss vor assign() aus dem Vector kopiert werden.
                        // identifiers.front() direkt an assign() zu uebergeben ist ungueltig,
                        // weil assign() den selben Container veraendert und die Referenz dadurch
                        // invalidiert werden kann (MSVC Debug Assertion: value cannot be a
                        // reference into the container).
                        const std::wstring sharedIdentifier = identifiers.front();
                        identifiers.assign(static_cast<size_t>(count), sharedIdentifier);
                    }
                    else if (identifiers.size() != static_cast<size_t>(count))
                    {
                        throw std::runtime_error(
                            "Die Anzahl der eingegebenen Box-/Kit-Nummern (" + std::to_string(identifiers.size()) +
                            ") muss der Anzahl Einheiten (" + std::to_string(count) +
                            ") entsprechen. Bitte eine Nummer pro Zeile eingeben oder 'Gleiche Box-/Kit-Nr. für alle Einheiten' aktivieren.");
                    }

                    int receivedOrderRow = -1;
                    if (m_batchOrderCombo)
                    {
                        const int selection = m_batchOrderCombo.SelectedIndex();
                        if (selection >= 0 && static_cast<size_t>(selection) < m_batchOpenOrderRows.size())
                            receivedOrderRow = m_batchOpenOrderRows[static_cast<size_t>(selection)];
                    }

                    m_repository.AppendInventoryBatch(
                        *s,
                        *impNow,
                        m_batchDelivery.Text().c_str(),
                        m_batchCharge.Text().c_str(),
                        m_batchExpiry.Text().c_str(),
                        identifiers,
                        receivedOrderRow);

                    ReloadCurrentStudyKeepingSelection();
                    ShowInfo(L"Der Wareneingang wurde gespeichert.");
                }
                catch (const std::exception& e)
                {
                    ShowError(e);
                }
            });
            batch.Children().Append(save);
        }
        else
        {
            batch.Children().Append(ui::Text(
                L"Für die Wareneingangserfassung benötigt das Inventar Spalten für Lieferdatum, Charge, Box-/Kit-Nr. und Verfallsdatum. Bitte die Spaltenbezeichnungen in der Studien-Excel prüfen.",
                12, false));
        }

        auto batchCard = ui::Card(batch);

        // 2) ausgewählten Inventardatensatz bearbeiten
        StackPanel editor;
        editor.Spacing(7);
        const int selectedRow = m_tableStates[inventoryTableKey].selectedKey;
        if (selectedRow > 0)
        {
            size_t sourceIndex = imp->inventory.rows.size();
            for (size_t r = 0; r < imp->inventory.rows.size(); ++r)
            {
                const int excelRow = r < imp->inventory.rowNumbers.size()
                    ? imp->inventory.rowNumbers[r]
                    : imp->inventory.firstRow + 1 + static_cast<int>(r);
                if (excelRow == selectedRow)
                {
                    sourceIndex = r;
                    break;
                }
            }

            if (sourceIndex < imp->inventory.rows.size())
            {
                editor.Children().Append(ui::Text(L"Inventardatensatz bearbeiten", 16, true));
                auto rowInfo = ui::Text(L"Ausgewählte Excel-Zeile " + std::to_wstring(selectedRow), 13, false);
                rowInfo.Foreground(ui::Brush(95, 107, 125));
                editor.Children().Append(rowInfo);

                m_inventoryEditInputs.clear();
                for (size_t c = 0; c < imp->inventory.headers.size(); ++c)
                {
                    const auto& header = imp->inventory.headers[c];
                    if (date::Trim(header).empty()) continue;

                    TextBox input;
                    if (c < imp->inventory.rows[sourceIndex].size())
                        input.Text(date::CellToDisplay(
                            imp->inventory.rows[sourceIndex][c],
                            date::HeaderLooksLikeDate(header)));

                    if (date::HeaderLooksLikeDate(header))
                        input.PlaceholderText(L"TT.MM.JJJJ");

                    m_inventoryEditInputs[header] = input;
                    editor.Children().Append(ui::LabeledField(header, input));
                }

                auto saveEdit = MakePrimaryButton(L"Änderungen speichern");
                saveEdit.HorizontalAlignment(HorizontalAlignment::Left);
                saveEdit.Click([this, selectedRow](auto&&, auto&&)
                {
                    try
                    {
                        auto s = CurrentStudy();
                        if (!s) return;
                        const auto* i = SelectedImp(*s);
                        if (!i) return;

                        std::map<std::wstring, std::wstring> values;
                        for (const auto& [header, input] : m_inventoryEditInputs)
                            values[header] = input.Text().c_str();

                        m_repository.UpdateInventoryRow(*s, *i, selectedRow, values);
                        ReloadCurrentStudyKeepingSelection();
                        ShowInfo(L"Der ausgewählte Inventardatensatz wurde aktualisiert.");
                    }
                    catch (const std::exception& e)
                    {
                        ShowError(e);
                    }
                });
                editor.Children().Append(saveEdit);
            }
        }

        if (editor.Children().Size() == 0)
        {
            editor.Children().Append(ui::Text(L"Inventardatensatz bearbeiten", 16, true));
            editor.Children().Append(ui::Text(
                L"Setze in der Bestandstabelle ein Häkchen bei der betreffenden Zeile. Hier kannst du anschließend z. B. Dispensing Date, Patient, Rückgabe, Restmenge oder Vernichtung dokumentieren.",
                12, false));
        }

        auto editorCard = ui::Card(editor);

        // 3) Dokumente & Vorlagen direkt im Arbeitsbereich anzeigen.
        auto inventoryDocs = BuildDocumentQuickLinks(study, L"Wareneingang");

        panel.Children().Append(MakeResponsiveThreeCardGrid(batchCard, editorCard, inventoryDocs));
        return panel;
    }

    std::wstring MainWindow::JoinVisitMetadata(const VisitRowDefinition& row)
    {
        std::wstring result;
        for (const auto& [key, value] : row.metadata)
        {
            if (!result.empty()) result += L" | ";
            result += key + L": " + value;
        }
        return result;
    }

    FrameworkElement MainWindow::BuildVisits(const StudyData& study)
    {
        StackPanel panel;
        panel.Spacing(12); panel.Margin(Thickness{ 0,12,0,0 });
        panel.Children().Append(ui::Text(L"Patientenvisiten – " + study.studyName, 18, true));
        if (study.visitTable.headers.empty())
        {
            panel.Children().Append(ui::Card(ui::Text(L"Das konfigurierte PatientenVisiten-Blatt wurde nicht gefunden.")));
            return panel;
        }

        std::vector<std::wstring> headers = study.visitMetadataHeaders;
        headers.push_back(L"Patient"); headers.push_back(L"Datum");
        std::vector<std::vector<std::wstring>> rows;
        std::vector<int> visitKeys;
        for (size_t index = 0; index < study.visits.size(); ++index)
        {
            const auto& v = study.visits[index];
            std::vector<std::wstring> row;
            for (const auto& [_, value] : v.metadata) row.push_back(value);
            row.push_back(v.patientId);
            row.push_back(v.hasDate ? date::FormatGermanDate(v.date) : L"");
            rows.push_back(std::move(row));
            visitKeys.push_back(static_cast<int>(index));
        }
        const std::wstring visitTableKey = study.studyName + L"|visits";
        panel.Children().Append(BuildInteractiveTable(visitTableKey, headers, rows, visitKeys, 180, true));

        // Auch die drei Visiten-Arbeitsbereiche werden responsiv angeordnet.

        // 1) Visite terminieren / bearbeiten
        StackPanel edit; edit.Spacing(6);
        edit.Children().Append(ui::Text(L"Visite terminieren / bearbeiten", 16, true));
        m_visitPatientCombo = ComboBox();
        for (const auto& patient : study.patientIds) m_visitPatientCombo.Items().Append(box_value(patient));
        if (!study.patientIds.empty()) m_visitPatientCombo.SelectedIndex(0);
        m_visitRowCombo = ComboBox();
        for (const auto& row : study.visitRows) m_visitRowCombo.Items().Append(box_value(JoinVisitMetadata(row)));
        if (!study.visitRows.empty()) m_visitRowCombo.SelectedIndex(0);

        const int selectedVisit = m_tableStates[visitTableKey].selectedKey;
        if (selectedVisit >= 0 && static_cast<size_t>(selectedVisit) < study.visits.size())
        {
            const auto& selected = study.visits[static_cast<size_t>(selectedVisit)];
            for (size_t pIndex = 0; pIndex < study.patientIds.size(); ++pIndex)
                if (date::Normalize(study.patientIds[pIndex]) == date::Normalize(selected.patientId)) { m_visitPatientCombo.SelectedIndex(static_cast<int>(pIndex)); break; }
            for (size_t r = 0; r < study.visitRows.size(); ++r)
                if (study.visitRows[r].excelRow == selected.excelRow) { m_visitRowCombo.SelectedIndex(static_cast<int>(r)); break; }
        }

        m_visitDateInput = TextBox(); m_visitDateInput.PlaceholderText(L"TT.MM.JJJJ");
        m_visitPatientCombo.SelectionChanged([this](auto&&, auto&&) { RefreshVisitDateInput(); });
        m_visitRowCombo.SelectionChanged([this](auto&&, auto&&) { RefreshVisitDateInput(); });
        edit.Children().Append(ui::LabeledField(L"Patient", m_visitPatientCombo));
        edit.Children().Append(ui::LabeledField(L"Visite / Merkmale", m_visitRowCombo));
        edit.Children().Append(ui::LabeledField(L"Datum", m_visitDateInput));
        auto saveDate = MakePrimaryButton(L"Visite speichern");
        saveDate.Click([this](auto&&, auto&&)
        {
            try
            {
                auto s = CurrentStudy(); if (!s) return;
                const int pIndex = m_visitPatientCombo.SelectedIndex();
                const int r = m_visitRowCombo.SelectedIndex();
                if (pIndex < 0 || r < 0 || static_cast<size_t>(pIndex) >= s->patientIds.size() || static_cast<size_t>(r) >= s->visitRows.size())
                    throw std::runtime_error("Bitte Patient und Visite auswählen.");
                const auto patient = s->patientIds[static_cast<size_t>(pIndex)];
                int headerCol = -1;
                for (size_t c = 0; c < s->visitTable.headers.size(); ++c)
                    if (date::Normalize(s->visitTable.headers[c]) == date::Normalize(patient)) { headerCol = static_cast<int>(c); break; }
                if (headerCol < 0) throw std::runtime_error("Patientenspalte konnte nicht gefunden werden.");
                m_repository.SetVisitDate(*s, s->visitRows[static_cast<size_t>(r)].excelRow, s->visitTable.firstColumn + headerCol, m_visitDateInput.Text().c_str());
                ReloadCurrentStudyKeepingSelection();
            }
            catch (const std::exception& e) { ShowError(e); }
        });
        edit.Children().Append(saveDate);
        auto editCard = ui::Card(edit);
        RefreshVisitDateInput();

        // 2) Patient hinzufügen
        StackPanel patientForm; patientForm.Spacing(6);
        patientForm.Children().Append(ui::Text(L"Patient hinzufügen", 16, true));
        m_newPatientInput = TextBox(); m_newPatientInput.PlaceholderText(L"z. B. 02-04");
        patientForm.Children().Append(ui::LabeledField(L"Patienten-ID", m_newPatientInput));
        auto addPatient = MakePrimaryButton(L"Patient hinzufügen");
        addPatient.Click([this](auto&&, auto&&)
        {
            try { if (auto s = CurrentStudy()) { m_repository.AddPatient(*s, m_newPatientInput.Text().c_str()); ReloadCurrentStudyKeepingSelection(); } }
            catch (const std::exception& e) { ShowError(e); }
        });
        patientForm.Children().Append(addPatient);
        auto patientCard = ui::Card(patientForm);

        // 3) Visitenzeile hinzufügen
        StackPanel visitForm; visitForm.Spacing(6);
        visitForm.Children().Append(ui::Text(L"Neue Visitenzeile hinzufügen", 16, true));
        m_visitMetaInputs.clear();
        for (const auto& header : study.visitMetadataHeaders)
        {
            TextBox input;
            m_visitMetaInputs[header] = input;
            visitForm.Children().Append(ui::LabeledField(header, input));
        }
        auto addVisit = MakePrimaryButton(L"Visitenzeile hinzufügen");
        addVisit.Click([this](auto&&, auto&&)
        {
            try
            {
                auto s = CurrentStudy(); if (!s) return;
                std::map<std::wstring, std::wstring> values;
                for (const auto& [header, input] : m_visitMetaInputs) values[header] = input.Text().c_str();
                m_repository.AddVisitRow(*s, values);
                ReloadCurrentStudyKeepingSelection();
            }
            catch (const std::exception& e) { ShowError(e); }
        });
        visitForm.Children().Append(addVisit);
        auto visitCard = ui::Card(visitForm);

        panel.Children().Append(MakeResponsiveThreeCardGrid(editCard, patientCard, visitCard));
        return panel;
    }

    void MainWindow::RefreshVisitDateInput()
    {
        if (!m_visitPatientCombo || !m_visitRowCombo || !m_visitDateInput) return;
        const auto* s = CurrentStudy(); if (!s) return;
        const int p = m_visitPatientCombo.SelectedIndex();
        const int r = m_visitRowCombo.SelectedIndex();
        if (p < 0 || r < 0 || static_cast<size_t>(p) >= s->patientIds.size() || static_cast<size_t>(r) >= s->visitRows.size()) return;
        const auto& patient = s->patientIds[static_cast<size_t>(p)];
        const int row = s->visitRows[static_cast<size_t>(r)].excelRow;
        for (const auto& visit : s->visits)
        {
            if (visit.excelRow == row && date::Normalize(visit.patientId) == date::Normalize(patient) && visit.hasDate)
            {
                m_visitDateInput.Text(date::FormatGermanDate(visit.date));
                return;
            }
        }
        m_visitDateInput.Text(L"");
    }

    FrameworkElement MainWindow::BuildReports(const StudyData& study)
    {
        StackPanel panel; panel.Spacing(12); panel.Margin(Thickness{ 0,12,0,0 });
        panel.Children().Append(ui::Text(L"Berichte – " + study.studyName, 18, true));

        if (study.imps.empty())
        {
            panel.Children().Append(ui::Card(ui::Text(L"In dieser Studie ist noch kein IMP definiert.")));
            return panel;
        }

        m_reportImpCombo = ComboBox();
        for (const auto& item : study.imps) m_reportImpCombo.Items().Append(box_value(item.name));
        m_reportImpCombo.SelectedIndex(static_cast<int>(std::min(m_selectedImp, study.imps.size() - 1)));
        m_reportImpCombo.SelectionChanged([this](auto&&, auto&&)
        {
            if (m_rendering || !m_reportImpCombo) return;
            const int index = m_reportImpCombo.SelectedIndex();
            if (index >= 0)
            {
                m_selectedImp = static_cast<size_t>(index);
                RenderOverview();
            }
        });
        panel.Children().Append(ui::LabeledField(L"IMP / Produkt", m_reportImpCombo));

        const auto* imp = SelectedImp(study);
        if (!imp) return panel;

        StackPanel card; card.Spacing(10);
        card.Children().Append(ui::Text(L"Drug Accountability – " + imp->name, 16, true));
        card.Children().Append(ui::Text(
            L"Excel-Berichte werden jeweils neu aus dem aktuellen DrugInventory erzeugt.", 12, false));

        auto exportInfo = ui::Text(L"Exportziel: " + ReportRoot(study).wstring(), 12, false);
        exportInfo.Foreground(ui::Brush(95, 107, 125));
        card.Children().Append(exportInfo);

        // -------- Gesamtbericht --------
        StackPanel overallBox; overallBox.Spacing(7);
        overallBox.Children().Append(ui::Text(L"DrugAccount pro IMP – Gesamt", 14, true));
        auto overallTemplateInfo = ui::Text(L"Vorlage: " + DefaultDrugAccountOverallTemplate().wstring(), 10, false);
        overallTemplateInfo.Foreground(ui::Brush(95, 107, 125));
        overallBox.Children().Append(overallTemplateInfo);

        auto total = MakeButton(L"DrugAccount gesamt erstellen (Excel)");
        total.Click([this](auto&&, auto&&)
        {
            try
            {
                auto current = CurrentStudy(); if (!current) return;
                const auto* selected = SelectedImp(*current); if (!selected) return;
                const auto impId = selected->id;
                auto fresh = m_repository.LoadOne(current->excelPath);
                const ImpData* freshImp = nullptr;
                for (const auto& candidate : fresh.imps) if (candidate.id == impId) { freshImp = &candidate; break; }
                if (!freshImp) throw std::runtime_error("Das ausgewählte IMP wurde nach dem Neueinlesen nicht mehr gefunden.");

                const auto files = ReportService::ExportDrugAccountOverall(
                    fresh, *freshImp, ReportRoot(fresh), DefaultDrugAccountOverallTemplate());
                ShowInfo(L"DrugAccount-Gesamtbericht wurde mit " + std::to_wstring(files.dataRows) +
                    L" Datensätzen erstellt.\n\nExcel: " + files.excelPath.wstring());
                OpenPath(files.excelPath);
            }
            catch (const std::exception& e) { ShowError(e); }
        });
        overallBox.Children().Append(total);
        card.Children().Append(ui::Card(overallBox, 12));

        // -------- Patientenbericht --------
        StackPanel patientBox; patientBox.Spacing(7);
        patientBox.Children().Append(ui::Text(L"DrugAccount pro Patient", 14, true));

        m_reportPatientCombo = ComboBox();
        std::vector<std::wstring> reportPatients;
        const int reportPatientCol = HeaderLike(imp->inventory, {
            L"Pat.ID", L"Pat ID", L"Pat.-ID", L"PatID", L"PatientID", L"Patient ID",
            L"Patient", L"Patienten-ID", L"Patienten ID", L"Patientennummer", L"Patienten-Nr.", L"Pat.Nr." });
        if (reportPatientCol >= 0)
        {
            for (const auto& row : imp->inventory.rows)
            {
                if (static_cast<size_t>(reportPatientCol) >= row.size()) continue;
                const auto patientId = date::Trim(date::CellToDisplay(row[static_cast<size_t>(reportPatientCol)]));
                if (patientId.empty()) continue;
                if (std::none_of(reportPatients.begin(), reportPatients.end(), [&](const auto& p) { return date::Normalize(p) == date::Normalize(patientId); }))
                    reportPatients.push_back(patientId);
            }
        }
        if (reportPatients.empty()) reportPatients = study.patientIds;
        for (const auto& p : reportPatients) m_reportPatientCombo.Items().Append(box_value(p));
        if (!reportPatients.empty()) m_reportPatientCombo.SelectedIndex(0);
        patientBox.Children().Append(ui::LabeledField(L"Patient", m_reportPatientCombo));

        auto patientTemplateInfo = ui::Text(L"Vorlage: " + DefaultDrugAccountPatientTemplate().wstring(), 10, false);
        patientTemplateInfo.Foreground(ui::Brush(95, 107, 125));
        patientBox.Children().Append(patientTemplateInfo);

        auto patient = MakeButton(L"DrugAccount pro Patient erstellen (Excel)");
        patient.Click([this](auto&&, auto&&)
        {
            try
            {
                auto current = CurrentStudy(); if (!current) return;
                const auto* selected = SelectedImp(*current); if (!selected) return;
                const int index = m_reportPatientCombo.SelectedIndex();
                if (index < 0) throw std::runtime_error("Bitte einen Patienten auswählen.");
                const std::wstring patientId = unbox_value<hstring>(m_reportPatientCombo.SelectedItem()).c_str();
                const auto impId = selected->id;
                auto fresh = m_repository.LoadOne(current->excelPath);
                const ImpData* freshImp = nullptr;
                for (const auto& candidate : fresh.imps) if (candidate.id == impId) { freshImp = &candidate; break; }
                if (!freshImp) throw std::runtime_error("Das ausgewählte IMP wurde nach dem Neueinlesen nicht mehr gefunden.");

                const auto files = ReportService::ExportDrugAccountPatient(
                    fresh, *freshImp, patientId, ReportRoot(fresh), DefaultDrugAccountPatientTemplate());
                ShowInfo(L"DrugAccount für den Patienten wurde mit " + std::to_wstring(files.dataRows) +
                    L" Datensätzen erstellt.\n\nExcel: " + files.excelPath.wstring());
                OpenPath(files.excelPath);
            }
            catch (const std::exception& e) { ShowError(e); }
        });
        patientBox.Children().Append(patient);
        card.Children().Append(ui::Card(patientBox, 12));

        auto open = MakeButton(L"Berichtsordner öffnen");
        open.Click([this](auto&&, auto&&) { if (auto s = CurrentStudy()) OpenPath(ReportRoot(*s)); });
        card.Children().Append(open);

        // Drug Accountability bleibt eine eigene visuelle Gruppe.
        panel.Children().Append(ui::Card(card));

        // -------- Dokumentationstext für Medikamentenausgabe / -rückgabe --------
        // Der Generator verwendet den ausgewählten DrugInventory-Datensatz als
        // alleinige Quelle für die fachlichen Detailangaben. Nur Einnahmebeginn
        // bzw. letzte Einnahme wird manuell ergänzt. Dadurch entstehen keine
        // vom Excel-Datenbestand abweichenden frei editierten Parallelwerte.
        StackPanel documentationBox; documentationBox.Spacing(8);
        documentationBox.Children().Append(ui::Text(L"Dokumentationstext für Ausgabe / Rückgabe", 14, true));
        documentationBox.Children().Append(ui::Text(
            L"Der Text wird aus dem ausgewählten DrugInventory-Datensatz erzeugt; nur Einnahmebeginn bzw. letzte Einnahme wird manuell ergänzt.",
            12, false));

        ComboBox documentationType;
        documentationType.Items().Append(box_value(L"Medikamentenausgabe"));
        documentationType.Items().Append(box_value(L"Medikamentenrückgabe"));
        documentationType.SelectedIndex(0);

        ComboBox documentationInventory;
        auto documentationTable = std::make_shared<SheetTable>(imp->inventory);
        auto documentationRows = std::make_shared<std::vector<size_t>>();

        const int docPatientCol = HeaderLike(*documentationTable, {
            L"Pat.ID", L"Pat ID", L"Pat.-ID", L"PatID", L"PatientID", L"Patient ID",
            L"Patient", L"Patienten-ID", L"Patienten ID", L"Patientennummer", L"Patienten-Nr.", L"Pat.Nr." });
        const int docBatchCol = HeaderLike(*documentationTable, {
            L"Charge No.", L"Charge No", L"Charge", L"Chargennummer", L"Batch", L"Batch No", L"Lot", L"Lot No" });
        const int docIdentifierCol = HeaderLike(*documentationTable, {
            L"Medikationsnummer", L"Medication Number", L"Medication No", L"Medication-No",
            L"Box-Nr.", L"Box-Nr", L"Box Nr", L"Box No", L"Kit-No.", L"Kit-No", L"Kit No",
            L"Vial No.", L"Vial No", L"Vial", L"Serial", L"Pack No" });
        const int docDispensingCol = HeaderLike(*documentationTable, {
            L"DispensingDate", L"Dispensing Date", L"Datum Ausgabe", L"Ausgabe am", L"Ausgegeben am", L"Dispensed" });
        const int docReturnCol = HeaderLike(*documentationTable, {
            L"Date returned", L"Return Date", L"Returned", L"Datum zurück gegeben am", L"Datum zurückgegeben am",
            L"Zurückgegeben am", L"Rückgabe am", L"Rückgabedatum" });

        // Einheitliche Darstellung der Excel-Zellwerte für Dokumentationszwecke.
        // Normale Datumsfelder werden als TT.MM.JJJJ dargestellt; Verfallsfelder
        // bewusst als MM/JJJJ, weil dies der üblichen Packungsangabe entspricht.
        auto docCell = [documentationTable](size_t rowIndex, int column) -> std::wstring
        {
            if (column < 0 || rowIndex >= documentationTable->rows.size()) return L"";
            const auto& row = documentationTable->rows[rowIndex];
            if (static_cast<size_t>(column) >= row.size()) return L"";

            const auto& header = static_cast<size_t>(column) < documentationTable->headers.size()
                ? documentationTable->headers[static_cast<size_t>(column)] : L"";
            const auto& value = row[static_cast<size_t>(column)];

            const auto normalized = date::Normalize(header);
            const bool expiryLike =
                normalized.find(L"expiry") != std::wstring::npos ||
                normalized.find(L"expiration") != std::wstring::npos ||
                normalized.find(L"verfall") != std::wstring::npos ||
                normalized.find(L"verwendbarbis") != std::wstring::npos ||
                normalized.find(L"haltbar") != std::wstring::npos ||
                normalized.find(L"gültig") != std::wstring::npos ||
                normalized.find(L"gueltig") != std::wstring::npos;

            if (expiryLike)
                return ExpiryForDocumentation(value, header);

            const bool dateLike =
                date::HeaderLooksLikeDate(header) ||
                normalized.find(L"ausgabe") != std::wstring::npos ||
                normalized.find(L"ausgegeben") != std::wstring::npos;

            return date::Trim(date::CellToDisplay(value, dateLike));
        };

        for (size_t r = 0; r < documentationTable->rows.size(); ++r)
        {
            if (!RowHasData(documentationTable->rows[r])) continue;
            documentationRows->push_back(r);

            const int excelRow = r < documentationTable->rowNumbers.size()
                ? documentationTable->rowNumbers[r]
                : documentationTable->firstRow + 1 + static_cast<int>(r);

            std::wstring label = L"Excel-Zeile " + std::to_wstring(excelRow);

            const auto patientValue = docCell(r, docPatientCol);
            if (!patientValue.empty())
            {
                const std::wstring patientHeader = docPatientCol >= 0 && static_cast<size_t>(docPatientCol) < documentationTable->headers.size()
                    ? documentationTable->headers[static_cast<size_t>(docPatientCol)] : L"Pat.ID";
                label += L"  |  " + patientHeader + L": " + patientValue;
            }

            const auto identifierValue = docCell(r, docIdentifierCol);
            if (!identifierValue.empty())
            {
                const std::wstring identifierHeader = docIdentifierCol >= 0 && static_cast<size_t>(docIdentifierCol) < documentationTable->headers.size()
                    ? documentationTable->headers[static_cast<size_t>(docIdentifierCol)] : L"ID";
                label += L"  |  " + identifierHeader + L": " + identifierValue;
            }

            const auto batchValue = docCell(r, docBatchCol);
            if (!batchValue.empty())
            {
                const std::wstring batchHeader = docBatchCol >= 0 && static_cast<size_t>(docBatchCol) < documentationTable->headers.size()
                    ? documentationTable->headers[static_cast<size_t>(docBatchCol)] : L"Charge";
                label += L"  |  " + batchHeader + L": " + batchValue;
            }

            documentationInventory.Items().Append(box_value(label));
        }
        if (!documentationRows->empty()) documentationInventory.SelectedIndex(0);

        TextBox docTherapyDate;
        docTherapyDate.PlaceholderText(L"TT.MM.JJJJ – einzige manuell zu ergänzende Angabe");

        TextBox docInventoryPreview;
        docInventoryPreview.AcceptsReturn(true);
        docInventoryPreview.TextWrapping(TextWrapping::Wrap);
        docInventoryPreview.IsReadOnly(true);
        docInventoryPreview.MinHeight(105);
        docInventoryPreview.PlaceholderText(L"Hier werden die befüllten Felder der ausgewählten DrugInventory-Zeile angezeigt.");

        TextBox docOutput;
        docOutput.AcceptsReturn(true);
        docOutput.TextWrapping(TextWrapping::Wrap);
        docOutput.IsReadOnly(true);
        docOutput.MinHeight(145);
        docOutput.PlaceholderText(L"Hier erscheint der aus DrugInventory vorerstellte Dokumentationstext.");

        StackPanel docInput; docInput.Spacing(7);
        docInput.Children().Append(ui::LabeledField(L"Vorgang", documentationType));
        docInput.Children().Append(ui::LabeledField(L"Inventardatensatz / Packung", documentationInventory));
        docInput.Children().Append(ui::LabeledField(L"Einnahmebeginn / letzte Einnahme (manuell)", docTherapyDate));

        StackPanel docSource; docSource.Spacing(7);
        docSource.Children().Append(ui::Text(L"Datenquelle: DrugInventory", 13, true));
        docSource.Children().Append(docInventoryPreview);

        documentationBox.Children().Append(MakeResponsiveTwoCardGrid(ui::Card(docInput, 10), ui::Card(docSource, 10)));

        auto fillDocumentationPreview = [
            documentationType, documentationInventory, documentationRows, documentationTable,
            docCell, docInventoryPreview]()
        {
            const int selected = documentationInventory.SelectedIndex();
            if (selected < 0 || static_cast<size_t>(selected) >= documentationRows->size())
            {
                docInventoryPreview.Text(L"");
                return;
            }

            const size_t rowIndex = (*documentationRows)[static_cast<size_t>(selected)];
            if (rowIndex >= documentationTable->rows.size())
            {
                docInventoryPreview.Text(L"");
                return;
            }

            const auto type = documentationType.SelectedIndex() == 1
                ? MedicationDocumentationType::Return
                : MedicationDocumentationType::Dispensing;

            std::wstring preview;
            for (size_t c = 0; c < documentationTable->headers.size(); ++c)
            {
                const auto header = date::Trim(documentationTable->headers[c]);
                if (header.empty() || !ShouldIncludeMedicationDocumentationField(type, header))
                    continue;

                const auto value = docCell(rowIndex, static_cast<int>(c));
                if (value.empty()) continue;

                if (!preview.empty()) preview += L"\r\n";
                preview += header + L": " + value;
            }
            docInventoryPreview.Text(preview);
        };

        documentationInventory.SelectionChanged([fillDocumentationPreview](auto&&, auto&&)
        {
            fillDocumentationPreview();
        });
        documentationType.SelectionChanged([fillDocumentationPreview](auto&&, auto&&)
        {
            fillDocumentationPreview();
        });
        fillDocumentationPreview();

        StackPanel docActions; docActions.Orientation(Orientation::Horizontal); docActions.Spacing(8);
        auto generateDocumentation = MakePrimaryButton(L"Dokumentationstext erstellen");
        generateDocumentation.Click([
            this, documentationType, documentationInventory, documentationRows, documentationTable,
            docCell, docPatientCol, docDispensingCol, docReturnCol,
            docTherapyDate, docOutput, medicationName = imp->name](auto&&, auto&&)
        {
            try
            {
                const int selected = documentationInventory.SelectedIndex();
                if (selected < 0 || static_cast<size_t>(selected) >= documentationRows->size())
                    throw std::runtime_error("Bitte einen DrugInventory-Datensatz auswählen.");

                const size_t rowIndex = (*documentationRows)[static_cast<size_t>(selected)];
                if (rowIndex >= documentationTable->rows.size())
                    throw std::runtime_error("Der ausgewählte DrugInventory-Datensatz ist nicht mehr verfügbar.");

                const auto type = documentationType.SelectedIndex() == 1
                    ? MedicationDocumentationType::Return
                    : MedicationDocumentationType::Dispensing;

                MedicationDocumentationData data;
                data.patientId = docCell(rowIndex, docPatientCol);
                data.medicationName = date::Trim(medicationName);
                data.eventDate = docCell(rowIndex,
                    type == MedicationDocumentationType::Return ? docReturnCol : docDispensingCol);
                data.therapyDate = date::Trim(std::wstring(docTherapyDate.Text().c_str()));
                data.packageCount = L"1";

                // Alle befüllten DrugInventory-Spalten werden ohne Umbenennung und
                // in der Originalreihenfolge der Excel-Tabelle an den Generator übergeben.
                for (size_t c = 0; c < documentationTable->headers.size(); ++c)
                {
                    const auto header = date::Trim(documentationTable->headers[c]);
                    if (header.empty()) continue;

                    const auto value = docCell(rowIndex, static_cast<int>(c));
                    if (value.empty()) continue;

                    data.inventoryFields.push_back(MedicationDocumentationField{ header, value });
                }

                docOutput.Text(BuildMedicationDocumentationText(type, data));
            }
            catch (const std::exception& e) { ShowError(e); }
        });
        docActions.Children().Append(generateDocumentation);

        auto copyDocumentation = MakeButton(L"Text kopieren");
        copyDocumentation.Click([this, docOutput](auto&&, auto&&)
        {
            try
            {
                const std::wstring text = docOutput.Text().c_str();
                if (date::Trim(text).empty())
                    throw std::runtime_error("Bitte zuerst einen Dokumentationstext erstellen.");
                CopyTextToClipboard(m_hwnd, text);
                ShowInfo(L"Dokumentationstext wurde in die Zwischenablage kopiert.");
            }
            catch (const std::exception& e) { ShowError(e); }
        });
        docActions.Children().Append(copyDocumentation);
        documentationBox.Children().Append(docActions);
        documentationBox.Children().Append(ui::LabeledField(L"Vorerstellter Text", docOutput));
        documentationBox.Children().Append(ui::Text(
            L"Der erzeugte Text wird nur kopiert; es erfolgt keine automatische Übertragung in andere Systeme.",
            10, false));

        // Dokumentationstext bewusst als eigene GroupBox/Card außerhalb von
        // Drug Accountability darstellen, damit die Berichteseite übersichtlich bleibt.
        panel.Children().Append(ui::Card(documentationBox));
        panel.Children().Append(BuildDocumentQuickLinks(study, L"DrugAccount"));
        return panel;
    }

    FrameworkElement MainWindow::BuildDocuments(const StudyData& study)
    {
        StackPanel panel; panel.Spacing(10); panel.Margin(Thickness{ 0,12,0,0 });
        panel.Children().Append(ui::Text(L"Dokumente & Vorlagen – " + study.studyName, 18, true));
        panel.Children().Append(ui::Text(L"Alle Dateien unter Documents der Studie werden automatisch angezeigt. .url-Dateien können für Sponsorportale oder Versandlinks verwendet werden; .eml/.txt-Dateien eignen sich als E-Mail-Vorlagen."));
        if (study.documents.empty())
        {
            StackPanel empty; empty.Spacing(8); empty.Children().Append(ui::Text(L"Noch keine Dokumente hinterlegt.", 14, true));
            auto open = MakeButton(L"Documents-Ordner öffnen"); open.Click([this](auto&&, auto&&) { if (auto s = CurrentStudy()) OpenPath(s->studyDirectory / L"Documents"); }); empty.Children().Append(open);
            panel.Children().Append(ui::Card(empty));
            return panel;
        }
        for (const auto& doc : study.documents)
        {
            Grid row;
            ColumnDefinition c1; c1.Width(GridLength{ 180, GridUnitType::Pixel });
            ColumnDefinition c2; c2.Width(GridLength{ 1, GridUnitType::Star });
            ColumnDefinition c3; c3.Width(GridLength{ 0, GridUnitType::Auto });
            row.ColumnDefinitions().Append(c1); row.ColumnDefinitions().Append(c2); row.ColumnDefinitions().Append(c3);
            auto cat = ui::Text(doc.category, 13, true); cat.VerticalAlignment(VerticalAlignment::Center); row.Children().Append(cat);
            auto title = ui::Text(doc.title + L"   (" + doc.path.extension().wstring() + L")", 13, false); title.VerticalAlignment(VerticalAlignment::Center); Grid::SetColumn(title, 1); row.Children().Append(title);
            auto open = MakeButton(L"Öffnen"); auto path = doc.path; open.Click([this, path](auto&&, auto&&) { OpenPath(path); }); Grid::SetColumn(open, 2); row.Children().Append(open);
            panel.Children().Append(ui::Card(row, 8));
        }
        auto folder = MakeButton(L"Documents-Ordner öffnen"); folder.HorizontalAlignment(HorizontalAlignment::Left); folder.Click([this](auto&&, auto&&) { if (auto s = CurrentStudy()) OpenPath(s->studyDirectory / L"Documents"); }); panel.Children().Append(folder);
        return panel;
    }

    FrameworkElement MainWindow::BuildNotes(const StudyData& study)
    {
        StackPanel panel; panel.Spacing(10); panel.Margin(Thickness{ 0,12,0,0 });
        panel.Children().Append(ui::Text(L"Studiennotizen – " + study.studyName, 18, true));
        m_notesInput = TextBox();
        m_notesInput.AcceptsReturn(true);
        m_notesInput.TextWrapping(TextWrapping::Wrap);
        m_notesInput.MinHeight(300);
        m_notesInput.Text(ReadTextFile(study.studyDirectory / L"Notes.txt"));
        panel.Children().Append(m_notesInput);
        auto save = MakeButton(L"Notizen speichern");
        save.HorizontalAlignment(HorizontalAlignment::Left);
        save.Click([this](auto&&, auto&&)
        {
            try { if (auto s = CurrentStudy()) { WriteUtf8File(s->studyDirectory / L"Notes.txt", m_notesInput.Text().c_str()); ShowInfo(L"Notizen gespeichert."); } }
            catch (const std::exception& e) { ShowError(e); }
        });
        panel.Children().Append(save);
        return ui::Card(panel);
    }

    void MainWindow::RenderStudies()
    {
        m_rendering = true;
        ScrollViewer scroll; StackPanel page; page.Padding(Thickness{ 18,16,18,24 }); page.Spacing(16);
        page.Children().Append(BuildTopBar(L"Studien", L"Kompakte Übersicht der erkannten Studien und ihrer verwalteten IMPs."));

        StackPanel explanation; explanation.Spacing(4);
        explanation.Children().Append(ui::Text(L"Was wird hier angezeigt?", 15, true));
        explanation.Children().Append(ui::Text(L"IMPs = alle im Studienprofil bzw. in der Legacy-Datei erkannten Produkte. 'Bestellpflichtig' zählt nur Produkte, für die das Programm eine Studienbestellung planen soll. Handelsware kann deshalb als IMP dokumentiert werden, ohne in diese Zahl einzugehen.", 12, false));
        page.Children().Append(ui::Card(explanation));

        std::vector<std::wstring> headers{ L"Studie", L"IMPs", L"Bestellpflichtig", L"Patienten", L"Arbeitsmappe", L"Datenmodell", L"Hinweis" };
        std::vector<std::vector<std::wstring>> rows;
        for (const auto& s : m_studies)
        {
            const auto orderCount = std::count_if(s.imps.begin(), s.imps.end(), [](const ImpData& imp) { return imp.orderRequired; });
            const bool profile = s.hasStudyProfile;
            rows.push_back({ s.studyName,
                std::to_wstring(s.imps.size()),
                std::to_wstring(orderCount),
                std::to_wstring(s.patientIds.size()),
                s.excelPath.filename().wstring(),
                profile ? L"Studienprofil" : L"Legacy / Meldebestand",
                s.loadWarning.empty() ? L"–" : L"Excel-Studienname weicht vom Ordnernamen ab" });
        }
        if (rows.empty()) rows.push_back({ L"Keine Studie erkannt", L"0", L"0", L"0", L"–", L"–", L"–" });
        page.Children().Append(BuildInteractiveTable(L"global|studies", headers, rows, {}, 100, false));

        for (const auto& s : m_studies)
        {
            StackPanel details; details.Spacing(7);
            details.Children().Append(ui::Text(s.studyName + L" – IMP-Definitionen", 15, true));
            std::vector<std::wstring> impHeaders{ L"IMP", L"Warenart", L"Form", L"Bestellung", L"Lieferzeit", L"Bestellblatt", L"Inventarblatt" };
            std::vector<std::vector<std::wstring>> impRows;
            for (const auto& imp : s.imps)
                impRows.push_back({ imp.name, imp.goodsType.empty() ? L"–" : imp.goodsType, imp.applicationForm.empty() ? L"–" : imp.applicationForm,
                    imp.orderRequired ? L"Ja" : L"Nein", std::to_wstring(imp.leadTimeDays) + L" T",
                    imp.orderSheet.empty() ? L"–" : imp.orderSheet, imp.inventorySheet.empty() ? L"–" : imp.inventorySheet });
            details.Children().Append(BuildInteractiveTable(L"studies|" + s.studyName + L"|imps", impHeaders, impRows, {}, 60, false));
            page.Children().Append(ui::Card(details));
        }

        scroll.Content(page); m_navigation.Content(scroll); m_rendering = false;
    }

    void MainWindow::RenderGlobalReports()
    {
        m_rendering = true;
        ScrollViewer scroll; StackPanel page; page.Padding(Thickness{ 18,16,18,24 }); page.Spacing(16);
        page.Children().Append(BuildTopBar(L"Berichte", L"Studienübergreifende Übersichten und Exportdateien."));
        StackPanel card; card.Spacing(9);
        card.Children().Append(ui::Text(L"Gesamtübersicht", 17, true));
        auto exportButton = MakeButton(L"Studienübersicht als CSV erstellen");
        exportButton.Click([this](auto&&, auto&&)
        {
            try { auto p = ReportService::ExportStudySummary(m_studies, GlobalReportRoot()); OpenPath(p); }
            catch (const std::exception& e) { ShowError(e); }
        });
        card.Children().Append(exportButton);
        auto open = MakeButton(L"Berichtsordner öffnen"); open.Click([this](auto&&, auto&&) { auto path = GlobalReportRoot(); std::filesystem::create_directories(path); OpenPath(path); }); card.Children().Append(open);
        page.Children().Append(ui::Card(card));
        scroll.Content(page); m_navigation.Content(scroll); m_rendering = false;
    }

    void MainWindow::RenderGlobalDocuments()
    {
        m_rendering = true;
        ScrollViewer scroll; StackPanel page; page.Padding(Thickness{ 18,16,18,24 }); page.Spacing(14);
        page.Children().Append(BuildTopBar(L"Vorlagen & Dokumente", L"Alle studienbezogenen Bestell-, Versand- und Dokumentationsunterlagen an einer Stelle."));

        if (m_studies.empty())
        {
            page.Children().Append(ui::Card(ui::Text(L"Keine Studie geladen.")));
            scroll.Content(page); m_navigation.Content(scroll); m_rendering = false; return;
        }

        Grid selectorRow;
        ColumnDefinition c1; c1.Width(GridLength{ 0, GridUnitType::Auto });
        ColumnDefinition c2; c2.Width(GridLength{ 340, GridUnitType::Pixel });
        ColumnDefinition c3; c3.Width(GridLength{ 1, GridUnitType::Star });
        ColumnDefinition c4; c4.Width(GridLength{ 0, GridUnitType::Auto });
        selectorRow.ColumnDefinitions().Append(c1); selectorRow.ColumnDefinitions().Append(c2); selectorRow.ColumnDefinitions().Append(c3); selectorRow.ColumnDefinitions().Append(c4);
        auto label = ui::Text(L"Studie:", 14, true); label.VerticalAlignment(VerticalAlignment::Center); label.Margin(Thickness{ 0,0,10,0 }); selectorRow.Children().Append(label);
        ComboBox selector;
        for (const auto& study : m_studies)
        {
            std::wstring display = study.studyName;
            if (!date::Trim(study.euctNumber).empty()) display += L" [" + date::Trim(study.euctNumber) + L"]";
            selector.Items().Append(box_value(display));
        }
        selector.SelectedIndex(static_cast<int>(m_selectedStudy));
        selector.SelectionChanged([this, selector](auto&&, auto&&)
        {
            if (m_rendering) return;
            const int index = selector.SelectedIndex();
            if (index >= 0 && static_cast<size_t>(index) < m_studies.size())
            {
                m_selectedStudy = static_cast<size_t>(index);
                m_selectedImp = 0;
                RenderGlobalDocuments();
            }
        });
        Grid::SetColumn(selector, 1); selectorRow.Children().Append(selector);
        auto info = ui::Text(CurrentStudy() ? CurrentStudy()->excelPath.filename().wstring() : L"", 12, false); info.VerticalAlignment(VerticalAlignment::Center); info.Margin(Thickness{ 12,0,8,0 }); Grid::SetColumn(info, 2); selectorRow.Children().Append(info);
        auto openFolder = MakeButton(L"Dokumentenordner öffnen");
        openFolder.Click([this](auto&&, auto&&) { if (auto s = CurrentStudy()) { auto path = s->studyDirectory / L"Documents"; std::filesystem::create_directories(path); OpenPath(path); } });
        Grid::SetColumn(openFolder, 3); selectorRow.Children().Append(openFolder);
        page.Children().Append(selectorRow);

        if (auto s = CurrentStudy()) page.Children().Append(BuildDocuments(*s));
        scroll.Content(page); m_navigation.Content(scroll); m_rendering = false;
    }

    void MainWindow::RenderSettings()
    {
        m_rendering = true;
        ScrollViewer scroll; StackPanel page; page.Padding(Thickness{ 18,16,18,24 }); page.Spacing(14);
        page.Children().Append(BuildTopBar(L"Einstellungen", L"Studien-Datenordner, Exportpfade und AuditLog-Ablage."));

        StackPanel card; card.Spacing(8);
        card.Children().Append(ui::Text(L"Studien-Datenordner", 17, true));
        card.Children().Append(ui::Text(L"In diesem Ordner liegt pro Studie ein eigener Unterordner. Der Unterordnername ist die eindeutige Studienidentität im Programm.", 13, false));
        m_settingsFolderInput = TextBox(); m_settingsFolderInput.Text(m_settings.studyFolder.wstring());
        card.Children().Append(ui::LabeledField(L"Aktiver Studien-Datenordner", m_settingsFolderInput));

        StackPanel buttons; buttons.Orientation(Orientation::Horizontal); buttons.Spacing(8);
        auto choose = MakeButton(L"Ordner auswählen...");
        choose.Click([this](auto&&, auto&&) { ChooseStudyFolder(); });
        buttons.Children().Append(choose);
        auto openFolder = MakeButton(L"Ordner öffnen");
        openFolder.Click([this](auto&&, auto&&) { OpenPath(m_settings.studyFolder); });
        buttons.Children().Append(openFolder);
        auto save = MakeButton(L"Pfad speichern");
        save.Click([this](auto&&, auto&&)
        {
            try
            {
                auto path = std::filesystem::path(std::wstring(m_settingsFolderInput.Text().c_str()));
                if (path.empty()) throw std::runtime_error("Bitte einen Studienordner angeben.");
                std::filesystem::create_directories(path);
                m_settings.studyFolder = std::filesystem::weakly_canonical(path);
                m_settings.Save();
                m_selectedStudy = 0;
                LoadStudies();
                RenderSettings();
                ShowInfo(L"Studien-Datenordner gespeichert.");
            }
            catch (const std::exception& e) { ShowError(e); }
        });
        buttons.Children().Append(save);
        card.Children().Append(buttons);
        card.Children().Append(ui::Text(L"Nur Excel-Dateien direkt im jeweiligen Studien-Unterordner werden als Studien-Arbeitsmappe betrachtet. Documents, Backups und Berichtsvorlagen dürfen weitere Excel-Dateien enthalten.", 12, false));
        page.Children().Append(ui::Card(card));

        StackPanel exports; exports.Spacing(9);
        exports.Children().Append(ui::Text(L"Exportpfade je Studie", 17, true));
        exports.Children().Append(ui::Text(L"Ohne eigene Einstellung werden Excel-Exporte direkt im jeweiligen Studien-Datenordner gespeichert. Hier kannst du für einzelne Studien einen abweichenden Ablageordner festlegen.", 13, false));
        m_exportFolderInputs.clear();
        if (m_studies.empty())
        {
            exports.Children().Append(ui::Text(L"Noch keine Studien geladen.", 13, false));
        }
        for (const auto& study : m_studies)
        {
            Grid row;
            ColumnDefinition r1; r1.Width(GridLength{ 230, GridUnitType::Pixel });
            ColumnDefinition r2; r2.Width(GridLength{ 1, GridUnitType::Star });
            ColumnDefinition r3; r3.Width(GridLength{ 0, GridUnitType::Auto });
            row.ColumnDefinitions().Append(r1); row.ColumnDefinitions().Append(r2); row.ColumnDefinitions().Append(r3);
            std::wstring labelText = study.studyName;
            if (!date::Trim(study.euctNumber).empty()) labelText += L" [" + date::Trim(study.euctNumber) + L"]";
            auto studyLabel = ui::Text(labelText, 13, true); studyLabel.VerticalAlignment(VerticalAlignment::Center); row.Children().Append(studyLabel);

            TextBox input;
            input.Text(m_settings.ExportFolderForStudy(study.studyName, study.studyDirectory).wstring());
            m_exportFolderInputs[study.studyName] = input;
            Grid::SetColumn(input, 1); row.Children().Append(input);

            StackPanel actions; actions.Orientation(Orientation::Horizontal); actions.Spacing(6);
            const auto studyName = study.studyName;
            const auto studyDirectory = study.studyDirectory;
            auto chooseExport = MakeButton(L"Auswählen");
            chooseExport.Click([this, studyName, studyDirectory, input](auto&&, auto&&)
            {
                try
                {
                    auto initial = std::filesystem::path(std::wstring(input.Text().c_str()));
                    if (initial.empty()) initial = studyDirectory;
                    const auto selected = PickFolder(initial);
                    if (!selected) return;
                    input.Text(selected->wstring());
                    m_settings.SetExportFolderForStudy(studyName, std::filesystem::weakly_canonical(*selected));
                    m_settings.Save();
                }
                catch (const std::exception& e) { ShowError(e); }
            });
            auto standard = MakeButton(L"Standard");
            standard.Click([this, studyName, studyDirectory, input](auto&&, auto&&)
            {
                m_settings.ClearExportFolderForStudy(studyName);
                m_settings.Save();
                input.Text(studyDirectory.wstring());
            });
            actions.Children().Append(chooseExport); actions.Children().Append(standard);
            Grid::SetColumn(actions, 2); row.Children().Append(actions);
            exports.Children().Append(row);
        }
        if (!m_studies.empty())
        {
            auto saveExports = MakeButton(L"Exportpfade speichern");
            saveExports.HorizontalAlignment(HorizontalAlignment::Left);
            saveExports.Click([this](auto&&, auto&&)
            {
                try
                {
                    for (const auto& study : m_studies)
                    {
                        const auto it = m_exportFolderInputs.find(study.studyName);
                        if (it == m_exportFolderInputs.end()) continue;
                        auto path = std::filesystem::path(std::wstring(it->second.Text().c_str()));
                        if (path.empty())
                        {
                            m_settings.ClearExportFolderForStudy(study.studyName);
                            continue;
                        }
                        const auto canonical = std::filesystem::weakly_canonical(path);
                        const auto studyCanonical = std::filesystem::weakly_canonical(study.studyDirectory);
                        if (canonical == studyCanonical)
                            m_settings.ClearExportFolderForStudy(study.studyName);
                        else
                        {
                            std::filesystem::create_directories(path);
                            m_settings.SetExportFolderForStudy(study.studyName, std::filesystem::weakly_canonical(path));
                        }
                    }
                    m_settings.Save();
                    ShowInfo(L"Exportpfade gespeichert.");
                }
                catch (const std::exception& e) { ShowError(e); }
            });
            exports.Children().Append(saveExports);
        }
        exports.Children().Append(ui::Text(L"Die studienübergreifende CSV-Übersicht wird unter <Studien-Datenordner>\\Exporte gespeichert.", 12, false));
        page.Children().Append(ui::Card(exports));

        // AuditLog ist absichtlich nicht abschaltbar. Der Ablageordner ist jedoch
        // konfigurierbar, damit auch freigegebene IT-/Netzwerkpfade verwendet werden
        // können. Der Standardpfad liegt weiterhin im lokalen Benutzerprofil.
        StackPanel audit; audit.Spacing(8);
        audit.Children().Append(ui::Text(L"AuditLog", 17, true));
        audit.Children().Append(ui::Text(
            L"Fachliche Änderungen und DrugAccount-Exporte werden automatisch protokolliert. Das AuditLog kann in der Anwendung nicht deaktiviert werden. Der Speicherort kann an die Vorgaben der IT angepasst werden.",
            13, false));

        m_auditFolderInput = TextBox();
        m_auditFolderInput.Text(m_settings.auditFolder.wstring());
        m_auditFolderInput.PlaceholderText(L"z. B. C:\\AuditLogs oder \\\\Server\\Freigabe\\Medikationsverwaltung\\Audit");
        audit.Children().Append(ui::LabeledField(L"AuditLog-Speicherort", m_auditFolderInput));

        StackPanel auditButtons; auditButtons.Orientation(Orientation::Horizontal); auditButtons.Spacing(8);

        auto chooseAudit = MakeButton(L"Ordner auswählen...");
        chooseAudit.Click([this](auto&&, auto&&)
        {
            try
            {
                auto initial = std::filesystem::path(std::wstring(m_auditFolderInput.Text().c_str()));
                if (initial.empty()) initial = AppSettings::DefaultAuditFolder();
                const auto selected = PickFolder(initial);
                if (selected) m_auditFolderInput.Text(selected->wstring());
            }
            catch (const std::exception& e) { ShowError(e); }
        });
        auditButtons.Children().Append(chooseAudit);

        auto standardAudit = MakeButton(L"Standard");
        standardAudit.Click([this](auto&&, auto&&)
        {
            m_auditFolderInput.Text(AppSettings::DefaultAuditFolder().wstring());
        });
        auditButtons.Children().Append(standardAudit);

        auto saveAudit = MakeButton(L"Audit-Pfad speichern");
        saveAudit.Click([this](auto&&, auto&&)
        {
            const auto previousDirectory = AuditLogger::AuditDirectory();
            try
            {
                auto path = std::filesystem::path(std::wstring(m_auditFolderInput.Text().c_str()));
                if (path.empty()) throw std::runtime_error("Bitte einen AuditLog-Ordner angeben.");
                if (path.is_relative()) path = m_settings.settingsDirectory / path;

                std::filesystem::create_directories(path);
                path = std::filesystem::weakly_canonical(path);

                // Vor dem Speichern wird tatsächlich geprüft, ob die Anwendung dort
                // eine Audit-Datei anlegen und beschreiben kann.
                AuditLogger::SetAuditDirectory(path);
                AuditLogger::EnsureWritable();

                m_settings.auditFolder = path;
                m_settings.Save();
                m_auditFolderInput.Text(path.wstring());
                ShowInfo(L"AuditLog-Speicherort gespeichert und Schreibzugriff erfolgreich geprüft.");
            }
            catch (const std::exception& e)
            {
                AuditLogger::SetAuditDirectory(previousDirectory);
                ShowError(e);
            }
        });
        auditButtons.Children().Append(saveAudit);

        auto openAudit = MakeButton(L"AuditLog-Ordner öffnen");
        openAudit.Click([this](auto&&, auto&&)
        {
            try
            {
                auto path = std::filesystem::path(std::wstring(m_auditFolderInput.Text().c_str()));
                if (path.empty()) path = AuditLogger::AuditDirectory();
                std::filesystem::create_directories(path);
                OpenPath(path);
            }
            catch (const std::exception& e) { ShowError(e); }
        });
        auditButtons.Children().Append(openAudit);
        audit.Children().Append(auditButtons);

        audit.Children().Append(ui::Text(
            L"Standardmäßig wird %LOCALAPPDATA%\\Medikationsverwaltung\\Audit verwendet; dieser Ordner benötigt normalerweise keine Administratorrechte. Für einen zentralen Netzwerkpfad müssen die erforderlichen Schreibrechte durch die IT vergeben werden.",
            12, false));
        audit.Children().Append(ui::Text(
            L"Die Einträge enthalten UTC-Zeitstempel, Vorgangs-ID, Windows-Benutzer, Computer, Programmversion, Studie, IMP, Excel-Zeile sowie alten und neuen Wert.",
            12, false));
        page.Children().Append(ui::Card(audit));

        StackPanel technical; technical.Spacing(5);
        technical.Children().Append(ui::Text(L"Technischer Hinweis", 15, true));
        technical.Children().Append(ui::Text(L"Für direktes Lesen, Schreiben und die Erstellung der Excel-Berichte wird die installierte Microsoft-Excel-Desktopanwendung über COM verwendet. Vor Änderungen an einer Studien-Excel wird weiterhin automatisch eine Sicherungskopie angelegt. Das AuditLog ergänzt diese Sicherung um eine nachvollziehbare Änderungsdokumentation und kann lokal oder auf einem durch die IT freigegebenen Pfad abgelegt werden.", 12, false));
        page.Children().Append(ui::Card(technical));

        scroll.Content(page); m_navigation.Content(scroll); m_rendering = false;
    }

    void MainWindow::OpenPath(const std::filesystem::path& path) const
    {
        if (!std::filesystem::exists(path) && path.extension() != L".url")
        {
            ShowError(L"Pfad nicht gefunden:\n" + path.wstring());
            return;
        }
        auto result = ::ShellExecuteW(m_hwnd, L"open", path.c_str(), nullptr, path.has_parent_path() ? path.parent_path().c_str() : nullptr, SW_SHOWNORMAL);
        if (reinterpret_cast<INT_PTR>(result) <= 32) ShowError(L"Die Datei bzw. der Link konnte nicht geöffnet werden.");
    }

    void MainWindow::ShowError(const std::wstring& message) const
    {
        ::MessageBoxW(m_hwnd, message.c_str(), L"Medikationsverwaltung – Fehler", MB_OK | MB_ICONERROR);
    }

    void MainWindow::ShowError(const std::exception& error) const
    {
        ShowError(Widen(error.what()));
    }

    void MainWindow::ShowInfo(const std::wstring& message) const
    {
        ::MessageBoxW(m_hwnd, message.c_str(), L"Medikationsverwaltung", MB_OK | MB_ICONINFORMATION);
    }
}
