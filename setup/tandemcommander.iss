#define MyAppName "Tandem Commander"
#define MyAppVersion "0.1.5"
#define MyAppPublisher "Pavel Stupka"
#define MyAppURL "https://tandemcommander.org/"
#define MyAppExeName "tandemcommander.exe"

[Setup]
; NOTE: The value of AppId uniquely identifies this application. Do not use the same AppId value in installers for other applications.
; (To generate a new GUID, click Tools | Generate GUID inside the IDE.)
AppId={{35C0B0DC-DB73-429C-AAA8-FBC41C937F66}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
;AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
UninstallDisplayIcon={app}\{#MyAppExeName}
; "ArchitecturesAllowed=x64compatible" specifies that Setup cannot run
; on anything but x64 and Windows 11 on Arm.
ArchitecturesAllowed=x64compatible
; "ArchitecturesInstallIn64BitMode=x64compatible" requests that the
; install be done in "64-bit mode" on x64 or Windows 11 on Arm,
; meaning it should use the native 64-bit Program Files directory and
; the 64-bit view of the registry.
ArchitecturesInstallIn64BitMode=x64compatible
; Uncomment the following line to use a 64-bit installer.
;SetupArchitecture=x64
DisableProgramGroupPage=yes
LicenseFile=license.txt
; Uncomment the following line to run in non administrative install mode (install for current user only).
;PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
OutputDir=output
OutputBaseFilename=tandemcommander-{#MyAppVersion}-x64-setup
SetupIconFile=setup.ico
SolidCompression=yes
WizardStyle=modern
; Feature 050: signing is strictly opt-in. build_setup.cmd sign compiles with
; /DSIGN=1 and defines the "tcsign" tool on the ISCC command line (/Stcsign=...),
; so an unsigned compile has no dependency on any certificate or sign tool.
#ifdef SIGN
SignTool=tcsign
SignedUninstaller=yes
#endif

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "czech"; MessagesFile: "compiler:Languages\Czech.isl"
Name: "dutch"; MessagesFile: "compiler:Languages\Dutch.isl"
Name: "french"; MessagesFile: "compiler:Languages\French.isl"
Name: "german"; MessagesFile: "compiler:Languages\German.isl"
Name: "hungarian"; MessagesFile: "compiler:Languages\Hungarian.isl"
Name: "slovak"; MessagesFile: "compiler:Languages\Slovak.isl"
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"

[CustomMessages]
; Disclaimer page chrome, localized per installer language; the disclaimer
; BODY itself intentionally stays English (single wording everywhere).
; Unprefixed entries are the English defaults / fallback.
DisclaimerCaption=Disclaimer
DisclaimerDescription=Please read the following important information before continuing
DisclaimerSubCaption=By continuing, you acknowledge the disclaimer below:
DisclaimerAccept=I understand and accept this disclaimer
czech.DisclaimerCaption=Prohlášení o vyloučení odpovědnosti
czech.DisclaimerDescription=Než budete pokračovat, přečtěte si prosím následující důležité informace
czech.DisclaimerSubCaption=Pokračováním berete na vědomí níže uvedené prohlášení:
czech.DisclaimerAccept=Rozumím a souhlasím s tímto prohlášením
dutch.DisclaimerCaption=Disclaimer
dutch.DisclaimerDescription=Lees de volgende belangrijke informatie voordat u verdergaat
dutch.DisclaimerSubCaption=Door verder te gaan, erkent u de onderstaande disclaimer:
dutch.DisclaimerAccept=Ik begrijp en aanvaard deze disclaimer
french.DisclaimerCaption=Avertissement
french.DisclaimerDescription=Veuillez lire les informations importantes suivantes avant de continuer
french.DisclaimerSubCaption=En continuant, vous reconnaissez l'avertissement ci-dessous :
french.DisclaimerAccept=Je comprends et j'accepte cet avertissement
german.DisclaimerCaption=Haftungsausschluss
german.DisclaimerDescription=Bitte lesen Sie die folgenden wichtigen Informationen, bevor Sie fortfahren
german.DisclaimerSubCaption=Indem Sie fortfahren, erkennen Sie den nachstehenden Haftungsausschluss an:
german.DisclaimerAccept=Ich habe den Haftungsausschluss verstanden und akzeptiere ihn
hungarian.DisclaimerCaption=Felelősségkizáró nyilatkozat
hungarian.DisclaimerDescription=Kérjük, olvassa el az alábbi fontos információkat, mielőtt folytatná
hungarian.DisclaimerSubCaption=A folytatással tudomásul veszi az alábbi nyilatkozatot:
hungarian.DisclaimerAccept=Megértettem és elfogadom ezt a nyilatkozatot
slovak.DisclaimerCaption=Vyhlásenie o vylúčení zodpovednosti
slovak.DisclaimerDescription=Skôr než budete pokračovať, prečítajte si nasledujúce dôležité informácie
slovak.DisclaimerSubCaption=Pokračovaním beriete na vedomie nižšie uvedené vyhlásenie:
slovak.DisclaimerAccept=Rozumiem a súhlasím s týmto vyhlásením
spanish.DisclaimerCaption=Descargo de responsabilidad
spanish.DisclaimerDescription=Lea la siguiente información importante antes de continuar
spanish.DisclaimerSubCaption=Al continuar, usted acepta el siguiente descargo de responsabilidad:
spanish.DisclaimerAccept=Entiendo y acepto este descargo de responsabilidad

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "..\build\tandemcommander\Release_x64\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
; Feature 050 (FR-014): never package linker byproducts, even if a stale
; build tree still contains them (the release build keeps the tree clean
; on its own - this Excludes is an independent safety net).
Source: "..\build\tandemcommander\Release_x64\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; Excludes: "*.pdb,*.lib,*.exp"
; NOTE: Don't use "Flags: ignoreversion" on any shared system files

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[Code]
{ AI disclaimer page: shown right after the GPL license page and gated by a
  checkbox - Next stays disabled until the user explicitly accepts. The page
  chrome (title, subtitle, checkbox) is localized via [CustomMessages]; the
  disclaimer body itself intentionally stays English. Silent installs skip
  wizard pages by Inno Setup design, like the license page. }
var
  DisclaimerPage: TOutputMsgMemoWizardPage;
  DisclaimerAcceptedCheck: TNewCheckBox;

procedure DisclaimerCheckClick(Sender: TObject);
begin
  WizardForm.NextButton.Enabled := DisclaimerAcceptedCheck.Checked;
end;

procedure InitializeWizard;
begin
  DisclaimerPage := CreateOutputMsgMemoPage(wpLicense,
    CustomMessage('DisclaimerCaption'),
    CustomMessage('DisclaimerDescription'),
    CustomMessage('DisclaimerSubCaption'),
    'DISCLAIMER' + #13#10 + #13#10 +
    'Tandem Commander was created using AI-assisted, agentic software ' +
    'development. Large portions of its source code, documentation, and ' +
    'release tooling were produced by AI agents following a spec-driven ' +
    'process, under human direction and review.' + #13#10 + #13#10 +
    'This software is provided "AS IS", without warranty of any kind, ' +
    'express or implied. The author assumes NO responsibility or liability ' +
    'for the use of this software, or for any damage or data loss that may ' +
    'result from it. You install and use this software entirely AT YOUR ' +
    'OWN RISK.' + #13#10 + #13#10 +
    'If you do not agree with these terms, please cancel the installation.');

  { make room for the acceptance checkbox below the memo }
  DisclaimerPage.RichEditViewer.Height :=
    DisclaimerPage.RichEditViewer.Height - ScaleY(28);

  DisclaimerAcceptedCheck := TNewCheckBox.Create(DisclaimerPage);
  DisclaimerAcceptedCheck.Parent := DisclaimerPage.Surface;
  DisclaimerAcceptedCheck.Left := DisclaimerPage.RichEditViewer.Left;
  DisclaimerAcceptedCheck.Top := DisclaimerPage.RichEditViewer.Top +
    DisclaimerPage.RichEditViewer.Height + ScaleY(8);
  DisclaimerAcceptedCheck.Width := DisclaimerPage.RichEditViewer.Width;
  DisclaimerAcceptedCheck.Height := ScaleY(17);
  DisclaimerAcceptedCheck.Caption := CustomMessage('DisclaimerAccept');
  DisclaimerAcceptedCheck.Checked := False;
  DisclaimerAcceptedCheck.OnClick := @DisclaimerCheckClick;
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  if CurPageID = DisclaimerPage.ID then
    WizardForm.NextButton.Enabled := DisclaimerAcceptedCheck.Checked;
end;

