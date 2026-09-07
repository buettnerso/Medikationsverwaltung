# Changelog – Medikationsverwaltung

Alle wesentlichen Änderungen des Projekts werden in dieser Datei dokumentiert.

Die Einträge wurden aus den bisherigen Einzel-Changelogs der Entwicklungsstände **v7 bis v8.11** zusammengeführt und vereinheitlicht.  
Einige Funktionen wurden in späteren Versionen erneut angepasst oder ersetzt. In diesen Fällen bleibt die ursprüngliche Änderung aus Gründen der Nachvollziehbarkeit erhalten und die spätere Version beschreibt den aktuelleren Stand.

\---



# \[v8.15] – AuditLog und Versionsanzeige

## AuditLog

* Neue zentrale Komponenten `AuditLogger.h` und `AuditLogger.cpp`.
* Fachliche Änderungen werden automatisch in einer lokalen monatlichen CSV-Datei protokolliert.
* Speicherort: `%LOCALAPPDATA%\\\\\\\\Medikationsverwaltung\\\\\\\\Audit`.
* Der Rechnername ist Bestandteil des Dateinamens.
* Jeder Eintrag enthält u. a. UTC-Zeitstempel, Vorgangs-ID, Windows-Benutzer, Computer, Programmversion, Studie, EUCT-No., IMP, Blatt, Excel-Zeile sowie alten und neuen Wert.
* Das AuditLog kann in der GUI nicht deaktiviert werden.
* Vor schreibenden Änderungen wird geprüft, ob der Audit-Speicher beschreibbar ist. Ist dies nicht möglich, wird die Änderung abgebrochen.
* Batch-Wareneingänge verwenden eine gemeinsame `OperationId` für zusammengehörige Inventarzeilen.
* DrugAccount-Exporte werden ebenfalls protokolliert.

## GUI

* Neue kleine Versionsanzeige in der Fußzeile der linken Navigation.
* Neue AuditLog-Karte unter **Einstellungen** mit Anzeige des Speicherortes und Button **„AuditLog-Ordner öffnen“**.

## Versionierung

* Neue zentrale Datei `AppVersion.h`.
* Aktuelle Produktversion: `1.1.0`.
* Die Versionsnummer wird sowohl in der GUI als auch im AuditLog verwendet.

## Hinweis zur Revisionssicherheit

Die lokale CSV-Lösung verbessert die Nachvollziehbarkeit erheblich, ist jedoch noch kein manipulationsgeschützter regulatorischer Audit Trail. Für einen revisionssicheren Produktivbetrieb wären zentraler geschützter Speicher, restriktive Berechtigungen und ggf. kryptografischer Manipulationsschutz erforderlich.



# Medikationsverwaltung v8.14

## Lesbarkeit / Schriftgrößen

Die Schriftgrößen wurden bewusst nur moderat erhöht, damit die Anwendung auf
kleineren Displays besser lesbar ist, ohne das responsive Layout erneut zu
verändern.

### Geändert

- Tabelleninhalt: `12 -> 13`
- Tabellenkopf: explizit `13`, zusätzlich `SemiBold`
- Kleine Sekundär- und Hinweistexte: `11 -> 12`
- Formularbeschriftungen: `12 -> 13`
- Tabellenzellen erhalten wegen der größeren Schrift minimal mehr vertikales Padding.
- Die Höhe der Tabellenkopfzeile wurde geringfügig angepasst.

### Unverändert

- Hauptüberschriften und größere Titel bleiben unverändert.
- Responsive Fensterlogik aus v8.12 bleibt erhalten.
- Card-Table-Optik aus v8.13 bleibt erhalten.
- Tabellenbreiten, Mindestbreiten und horizontales Scrollen bleiben unverändert.
- Fixierte Tabellenköpfe bleiben erhalten.
- Tabellenhöhe von ca. 510 px bleibt erhalten.
- Filter, Sortierung, Auswahl und gespeicherte Scrollpositionen bleiben erhalten.



