#include "pch.h"
#include "AuditLogger.h"

#include "AppVersion.h"

#include <fstream>
#include <mutex>
#include <sstream>

namespace
{
    std::mutex g_auditMutex;

    std::wstring EnvironmentValue(const wchar_t* name)
    {
        const DWORD required = ::GetEnvironmentVariableW(name, nullptr, 0);
        if (required == 0) return L"";

        std::wstring value(required, L'\0');
        const DWORD written = ::GetEnvironmentVariableW(name, value.data(), required);
        if (written == 0 || written >= required) return L"";
        value.resize(written);
        return value;
    }

    std::wstring WindowsUser()
    {
        const auto domain = EnvironmentValue(L"USERDOMAIN");
        const auto user = EnvironmentValue(L"USERNAME");
        if (!domain.empty() && !user.empty()) return domain + L"\\" + user;
        if (!user.empty()) return user;
        return L"Unbekannt";
    }

    std::wstring ComputerName()
    {
        const auto value = EnvironmentValue(L"COMPUTERNAME");
        return value.empty() ? L"Unbekannt" : value;
    }

    std::wstring UtcTimestamp()
    {
        SYSTEMTIME st{};
        ::GetSystemTime(&st);
        wchar_t buffer[64]{};
        swprintf_s(
            buffer,
            L"%04u-%02u-%02uT%02u:%02u:%02u.%03uZ",
            st.wYear,
            st.wMonth,
            st.wDay,
            st.wHour,
            st.wMinute,
            st.wSecond,
            st.wMilliseconds);
        return buffer;
    }

    std::wstring UtcMonth()
    {
        SYSTEMTIME st{};
        ::GetSystemTime(&st);
        wchar_t buffer[16]{};
        swprintf_s(buffer, L"%04u-%02u", st.wYear, st.wMonth);
        return buffer;
    }

    std::string ToUtf8(const std::wstring& value)
    {
        if (value.empty()) return {};
        const int required = ::WideCharToMultiByte(
            CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
            nullptr, 0, nullptr, nullptr);
        if (required <= 0) return {};

        std::string utf8(static_cast<size_t>(required), '\0');
        ::WideCharToMultiByte(
            CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
            utf8.data(), required, nullptr, nullptr);
        return utf8;
    }

    std::string Csv(const std::wstring& value)
    {
        auto utf8 = ToUtf8(value);
        bool quote = false;
        for (char ch : utf8)
        {
            if (ch == ';' || ch == '"' || ch == '\r' || ch == '\n')
            {
                quote = true;
                break;
            }
        }
        if (!quote) return utf8;

        std::string escaped;
        escaped.reserve(utf8.size() + 8);
        escaped.push_back('"');
        for (char ch : utf8)
        {
            if (ch == '"') escaped += "\"\"";
            else escaped.push_back(ch);
        }
        escaped.push_back('"');
        return escaped;
    }

    std::wstring SafeFilePart(std::wstring value)
    {
        if (value.empty()) return L"PC";
        for (auto& ch : value)
        {
            switch (ch)
            {
            case L'<': case L'>': case L':': case L'"': case L'/':
            case L'\\': case L'|': case L'?': case L'*':
                ch = L'_';
                break;
            default:
                break;
            }
        }
        return value;
    }

    const char* HeaderLine()
    {
        return "TimestampUTC;OperationId;User;Computer;AppVersion;Study;EUCT;Workbook;IMP;Action;Sheet;ExcelRow;Field;OldValue;NewValue;Reason;Details\r\n";
    }

    void EnsureHeader(std::ofstream& out, const std::filesystem::path& path)
    {
        std::error_code ec;
        const auto size = std::filesystem::exists(path, ec)
            ? std::filesystem::file_size(path, ec)
            : 0;
        if (!ec && size > 0) return;

        static constexpr unsigned char bom[] = { 0xEF, 0xBB, 0xBF };
        out.write(reinterpret_cast<const char*>(bom), sizeof(bom));
        out << HeaderLine();
    }

