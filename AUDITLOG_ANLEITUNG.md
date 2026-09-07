# AuditLog – Medikationsverwaltung v8.15

## Zweck

Das AuditLog protokolliert fachlich relevante Änderungen automatisch. Es ergänzt die vorhandenen Excel-Backups um die Information, **wer wann welche Änderung durchgeführt hat**.

Das AuditLog ist in der Anwendung **nicht abschaltbar**.

## Speicherort

Jedes lokale Windows-Benutzerprofil führt auf dem jeweiligen PC eine eigene Audit-Datei unter:

```text
%LOCALAPPDATA%\Medikationsverwaltung\Audit\
```

Beispiel:

```text
C:\Users\<Benutzer>\AppData\Local\Medikationsverwaltung\Audit\
└── Audit_STUDIEN-PC-04_2026-09.csv
```

Der Rechnername ist Teil des Dateinamens. Zusätzlich werden Windows-Benutzer und Computername in jeder Audit-Zeile gespeichert.

Unter **Einstellungen → AuditLog** kann der lokale Audit-Ordner direkt geöffnet werden.

## Protokollierte Informationen

Jede CSV-Zeile enthält:

- UTC-Zeitstempel
- OperationId (GUID)
- Windows-Benutzer
- Computername
- Programmversion
- Studie
- EUCT-No.
- Pfad der Studien-Excel
- IMP
- Aktion
- Excel-Blatt
- Excel-Zeile
- geändertes Feld
- alter Wert
- neuer Wert
- Änderungsgrund (für spätere Erweiterung vorgesehen)
- technische/fachliche Zusatzinformation

## Protokollierte Aktionen

Aktuell werden u. a. protokolliert:

- `ORDER_CREATE` – neue Bestellung
- `ORDER_UPDATE` – Bestellung geändert
- `ORDER_RECEIVED` – offene Bestellung durch Wareneingang als erhalten markiert
- `INVENTORY_CREATE` – einzelner Inventardatensatz angelegt
- `INVENTORY_UPDATE` – Inventardatensatz geändert
- `GOODS_RECEIPT` – Wareneingang / neue Box-/Kit-Zeile
- `PATIENT_CREATE` – Patientenspalte hinzugefügt
- `VISIT_CREATE` – neue Visitenzeile
- `VISIT_DATE_UPDATE` – Visitendatum geändert
- `PROFILE_UPDATE` – Studienprofil-Zeile geändert
- `STUDY_METADATA_UPDATE` – Studienmetadaten geändert
- `REPORT_EXPORT_OVERALL` – DrugAccount Gesamt exportiert
- `REPORT_EXPORT_PATIENT` – DrugAccount pro Patient exportiert

Bei einem Batch-Wareneingang erhalten alle zusammengehörigen neuen Inventarzeilen dieselbe `OperationId`.

## Verhalten bei nicht verfügbarem AuditLog

Vor schreibenden fachlichen Änderungen prüft die Anwendung, ob die lokale Audit-Datei beschreibbar ist. Ist dies nicht möglich, wird die Änderung abgebrochen.

Dies reduziert das Risiko nicht protokollierter Änderungen. Eine echte atomare Transaktion zwischen Excel-Datei und Audit-Datei existiert jedoch nicht. Ein technisch oder regulatorisch manipulationssicherer Audit Trail würde zusätzliche Maßnahmen benötigen.

## Sicherheitsgrenze der aktuellen Lösung

Die lokale CSV ist eine **Nachvollziehbarkeits- und Pilotlösung**, aber kein manipulationsgeschützter GxP-/21-CFR-Part-11-Audit-Trail.

Ein lokaler Benutzer mit ausreichenden Dateirechten kann die CSV grundsätzlich verändern oder löschen. Für einen produktiven, revisionssicheren Einsatz wären sinnvoll:

- zentraler Audit-Speicher auf einem Server,
- restriktive NTFS-/SMB-Berechtigungen,
- append-only bzw. Write-Only-Konzept,
- definierte Aufbewahrungsfristen,
- regelmäßige Sicherung,
- ggf. Hash-/Signaturverkettung der Einträge,
- optional verpflichtender Änderungsgrund bei Korrekturen bestehender Werte.

## Programmversion

Die aktuelle Produktversion wird zentral in:

```text
Medikationsverwaltung\AppVersion.h
```

definiert. Die gleiche Version erscheint klein in der Navigations-Fußzeile und wird in jeden Audit-Eintrag geschrieben.

Bei einem Release sollte dieselbe Versionsnummer auch im Inno-Setup-Skript verwendet werden.
