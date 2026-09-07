# Medikationsverwaltung – WinUI/C++ 

Die Anwendung verwendet **eine Excel-Datei pro Studie als eigentliche Datenhaltung**. Die Benutzeroberfläche liest, bearbeitet und exportiert diese Daten, ohne eine zusätzliche Datenbank einzuführen.

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



# v8.15 – AuditLog und Versionsanzeige

## AuditLog

- Neue zentrale Komponenten `AuditLogger.h` und `AuditLogger.cpp`.
- Fachliche Änderungen werden automatisch in einer lokalen monatlichen CSV-Datei protokolliert.
- Speicherort: `%LOCALAPPDATA%\Medikationsverwaltung\Audit`.
- Der Rechnername ist Bestandteil des Dateinamens.
- Jeder Eintrag enthält u. a. UTC-Zeitstempel, Vorgangs-ID, Windows-Benutzer, Computer, Programmversion, Studie, EUCT-No., IMP, Blatt, Excel-Zeile sowie alten und neuen Wert.
- Das AuditLog kann in der GUI nicht deaktiviert werden.
- Vor schreibenden Änderungen wird geprüft, ob der Audit-Speicher beschreibbar ist. Ist dies nicht möglich, wird die Änderung abgebrochen.
- Batch-Wareneingänge verwenden eine gemeinsame `OperationId` für zusammengehörige Inventarzeilen.
- DrugAccount-Exporte werden ebenfalls protokolliert.

## GUI

- Neue kleine Versionsanzeige in der Fußzeile der linken Navigation.
- Neue AuditLog-Karte unter **Einstellungen** mit Anzeige des Speicherortes und Button **„AuditLog-Ordner öffnen“**.

## Versionierung

- Neue zentrale Datei `AppVersion.h`.
- Aktuelle Produktversion: `1.1.0`.
- Die Versionsnummer wird sowohl in der GUI als auch im AuditLog verwendet.

## Hinweis zur Revisionssicherheit

Die lokale CSV-Lösung verbessert die Nachvollziehbarkeit erheblich, ist jedoch noch kein manipulationsgeschützter regulatorischer Audit Trail. Für einen revisionssicheren Produktivbetrieb wären zentraler geschützter Speicher, restriktive Berechtigungen und ggf. kryptografischer Manipulationsschutz erforderlich.


