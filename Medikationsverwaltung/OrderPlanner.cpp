// ============================================================================
// Datei: OrderPlanner.cpp
// Zweck: Berechnet aus Bestand, Mindestbestand, Lieferzeit und geplanten Visiten den nächsten Bestellzeitraum.
//
// Verantwortlichkeiten:
// - Berücksichtigt offene Bestellungen und optional prognostizierte zukünftige Visiten.
// - Enthält ausschließlich Geschäftslogik und keinen Excel- oder GUI-Code.
//
// Hinweis: Kommentare erläutern Architektur und nicht offensichtliche Logik.
// Triviale Sprachkonstrukte werden bewusst nicht zeilenweise kommentiert.
// ============================================================================
#include "pch.h"
#include "OrderPlanner.h"
#include "DateUtils.h"

#include <algorithm>
#include <cmath>

namespace
{
    // -------------------------------------------------------------------------
    // Interne Hilfsfunktionen
    // -------------------------------------------------------------------------
    // VisitMatches erlaubt IMP-spezifische Visitenfilter. Priority definiert
    // lediglich die Reihenfolge, wenn ein Gesamtstatus für eine Studie benötigt wird.
    bool VisitMatches(const med::VisitRecord& visit, const std::wstring& filter)
    {
        if (med::date::Trim(filter).empty()) return true;
        const auto needle = med::date::Normalize(filter);
        for (const auto& [key, value] : visit.metadata)
        {
            const auto combined = med::date::Normalize(key + L" " + value);
            if (combined.find(needle) != std::wstring::npos) return true;
        }
        return false;
    }

    int Priority(med::OrderState state)
    {
        switch (state)
        {
        case med::OrderState::Due: return 0;
        case med::OrderState::Soon: return 1;
        case med::OrderState::Pending: return 2;
        case med::OrderState::Ok: return 3;
        case med::OrderState::NotRequired: return 4;
        default: return 5;
        }
    }
}

