; Inno Setup Script for DiscNativePro (Windows x64)
; Produces DiscNativePro-Windows-x64-Setup.exe

#ifndef MyAppVersion
#error MyAppVersion must be supplied by the release build with /DMyAppVersion=...
#endif

#define MyAppName "DiscNativePro"
#define MyAppPublisher "DiscNativePro"
#define MyAppURL "https://github.com/axel-molina/DiscNativePro"
#define MyAppExeName "DiscNativePro.exe"

[Setup]
; Unique GUID for DiscNativePro
AppId={{D37E9E8C-4F71-4A59-8809-5D8840BA031F}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DisableProgramGroupPage=yes
UninstallDisplayIcon={app}\{#MyAppExeName}
SetupIconFile=..\..\assets\icon.ico
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
OutputDir=..\..\
OutputBaseFilename=DiscNativePro-Windows-x64-Setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequiredOverridesAllowed=dialog commandline

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "..\..\dist\DiscNativePro.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\dist\WebView2Loader.dll"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
