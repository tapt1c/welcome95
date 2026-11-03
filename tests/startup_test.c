/* Uses private HKCU keys and a temporary INI, never the system's startup. */
#include <windows.h>
#include "../src/startup.h"
static HANDLE output;
static int failures,fail_ini;
static void check(BOOL okay,const char *text)
{
    DWORD written;
    const char *prefix=okay ? "PASS: " : "FAIL: ";
    if(!okay) ++failures;
    WriteFile(output,prefix,(DWORD)lstrlenA(prefix),&written,NULL);
    WriteFile(output,text,(DWORD)lstrlenA(text),&written,NULL); WriteFile(output,"\r\n",2,&written,NULL);
}
static BOOL WINAPI test_write(LPCSTR section,LPCSTR key,LPCSTR value,LPCSTR path)
{
    if(fail_ini==1) { fail_ini=0; return FALSE; }
    if(fail_ini==2) { fail_ini=0; return WritePrivateProfileStringA(section,key,"WRONG.EXE",path); }
    return WritePrivateProfileStringA(section,key,value,path);
}
static void value(const char *key,const char *name,const char *text)
{
    HKEY handle;
    RegCreateKeyExA(HKEY_CURRENT_USER,key,0,NULL,0,KEY_SET_VALUE,NULL,&handle,NULL);
    if(text) RegSetValueExA(handle,name,0,REG_SZ,(const BYTE *)text,(DWORD)lstrlenA(text)+1);
    else RegDeleteValueA(handle,name);
    RegCloseKey(handle);
}
static BOOL equal_value(const char *key,const char *name,const char *text)
{
    HKEY handle;
    char data[STARTUP_TEXT]; DWORD size=sizeof(data),type,error;
    if(RegOpenKeyExA(HKEY_CURRENT_USER,key,0,KEY_QUERY_VALUE,&handle)) return !text;
    error=RegQueryValueExA(handle,name,NULL,&type,(BYTE *)data,&size); RegCloseKey(handle);
    return text ? (!error && type==REG_SZ && size && !lstrcmpA(text,data)) : error==ERROR_FILE_NOT_FOUND;
}
static BOOL equal_shell(const char *path,const char *text)
{
    char data[STARTUP_TEXT];
    GetPrivateProfileStringA("boot","shell","ABSENT",data,sizeof(data),path);
    return !lstrcmpA(data,text ? text : "ABSENT");
}
static BOOL recovery_matches(const char *key,const char *shell)
{
    HKEY handle; HANDLE file;
    char path[MAX_PATH],data[STARTUP_TEXT+32],expected[STARTUP_TEXT+32];
    DWORD size=sizeof(path),type,read=0;
    if(RegOpenKeyExA(HKEY_CURRENT_USER,key,0,KEY_QUERY_VALUE,&handle)) return FALSE;
    type=0;
    if(RegQueryValueExA(handle,"RecoveryFile",NULL,&type,(BYTE *)path,&size) || type!=REG_SZ) { RegCloseKey(handle); return FALSE; }
    RegCloseKey(handle);
    file=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    if(file==INVALID_HANDLE_VALUE) return FALSE;
    if(!ReadFile(file,data,sizeof(data)-1,&read,NULL)) read=0;
    CloseHandle(file); data[read]=0; wsprintfA(expected,"[boot]\r\nshell=%s\r\n",shell);
    return read>0 && !lstrcmpA(data,expected);
}
void startup_test_entry(void)
{
    static char temporary[MAX_PATH],directory[MAX_PATH],executable[MAX_PATH],copy[MAX_PATH],ini[MAX_PATH];
    static char base[128],service[160],run[160],once[160],backup[160],command[STARTUP_TEXT],saved[STARTUP_TEXT],other[32];
    StartupConfig config;
    WIN32_FIND_DATAA data; HANDLE search;
    char pattern[MAX_PATH],path[MAX_PATH],alias[MAX_PATH];
    output=CreateFileA("startup-test.log",GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(output==INVALID_HANDLE_VALUE) ExitProcess(2);
    if(!GetTempPathA(MAX_PATH,temporary) || !GetTempFileNameA(temporary,"WST",0,directory) ||
       !DeleteFileA(directory) || !CreateDirectoryA(directory,NULL)) ExitProcess(2);
    GetModuleFileNameA(NULL,executable,sizeof(executable));
    wsprintfA(copy,"%s\\Welcome95 long filename.exe",directory); wsprintfA(ini,"%s\\SYSTEM.INI",directory);
    if(!CopyFileA(executable,copy,TRUE)) ExitProcess(2);
    wsprintfA(base,"Software\\Welcome95.Tests\\Startup%lu",GetCurrentProcessId());
    wsprintfA(service,"%s\\Services",base); wsprintfA(run,"%s\\Run",base);
    wsprintfA(once,"%s\\Once",base); wsprintfA(backup,"%s\\Backup",base);
    config.root=HKEY_CURRENT_USER; config.service_key=service; config.run_key=run;
    config.once_key=once; config.backup_key=backup; config.executable=copy; config.ini=ini; config.write_ini=test_write;
    WritePrivateProfileStringA("boot","shell","EXPLOR95.EXE /desktop",ini);
    WritePrivateProfileStringA("Other","Keep","KEEP",ini);
    value(run,"Unrelated","KEEP"); value(service,"Unrelated","KEEP"); value(once,"Unrelated","KEEP");
    check(!startup_installed(&config),"a fresh fixture is not enabled");
    check(startup_original(&config,saved,sizeof(saved))==ERROR_FILE_NOT_FOUND,"a missing shell backup is never fabricated");
    wsprintfA(command,"\"%s\" /shell",copy); WritePrivateProfileStringA("boot","shell",command,ini);
    check(startup_configure(&config,TRUE)==ERROR_INVALID_DATA,"an alternate spelling of the proxy cannot become a recursive original shell");
    WritePrivateProfileStringA("boot","shell","EXPLOR95.EXE /desktop",ini);
    value(run,"Welcome95","FOREIGN.EXE");
    check(startup_configure(&config,TRUE)==ERROR_BUSY && equal_shell(ini,"EXPLOR95.EXE /desktop"),"another installation's entry prevents any shell modification");
    value(run,"Welcome95",NULL);
    fail_ini=1;
    check(startup_configure(&config,TRUE)!=0 && equal_shell(ini,"EXPLOR95.EXE /desktop"),"an INI write failure keeps the original shell");
    check(equal_value(run,"Welcome95",NULL) && equal_value(service,"Welcome95",NULL) && equal_value(once,"Welcome95",NULL),"failed installation rolls back all three owned startup values");
    check(recovery_matches(backup,"EXPLOR95.EXE /desktop"),"a readable DOS recovery file survives a rolled-back installation");
    fail_ini=2;
    check(startup_configure(&config,TRUE)!=0 && equal_shell(ini,"EXPLOR95.EXE /desktop"),"INI readback mismatch rolls back a write that claimed success");
    check(startup_configure(&config,TRUE)==0 && startup_installed(&config),"install the service, synchronous gate and shell proxy in the isolated fixture");
    check(startup_original(&config,saved,sizeof(saved))==0 && !lstrcmpA(saved,"EXPLOR95.EXE /desktop"),"original shell parameters survive installation exactly");
    check(recovery_matches(backup,"EXPLOR95.EXE /desktop"),"the original shell is flushed to a unique readable BAK beside the executable");
    GetShortPathNameA(copy,alias,sizeof(alias)); config.executable=alias;
    check(startup_installed(&config) && startup_original(&config,saved,sizeof(saved))==0 && !lstrcmpA(saved,"EXPLOR95.EXE /desktop"),
        "a short-path shell invocation owns the long-path installation and reads its original shell");
    check(startup_rearm(&config)==0,"short-path invocation can rearm the long-path installation");
    config.executable=copy;
    GetShortPathNameA(copy,command,sizeof(command)); lstrcatA(command," /shell");
    check(equal_shell(ini,command),"SYSTEM.INI points to the short-path shell gate");
    check(startup_configure(&config,TRUE)==0 && startup_original(&config,saved,sizeof(saved))==0 && !lstrcmpA(saved,"EXPLOR95.EXE /desktop"),"repeated Enable cannot back up the proxy as the original shell");
    check(equal_value(run,"Unrelated","KEEP") && equal_value(service,"Unrelated","KEEP") && equal_value(once,"Unrelated","KEEP"),"unrelated startup entries stay unchanged");
    GetPrivateProfileStringA("Other","Keep","",other,sizeof(other),ini);
    check(!lstrcmpA(other,"KEEP"),"other INI sections remain unchanged");
    value(once,"Welcome95",NULL);
    check(startup_installed(&config) && startup_rearm(&config)==0,"consumed RunOnce does not disable the install and can be rearmed");
    value(once,"Welcome95","FOREIGN.EXE");
    check(startup_rearm(&config)==ERROR_BUSY && equal_value(once,"Welcome95","FOREIGN.EXE"),"rearming cannot overwrite a foreign RunOnce value");
    value(once,"Welcome95",NULL); startup_rearm(&config);
    WritePrivateProfileStringA("boot","shell","NEW.EXE",ini);
    check(startup_configure(&config,FALSE)==ERROR_BUSY && equal_shell(ini,"NEW.EXE"),"Disable refuses to clobber a shell changed externally while enabled");
    WritePrivateProfileStringA("boot","shell",command,ini);
    fail_ini=1;
    check(startup_configure(&config,FALSE)!=0 && startup_installed(&config),"failed shell restoration leaves a recoverable enabled installation");
    config.executable=alias;
    check(startup_configure(&config,FALSE)==0 && equal_shell(ini,"EXPLOR95.EXE /desktop") && !startup_installed(&config),"Disable restores the original shell and removes only owned startup values");
    config.executable=copy;
    check(startup_original(&config,saved,sizeof(saved))==0 && !lstrcmpA(saved,"EXPLOR95.EXE /desktop"),"Disable retains recovery data for an in-flight shell gate");
    check(recovery_matches(backup,"EXPLOR95.EXE /desktop"),"Disable also retains its DOS-readable shell recovery file");
    WritePrivateProfileStringA("boot","shell","CHANGED.EXE /new",ini);
    check(startup_configure(&config,TRUE)==0 && startup_original(&config,saved,sizeof(saved))==0 && !lstrcmpA(saved,"CHANGED.EXE /new"),"reenabling after Disable backs up the currently configured shell");
    check(startup_configure(&config,FALSE)==0 && equal_shell(ini,"CHANGED.EXE /new"),"the updated shell also restores correctly");
    config.executable=executable;
    check(startup_configure(&config,TRUE)==0 && startup_installed(&config),"a new copy may take over after the old copy was disabled");
    check(startup_configure(&config,FALSE)==0 && equal_shell(ini,"CHANGED.EXE /new"),"the new copy preserves the disabled copy's current shell");
    WritePrivateProfileStringA("boot","shell",NULL,ini);
    check(startup_configure(&config,TRUE)==0 && startup_original(&config,saved,sizeof(saved))==0 && !lstrcmpA(saved,"Explorer.exe"),"an absent shell key preserves Windows' default shell semantics");
    check(startup_configure(&config,FALSE)==0 && equal_shell(ini,NULL),"Disable restores an originally absent shell key as absent");
    RegDeleteKeyA(HKEY_CURRENT_USER,service); RegDeleteKeyA(HKEY_CURRENT_USER,run);
    RegDeleteKeyA(HKEY_CURRENT_USER,once); RegDeleteKeyA(HKEY_CURRENT_USER,backup); RegDeleteKeyA(HKEY_CURRENT_USER,base);
    wsprintfA(pattern,"%s\\W95*.BAK",directory); search=FindFirstFileA(pattern,&data);
    if(search!=INVALID_HANDLE_VALUE) {
        do { wsprintfA(path,"%s\\%s",directory,data.cFileName); DeleteFileA(path); } while(FindNextFileA(search,&data));
        FindClose(search);
    }
    check(DeleteFileA(ini) && DeleteFileA(copy) && RemoveDirectoryA(directory),"remove only private startup fixtures");
    CloseHandle(output); ExitProcess(failures ? 1 : 0);
}
