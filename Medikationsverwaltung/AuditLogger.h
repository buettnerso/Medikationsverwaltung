#pragma once

#include "Models.h"

#include <filesystem>
#include <string>
#include <vector>

namespace med
{
    // Einzelne fachliche Wertänderung innerhalb eines Audit-Vorgangs.
    struct AuditChange
    {
        std::wstring field;
        std::wstring oldValue;
        std::wstring newValue;
    };

    // Beschreibung eines fachlichen Vorgangs. Ein Vorgang kann mehrere
    // Feldänderungen enthalten; im CSV wird pro Feldänderung eine Zeile
    // geschrieben, alle mit derselben OperationId.
    struct AuditEntry
    {
        std::wstring operationId;
        std::wstring imp;
        std::wstring action;
        std::wstring sheet;
        int excelRow{ 0 };
        std::wstring reason;
        std::wstring details;
        std::vector<AuditChange> changes;
    };

    class AuditLogger
    {
    public:
        // Konfiguriert den Audit-Ordner für die laufende Anwendung.
        // Standard ist %LOCALAPPDATA%\Medikationsverwaltung\Audit; in den
        // Einstellungen kann aber z. B. ein freigegebener Netzwerkordner gewählt werden.
        static void SetAuditDirectory(const std::filesystem::path& directory);

        // Aktuell verwendeter Audit-Ordner.
        static std::filesystem::path AuditDirectory();

        // Aktuelle monatliche Audit-Datei. Der Rechnername ist Bestandteil
        // des Dateinamens, damit Exporte verschiedener PCs unterscheidbar sind.
        static std::filesystem::path CurrentLogFile();

        // Prüft VOR einer fachlichen Änderung, ob der Audit-Speicher
        // beschreibbar ist. Schlägt die Prüfung fehl, wird die Datenänderung
        // abgebrochen (fail closed).
        static void EnsureWritable();

        // Eindeutige ID für zusammengehörige Aktionen, z. B. mehrere neue
        // Inventarzeilen eines einzigen Wareneingangs.
        static std::wstring NewOperationId();

        // Protokolliert einen erfolgreich abgeschlossenen Vorgang.
        // Der Benutzer kann das Audit-Logging nicht über die GUI deaktivieren.
        static void Write(const StudyData& study, const AuditEntry& entry);
    };
}
