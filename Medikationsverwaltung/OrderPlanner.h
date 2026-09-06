// ============================================================================
// Datei: OrderPlanner.h
// Zweck: Deklariert die Geschäftslogik zur Ermittlung von Bestellstatus und Bestellfenstern.
//
// Verantwortlichkeiten:
// - Berechnet Bestellbedarf entweder für eine konkrete IMP-Konfiguration oder für Legacy-Ein-IMP-Studien.
//
// Hinweis: Kommentare erläutern Architektur und nicht offensichtliche Logik.
// Triviale Sprachkonstrukte werden bewusst nicht zeilenweise kommentiert.
// ============================================================================
#pragma once

#include "Models.h"

namespace med
{
    /// Reine Geschäftslogik für die Bestellprognose.
    /// Die Klasse greift weder auf Excel noch auf WinUI zu und ist dadurch separat testbar.
    class OrderPlanner
    {
    public:
        /// Legacy-Variante: berechnet den Plan für das erste/primäre IMP einer Studie.
        static OrderPlan Calculate(const StudyData& study, Day today);
        /// Berechnet Status und Bestellfenster für genau ein IMP zum angegebenen Stichtag.
        static OrderPlan Calculate(const StudyData& study, const ImpData& imp, Day today);
        /// Liefert die deutschsprachige Anzeige des internen Bestellstatus.
        static std::wstring StateText(OrderState state);
    };
}