# \[v8.13] Medikationsverwaltung

## Tabellenoptik

* Interaktive Tabellen werden jetzt als zusammenhängende **Card Tables** dargestellt.
* Tabellenkopf und Tabellenkörper erhalten einen gemeinsamen, dezenten Außenrahmen.
* Die oberen Ecken des Tabellenkopfs und die unteren Ecken des Tabellenkörpers sind abgerundet.
* Die bisherige zusätzliche äußere `ui::Card`-Kapselung wurde an den betreffenden Tabellen entfernt, damit kein doppelter Rahmen entsteht.
* Kopfzellen verwenden eine ruhige helle Hintergrundfläche statt einzelner stark abgegrenzter Rechtecke.
* Gitternetzlinien wurden aufgehellt und treten visuell stärker in den Hintergrund.
* Alternierende Datenzeilen erhalten einen sehr dezenten Hintergrund zur besseren Zeilenführung.
* Ausgewählte Zeilen bleiben dezent blau hervorgehoben.
* Zell-Padding wurde leicht angepasst, ohne die Schriftgröße zu verändern.

## Tabellenverhalten

Unverändert bleiben:

* responsive Tabellenbreite,
* identische Spaltenbreiten von Kopf und Datenkörper,
* fixierte Kopfzeile beim vertikalen Scrollen,
* horizontales Scrollen bei zu geringer Fensterbreite,
* Tabellenhöhe von maximal ca. 510 px,
* Filter, Sortierung und Zeilenauswahl,
* Wiederherstellung der horizontalen und vertikalen Scrollposition.

## Nächste geplante Visiten

Die Übersicht zeigt pro zukünftiger Visite nur noch:

1. Datum
2. Patient
3. Visitenbezeichnung
4. Visitenzusatz

Weitere Metadaten werden in dieser kompakten Übersicht nicht mehr angehängt.



# v8.12 – Responsive Oberfläche für kleinere Displays

## Fenstergröße
- Die Anwendung startet nicht mehr fest mit 1500 × 900 Pixeln.
- Die Startgröße orientiert sich an ca. 980 × 680 logischen Pixeln und damit an einem kompakten Querformat.
- Bei kleineren Displays wird die Startgröße automatisch auf maximal rund 90 % des verfügbaren Arbeitsbereichs begrenzt.
- Das Fenster kann weiterhin frei vergrößert und maximiert werden.
- Die Schriftgröße wird nicht dynamisch verkleinert oder vergrößert.

## Navigation
- Die linke Navigation verwendet jetzt den adaptiven `NavigationViewPaneDisplayMode::Auto`.
- Auf großen Fenstern ist die Navigation vollständig geöffnet.
- Bei mittleren Breiten wechselt sie in die kompakte Darstellung.
- Auf kleinen Fenstern wird sie als Overlay/Hamburger-Menü dargestellt, damit mehr Breite für den Arbeitsbereich bleibt.

## Responsive Arbeitsbereiche
- Die drei Arbeitskarten in Bestellungen, Wareneingang/Bestand und Patientenvisiten werden abhängig von der verfügbaren Breite angeordnet:
  - groß: 3 Spalten,
  - mittel: 2 Spalten,
  - klein: 1 Spalte.
- Die Karten werden nicht skaliert; nur ihre Position im Layout ändert sich.
- Die beiden Karten in der Studienübersicht wechseln auf kleinen Fenstern von zwei Spalten auf eine vertikale Anordnung.

## Studienauswahl
- Studienauswahl und Schnellaktionen reagieren auf die verfügbare Breite.
- Auf breiten Fenstern stehen Aktionen rechts neben der Studienauswahl.
- Auf schmaleren Fenstern wechseln die Aktionen in eine zweite Zeile.
- Die Aktionsleiste kann bei sehr kleinen Breiten horizontal scrollen, anstatt Buttons oder Texte zusammenzudrücken.

