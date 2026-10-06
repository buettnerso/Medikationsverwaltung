# Medikationsverwaltung – WinUI/C++ v8.16

Die Anwendung verwendet weiterhin **eine Excel-Datei pro Studie als eigentliche Datenhaltung**. Die Benutzeroberfläche liest, bearbeitet und exportiert diese Daten, ohne eine zusätzliche Datenbank einzuführen.

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



## UI-/Datenqualitätskorrekturen – v8.19

- Interaktive Tabellen passen ihre Spaltenbreiten jetzt an die tatsächlich verfügbare Fensterbreite an.
- Header und Datenzeilen verwenden weiterhin exakt dasselbe Spaltenmodell; lange Zellinhalte werden mit `…` gekürzt.
- Erst wenn die sinnvollen Mindestbreiten nicht mehr in das Fenster passen, wird horizontal gescrollt.
- Platzhalter-IMPs wie `0` oder Vorlagentexte wie `Namen der Prüfmedikation eintragen ...` werden zentral beim Einlesen herausgefiltert.
- Die kompakte Seitennavigation zeigt Icons statt abgeschnittener Menütexte; die Versionszeile wird im kompakten Zustand ausgeblendet.

## AuditLog – v8.16

Fachlich relevante Änderungen werden automatisch protokolliert. Standardmäßig liegt das AuditLog unter:

```text
%LOCALAPPDATA%\Medikationsverwaltung\Audit
```

Unter **Einstellungen → AuditLog** kann ein anderer lokaler oder zentraler Netzwerkpfad gewählt werden. Vor der Übernahme prüft die Anwendung den tatsächlichen Schreibzugriff. Das AuditLog kann nicht deaktiviert werden. Die monatlichen Dateinamen enthalten den Computernamen, sodass bei einem gemeinsamen Netzwerkordner getrennte Dateien pro PC geführt werden.

Die CSV enthält u. a. UTC-Zeitstempel, Vorgangs-ID, Windows-Benutzer, Computer, Programmversion, Studie, IMP, Excel-Zeile sowie alten und neuen Wert. Details siehe `AUDITLOG_ANLEITUNG.md`.

## Dokumentationstext für Ausgabe / Rückgabe – v8.18

Im studienbezogenen Reiter **Berichte** kann aus einer vorhandenen `DrugInventory`-Zeile ein vorformulierter Dokumentationstext für Medikamentenausgabe oder -rückgabe erzeugt werden. Patient, Ereignisdatum und die zum jeweiligen Vorgang passenden Inventarfelder stammen direkt aus Excel. Nur **Einnahmebeginn** bei Ausgabe bzw. **letzte Einnahme** bei Rückgabe wird manuell ergänzt. Liefer- und Vernichtungsdaten werden nicht in den Text übernommen; Rückgabemengen erscheinen nur bei Rückgabe. Die Original-Spaltenbezeichnungen der Excel werden beibehalten. Der Text ist read-only und kann in die Zwischenablage kopiert werden. Details siehe `MEDIKATIONSDOKUMENTATION_ANLEITUNG.md`.

Die aktuelle Produktversion **1.8.19** wird zentral in `AppVersion.h` gepflegt, in der Navigations-Fußzeile angezeigt, in das AuditLog geschrieben und mit Inno Setup synchronisiert.
