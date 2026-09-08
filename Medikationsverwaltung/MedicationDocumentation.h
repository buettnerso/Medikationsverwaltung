#pragma once

#include <string>
#include <vector>

namespace med
{
    enum class MedicationDocumentationType
    {
        Dispensing,
        Return
    };

    // Ein Feld des ausgewählten DrugInventory-Datensatzes. Der Header wird
    // bewusst unverändert aus Excel übernommen, damit der Dokumentationstext
    // dieselbe Terminologie wie die jeweilige Studienvorlage verwendet.
    struct MedicationDocumentationField
    {
        std::wstring header;
        std::wstring value;
    };

    struct MedicationDocumentationData
    {
        std::wstring patientId;
        std::wstring medicationName;
        std::wstring eventDate;

        // Einzige fachliche Angabe, die im Generator manuell ergänzt wird:
        // Einnahmebeginn bei Ausgabe bzw. letzte Einnahme bei Rückgabe.
        std::wstring therapyDate;

        // Ein DrugInventory-Datensatz entspricht im aktuellen Inventarmodell
        // einer einzelnen Packung / einem einzelnen Kit.
        std::wstring packageCount{ L"1" };

        // Alle BEFÜLLTEN Spalten des ausgewählten DrugInventory-Datensatzes
        // in Originalreihenfolge und mit Original-Excel-Header.
        std::vector<MedicationDocumentationField> inventoryFields;
    };

    // Legt fest, ob eine befüllte DrugInventory-Spalte für den gewählten
    // Dokumentationsvorgang als zusätzliches Feld ausgegeben werden soll.
    // Felder, die bereits im Einleitungssatz verwendet werden (Patient und
    // Ereignisdatum), sowie Liefer- und Vernichtungsdaten werden nicht doppelt
    // ausgegeben. Rückgabefelder erscheinen nur bei Rückgabe.
    bool ShouldIncludeMedicationDocumentationField(
        MedicationDocumentationType type,
        const std::wstring& header);

    // Erzeugt einen kopierbaren Dokumentationstext. Alle fachlichen Details
    // stammen aus DrugInventory bzw. dem Excel-basierten IMP-Profil; lediglich
    // Einnahmebeginn / letzte Einnahme wird manuell ergänzt.
    std::wstring BuildMedicationDocumentationText(
        MedicationDocumentationType type,
        const MedicationDocumentationData& data);
}
