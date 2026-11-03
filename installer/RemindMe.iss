; SPDX-License-Identifier: MIT

#define MyAppName "RemindMe"

#ifndef MyAppVersion
  #define MyAppVersion "0.0.0"
#endif

#ifndef MySourceDir
  #error "MySourceDir must point to the portable package folder."
#endif

[Setup]
AppId={{1E130A1E-BC16-4FF4-B5D9-A80CECA0D84D}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher=RemindMe
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
UninstallDisplayIcon={app}\RemindMe.exe
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
Compression=lzma
SolidCompression=yes
OutputDir=dist
OutputBaseFilename={#MyAppName}-{#MyAppVersion}-setup
WizardStyle=modern

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional icons:"; Flags: unchecked

[InstallDelete]
; 2.0 is a single static exe; remove the Qt runtime that 1.x installed alongside it.
Type: files; Name: "{app}\*.dll"
Type: filesandordirs; Name: "{app}\platforms"
Type: filesandordirs; Name: "{app}\styles"
Type: filesandordirs; Name: "{app}\imageformats"
Type: filesandordirs; Name: "{app}\iconengines"
Type: filesandordirs; Name: "{app}\networkinformation"
Type: filesandordirs; Name: "{app}\tls"
Type: filesandordirs; Name: "{app}\generic"
Type: filesandordirs; Name: "{app}\translations"

[Files]
Source: "{#MySourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\RemindMe"; Filename: "{app}\RemindMe.exe"
Name: "{group}\Uninstall RemindMe"; Filename: "{uninstallexe}"
Name: "{autodesktop}\RemindMe"; Filename: "{app}\RemindMe.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\RemindMe.exe"; Description: "Launch RemindMe"; Flags: nowait postinstall skipifsilent
