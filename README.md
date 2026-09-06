# Medikationsverwaltung – WinUI/C++ v8.11

Die Anwendung verwendet weiterhin **eine Excel-Datei pro Studie als eigentliche Datenhaltung**. Die Benutzeroberfläche liest, bearbeitet und exportiert diese Daten, ohne eine zusätzliche Datenbank einzuführen.

## Code-Dokumentation

Die aktiven C++-, Header-, Ressourcen- und Build-Dateien sind mit Datei-, Abschnitts- und Architekturkommentaren versehen. Für einen schnellen Einstieg in die Zuständigkeiten der einzelnen Komponenten siehe [`CODE_UEBERSICHT.md`](CODE_UEBERSICHT.md).

Die Kommentierung beschreibt vor allem **Warum**, **Datenfluss** und **Verantwortung** eines Codeabschnitts. Offensichtliche einzelne C++-Anweisungen werden bewusst nicht zeilenweise kommentiert, damit die Kommentare nicht den eigentlichen Code überdecken.

## Studienordner

Unter **Einstellungen** wird der allgemeine Studien-Datenordner festgelegt. Darunter liegt pro Studie ein Unterordner, z. B.:

```text
Studien\
├── RUX-ECP\
│   ├── RUX_ECP_Bestellübersicht.xlsx
│   └── Documents\
└── SEQUENCE\
    ├── SEQUENCE_Bestellübersicht.xlsx
    └── Documents\
```

Der Unterordnername ist die eindeutige Studienidentität. Eine im ersten Tabellenblatt hinterlegte EUCT-No. wird in der Studienauswahl als `Studienname [EUCT-No.]` ergänzt.

## Navigation

Seitliches Menü:

- Übersicht
- Berichte
- Vorlagen & Dokumente
- Einstellungen

Studienbezogene Reiter:

- Übersicht
- Bestellungen
- Wareneingang / Bestand
- Patientenvisiten
- Berichte
- Notizen

Das Studienprofil bleibt ausschließlich in der Excel-Datei.

## Tabellen

Interaktive Tabellen unterstützen:

- Filtern pro Spalte über `▾`
- Sortieren auf-/absteigend
- kompakte Häkchen-Auswahl für bearbeitbare Zeilen
- gemeinsame ColumnDefinitions für Kopf und Datenzeilen
- proportionale Breiten bei normalen Tabellen
- horizontales Scrollen bei sehr breiten Tabellen

Die Tabellen-Schriftgröße wurde in v8.4 bewusst **nicht vergrößert**.

## Wareneingang / Bestand – v8.4

Der Reiter enthält nur noch zwei Arbeitsbereiche:

1. **Wareneingang erfassen**
2. **Inventardatensatz bearbeiten**

Bei einem Wareneingang werden angegeben:

- Lieferdatum
- Charge No.
- Verfallsdatum
- Anzahl Einheiten
- Box-/Kit-Nummern
- optional eine konkrete offene Bestellung

Die Box-/Kit-Nummern werden als **freie Textwerte** erfasst. Es findet keine automatische Nummerierung statt. Bei mehreren Einheiten wird eine Nummer pro Zeile eingegeben; alternativ kann dieselbe Nummer für alle Einheiten übernommen werden.

Die Zuordnung zu einer Bestellung ist optional. Wird eine offene Bestellung ausgewählt, wird ausschließlich diese Bestellzeile als erhalten markiert.

Nach dem Wareneingang erfolgt die weitere Dokumentation wie Dispensing, Patientenzuordnung, Rückgabe oder Vernichtung über **„Inventardatensatz bearbeiten“** durch Auswahl der betreffenden Tabellenzeile.

## DrugAccount-Berichte

DrugAccount-Berichte werden als **Excel-Dateien** erzeugt. Es gibt zwei Berichtstypen:

- `DrugAccount pro IMP – Gesamt`
- `DrugAccount pro Patient`

Die Standardvorlagen liegen unter:

- `Templates\DrugAccount_Gesamt.xlsx`
- `Templates\DrugAccount_Patient.xlsx`

Optional kann eine Studie eigene Vorlagen unter `Documents\DrugAccount` bzw. `Documents\DrugAccount\<IMP-ID>` verwenden.

## Exportpfade

Unter **Einstellungen → Exportpfade je Studie** kann für jede Studie ein eigener Ablageordner gesetzt werden. Ohne individuelle Einstellung werden Exporte im jeweiligen Studienordner gespeichert.

## Build

1. `Medikationsverwaltung_WinUI.sln` in Visual Studio öffnen.
2. `Debug | x64` auswählen.
3. NuGet-Wiederherstellung abwarten.
4. **Projektmappe bereinigen** und anschließend **Projektmappe neu erstellen**.
5. Desktop-Excel muss für Lesen und Schreiben der Studien-Dateien installiert sein.

Ein echter MSVC-/WinUI-/Excel-COM-Build kann in der Bereitstellungsumgebung dieses Pakets nicht ausgeführt werden. Die Projektdateien werden daher statisch geprüft; der abschließende Build erfolgt unter Windows in Visual Studio.

## Änderungen ab v8.5

- Bestellungen: Bearbeiten, Neuerfassung und Dokumente stehen dreispaltig nebeneinander.
- Wareneingang/Bestand: Wareneingang, Datensatzbearbeitung und Dokumente stehen dreispaltig nebeneinander.
- Offene Bestellungen werden ausschließlich über eine leere Zelle in `Erhalten am` bestimmt.

## Änderungen v8.6

v8.6 behebt die Erfassung mehrerer Box-/Kit-Nummern bei unterschiedlichen WinUI-Zeilenumbruchformaten und reduziert den Excel-COM-Overhead beim Laden und nach Speichervorgängen. Details siehe `CHANGELOG_v8_6.md`.

## Änderungen in v8.7

- Datumswerte werden beim Schreiben als COM-Datum an Excel übergeben.
- Tabellenköpfe bleiben beim vertikalen Scrollen sichtbar.
- Tabellenansichten sind niedriger, damit Eingabebereiche (insbesondere Wareneingang) ohne langes Seitenscrollen erreichbar bleiben.
- Tabellen-Schriftgröße wurde nicht verändert.
