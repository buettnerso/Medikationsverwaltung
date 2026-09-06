// ============================================================================
// Datei: App.cpp
// Zweck: Implementiert den Start der WinUI-Anwendung und erzeugt das Hauptfenster.
//
// Verantwortlichkeiten:
// - Lädt die Standardressourcen von WinUI.
// - Erzeugt und aktiviert genau eine MainWindow-Instanz.
//
// Hinweis: Kommentare erläutern Architektur und nicht offensichtliche Logik.
// Triviale Sprachkonstrukte werden bewusst nicht zeilenweise kommentiert.
// ============================================================================
#include "pch.h"
#include "App.h"
#include "MainWindow.h"

namespace med
{
    // -------------------------------------------------------------------------
    // Anwendungslebenszyklus
    // -------------------------------------------------------------------------
    App::App() = default;
    App::~App() = default;

    /// Registriert WinUI-Standardressourcen und erzeugt das Hauptfenster.
    void App::OnLaunched(winrt::Microsoft::UI::Xaml::LaunchActivatedEventArgs const&)
    {
        Resources().MergedDictionaries().Append(winrt::Microsoft::UI::Xaml::Controls::XamlControlsResources());
        m_mainWindow = std::make_unique<MainWindow>();
        m_mainWindow->Activate();
    }

    winrt::Microsoft::UI::Xaml::Markup::IXamlType App::GetXamlType(winrt::Windows::UI::Xaml::Interop::TypeName const& type)
    {
        return m_metadataProvider.GetXamlType(type);
    }

    winrt::Microsoft::UI::Xaml::Markup::IXamlType App::GetXamlType(winrt::hstring const& fullName)
    {
        return m_metadataProvider.GetXamlType(fullName);
    }

    winrt::com_array<winrt::Microsoft::UI::Xaml::Markup::XmlnsDefinition> App::GetXmlnsDefinitions()
    {
        return m_metadataProvider.GetXmlnsDefinitions();
    }
}
