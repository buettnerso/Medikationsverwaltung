Medikationsverwaltung (WinUI / C++)

---

### Übersicht

Medikationsverwaltung ist eine Windows-Anwendung zur Verwaltung studienbezogener Prüfmedikation auf Basis von WinUI und C++.

Die Anwendung verwendet bewusst eine Excel-basierte Datenhaltung. Jede Studie wird über eine eigene Excel-Datei verwaltet, die als führendes Datensystem dient. Eine zusätzliche Datenbank wird nicht benötigt.



#### Hauptfunktionen

* Verwaltung mehrerer Studien
* Bestell- und Bestandsverwaltung
* Dokumentation von Wareneingängen
* Verwaltung von DrugInventory-Datensätzen
* Patienten- und Visitenverwaltung
* Erstellung von DrugAccount-Berichten
* Generierung von Dokumentationstexten für Medikationsausgabe und -Rückgabe
* Auditierung fachlicher Änderungen


#### Architektur

Die Anwendung dient als Benutzeroberfläche für die Bearbeitung und Auswertung der Studiendaten.



In den Excel-Dateien werden unter anderem gespeichert:

* Studienstammdaten
* IMP-Definitionen
* Bestellungen
* Inventory-Daten
* Patienten- und Visiteninformationen
* Notizen





#### Code-Dokumentation



Die aktiven C++-, Header-, Ressourcen- und Build-Dateien enthalten Architektur- und Bereichskommentare.



Schwerpunkte der Dokumentation:



* Verantwortlichkeiten einzelner Komponenten
* Datenflüsse
* Fachliche Entscheidungen



Eine Übersicht der wichtigsten Komponenten befindet sich in:



CODE\_UEBERSICHT.md







#### Studienstruktur

Beispiel:

Studien  
├── RUX-ECP  
│   ├── RUX\_ECP\_Bestellübersicht.xlsx
│   └── Documents  
└── SEQUENCE  
├── SEQUENCE\_Bestellübersicht.xlsx
    └── Documents\\



Der Ordnername dient als eindeutige Studienidentifikation.

Sofern vorhanden, wird die EUCT-Nummer in der Studienauswahl als Studienname \[EUCT-No.] angezeigt.



#### Navigation

* Hauptbereiche
* Übersicht
* Berichte
* Vorlagen \& Dokumente
* Einstellungen
* Studienbezogene Bereiche
* Übersicht
* Bestellungen
* Wareneingang / Bestand
* Patientenvisiten
* Berichte
* Notizen
* Tabellen



#### Unterstützte Funktionen:



* Filtern und Sortieren
* Zeilenauswahl
* Fixierte Tabellenköpfe
* Responsive Spaltenbreiten
* Horizontales Scrollen bei großen Tabellen
* Wiederherstellung der Scrollposition
* Wareneingang und Bestand



Der Bereich besteht aus zwei Arbeitsabläufen:



* Wareneingang erfassen
* Inventardatensatz bearbeiten



Beim Wareneingang können unter anderem erfasst werden:



* Lieferdatum
* Charge
* Verfallsdatum
* Anzahl Einheiten
* Box-/Kit-/Vial-Kennungen
* optionale Zuordnung zu einer offenen Bestellung



Kennungen werden als freie Textwerte gespeichert. Es erfolgt keine automatische Nummerierung.



Nach dem Wareneingang werden weitere Vorgänge über den Inventardatensatz dokumentiert, beispielsweise:



* Dispensing
* Patientenzuordnung
* Rückgaben
* Vernichtung
* Bestandskorrekturen
* Berichte
* DrugAccount



#### Verfügbare Berichtstypen:



* DrugAccount pro IMP
* DrugAccount pro Patient



Standardvorlagen:Templates  
├── DrugAccount\_Gesamt.xlsx
└── DrugAccount\_Patient.xlsx



Optional können studienspezifische Vorlagen verwendet werden:

1. Documents\\DrugAccount  
2. Documents\\DrugAccount<IMP-ID>\\





#### Dokumentationstexte



Für Medikationsausgaben und -rückgaben können automatisch Dokumentationstexte aus vorhandenen DrugInventory-Daten erzeugt werden.



Die Textgenerierung verändert keine Studiendaten.



#### AuditLog



Fachliche Änderungen werden automatisch protokolliert.



###### Merkmale:



* CSV-basierter Audit Trail
* nicht deaktivierbar
* konfigurierbarer Speicherort
* Protokollierung von Exporten
* gemeinsame OperationId für zusammengehörige Batch-Vorgänge



Erfasste Informationen umfassen unter anderem:



* Zeitstempel
* Benutzer
* Computer
* Programmversion
* Studie
* IMP
* Excel-Zeile
* alter Wert
* neuer Wert
* Versionierung



Die Produktversion wird zentral in AppVersion.h verwaltet und in GUI, AuditLog sowie Installationspaket verwendet.



Aktuelle Version:   1.8.18



#### Voraussetzungen

* Windows
* Visual Studio
* MSVC
* WinUI
* Microsoft Excel Desktop
* NuGet
* Build
* Medikationsverwaltung\_WinUI.sln öffnen
* Konfiguration Debug | x64 auswählen
* NuGet-Pakete wiederherstellen
* Projektmappe bereinigen
* Projektmappe neu erstellen
* Hinweis



Aufgrund der WinUI- und Excel-COM-Abhängigkeiten kann ein vollständiger Build ausschließlich in einer Windows-Entwicklungsumgebung durchgeführt werden.



#### Changelog



Die vollständige Versionshistorie befindet sich in:



CHANGELOG.md

