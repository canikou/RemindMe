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

[Files]
Source: "{#MySourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\RemindMe"; Filename: "{app}\RemindMe.exe"
Name: "{group}\Uninstall RemindMe"; Filename: "{uninstallexe}"
Name: "{autodesktop}\RemindMe"; Filename: "{app}\RemindMe.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\RemindMe.exe"; Description: "Launch RemindMe"; Flags: nowait postinstall skipifsilent
