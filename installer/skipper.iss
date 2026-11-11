; Installs Skipper after a single directory page. No welcome, shortcuts
; choice, ready, or finished page.

#define AppVersion "0.1"
#define BuildDir "..\build"
#define FfmpegDir "..\third_party\ffmpeg"

[Setup]
AppId={{7C2E9B14-5A6D-4F38-8E21-0B9D4C6A1E75}
AppName=Skipper
AppVersion={#AppVersion}
DefaultDirName={autopf}\Skipper
DefaultGroupName=Skipper
OutputDir=..\dist
OutputBaseFilename=SkipperSetup-{#AppVersion}
SetupIconFile=..\icons\skipper.ico
UninstallDisplayIcon={app}\skipper.exe
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
DisableStartupPrompt=yes
DisableWelcomePage=yes
DisableDirPage=no
DisableProgramGroupPage=yes
DisableReadyPage=yes
DisableFinishedPage=yes
ShowLanguageDialog=no
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "chinesesimp"; MessagesFile: "ChineseSimplified.isl"

[Files]
Source: "{#BuildDir}\skipper.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\*.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\generic\*"; DestDir: "{app}\generic"; Flags: ignoreversion recursesubdirs
Source: "{#BuildDir}\iconengines\*"; DestDir: "{app}\iconengines"; Flags: ignoreversion recursesubdirs
Source: "{#BuildDir}\imageformats\*"; DestDir: "{app}\imageformats"; Flags: ignoreversion recursesubdirs
Source: "{#BuildDir}\networkinformation\*"; DestDir: "{app}\networkinformation"; Flags: ignoreversion recursesubdirs
Source: "{#BuildDir}\platforms\*"; DestDir: "{app}\platforms"; Flags: ignoreversion recursesubdirs
Source: "{#BuildDir}\styles\*"; DestDir: "{app}\styles"; Flags: ignoreversion recursesubdirs
Source: "{#BuildDir}\tls\*"; DestDir: "{app}\tls"; Flags: ignoreversion recursesubdirs
Source: "{#FfmpegDir}\ffmpeg.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#FfmpegDir}\ffprobe.exe"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\Skipper"; Filename: "{app}\skipper.exe"
