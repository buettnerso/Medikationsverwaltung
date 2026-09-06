// ============================================================================
// Datei: pch.h
// Zweck: Zentrale Precompiled-Header-Datei für häufig verwendete Windows-, WinUI- und C++/WinRT-Abhängigkeiten.
//
// Verantwortlichkeiten:
// - Reduziert Kompilierzeit und stellt systemweite Includes an einer Stelle bereit.
//
// Hinweis: Kommentare erläutern Architektur und nicht offensichtliche Logik.
// Triviale Sprachkonstrukte werden bewusst nicht zeilenweise kommentiert.
// ============================================================================
#pragma once

// Win32-/Shell-/COM-Grundlagen. NOMINMAX verhindert Makrokollisionen mit std::min/max.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <wrl/client.h>

// Häufig verwendete C++-Standardbibliothek.
#include <algorithm>
#include <cmath>
#include <stdexcept>

// C++/WinRT- und WinUI-Projektionen.
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.Xaml.Interop.h>
#include <winrt/Windows.UI.h>
#include <winrt/Windows.UI.Text.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Microsoft.UI.Xaml.XamlTypeInfo.h>
#include <winrt/Microsoft.UI.Windowing.h>

// Native HWND-Brücke für ein WinUI-XAML-Fenster.
#include <microsoft.ui.xaml.window.h>
