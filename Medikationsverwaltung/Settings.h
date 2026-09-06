// ============================================================================
// Datei: Settings.h
// Zweck: Deklariert die Anwendungs- und Studienkonfiguration.
//
// Verantwortlichkeiten:
// - Verwaltet den globalen Studienordner und studienspezifische Exportpfade.
// - Beschreibt die optionale study.config.ini einer Studie.
//
// Hinweis: Kommentare erläutern Architektur und nicht offensichtliche Logik.
// Triviale Sprachkonstrukte werden bewusst nicht zeilenweise kommentiert.
// ============================================================================
#pragma once

#include "Models.h"
#include <filesystem>
#include <map>
#include <string>

namespace med
{
    /// Minimaler INI-ähnlicher Schlüssel/Wert-Leser für kleine Konfigurationsdateien.
    /// Absichtlich ohne Abschnitte oder externe Bibliothek gehalten.
    class SimpleIni
    {
    public:
        static std::map<std::wstring, std::wstring> Load(const std::filesystem::path& path);
        static void Save(const std::filesystem::path& path, const std::map<std::wstring, std::wstring>& values);
    };

    /// Benutzerspezifische Programmeinstellungen.
    /// Diese Daten gehören nicht in die Studien-Excel und werden separat gespeichert.
    struct AppSettings
    {
        std::filesystem::path executableDirectory;
        std::filesystem::path settingsDirectory;
        /// Übergeordneter Ordner, dessen direkte Unterordner als Studien erkannt werden.
        std::filesystem::path studyFolder;
        /// Optionale, pro Studienname überschreibbare Ablageorte für Berichte/Exporte.
        std::map<std::wstring, std::filesystem::path> studyExportFolders;

        static AppSettings Load();
        void Save() const;
        std::filesystem::path SettingsPath() const;
        static std::filesystem::path DefaultStudyFolder();
        std::filesystem::path ExportFolderForStudy(const std::wstring& studyName, const std::filesystem::path& studyDirectory) const;
        void SetExportFolderForStudy(const std::wstring& studyName, const std::filesystem::path& path);
        void ClearExportFolderForStudy(const std::wstring& studyName);
    };

    /// Lädt optionale study.config.ini-Werte aus einem Studienordner.
    /// Fehlende Werte werden durch die Standardwerte aus StudyConfig ersetzt.
    StudyConfig LoadStudyConfig(const std::filesystem::path& studyDirectory);
}
