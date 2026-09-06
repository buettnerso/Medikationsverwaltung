// ============================================================================
// Datei: main.cpp
// Zweck: Enthält den nativen Windows-Einstiegspunkt der Anwendung.
//
// Verantwortlichkeiten:
// - Initialisiert C++/WinRT im Single-Threaded-Apartment (STA).
// - Startet anschließend die WinUI-Application med::App.
//
// Hinweis: Kommentare erläutern Architektur und nicht offensichtliche Logik.
// Triviale Sprachkonstrukte werden bewusst nicht zeilenweise kommentiert.
// ============================================================================
#include "pch.h"
#include "App.h"

// -----------------------------------------------------------------------------
// Programmeinstieg
// -----------------------------------------------------------------------------
// WinUI erwartet ein STA-Apartment. Danach übernimmt Application::Start den
// Nachrichtenloop und erzeugt die App-Instanz über die übergebene Factory.
int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    winrt::init_apartment(winrt::apartment_type::single_threaded);
    winrt::Microsoft::UI::Xaml::Application::Start([](auto&&)
    {
        winrt::make<med::App>();
    });
    return 0;
}
