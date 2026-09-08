#include "pch.h"
#include "MedicationDocumentation.h"
#include "DateUtils.h"

namespace
{
    using med::MedicationDocumentationType;

    std::wstring SourceValueOrPlaceholder(const std::wstring& value)
    {
        return value.empty() ? L"[in DrugInventory nicht befüllt]" : value;
    }

    std::wstring MedicationValueOrPlaceholder(const std::wstring& value)
    {
        return value.empty() ? L"[im Studienprofil nicht befüllt]" : value;
    }

    std::wstring ManualValueOrPlaceholder(const std::wstring& value)
    {
        return value.empty() ? L"[bitte ergänzen]" : value;
    }

    std::wstring CountOrOne(const std::wstring& value)
    {
        return value.empty() ? L"1" : value;
    }

    bool Contains(const std::wstring& value, const wchar_t* token)
    {
        return value.find(token) != std::wstring::npos;
    }

    bool IsPatientField(const std::wstring& n)
    {
        return n == L"patid" || n == L"patnr" ||
            n == L"patient" || n == L"patientid" ||
            n == L"patientenid" || n == L"patientennr" ||
            n == L"patientennummer";
    }

    bool IsMedicationField(const std::wstring& n)
    {
        return n == L"imp" || n == L"produkt" || n == L"product" ||
            n == L"medikation" || n == L"medication" ||
            n == L"studienmedikation" || n == L"drug";
    }

    bool IsDeliveryField(const std::wstring& n)
    {
        return Contains(n, L"deliverydate") || Contains(n, L"lieferdatum") ||
            Contains(n, L"lieferdate") || Contains(n, L"wareneingang") ||
            Contains(n, L"eingangsdatum");
    }

    bool IsDestructionField(const std::wstring& n)
    {
        return Contains(n, L"vernichtung") || Contains(n, L"destruction") ||
            Contains(n, L"destroyed") || Contains(n, L"destroydate");
    }

    bool IsDispensingDateField(const std::wstring& n)
    {
        const bool dispensingWord =
            Contains(n, L"dispensing") || Contains(n, L"dispensed") ||
            Contains(n, L"ausgabe") || Contains(n, L"ausgegeben");

        const bool dateWord =
            Contains(n, L"date") || Contains(n, L"datum") ||
            n.ends_with(L"am") || n == L"dispensed";

        return dispensingWord && dateWord;
    }

    bool IsReturnDateField(const std::wstring& n)
    {
        const bool returnWord =
            Contains(n, L"returned") || Contains(n, L"returndate") ||
            Contains(n, L"returnam") || Contains(n, L"zuruckgegeben") ||
            Contains(n, L"zurueckgegeben") || Contains(n, L"zurückgegeben") ||
            Contains(n, L"zurckgegeben") || Contains(n, L"ruckgabe") ||
            Contains(n, L"rueckgabe") || Contains(n, L"rückgabe") ||
            Contains(n, L"rckgabe");

        const bool dateWord =
            Contains(n, L"date") || Contains(n, L"datum") ||
            n.ends_with(L"am") || n == L"returned";

        return returnWord && dateWord;
    }

    bool IsReturnSpecificField(const std::wstring& n)
    {
        return Contains(n, L"unused") || Contains(n, L"remaining") ||
            Contains(n, L"restmenge") || Contains(n, L"uebrig") ||
            Contains(n, L"übrig") || Contains(n, L"brig") ||
            Contains(n, L"zuruckgegeben") || Contains(n, L"zurueckgegeben") ||
            Contains(n, L"zurückgegeben") || Contains(n, L"zurckgegeben") ||
            Contains(n, L"rueckgabe") || Contains(n, L"rückgabe") ||
            Contains(n, L"rckgabe") || Contains(n, L"return");
    }

    void AppendInventoryFields(
        std::wstring& text,
        MedicationDocumentationType type,
        const std::vector<med::MedicationDocumentationField>& fields)
    {
        for (const auto& field : fields)
        {
            if (field.header.empty() || field.value.empty())
                continue;

            if (!med::ShouldIncludeMedicationDocumentationField(type, field.header))
                continue;

            text += L" | " + field.header + L": " + field.value;
        }
    }
}

namespace med
{
    bool ShouldIncludeMedicationDocumentationField(
        MedicationDocumentationType type,
        const std::wstring& header)
    {
        const auto n = date::Normalize(header);
        if (n.empty())
            return false;

        // Diese Felder sind fachlich entweder bereits im Einleitungssatz
        // enthalten oder für Ausgabe/Rückgabe nicht sinnvoll.
        if (IsPatientField(n) || IsMedicationField(n) ||
            IsDeliveryField(n) || IsDestructionField(n) ||
            IsDispensingDateField(n) || IsReturnDateField(n))
        {
            return false;
        }

        // Angaben wie Restmenge / "unused" / Rückgabedetails gehören nur in
        // einen Rückgabetext und dürfen bei der Medikamentenausgabe nicht
        // erscheinen.
        if (type == MedicationDocumentationType::Dispensing && IsReturnSpecificField(n))
            return false;

        return true;
    }

    std::wstring BuildMedicationDocumentationText(
        MedicationDocumentationType type,
        const MedicationDocumentationData& data)
    {
        const auto patient = SourceValueOrPlaceholder(data.patientId);
        const auto medication = MedicationValueOrPlaceholder(data.medicationName);
        const auto eventDate = SourceValueOrPlaceholder(data.eventDate);
        const auto therapyDate = ManualValueOrPlaceholder(data.therapyDate);
        const auto packageCount = CountOrOne(data.packageCount);

        std::wstring text;

        if (type == MedicationDocumentationType::Return)
        {
            text =
                L"Pat.ID " + patient + L" hat am " + eventDate + L" " + packageCount +
                L" Packung(en) der Studienmedikation (" + medication +
                L") zurückgegeben. | Letzte Einnahme am: " + therapyDate;
        }
        else
        {
            text =
                L"Studienpatient " + patient + L" hat am " + eventDate + L" " + packageCount +
                L" Packung(en) der Studienmedikation (" + medication +
                L") erhalten. | Einnahmebeginn am: " + therapyDate;
        }

        AppendInventoryFields(text, type, data.inventoryFields);
        return text;
    }
}