## Tabellen
- Das bestehende Tabellenprinzip bleibt erhalten:
  - sinnvolle Mindestbreiten pro Spalte,
  - keine Skalierung der Tabellen-Schrift,
  - horizontales Scrollen, sobald die Mindestbreiten nicht mehr in das Fenster passen,
  - fixierte Spaltenüberschrift,
  - gespeicherte Scrollposition.
- Die Tabellenhöhe bleibt auf dem größeren Stand aus v8.9.

## Ziel
Die Oberfläche soll auf kleinen Notebooks und unterschiedlichen Windows-Skalierungsfaktoren stabil lesbar bleiben. Statt alle Elemente proportional zu verkleinern, werden Bereiche umgebrochen und Tabellen bei Bedarf gescrollt.






## \[v8.11] – App-Icon zuverlässig eingebettet

### Geändert

* Das App-Icon ist nicht mehr von einer zur Laufzeit kopierten `.ico`-Datei abhängig.
* `Medikationsverwaltung\\\\\\\_AppIcon.ico` wird über `AppIcon.rc` direkt in die ausführbare Datei eingebettet.
* Das Fenster erhält das Icon zusätzlich über `WM\\\\\\\_SETICON` für große und kleine Darstellung.

### Ergebnis

* Das Anwendungssymbol kann dadurch in folgenden Bereichen verwendet werden:

  * Titelleiste
  * Taskleiste
  * Alt+Tab
  * ausführbare `.exe`

### Hinweis

* Bei bereits angehefteten Taskleisten-Verknüpfungen kann Windows ein zuvor gecachtes Symbol anzeigen. In diesem Fall muss die Verknüpfung einmal gelöst und anschließend neu angeheftet werden.

\---

## \[v8.10] – Stabilisierung des Batch-Wareneingangs

### Behoben

* Absturz bei aktivierter Option **„Gleiche Box-/Kit-Nr. für alle Einheiten“** behoben.
* Ursache war die Verwendung von `std::vector::assign(count, identifiers.front())`, wobei der Zuweisungswert als Referenz auf ein Element desselben Vektors verwendet wurde.
* Die gemeinsame Box-/Kit-Kennzeichnung wird nun zunächst in eine eigene `std::wstring` kopiert und anschließend für alle Einheiten vervielfältigt.

### Unverändert

* Alphanumerische Kennzeichnungen und Sonderzeichen bleiben unverändert als Text erhalten.
* Tabellenlayout, Scrollverhalten und Excel-Schema entsprechen weiterhin v8.9.

\---

## \[v8.9] – Tabellenlayout und Scrollposition

### Geändert

* Spaltenkopf und Datenkörper verwenden ein gemeinsames Pixel-Breitenmodell.
* Beide Grids erhalten bei jeder Fensterbreite exakt dieselben Spaltenbreiten.
* Die Tabelle nutzt die verfügbare Fensterbreite vollständig aus.
* Erst wenn die Mindestbreiten nicht mehr in das Fenster passen, wird horizontal gescrollt.
* Die Kopfzeile bleibt beim vertikalen Scrollen fixiert.
* Die Tabellenhöhe wurde wieder auf den größeren Stand vor v8.7 zurückgesetzt:

  * Datenkörper maximal ca. `510 px`

### Bedienbarkeit

* Vertikale und horizontale Scrollposition werden für jede Tabelle gespeichert und nach einem Neuaufbau wiederhergestellt.
* Auch die vertikale Scrollposition der gesamten Übersichtsseite wird bei Refresh- und Speichervorgängen wiederhergestellt.
* Dadurch springt die Ansicht nach einer Zeilenauswahl oder einem Speichervorgang nicht mehr automatisch zum Tabellen- bzw. Seitenanfang.

### Technischer Hinweis

* Der damalige Projektstand wurde statisch auf konsistente Dateistruktur und ausgeglichene C++-Blockklammern geprüft.
* Ein vollständiger MSVC-/WinUI-Build musste weiterhin unter Visual Studio erfolgen.

\---

## \[v8.8] – Responsive Tabellenbreite

### Geändert

