[Setup]
AppId=PongV3
AppName=Pong
AppVersion=3.0.0
DefaultDirName={localappdata}\Programs\Pong
DefaultGroupName=Pong
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=release
OutputBaseFilename=Pong-Tk-Setup-3.0.0-win-x64
Compression=lzma2
SolidCompression=yes

[Files]
Source: "dist\Pong\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Tasks]
Name: desktopicon; Description: "Create a desktop shortcut"; Flags: unchecked

[Icons]
Name: "{autoprograms}\Pong"; Filename: "{app}\Pong.exe"
Name: "{autodesktop}\Pong"; Filename: "{app}\Pong.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\Pong.exe"; Description: "Launch Pong"; Flags: nowait postinstall skipifsilent
