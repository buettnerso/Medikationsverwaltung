// ============================================================================
// Datei: App.h
// Zweck: Deklariert die WinUI-Anwendungsklasse und hält das Hauptfenster während der gesamten Programmlaufzeit am Leben.
//
// Verantwortlichkeiten:
// - Implementiert den WinUI-Anwendungslebenszyklus.
// - Stellt den XAML-Metadatenprovider für programmgesteuert erzeugte WinUI-Controls bereit.
//
// Hinweis: Kommentare erläutern Architektur und nicht offensichtliche Logik.
// Triviale Sprachkonstrukte werden bewusst nicht zeilenweise kommentiert.
// ============================================================================
#pragma once

#include "pch.h"
#include <memory>

namespace med { class MainWindow; }

namespace med
{
    /// WinUI-Anwendungsklasse. Sie wird genau einmal in wWinMain erzeugt und
    /// verwaltet den Lebenszyklus des Hauptfensters.
    struct App : winrt::Microsoft::UI::Xaml::ApplicationT<App, winrt::Microsoft::UI::Xaml::Markup::IXamlMetadataProvider>
    {
        App();
        ~App();
        /// Wird von WinUI nach erfolgreichem Start aufgerufen und erzeugt das Hauptfenster.
        void OnLaunched(winrt::Microsoft::UI::Xaml::LaunchActivatedEventArgs const&);

        // Weiterleitung an den WinUI-Metadatenprovider. Diese Methoden sind nötig,
        // weil die Oberfläche programmgesteuert und ohne eigene XAML-Seiten aufgebaut wird.
        winrt::Microsoft::UI::Xaml::Markup::IXamlType GetXamlType(winrt::Windows::UI::Xaml::Interop::TypeName const& type);
        winrt::Microsoft::UI::Xaml::Markup::IXamlType GetXamlType(winrt::hstring const& fullName);
        winrt::com_array<winrt::Microsoft::UI::Xaml::Markup::XmlnsDefinition> GetXmlnsDefinitions();

    private:
        winrt::Microsoft::UI::Xaml::XamlTypeInfo::XamlControlsXamlMetaDataProvider m_metadataProvider;
        std::unique_ptr<MainWindow> m_mainWindow;
    };
}
