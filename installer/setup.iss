; 小钦的工具 v4.1.0 安装脚本
#define MyAppName "小钦的工具"
#define MyAppVersion "4.1.0"
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
; output into the script directory (repo-local, no hardcoded machine paths)
OutputDir=.
OutputBaseFilename=XiaoQinTools-{#MyAppVersion}-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; 强制覆盖旧的重复安装（同 AppId 自动升级）
UsePreviousAppDir=no
; 安装后可卸载，但默认保留 %APPDATA% 数据（可在卸载时勾选删除）
UninstallDisplayIcon={app}\{#MyAppExeName}

[Languages]
Name: "chinesesimplified"; MessagesFile: "..\ChineseSimplified.isl"

[Tasks]
Name: "desktopicon"; Description: "创建桌面快捷方式"; GroupDescription: "附加任务:"; Flags: unchecked
Name: "startup"; Description: "开机自启动"; GroupDescription: "附加任务:"; Flags: unchecked

[Files]
Source: "..\dist\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
; 静默安装（自动更新）后也自动启动新程序，确保用户看到的永远是最新版
Filename: "{app}\{#MyAppExeName}"; Description: "启动 {#MyAppName}"; Flags: nowait postinstall

[Code]
// 清理旧副本：仅允许删除 {autopf}（Program Files）下与本应用关联的目录。
// 绝不动用户桌面 / 文档 / 其他 shell 文件夹（防止误删用户同名个人文件夹）。
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
    // 只列 {autopf} 下的旧安装目录候选
    cands.Add(ExpandConstant('{autopf}') + '\XiaoQinTools');
    cands.Add(ExpandConstant('{autopf}') + '\XiaoQinTools_test');
    cands.Add(ExpandConstant('{autopf}') + '\xiaoqintools');

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

// ---- uninstall: optional "delete user data" confirmation (safe default: No) ----
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usUninstall then begin
    // opt-in only: default is to KEEP %APPDATA% data. Deleting requires an
    // explicit Yes on the confirmation prompt.
    if MsgBox('是否同时删除用户数据与配置？' + Chr(13) + Chr(10) + Chr(13) + Chr(10) +
              ExpandConstant('{userappdata}\XiaoQinTools') +
              Chr(13) + Chr(10) + Chr(13) + Chr(10) + '包含 API 配置、记忆等个人数据，删除后不可恢复。',
              mbConfirmation, MB_YESNO) = IDYES then
      DelTree(ExpandConstant('{userappdata}\XiaoQinTools'), True, True, True);
  end;
end;