* Die Tabellenbreite wurde wieder responsiv an die verfügbare Fensterbreite gekoppelt.
* Die in v8.7 eingeführte fixierte Spaltenüberschrift blieb erhalten.
* Für Tabellen mit bis zu zehn Fachspalten wurden `Star`-Spalten verwendet, damit die gesamte Fensterbreite genutzt wird.
* Sehr breite Tabellen behalten Mindestbreiten und können horizontal scrollen.
* Kopf- und Datenbereich verwenden identische `ColumnDefinitions`.

### Unverändert

* Die reduzierte Tabellenhöhe aus v8.7 blieb in diesem Zwischenstand zunächst bestehen.
* Die Schriftgröße der Tabellen wurde nicht verändert.

\---

## \[v8.7] – Datumswerte und fixierte Tabellenköpfe

### Behoben

* Datumswerte werden beim Schreiben an Excel als echter COM-Datentyp `VT\\\\\\\_DATE` über `Range.Value` übergeben.
* Dadurch werden fehlerhafte kleine Zahlenwerte in Datumsfeldern vermieden.
* Die lokale Datumsdarstellung bleibt:

  * primär `TT.MM.JJJJ`
  * Fallback `dd.mm.yyyy`

### Tabellen

* Die Spaltenüberschrift bleibt beim vertikalen Scrollen sichtbar.
* Kopf und Datenkörper verwenden identische, explizit berechnete Pixelbreiten.
* Horizontal werden Kopf und Daten gemeinsam gescrollt.
* Tabellen wurden in diesem Zwischenstand vertikal kompakter:

  * Inventar: ca. `235 px`
  * Bestellungen: ca. `245 px`
  * Visiten: ca. `270 px`
  * globale Übersicht: ca. `255 px`
* Datenzeilen erhielten etwas weniger vertikales Padding.
* Die Schriftgröße der Tabelleninhalte blieb unverändert.

> \\\\\\\*\\\\\\\*Später geändert:\\\\\\\*\\\\\\\* Die reduzierte Tabellenhöhe wurde in v8.9 wieder zurückgenommen.

\---

## \[v8.6] – Performance und robuste Box-/Kit-Eingabe

### Wareneingang

* Zeilenumbrüche im mehrzeiligen Eingabefeld werden robust als folgende Varianten erkannt:

  * CRLF
  * LF
  * einzelnes CR
  * Unicode-Zeilentrenner
* Mehrere Box-/Kit-IDs werden dadurch zuverlässig als einzelne Kennzeichnungen verarbeitet.
* Bei Abweichungen zeigt die Fehlermeldung zusätzlich:

  * Anzahl erkannter Kennzeichnungen
  * Anzahl angegebener Einheiten
* Box-/Kit-IDs bleiben unverändert Textwerte.
* Es findet weiterhin keine automatische Nummerierung statt.

### Performance

* Nach schreibenden Aktionen wird nur noch die aktuell geänderte Studien-Excel neu eingelesen.
* Der Button **„Daten neu einlesen“** führt weiterhin bewusst einen vollständigen Reload aller Studien aus.
* Beim vollständigen Laden wird ein gemeinsamer Excel-COM-Prozess für alle Studien verwendet, statt Excel für jede Studie neu zu starten.
* Gemeinsam verwendete Blätter, z. B. `Bestellübersicht` oder `DrugInventory`, werden innerhalb eines Ladevorgangs gecacht und bei mehreren IMPs nicht mehrfach über COM eingelesen.
* Beim Batch-Wareneingang werden nur die tatsächlich benötigten Zellen geschrieben.
* Leere Spalten erzeugen keine zusätzlichen COM-Schreibaufrufe.
* `ScreenUpdating` und die Excel-Statusleiste werden für die unsichtbare Excel-Automation deaktiviert.

### Unverändert

* Backups vor schreibenden Änderungen bleiben erhalten.
* Die Prüfung auf zwischenzeitliche externe Dateiänderungen bleibt erhalten.
* Excel bleibt die führende Datenhaltung.

