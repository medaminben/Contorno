; Inno Setup script of the Contorno Indicator installer (target Indicator_installer).
; buildkit passes /DAppVersion=<version> and /DStageDir=<the cmake --install tree>.
; Only bin and share\contorno are packed. The program with its DLLs and the
; model go in the program folder (bin\..\share\contorno is where the program
; looks for the model). The samples, manual and articles go in the data folder
; the user picks on its own page (Documents\Contorno by default); its path is
; written to share\contorno\data-folder for the program to read.
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
; the data folder defaults to the installing user's Documents, also in an administrative install
UsedUserAreasWarning=no

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
Source: "{#StageDir}\share\contorno\models\*"; DestDir: "{app}\share\contorno\models"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist
Source: "{#StageDir}\share\contorno\samples\*"; DestDir: "{code:DataDir}\samples"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StageDir}\share\contorno\manual\*"; DestDir: "{code:DataDir}\manual"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StageDir}\share\contorno\articles\*"; DestDir: "{code:DataDir}\articles"; Flags: ignoreversion recursesubdirs createallsubdirs

[UninstallDelete]
Type: files; Name: "{app}\share\contorno\data-folder"

[Icons]
Name: "{autoprograms}\Contorno Indicator"; Filename: "{app}\bin\Indicator.exe"
Name: "{autodesktop}\Contorno Indicator"; Filename: "{app}\bin\Indicator.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\bin\Indicator.exe"; Description: "{cm:LaunchProgram,Contorno Indicator}"; Flags: nowait postinstall skipifsilent

[CustomMessages]
en.DataDirCaption=Samples, manual and articles
en.DataDirDescription=Where should the sample photos, the manual and the articles go?
en.DataDirText=Setup will put the sample photos, the user manual and the articles in the following folder. The file dialogs of Contorno Indicator open the samples first. To use another folder, click Browse.
de.DataDirCaption=Beispiele, Handbuch und Artikel
de.DataDirDescription=Wohin sollen die Beispielfotos, das Handbuch und die Artikel?
de.DataDirText=Setup legt die Beispielfotos, das Benutzerhandbuch und die Artikel in den folgenden Ordner. Die Dateidialoge von Contorno Indicator öffnen zuerst die Beispiele. Um einen anderen Ordner zu wählen, klicken Sie auf „Durchsuchen“.
es.DataDirCaption=Ejemplos, manual y artículos
es.DataDirDescription=¿Dónde se guardan las fotos de ejemplo, el manual y los artículos?
es.DataDirText=El programa de instalación pondrá las fotos de ejemplo, el manual de usuario y los artículos en la carpeta siguiente. Los diálogos de archivo de Contorno Indicator abren primero los ejemplos. Para usar otra carpeta, haga clic en Examinar.
it.DataDirCaption=Esempi, manuale e articoli
it.DataDirDescription=Dove vanno le foto di esempio, il manuale e gli articoli?
it.DataDirText=Il programma di installazione metterà le foto di esempio, il manuale utente e gli articoli nella cartella seguente. Le finestre di dialogo dei file di Contorno Indicator aprono prima gli esempi. Per usare un'altra cartella, fare clic su Sfoglia.
fr.DataDirCaption=Exemples, manuel et articles
fr.DataDirDescription=Où placer les photos d'exemple, le manuel et les articles ?
fr.DataDirText=L'assistant placera les photos d'exemple, le manuel d'utilisation et les articles dans le dossier suivant. Les boîtes de dialogue de fichiers de Contorno Indicator ouvrent d'abord les exemples. Pour choisir un autre dossier, cliquez sur Parcourir.
nl.DataDirCaption=Voorbeelden, handleiding en artikelen
nl.DataDirDescription=Waar moeten de voorbeeldfoto's, de handleiding en de artikelen komen?
nl.DataDirText=Setup zet de voorbeeldfoto's, de gebruikershandleiding en de artikelen in de volgende map. De bestandsvensters van Contorno Indicator openen eerst de voorbeelden. Klik op Bladeren om een andere map te kiezen.

[Code]
var
  DataDirPage: TInputDirWizardPage;

procedure InitializeWizard;
begin
  DataDirPage := CreateInputDirPage(wpSelectDir, CustomMessage('DataDirCaption'),
    CustomMessage('DataDirDescription'), CustomMessage('DataDirText'), False, '');
  DataDirPage.Add('');
  DataDirPage.Values[0] := GetPreviousData('DataDir', ExpandConstant('{userdocs}\Contorno'));
end;

{ an update keeps the folder, as Inno Setup keeps the program folder }
function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := (PageID = DataDirPage.ID) and (GetPreviousData('DataDir', '') <> '');
end;

procedure RegisterPreviousData(PreviousDataKey: Integer);
begin
  SetPreviousData(PreviousDataKey, 'DataDir', DataDirPage.Values[0]);
end;

function DataDir(Param: String): String;
begin
  Result := DataDirPage.Values[0];
end;

{ the program reads the chosen folder from here (UTF-8, one line) }
procedure CurStepChanged(CurStep: TSetupStep);
var
  Lines: TArrayOfString;
begin
  if CurStep = ssPostInstall then
  begin
    SetArrayLength(Lines, 1);
    Lines[0] := DataDir('');
    SaveStringsToUTF8File(ExpandConstant('{app}\share\contorno\data-folder'), Lines, False);
  end;
end;
