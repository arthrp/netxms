; Script generated by the Inno Setup Script Wizard.
; SEE THE DOCUMENTATION FOR DETAILS ON CREATING INNO SETUP SCRIPT FILES!

[Setup]
#include "setup.iss"
OutputBaseFilename=nxagent-{#VersionString}-x64
ArchitecturesInstallIn64BitMode=x64
ArchitecturesAllowed=x64

[Files]
; Installer helpers
Source: "..\..\..\x64\release\nxreload.exe"; DestDir: "{tmp}"; Flags: dontcopy signonce
; Agent
Source: "..\..\..\x64\release\libethernetip.dll"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\release\libnetxms.dll"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\release\libnxagent.dll"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\release\libnxdb.dll"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\release\libnxjava.dll"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\release\libnxlp.dll"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\release\libnxsnmp.dll"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\release\appagent.dll"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\release\nxagentd.exe"; DestDir: "{app}\bin"; BeforeInstall: RenameOldFile; Flags: ignoreversion signonce
Source: "..\..\..\x64\Release\nxsagent.exe"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\release\bind9.nsm"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\release\db2.nsm"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\release\dbquery.nsm"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\release\devemu.nsm"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\release\ecs.nsm"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\release\filemgr.nsm"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\Release\informix.nsm"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\Release\java.nsm"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\release\logwatch.nsm"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\release\mqtt.nsm"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\release\mysql.nsm"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\release\netsvc.nsm"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\Release\oracle.nsm"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\release\pgsql.nsm"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\release\ping.nsm"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\release\portcheck.nsm"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\release\sms.nsm"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\release\ssh.nsm"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\release\tuxedo.nsm"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\release\ups.nsm"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\release\wineventsync.nsm"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\release\winnt.nsm"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\release\winperf.nsm"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\release\wmi.nsm"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\Release\db2.ddr"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\Release\informix.ddr"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\Release\mariadb.ddr"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\Release\mssql.ddr"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\Release\mysql.ddr"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\Release\odbc.ddr"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\Release\oracle.ddr"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\Release\pgsql.ddr"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\Release\sqlite.ddr"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\contrib\nxagentd.conf-dist"; DestDir: "{app}\etc"; Flags: ignoreversion
Source: "..\..\..\x64\Release\libexpat.dll"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\Release\libpng.dll"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\Release\nxsqlite.dll"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\Release\nxzlib.dll"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\Release\jansson.dll"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\install\files\windows\x64\libcrypto.dll"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\install\files\windows\x64\libcurl.dll"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\install\files\windows\x64\libmosquitto.dll"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\install\files\windows\x64\libssh.dll"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\install\files\windows\x64\libssl.dll"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\install\files\windows\x64\openssl.exe"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\install\files\windows\x64\pcre.dll"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\install\files\windows\x64\pcre16.dll"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\install\files\windows\x64\CRT\*"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "..\..\java-common\netxms-base\target\netxms-base-{#VersionString}.jar"; DestDir: "{app}\lib\java"; Flags: ignoreversion
Source: "..\..\libnxjava\java\target\netxms-java-bridge-{#VersionString}.jar"; DestDir: "{app}\lib\java"; Flags: ignoreversion
Source: "..\subagents\java\java\target\netxms-agent-{#VersionString}.jar"; DestDir: "{app}\lib\java"; Flags: ignoreversion
Source: "..\subagents\jmx\target\jmx.jar"; DestDir: "{app}\lib\java"; Flags: ignoreversion
Source: "..\subagents\ubntlw\target\ubntlw.jar"; DestDir: "{app}\lib\java"; Flags: ignoreversion
; Command-line tools
Source: "..\..\..\x64\release\nxaevent.exe"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\release\nxappget.exe"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\release\nxapush.exe"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\release\nxcsum.exe"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\release\nxethernetip.exe"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\release\nxevent.exe"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\release\nxhwid.exe"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\release\nxpush.exe"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\release\nxsnmpget.exe"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\release\nxsnmpset.exe"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\release\nxsnmpwalk.exe"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
Source: "..\..\..\x64\release\libnxclient.dll"; DestDir: "{app}\bin"; Flags: ignoreversion signonce
; User agent
Source: "..\..\..\x64\release\nxuseragent.exe"; DestDir: "{app}\bin"; Flags: ignoreversion signonce

;#include "custom.iss"

#include "common.iss"

Function GetCustomCmdLine(Param: String): String;
Begin
  Result := '';
End;