\---

## \[v8.5] – Arbeitsbereiche und Definition offener Bestellungen

### GUI

#### Reiter „Bestellungen“

Drei Arbeitsbereiche stehen nebeneinander:

1. **Ausgewählte Bestellung bearbeiten**
2. **Neue Bestellung erfassen**
3. **Dokumente \& Vorlagen**

#### Reiter „Wareneingang / Bestand“

Drei Arbeitsbereiche stehen nebeneinander:

1. **Wareneingang erfassen**
2. **Inventardatensatz bearbeiten**
3. **Dokumente \& Vorlagen**
* Tabellen und Tabellen-Schriftgrößen wurden nicht verändert.

### Offene Bestellungen

* Eine Bestellung gilt ausschließlich dann als **offen**, wenn die Zelle in der Spalte **„Erhalten am“** leer ist.
* Ein separater Bestellstatus ist dafür nicht erforderlich.
* Alle Zeilen mit leerem `Erhalten am` werden im Auswahlfeld angeboten.
* Bei Multi-IMP-Bestelltabellen wird die Menge des aktuell ausgewählten IMP nur informativ angezeigt und beeinflusst nicht den Offen-Status.
* Wird eine Bestellung ausgewählt, schreibt der Wareneingang das Lieferdatum gezielt in die Spalte `Erhalten am` dieser Bestellzeile.

### Kompatible Spaltennamen

Bevorzugt:

* `Erhalten am`

Unterstützte Fallback-Bezeichnungen:

* `Eingang am`
* `Date received`
* `Received date`
* `Received on`
* `Wareneingang`

\---

## \[v8.4] – Überarbeitung Wareneingang / Bestand

### Wareneingang und Inventar

* Die redundante GroupBox **„Einzelnen Inventardatensatz erfassen“** wurde entfernt.
* Der Arbeitsbereich bestand in diesem Zwischenstand aus zwei gleich breiten Karten:

  * **Wareneingang erfassen**
  * **Inventardatensatz bearbeiten**
* Eine Lieferung kann mehrere Einheiten gleichzeitig anlegen.
* Für jede Einheit wird eine Box-/Kit-Nummer als freier Text angegeben.
* Box-/Kit-Nummern werden nicht automatisch hochgezählt und nicht aus bestehenden Werten abgeleitet.
* Zulässig sind:

  * Buchstaben
  * Zahlen
  * Sonderzeichen
  * identische Werte für mehrere Einheiten
* Die Eingabe mehrerer Box-/Kit-Nummern erfolgt zeilenweise.
* Optional kann eine Kennzeichnung für alle Einheiten übernommen werden.

### Zuordnung zu Bestellungen

* Die Checkbox **„Offene Bestellung als erhalten markieren“** wurde entfernt.
* Stattdessen gibt es ein optionales Dropdown **„Zugehörige offene Bestellung“**.
* Nur die konkret ausgewählte Bestellzeile erhält das Lieferdatum in `Erhalten am`.
* Ein Wareneingang kann auch ohne Bestellzuordnung vollständig gespeichert werden.

### GUI

* Tabelleninhalte und Tabellenköpfe behalten ihre kompakte Schriftgröße.
* Primäre Speichern-Aktionen werden dezent blau hervorgehoben.
* GroupBox- und Arbeitsbereichstitel wurden stärker gewichtet.
* Zwischen studienübergreifender Übersicht bzw. Studienauswahl und studienspezifischem Arbeitsbereich wurde mehr Weißraum ergänzt.
* Eine feine Trennlinie unterstützt die visuelle Gliederung.
* Der aktive Studienbereich erhält einen dezenten Kopf mit:

  * Studienname
  * EUCT-No.
  * IMP-Anzahl

### Technisch

* `AppendInventoryBatch` erhält:

  * eine Liste expliziter Box-/Kit-Identifikatoren
  * optional die Excel-Zeilennummer einer konkreten offenen Bestellung
* Die Identifikatoren werden unverändert als Excel-Text geschrieben.