    void WriteRow(
        std::ofstream& out,
        const med::StudyData& study,
        const med::AuditEntry& entry,
        const med::AuditChange* change)
    {
        out
            << Csv(UtcTimestamp()) << ';'
            << Csv(entry.operationId) << ';'
            << Csv(WindowsUser()) << ';'
            << Csv(ComputerName()) << ';'
            << Csv(std::wstring(med::version::Current)) << ';'
            << Csv(study.studyName) << ';'
            << Csv(study.euctNumber) << ';'
            << Csv(study.excelPath.wstring()) << ';'
            << Csv(entry.imp) << ';'
            << Csv(entry.action) << ';'
            << Csv(entry.sheet) << ';'
            << entry.excelRow << ';'
            << Csv(change ? change->field : L"") << ';'
            << Csv(change ? change->oldValue : L"") << ';'
            << Csv(change ? change->newValue : L"") << ';'
            << Csv(entry.reason) << ';'
            << Csv(entry.details)
            << "\r\n";
    }
}

namespace med
{
    std::filesystem::path AuditLogger::AuditDirectory()
    {
        auto localAppData = EnvironmentValue(L"LOCALAPPDATA");
        if (localAppData.empty())
            throw std::runtime_error("Der lokale Windows-Anwendungsdatenordner (LOCALAPPDATA) konnte nicht ermittelt werden.");

        return std::filesystem::path(localAppData)
            / L"Medikationsverwaltung"
            / L"Audit";
    }

    std::filesystem::path AuditLogger::CurrentLogFile()
    {
        return AuditDirectory()
            / (L"Audit_" + SafeFilePart(ComputerName()) + L"_" + UtcMonth() + L".csv");
    }

    void AuditLogger::EnsureWritable()
    {
        std::scoped_lock lock(g_auditMutex);

        const auto directory = AuditDirectory();
        std::filesystem::create_directories(directory);
        const auto path = CurrentLogFile();

        std::ofstream out(path, std::ios::binary | std::ios::app);
        if (!out)
            throw std::runtime_error("Das lokale AuditLog kann nicht geöffnet werden. Die Änderung wurde aus Sicherheitsgründen nicht durchgeführt.");

        EnsureHeader(out, path);
        out.flush();
        if (!out)
            throw std::runtime_error("Das lokale AuditLog ist nicht beschreibbar. Die Änderung wurde aus Sicherheitsgründen nicht durchgeführt.");
    }

    std::wstring AuditLogger::NewOperationId()
    {
        GUID guid{};
        if (SUCCEEDED(::CoCreateGuid(&guid)))
        {
            wchar_t buffer[64]{};
            if (::StringFromGUID2(guid, buffer, static_cast<int>(sizeof(buffer) / sizeof(buffer[0]))) > 0)
                return buffer;
        }

        // Fallback ist nur für den sehr unwahrscheinlichen Fall gedacht, dass
        // keine GUID erzeugt werden kann.
        return UtcTimestamp() + L"-" + std::to_wstring(::GetCurrentProcessId());
    }

    void AuditLogger::Write(const StudyData& study, const AuditEntry& entry)
    {
        std::scoped_lock lock(g_auditMutex);

        const auto directory = AuditDirectory();
        std::filesystem::create_directories(directory);
        const auto path = CurrentLogFile();

        std::ofstream out(path, std::ios::binary | std::ios::app);
        if (!out)
            throw std::runtime_error("Der Audit-Eintrag konnte nicht gespeichert werden.");

        EnsureHeader(out, path);

        AuditEntry normalized = entry;
        if (normalized.operationId.empty())
            normalized.operationId = NewOperationId();

        if (normalized.changes.empty())
        {
            WriteRow(out, study, normalized, nullptr);
        }
        else
        {
            for (const auto& change : normalized.changes)
                WriteRow(out, study, normalized, &change);
        }

        out.flush();
        if (!out)
            throw std::runtime_error("Der Audit-Eintrag konnte nicht vollständig gespeichert werden.");
    }
}
