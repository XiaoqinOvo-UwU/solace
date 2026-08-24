; 小钦的工具 v4.0.4 安装脚本
#define MyAppName "小钦的工具"
#define MyAppVersion "4.0.4"
#define MyAppExeName "XiaoQinTools.exe"
#define MyAppPublisher "XiaoQinUwU"
#define MyAppURL "https://github.com/XiaoqinOvo-UwU/xiaoqintools"

[Setup]
AppId={{8F5C3E2A-1B4D-4E7C-9A2B-2F6A4C8E1B7D}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
DefaultDirName={autopf}\XiaoQinTools
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=C:\XiaoQinTools\installer
OutputBaseFilename=XiaoQinTools-{#MyAppVersion}-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; 强制覆盖旧的重复安装（同 AppId 自动升级）
UsePreviousAppDir=no
; 安装后可卸载，但保留 %APPDATA% 数据
UninstallDisplayIcon={app}\{#MyAppExeName}

[Languages]
Name: "chinesesimplified"; MessagesFile: "..\ChineseSimplified.isl"

[Tasks]
Name: "desktopicon"; Description: "创建桌面快捷方式"; GroupDescription: "附加任务:"; Flags: unchecked
Name: "startup"; Description: "开机自启动"; GroupDescription: "附加任务:"; Flags: unchecked

[Files]
Source: "C:\XiaoQinTools\dist\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
; 静默安装（自动更新）后也自动启动新程序，确保用户看到的永远是最新版
Filename: "{app}\{#MyAppExeName}"; Description: "启动 {#MyAppName}"; Flags: nowait postinstall

[Code]
// 清理旧副本：删除其他位置残留的 XiaoQinTools 安装（跳过正式安装目录）
procedure CleanupStrayCopies();
var
  appDir: String;
  cands: TStringList;
  i: Integer;
  subDir: String;
begin
  appDir := LowerCase(ExpandConstant('{app}'));

  cands := TStringList.Create;
  try
    // 已知的旧副本候选位置（深度 1-2）
    cands.Add(ExpandConstant('{userdesktop}') + '\xiaoqintools-test');
    cands.Add(ExpandConstant('{userdesktop}') + '\XiaoQinTools');
    cands.Add(ExpandConstant('{userdesktop}') + '\小钦的工具');
    cands.Add(ExpandConstant('{userdocs}') + '\xiaoqintools-test');
    cands.Add(ExpandConstant('{userdocs}') + '\XiaoQinTools');
    cands.Add('C:\XiaoQinTools\dist_old');
    cands.Add('C:\XiaoQinTools\test-311');
    cands.Add('C:\XiaoQinTools\zip-install');

    for i := 0 to cands.Count - 1 do begin
      subDir := cands[i];
      if DirExists(subDir) then begin
        if FileExists(subDir + '\XiaoQinTools.exe') then begin
          if LowerCase(subDir) <> appDir then begin
            // 保留 %APPDATA% 数据，只删程序目录
            DelTree(subDir, True, True, True);
          end;
        end;
      end;
    end;
  finally
    cands.Free;
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssInstall then
    CleanupStrayCopies();
end;
