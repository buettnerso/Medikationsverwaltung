#pragma once

#include "Models.h"
#include <filesystem>
#include <map>
#include <string>

namespace med
{
    class SimpleIni
    {
    public:
        static std::map<std::wstring, std::wstring> Load(const std::filesystem::path& path);
        static void Save(const std::filesystem::path& path, const std::map<std::wstring, std::wstring>& values);
    };

    struct AppSettings
    {
        std::filesystem::path executableDirectory;
        std::filesystem::path settingsDirectory;
        std::filesystem::path studyFolder;
        std::filesystem::path auditFolder;
        std::map<std::wstring, std::filesystem::path> studyExportFolders;

        static AppSettings Load();
        void Save() const;
        std::filesystem::path SettingsPath() const;
        static std::filesystem::path DefaultStudyFolder();
        static std::filesystem::path DefaultAuditFolder();
        std::filesystem::path ExportFolderForStudy(const std::wstring& studyName, const std::filesystem::path& studyDirectory) const;
        void SetExportFolderForStudy(const std::wstring& studyName, const std::filesystem::path& path);
        void ClearExportFolderForStudy(const std::wstring& studyName);
    };

    StudyConfig LoadStudyConfig(const std::filesystem::path& studyDirectory);
}
