; ============================================================================
; Medikationsverwaltung - Inno Setup
; Ziel: klassischer x64-Installer für die unpackaged WinUI-3-Anwendung
;
; Voraussetzung:
;   1. In Visual Studio "Release | x64" bauen.
;   2. Der fertige Programmordner liegt unter:
;      x64\Release\Medikationsverwaltung\
;   3. Dieses .iss-Skript liegt im Stammordner der Solution.
;
; Falls dein tatsächlicher Release-Pfad anders heißt, nur MyAppSourceDir ändern.
; ============================================================================

#define MyAppName "Medikationsverwaltung"
#define MyAppVersion "1.1.0"
#define MyAppPublisher "Medikationsverwaltung"
#define MyAppExeName "Medikationsverwaltung.exe"
#define MyAppSourceDir "x64\Release\Medikationsverwaltung"

[Setup]
; Diese AppId muss für spätere Updates derselben Anwendung unverändert bleiben.
AppId={{5CE1BF74-BC88-426F-966E-9AA039343834}

AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}

; Installation für alle Benutzer unter Program Files.
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}

; Der Installer selbst wird hier erzeugt.
OutputDir=Installer\Output
OutputBaseFilename=Medikationsverwaltung_Setup_{#MyAppVersion}

; Das eingebettete App-Icon auch für den Setup verwenden.
; Falls deine ICO-Datei woanders liegt, diesen Pfad anpassen.
SetupIconFile=Medikationsverwaltung\Assets\Medikationsverwaltung_AppIcon.ico
UninstallDisplayIcon={app}\{#MyAppExeName}

Compression=lzma2
SolidCompression=yes
WizardStyle=modern

; Deine Anwendung ist x64.
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

; Installation nach Program Files benötigt Administratorrechte.
PrivilegesRequired=admin

; Vor einem Update nach Möglichkeit die laufende Anwendung schließen.
CloseApplications=yes
RestartApplications=no

; Bestehenden Installationsordner bei Updates wiederverwenden.
UsePreviousAppDir=yes

; Uninstaller anzeigen und registrieren.
Uninstallable=yes

[Languages]
Name: "german"; MessagesFile: "compiler:Languages\German.isl"

[Tasks]
Name: "desktopicon";     Description: "Desktop-Verknüpfung erstellen";     GroupDescription: "Zusätzliche Verknüpfungen:";     Flags: unchecked

[Files]
; WICHTIG:
; Den kompletten Release-Ausgabeordner übernehmen.
; DLLs, PRI-Dateien, Assets, Templates usw. dürfen nicht einzeln herausgezogen werden.
Source: "{#MyAppSourceDir}\*";     DestDir: "{app}";     Flags: ignoreversion recursesubdirs createallsubdirs;     Excludes: "*.pdb,*.ilk"

[Icons]
; Startmenü
Name: "{group}\{#MyAppName}";     Filename: "{app}\{#MyAppExeName}";     WorkingDir: "{app}"

; Optionale Desktop-Verknüpfung
Name: "{autodesktop}\{#MyAppName}";     Filename: "{app}\{#MyAppExeName}";     WorkingDir: "{app}";     Tasks: desktopicon

[Run]
; Nach erfolgreicher Installation optional direkt starten.
Filename: "{app}\{#MyAppExeName}";     Description: "{#MyAppName} starten";     Flags: nowait postinstall skipifsilent