\---

## \[v8.3] – Erweiterte Spalten- und Datumerkennung

### DrugAccount / Headererkennung

* Deutsche `DrugInventory`-Spalten werden umfassender erkannt.

Beispiele:

* `Datum zurück gegeben am` / `Rückgabedatum` → `Date returned`
* `Anzahl übrig gebliebener` / `Restmenge` → `Number unused`
* `Datum Vernichtung` / `Vernichtungsdatum` → `Destruction Date`
* zusätzliche deutsche Varianten für:

  * Patient
  * Charge
  * Ausgabe
  * Verfall
  * Kommentare

### Kommentarspalten

* Zwei gleich benannte generische Kommentarspalten werden getrennt interpretiert:

  * erste generische Kommentarspalte → Dispensing Comment
  * zweite generische Kommentarspalte → Return/Destruction Comment

### Patientenkennung

Zusätzliche unterstützte Bezeichnungen:

* `Patienten-ID`
* `Patientennummer`
* `Patienten-Nr.`
* weitere vergleichbare Varianten

### Datumsanzeige

* Numerische Excel-Datumswerte werden als `TT.MM.JJJJ` dargestellt.
* Als Text gespeicherte Excel-Seriennummern werden in Datumsfeldern erkannt.
* Deutsche Rückgabe-, Vernichtungs- und Verfallsüberschriften werden als Datumsfelder erkannt.

### Datumsschreiben nach Excel

* Werte bleiben echte Excel-Datumsserienwerte.
* Bei deutsch lokalisiertem Excel wird `NumberFormatLocal = TT.MM.JJJJ` verwendet.
* Fallback: `NumberFormat = dd.mm.yyyy`.

### Unterstützte Datumseingaben

* `TT.MM.JJJJ`
* `TT/MM/JJJJ`
* `TT-MM-JJJJ`
* ISO `JJJJ-MM-TT`
* vorhandene Excel-Seriennummern in Datumsfeldern

\---

## \[v8.2] – Excel-only DrugAccount-Berichte

### Berichte

* Die PDF-Erstellung für DrugAccount wurde vollständig aus dem Workflow entfernt.
* Es gibt zwei getrennte Excel-Berichtstypen:

  * `DrugAccount\\\\\\\_Gesamt.xlsx` – alle Datensätze eines IMP
  * `DrugAccount\\\\\\\_Patient.xlsx` – ein IMP und ein ausgewählter Patient
* Jeder Bericht wird unmittelbar vor dem Export aus dem aktuellen `DrugInventory` neu erzeugt.
* Die erzeugte Excel-Datei wird nach dem Export direkt geöffnet.

### Datumswerte in Berichten

* Berichtsdatumsfelder werden nicht mehr als Excel-Serienzahl mit abhängigem `NumberFormat` geschrieben.
* Der `ReportService` wandelt erkannte Datumswerte vor dem Schreiben in sichtbaren Text `TT.MM.JJJJ` um.
* Die Berichtsdarstellung ist damit unabhängig vom lokalen Excel-Datumsformat.

### DrugAccount Gesamt

Spalten:

1. `Delivery Date`
2. `Charge No.`
3. `Box-Nr. / Kit-No.`
4. `Expiry Date`
5. `Dispensing Date`
6. `Pat.ID`
7. `Comment`
8. `Date returned`
9. `Number unused`
10. `Destruction Date`
11. `Comment`

### DrugAccount pro Patient

Spalten:

1. `Charge No.`
2. `Box-Nr. / Kit-No.`
3. `Expiry Date`
4. `Dispensing Date`
5. `Comment`
6. `Signature`
7. `Date returned`
8. `Number unused`
9. `Destruction Date`
10. `Comment`
11. `Signature`
* Die beiden `Signature`-Spalten bleiben bewusst leer.

### Berichtskopf

Beide Vorlagen enthalten:

* Studie
* EUCT-No.
* IMP
* Stand

Der Patientenbericht enthält zusätzlich:

