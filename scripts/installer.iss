; FlowSight 商业化专业安装向导配置脚本 (Inno Setup 6)
; 适用于 Windows 10/11 x64 纯净环境一键安装

#define MyAppName "FlowSight"
#define MyAppVersion "1.2.0"
#define MyAppPublisher "FlowSight AI Team"
#define MyAppURL "https://github.com/huangjingpei/FlowSight"
#define MyAppExeName "FlowSight.exe"

[Setup]
AppId={{D86A34F5-6B9E-4340-9E11-9F22DA53C61B}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
LicenseFile=
OutputDir=..\dist
OutputBaseFilename=FlowSight_v1.2.0_Setup
Compression=lzma2/ultra64
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern
PrivilegesRequired=lowest
DisableWelcomePage=no

[Languages]
Name: "chinesesimp"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "quicklaunchicon"; Description: "创建快速启动栏快捷方式"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; 复制便携式发布目录下的全部文件与子目录
Source: "..\dist\FlowSight_v1.2.0_Portable\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\{#MyAppName}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autoprograms}\{#MyAppName}\卸载 {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
