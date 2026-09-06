// ============================================================================
// Datei: Settings.cpp
// Zweck: Liest und schreibt die lokale Anwendungskonfiguration sowie optionale study.config.ini-Dateien.
//
// Verantwortlichkeiten:
// - Speichert Benutzereinstellungen außerhalb des Programmordners.
// - Verwendet ein bewusst einfaches Schlüssel=Wert-Format ohne externe Abhängigkeiten.
//
// Hinweis: Kommentare erläutern Architektur und nicht offensichtliche Logik.
// Triviale Sprachkonstrukte werden bewusst nicht zeilenweise kommentiert.
// ============================================================================
#include "pch.h"
#include "Settings.h"
#include "Models.h"
#include "DateUtils.h"

#include <fstream>

namespace
{
    // -------------------------------------------------------------------------
    // Pfad-Hilfsfunktionen
    // -------------------------------------------------------------------------
    std::filesystem::path ExecutableDirectory()
    {
        std::wstring buffer(32768, L'\0');
        const DWORD len = ::GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        buffer.resize(len);
        return std::filesystem::path(buffer).parent_path();
    }

    std::filesystem::path KnownFolder(REFKNOWNFOLDERID id)
    {
        PWSTR raw = nullptr;
        if (SUCCEEDED(::SHGetKnownFolderPath(id, KF_FLAG_DEFAULT, nullptr, &raw)) && raw)
        {
            std::filesystem::path result(raw);
            ::CoTaskMemFree(raw);
            return result;
        }
        if (raw) ::CoTaskMemFree(raw);
        return {};
    }
}

namespace med
{
    std::map<std::wstring, std::wstring> SimpleIni::Load(const std::filesystem::path& path)
    {
        std::map<std::wstring, std::wstring> result;
        std::wifstream input(path);
        if (!input) return result;

        std::wstring line;
        while (std::getline(input, line))
        {
            line = date::Trim(line);
            if (line.empty() || line[0] == L'#' || line[0] == L';' || line[0] == L'[') continue;
            const auto pos = line.find(L'=');
            if (pos == std::wstring::npos) continue;
            auto key = date::Normalize(line.substr(0, pos));
            auto value = date::Trim(line.substr(pos + 1));
            if (!key.empty()) result[key] = value;
        }
        return result;
    }

    void SimpleIni::Save(const std::filesystem::path& path, const std::map<std::wstring, std::wstring>& values)
    {
        std::filesystem::create_directories(path.parent_path());
        std::wofstream output(path, std::ios::trunc);
        output << L"# Medikationsverwaltung - lokale Einstellungen\n";
        for (const auto& [key, value] : values)
            output << key << L"=" << value << L"\n";
    }

    std::filesystem::path AppSettings::DefaultStudyFolder()
    {
        auto documents = KnownFolder(FOLDERID_Documents);
        if (documents.empty()) documents = ExecutableDirectory();
        return documents / L"Medikationsverwaltung" / L"Studien";
    }

    AppSettings AppSettings::Load()
    {
        AppSettings settings;
        settings.executableDirectory = ExecutableDirectory();

        auto localAppData = KnownFolder(FOLDERID_LocalAppData);
        if (localAppData.empty()) localAppData = settings.executableDirectory;
        settings.settingsDirectory = localAppData / L"Medikationsverwaltung";
        settings.studyFolder = DefaultStudyFolder();

        const auto values = SimpleIni::Load(settings.SettingsPath());
        if (const auto it = values.find(L"studyfolder"); it != values.end() && !it->second.empty())
        {
            std::filesystem::path p = it->second;
            if (p.is_relative()) p = settings.executableDirectory / p;
            settings.studyFolder = p;
        }
        for (const auto& [key, value] : values)
        {
            const std::wstring prefix = L"exportfolder";
            if (key.rfind(prefix, 0) != 0 || key.size() <= prefix.size()) continue;
            auto studyKey = key.substr(prefix.size());
            while (!studyKey.empty() && (studyKey.front() == L'.' || studyKey.front() == L'_' || studyKey.front() == L'-')) studyKey.erase(studyKey.begin());
            if (studyKey.empty() || value.empty()) continue;
            std::filesystem::path p = value;
            if (p.is_relative()) p = settings.studyFolder / p;
            settings.studyExportFolders[studyKey] = p;
        }

        std::filesystem::create_directories(settings.studyFolder);
        settings.studyFolder = std::filesystem::weakly_canonical(settings.studyFolder);
        return settings;
    }

    void AppSettings::Save() const
    {
        std::map<std::wstring, std::wstring> values{ {L"StudyFolder", studyFolder.wstring()} };
        for (const auto& [studyKey, path] : studyExportFolders)
            values[L"ExportFolder." + studyKey] = path.wstring();
        SimpleIni::Save(SettingsPath(), values);
    }

    std::filesystem::path AppSettings::ExportFolderForStudy(const std::wstring& studyName, const std::filesystem::path& studyDirectory) const
    {
        const auto key = date::Normalize(studyName);
        const auto it = studyExportFolders.find(key);
        if (it != studyExportFolders.end() && !it->second.empty()) return it->second;
        return studyDirectory;
    }

    void AppSettings::SetExportFolderForStudy(const std::wstring& studyName, const std::filesystem::path& path)
    {
        studyExportFolders[date::Normalize(studyName)] = path;
    }

    void AppSettings::ClearExportFolderForStudy(const std::wstring& studyName)
    {
        studyExportFolders.erase(date::Normalize(studyName));
    }

    std::filesystem::path AppSettings::SettingsPath() const
    {
        return settingsDirectory / L"appsettings.ini";
    }

    StudyConfig LoadStudyConfig(const std::filesystem::path& studyDirectory)
    {
        StudyConfig config;
        const auto values = SimpleIni::Load(studyDirectory / L"study.config.ini");
        auto get = [&](const wchar_t* key) -> std::wstring
        {
            const auto it = values.find(date::Normalize(key));
            return it == values.end() ? L"" : it->second;
        };

        // StudyName wird aus Kompatibilitätsgründen weiterhin gelesen, aber der
        // sichtbare Studienname wird bewusst aus dem Ordnernamen abgeleitet.
        if (auto v = get(L"StudyName"); !v.empty()) config.studyNameOverride = v;
        if (auto v = get(L"Medication"); !v.empty()) config.medicationOverride = v;
        if (auto v = get(L"Workbook"); !v.empty()) config.workbookFile = v;
        if (auto v = get(L"DrugAccountTemplate"); !v.empty()) config.drugAccountTemplate = v;
        if (auto v = get(L"StockSheet"); !v.empty()) config.stockSheet = v;
        if (auto v = get(L"OrderSheet"); !v.empty()) config.orderSheet = v;
        if (auto v = get(L"InventorySheet"); !v.empty()) config.inventorySheet = v;
        if (auto v = get(L"VisitSheet"); !v.empty()) config.visitSheet = v;
        if (auto v = get(L"ConsumptionPerVisit"); !v.empty())
        {
            try { config.consumptionPerVisit = std::stod(v); } catch (...) {}
        }
        if (auto v = get(L"PatientColumnsStart"); !v.empty())
        {
            try { config.patientColumnsStart = std::stoi(v); } catch (...) {}
        }
        return config;
    }
}