* `Pat.ID`

### Vorlagen

Globale Standardvorlagen:

* `Templates\\\\\\\\DrugAccount\\\\\\\_Gesamt.xlsx`
* `Templates\\\\\\\\DrugAccount\\\\\\\_Patient.xlsx`

Studienspezifische Overrides können unter folgendem Pfad mit identischen Dateinamen abgelegt werden:

* `Documents\\\\\\\\DrugAccount`

\---

## \[v8.0] – DrugAccount-Schema und Tabellenstruktur

### DrugAccount pro Patient

Der Patientenbericht wurde auf ein verbindliches Schema mit elf Spalten umgestellt:

1. `ChargNo`
2. `Box-Nr. / Kit-No.`
3. `Expiry Date`
4. `Dispensing Date`
5. `Comment`
6. `Signature`
7. `Date returned`
8. `Number of unused`
9. `Destruction Date`
10. `Comment`
11. `Signature`
* Die beiden `Signature`-Spalten bleiben bewusst leer.
* Rückgabe- und Vernichtungsfelder werden nur befüllt, wenn die entsprechenden Spalten in der Studien-Excel vorhanden sind.
* Der Berichtskopf enthält:

  * Studienname
  * IMP
  * `EUCT No.:`
  * Patientennummer
* Die ersten drei Zeilen wurden für den damaligen PDF-Druck auf jeder Seite wiederholt.

### Headererkennung

* Exakte Spaltennamen werden vor Teiltreffern priorisiert.
* Dadurch wird beispielsweise `Return Comment` nicht irrtümlich als allgemeines Feld `Comment` interpretiert.

### Tabellenlayout

* Interaktive Programmtabellen verwenden für Kopf und Datenzeilen ein gemeinsames WinUI-`Grid` mit gemeinsamen `ColumnDefinitions`.
* Dadurch können Kopf- und Datenzellen derselben Spalte ihre Breiten nicht unabhängig voneinander berechnen.
* Filter, Sortierung und Zeilenauswahl bleiben erhalten.

### Vorlage

* `Templates/Bericht\\\\\\\_DrugAccount.xlsx` wurde auf das damalige Patientenberichtsschema aktualisiert.
* `Tabelle1` für den Gesamtbericht blieb unverändert.

> \\\\\\\*\\\\\\\*Später geändert:\\\\\\\*\\\\\\\* In v8.2 wurde der PDF-Workflow vollständig entfernt und die Berichtserzeugung auf zwei getrennte Excel-Vorlagen umgestellt.

\---

## \[v7] – Neustrukturierung der Oberfläche und Exportlogik

### Oberfläche

* Tabellen verwenden keine starren Pixelbreiten mehr, sondern verteilen normale Spalten proportional auf die verfügbare Fensterbreite.
* Erst bei sehr breiten Tabellen mit mehr als zehn Spalten wird horizontal gescrollt.
* Tabellenzeilen und Spaltenköpfe wurden kompakter gestaltet.
* Die breite Schaltfläche **„Auswählen“** wurde durch eine schmale Häkchen-Spalte ersetzt.
* Das Studienprofil ist nicht mehr als eigener Programm-Reiter sichtbar und verbleibt als Konfiguration in der Excel-Datei.
* Der studienspezifische Reiter **„Dokumente \& Vorlagen“** wurde entfernt.
* Dokumente werden zentral über den seitlichen Menüpunkt **„Vorlagen \& Dokumente“** angezeigt.
* Der seitliche Menüpunkt **„Studien“** wurde entfernt, da die relevanten Studieninformationen bereits in der Gesamtübersicht enthalten sind.
* Der aktive Studien-Datenordner wird ausschließlich unter **„Einstellungen“** angezeigt.
* In der Studienauswahl wird die EUCT-No. – sofern vorhanden – hinter dem Studiennamen in eckigen Klammern dargestellt.
* Folgende Aktionen befinden sich gemeinsam rechts neben der Studienauswahl und bleiben in allen studienspezifischen Reitern sichtbar:

  * **Studien-Excel öffnen**
  * **Dokumentenordner öffnen**
  * **Daten neu einlesen**

