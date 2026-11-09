; Inno Setup script of the Contorno Indicator installer (target Indicator_installer).
; buildkit passes /DAppVersion=<version> and /DStageDir=<the cmake --install tree>.
; Only bin and share\contorno are packed: the program with its DLLs, and the
; model, manual and articles. bin\..\share\contorno is where the program looks.
#ifndef AppVersion
  #define AppVersion "0.0.0-dev"
#endif
#ifndef StageDir
  #error StageDir is not defined
#endif

[Setup]
AppId={{6C2E9F4A-3B1D-4E8A-9F57-2D0C8B4A7E31}
AppName=Contorno Indicator
AppVersion={#AppVersion}
AppPublisher=Mohamed Lamine Ben Hassine
AppPublisherURL=https://github.com/medaminben/Contorno
AppSupportURL=https://github.com/medaminben/Contorno/issues
DefaultDirName={autopf}\Contorno Indicator
DisableProgramGroupPage=yes
LicenseFile=..\..\..\LICENSE
SetupIconFile=indicator.ico
UninstallDisplayIcon={app}\bin\Indicator.exe
OutputBaseFilename=contorno-indicator-{#AppVersion}-setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequiredOverridesAllowed=dialog
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"
Name: "de"; MessagesFile: "compiler:Languages\German.isl"
Name: "es"; MessagesFile: "compiler:Languages\Spanish.isl"
Name: "it"; MessagesFile: "compiler:Languages\Italian.isl"
Name: "fr"; MessagesFile: "compiler:Languages\French.isl"
Name: "nl"; MessagesFile: "compiler:Languages\Dutch.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#StageDir}\bin\*"; DestDir: "{app}\bin"; Flags: ignoreversion recursesubdirs
Source: "{#StageDir}\share\contorno\*"; DestDir: "{app}\share\contorno"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\Contorno Indicator"; Filename: "{app}\bin\Indicator.exe"
Name: "{autodesktop}\Contorno Indicator"; Filename: "{app}\bin\Indicator.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\bin\Indicator.exe"; Description: "{cm:LaunchProgram,Contorno Indicator}"; Flags: nowait postinstall skipifsilent