namespace med
{
    OrderPlan OrderPlanner::Calculate(const StudyData& study, const ImpData& imp, Day today)
    {
        OrderPlan plan;

        if (!imp.orderRequired)
        {
            plan.state = OrderState::NotRequired;
            plan.explanation = L"Dieses Produkt ist als nicht bestellpflichtig gekennzeichnet (z. B. Handelsware). Verabreichungen können dokumentiert werden, eine Studienbestellung wird jedoch nicht geplant.";
            return plan;
        }

        std::vector<Day> allMatchingDates;
        std::vector<Day> futureDates;
        allMatchingDates.reserve(study.visits.size());
        futureDates.reserve(study.visits.size());
        for (const auto& visit : study.visits)
        {
            if (!visit.hasDate || !VisitMatches(visit, imp.visitFilter)) continue;
            allMatchingDates.push_back(visit.date);
            if (visit.date >= today) futureDates.push_back(visit.date);
        }
        std::sort(allMatchingDates.begin(), allMatchingDates.end());
        std::sort(futureDates.begin(), futureDates.end());

        plan.futureVisits = static_cast<int>(futureDates.size());
        const auto leadEnd = today + std::chrono::days{ std::max(0, imp.leadTimeDays) };
        plan.visitsDuringLeadTime = static_cast<int>(std::count_if(futureDates.begin(), futureDates.end(), [&](Day d)
        {
            return d <= leadEnd;
        }));

        if (imp.pendingOrder)
        {
            plan.state = OrderState::Pending;
            plan.explanation = L"Für dieses IMP ist bereits eine Bestellung erfasst und noch kein Wareneingang dokumentiert.";
            return plan;
        }

        if (imp.stock <= imp.minimumStock)
        {
            plan.state = OrderState::Due;
            plan.recommendedFrom = today;
            plan.latestOrderDate = today;
            plan.explanation = L"Der aktuelle Bestand liegt am oder unter dem hinterlegten Mindestbestand.";
            return plan;
        }

        const double perVisit = std::max(0.000001, imp.consumptionPerVisit);
        const double consumableAboveMinimum = std::max(0.0, imp.stock - imp.minimumStock);
        const int visitsBeforeMinimum = static_cast<int>(std::floor(consumableAboveMinimum / perVisit));

        // Für Status OK soll nach Möglichkeit trotzdem ein zukünftiges Bestellfenster sichtbar sein.
        // Reichen die bereits terminierten Visiten nicht bis zum kritischen Verbrauch, wird nur
        // dann extrapoliert, wenn ein eigenes Prognoseintervall hinterlegt ist oder aus den
        // bisherigen passenden Verbrauchsterminen ein positives typisches Intervall ableitbar ist.
        if (visitsBeforeMinimum >= static_cast<int>(futureDates.size()))
        {
            int interval = std::max(0, imp.forecastIntervalDays);
            if (interval <= 0 && allMatchingDates.size() >= 2)
            {
                std::vector<int> deltas;
                for (size_t i = 1; i < allMatchingDates.size(); ++i)
                {
                    const auto delta = static_cast<int>((allMatchingDates[i] - allMatchingDates[i - 1]).count());
                    if (delta > 0) deltas.push_back(delta);
                }
                if (!deltas.empty())
                {
                    std::sort(deltas.begin(), deltas.end());
                    interval = deltas[deltas.size() / 2];
                }
            }

            Day anchor{};
            bool hasAnchor = interval > 0;
            if (hasAnchor && !futureDates.empty())
            {
                anchor = futureDates.back();
            }
            else if (hasAnchor && !allMatchingDates.empty())
            {
                anchor = allMatchingDates.back();
                while (anchor <= today) anchor += std::chrono::days{ interval };
                // Der erste hochgerechnete Termin ist bereits der nächste Verbrauchstermin.
                futureDates.push_back(anchor);
                ++plan.projectedVisits;
            }
            else
            {
                hasAnchor = false;
            }

            if (hasAnchor)
            {
                const int needed = visitsBeforeMinimum + 1;
                while (static_cast<int>(futureDates.size()) < needed && plan.projectedVisits < 2000)
                {
                    anchor += std::chrono::days{ interval };
                    futureDates.push_back(anchor);
                    ++plan.projectedVisits;
                }
                if (plan.projectedVisits > 0) plan.estimated = true;
            }
        }

        if (futureDates.empty() || visitsBeforeMinimum >= static_cast<int>(futureDates.size()))
        {
            plan.state = OrderState::Ok;
            plan.explanation = L"Für dieses IMP kann aktuell kein belastbares nächstes Bestellfenster berechnet werden, weil keine ausreichenden zukünftigen Visiten bzw. kein verwertbares Intervall vorliegen.";
            return plan;
        }

        const Day critical = futureDates[static_cast<size_t>(visitsBeforeMinimum)];
        const Day latest = critical - std::chrono::days{ std::max(0, imp.leadTimeDays) };
        const Day from = latest - std::chrono::days{ std::max(0, imp.bufferDays) };
        plan.criticalVisitDate = critical;
        plan.latestOrderDate = latest;
        plan.recommendedFrom = from;

        if (today >= latest)
        {
            plan.state = OrderState::Due;
            plan.explanation = L"Unter Berücksichtigung von Mindestbestand, Verbrauch, terminierten bzw. prognostizierten Visiten und Lieferzeit sollte die Bestellung jetzt ausgelöst werden.";
        }
        else if (today >= from)
        {
            plan.state = OrderState::Soon;
            plan.explanation = L"Das berechnete Bestellfenster ist erreicht.";
        }
        else
        {
            plan.state = OrderState::Ok;
            plan.explanation = plan.estimated
                ? L"Der nächste Bestellzeitpunkt liegt noch in der Zukunft. Ein Teil der Berechnung ist als Prognose aus dem hinterlegten Visitenintervall abgeleitet."
                : L"Der berechnete Bestellzeitpunkt liegt noch in der Zukunft.";
        }
        return plan;
    }

    OrderPlan OrderPlanner::Calculate(const StudyData& study, Day today)
    {
        if (!study.imps.empty())
        {
            std::optional<OrderPlan> bestPlan;
            for (const auto& imp : study.imps)
            {
                const auto candidate = Calculate(study, imp, today);
                if (!bestPlan || Priority(candidate.state) < Priority(bestPlan->state)) bestPlan = candidate;
            }
            if (bestPlan) return *bestPlan;
        }

        ImpData legacy;
        legacy.name = study.medication;
        legacy.stock = study.stock;
        legacy.minimumStock = study.minimumStock;
        legacy.leadTimeDays = study.leadTimeDays;
        legacy.bufferDays = study.bufferDays;
        legacy.consumptionPerVisit = study.config.consumptionPerVisit;
        legacy.pendingOrder = study.pendingOrder;
        legacy.pendingOrderDate = study.pendingOrderDate;
        return Calculate(study, legacy, today);
    }

    /// Trennt den internen Enum-Wert von seiner deutschsprachigen UI-Darstellung.
    std::wstring OrderPlanner::StateText(OrderState state)
    {
        switch (state)
        {
        case OrderState::Due: return L"BESTELLEN";
        case OrderState::Soon: return L"Bald fällig";
        case OrderState::Pending: return L"Bestellt / Eingang offen";
        case OrderState::Ok: return L"OK";
        case OrderState::NotRequired: return L"Keine Studienbestellung";
        default: return L"Unklar";
        }
    }
}
