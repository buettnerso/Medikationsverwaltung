# Code-Übersicht – Medikationsverwaltung

Diese Datei ergänzt die Kommentare im Quellcode und dient als Einstieg für Personen, die das Projekt erstmals öffnen.

## Architektur in einem Satz

Die Anwendung ist eine **WinUI-3/C++20-Desktopoberfläche über studienspezifischen Excel-Arbeitsmappen**. Excel bleibt die Datenhaltung; C++ übernimmt Erkennung, Verarbeitung, Bestellplanung, Eingabe und Berichtsexport.

## Datenfluss

```text
Studienordner
    ↓
ExcelStudyRepository
    ↓
StudyData / ImpData / SheetTable
    ├── OrderPlanner      → Bestellstatus und Bestellfenster
    ├── MainWindow        → Anzeige und Benutzereingaben
    └── ReportService     → Excel-Berichte

Schreibende GUI-Aktion
    ↓
ExcelStudyRepository
    ├── prüft externe Dateiänderungen
    ├── erzeugt Backup
    ├── schreibt gezielt in Excel
    └── lädt die geänderte Studie erneut
```

## Dateien und Verantwortlichkeiten

| Datei | Aufgabe |
|---|---|
| `main.cpp` | Windows-Einstiegspunkt und Start von C++/WinRT/WinUI |
| `App.h/.cpp` | WinUI-Anwendungslebenszyklus und Erzeugung des Hauptfensters |
| `MainWindow.h/.cpp` | Gesamte programmgesteuerte GUI, Navigation, Tabellenzustand und Eventhandler |
| `Models.h` | Gemeinsame Domänenmodelle für Studien, IMPs, Tabellen, Visiten und Bestellplanung |
| `DateUtils.h/.cpp` | Datumskonvertierung, Excel-Serienwerte und Textnormalisierung |
| `Settings.h/.cpp` | Globaler Studienordner, Exportpfade und optionale `study.config.ini` |
| `OrderPlanner.h/.cpp` | Reine Geschäftslogik zur Bestellprognose |
| `ExcelCom.h/.cpp` | Technische COM-Schicht für `Excel.Application`, Workbook und Zellzugriff |
| `ExcelStudyRepository.h/.cpp` | Übersetzt zwischen Studien-Excel und internem Studienmodell; einzige zentrale Persistenzschicht |
| `ReportService.h/.cpp` | Erzeugt DrugAccount- und Übersichtsberichte aus dem geladenen Modell |
| `UiHelpers.h/.cpp` | Kleine wiederverwendbare WinUI-Darstellungshelfer |
| `pch.h/.cpp` | Precompiled Header mit Windows-/WinUI-Abhängigkeiten |
| `AppIcon.rc`, `resource.h` | Native Einbettung des Anwendungsicons |
| `app.manifest` | DPI-, Betriebssystem- und Berechtigungseinstellungen |
| `Medikationsverwaltung.vcxproj` | Build-Konfiguration, Pakete, Bibliotheken, Ressourcen und Templates |

## Zentrale Modelle

### `StudyData`
Repräsentiert eine vollständig geladene Studie. Enthält u. a.:

- Pfad und Änderungszeit der Excel-Datei,
- Studienname und EUCT-Nummer,
- alle `ImpData`-Einträge,
- eingelesene Bestell-, Inventar- und Visiteninformationen,
- Dokumente/Vorlagen,
- Legacy-Felder für ältere Ein-IMP-Dateien.

### `ImpData`
Repräsentiert ein einzelnes IMP oder begleitendes Produkt innerhalb einer Studie. Dazu gehören z. B.:

- Bezeichnung und interne ID,
- Studienware/Handelsware,
- Bestellpflicht,
- Mindestbestand und Lieferzeit,
- Bestell- und Inventarblatt/-spalte,
- aktueller Bestand und Bestellplan.

### `SheetTable`
Ist die Excel-unabhängige Tabellenrepräsentation. Wichtig ist `rowNumbers`: Diese Liste behält die **ursprüngliche Excel-Zeilennummer**, selbst wenn die GUI filtert oder sortiert. Dadurch wird beim Bearbeiten immer die richtige Quellzeile geändert.

## Schreibschutz und Backups

Schreibende Änderungen laufen zentral über `ExcelStudyRepository`.

Vor einer Änderung:

1. `CheckUnchanged()` vergleicht den aktuellen Dateizeitstempel mit dem Zeitpunkt des Einlesens.
2. Wurde die Datei extern geändert, wird der Schreibvorgang abgebrochen statt fremde Änderungen zu überschreiben.
3. `CreateBackup()` erzeugt eine Sicherungskopie.
4. Erst danach wird Excel schreibend geöffnet.

## Tabellen in der GUI

`MainWindow::BuildInteractiveTable()` ist die zentrale Tabellenkomponente. Sie übernimmt:

- Filter pro Spalte,
- auf-/absteigende Sortierung,
- optionale Zeilenauswahl,
- Erhalt der ursprünglichen Excel-Zeilenschlüssel,
- gespeicherte horizontale/vertikale Scrollposition,
- fixierten Tabellenkopf,
- gemeinsame Breitenberechnung für Kopf und Datenkörper.

Wenn eine Tabellenfunktion geändert werden soll, sollte zuerst geprüft werden, ob die Änderung hier zentral statt in einem einzelnen Reiter umgesetzt werden kann.

## Zuständigkeit der Studienreiter in `MainWindow.cpp`

| Funktion | Ansicht |
|---|---|
| `BuildStudyOverview()` | Übersicht der ausgewählten Studie/IMPs |
| `BuildOrders()` | Bestellungen anzeigen, erfassen und bearbeiten |
| `BuildInventory()` | Wareneingang und Inventardatensätze |
| `BuildVisits()` | Patienten und Visiten |
| `BuildReports()` | DrugAccount-Berichte |
| `BuildNotes()` | Freie Studiennotizen |
| `BuildDocumentQuickLinks()` | Kontextbezogene Dokumente/Vorlagen |

## Regel für Änderungen am Projekt

- **GUI-Layout:** `MainWindow.cpp` bzw. `UiHelpers.cpp`
- **Excel lesen/schreiben:** `ExcelStudyRepository.cpp`
- **niedriges COM-/Excel-Detail:** `ExcelCom.cpp`
- **Bestellberechnung:** `OrderPlanner.cpp`
- **Berichtsinhalte:** `ReportService.cpp`
- **Datenmodell:** `Models.h`

Fachlogik sollte nicht direkt in UI-Eventhandler oder COM-Hilfsfunktionen verschoben werden. Dadurch bleibt die Trennung zwischen Oberfläche, Geschäftslogik und Persistenz nachvollziehbar.