### Layout der studienspezifischen Arbeitsbereiche

#### Übersicht

* Die Schnellaktionen-Box wurde entfernt.
* **„Nächste geplante Visiten“** und passende Dokumente bzw. Vorlagen stehen nebeneinander.

#### Bestellungen

* **„Neue Bestellung erfassen“** und **„Dokumente \& Vorlagen“** stehen nebeneinander.

#### Wareneingang / Bestand

* In diesem Zwischenstand standen folgende Bereiche dreispaltig nebeneinander:

  * **Inventardatensatz bearbeiten**
  * **Wareneingang erfassen**
  * **Einzelnen Inventardatensatz erfassen**
* Dokumente folgten darunter.

#### Patientenvisiten

Drei Arbeitsbereiche stehen nebeneinander:

* **Visite terminieren/bearbeiten**
* **Patient hinzufügen**
* **Neue Visitenzeile hinzufügen**

### Berichte

* DrugAccount-Exporte verwerfen unvollständige Inventarzeilen nicht mehr allein deshalb, weil eine Kit-, Vial-, Box- oder Packnummer fehlt.
* Im Gesamtbericht werden alle vorhandenen Inventardatensätze des ausgewählten IMP berücksichtigt.
* Im Patientenbericht werden alle passenden Patientendatensätze berücksichtigt.
* Die Exportprüfung kontrolliert, ob in der ersten und letzten geschriebenen Berichtzeile Daten vorhanden sind, statt ein bestimmtes Identifikationsfeld zwingend vorauszusetzen.
* Der in der Anwendung angezeigte Zähler für Berichtsdaten zählt alle vorhandenen Inventardatensätze des ausgewählten IMP.

### Exportpfade

* Für jede Studie kann unter **„Einstellungen“** ein eigener Exportordner hinterlegt werden.
* Ohne individuellen Pfad wurden DrugAccount-PDF-/Excel-Dateien zu diesem Zeitpunkt direkt im jeweiligen Studien-Datenordner abgelegt.
* Der Button **„Vorlage für erstes Tabellenblatt öffnen“** wurde aus den Einstellungen entfernt.
* Die studienübergreifende CSV-Übersicht wird unter folgendem Pfad gespeichert:

  * `<Studien-Datenordner>\\\\\\\\Exporte`

\---

## Versionshistorie auf einen Blick

|Version|Schwerpunkt|
|-|-|
|**v8.11**|App-Icon als Ressource in die EXE eingebettet|
|**v8.10**|Absturz bei gemeinsamer Box-/Kit-Kennzeichnung behoben|
|**v8.9**|Tabellenbreiten und Scrollpositionen stabilisiert|
|**v8.8**|Responsive Tabellenbreite|
|**v8.7**|`VT\\\\\\\_DATE`, fixe Tabellenköpfe|
|**v8.6**|Performanceoptimierung und robuste Mehrzeileneingabe|
|**v8.5**|Offene Bestellungen eindeutig über `Erhalten am`|
|**v8.4**|Batch-Wareneingang und freie Box-/Kit-IDs|
|**v8.3**|Deutsche Header- und Datumerkennung|
|**v8.2**|DrugAccount vollständig auf Excel-Workflow umgestellt|
|**v8.0**|Patientenbericht und Tabellenstruktur überarbeitet|
|**v7**|GUI-, Dokument- und Exportstruktur neu geordnet|

\---

## Hinweise zur Versionsführung

* Die Versionsnummern v7 bis v8.11 dokumentieren Entwicklungsstände des Projekts.
* Änderungen, die in späteren Versionen ersetzt oder korrigiert wurden, bleiben zur historischen Nachvollziehbarkeit aufgeführt.
* Für eine erste veröffentlichte Produktversion kann unabhängig davon mit einer neuen Versionslinie, z. B. `1.0.0`, begonnen werden.

