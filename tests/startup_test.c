/* Uses private HKCU keys and a temporary INI, never the system's startup. */
#include <windows.h>
#include "../src/startup.h"
static HANDLE output;
static int failures,fail_ini,fail_after;
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
    if(section && fail_after>0 && !--fail_after) return FALSE;
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
static void write_fixture(const char *path,const char *text)
{
    DWORD written;
    HANDLE file=CreateFileA(path,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,0,NULL);
    if(file==INVALID_HANDLE_VALUE) ExitProcess(2);
    if(!WriteFile(file,text,(DWORD)lstrlenA(text),&written,NULL)) ExitProcess(2);
    CloseHandle(file);
}
static BOOL file_text(const char *path,const char *text)
{
    char buffer[80]; DWORD read=0; HANDLE file;
    file=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    if(file==INVALID_HANDLE_VALUE) return FALSE;
    ReadFile(file,buffer,sizeof(buffer)-1,&read,NULL); CloseHandle(file); buffer[read]=0;
    return !lstrcmpA(buffer,text);
}
static void remove_fixture(const char *directory)
{
    WIN32_FIND_DATAA data; HANDLE search; char pattern[MAX_PATH],path[MAX_PATH];
    wsprintfA(pattern,"%s\\*",directory); search=FindFirstFileA(pattern,&data);
    if(search!=INVALID_HANDLE_VALUE) {
        do {
            if(!lstrcmpA(data.cFileName,".") || !lstrcmpA(data.cFileName,"..")) continue;
            wsprintfA(path,"%s\\%s",directory,data.cFileName);
            if(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) { remove_fixture(path); RemoveDirectoryA(path); }
            else DeleteFileA(path);
        } while(FindNextFileA(search,&data));
        FindClose(search);
    }
}
void startup_test_entry(void)
{
    static char temporary[MAX_PATH],directory[MAX_PATH],executable[MAX_PATH],copy[MAX_PATH],ini[MAX_PATH];
    static char base[128],service[160],run[160],once[160],backup[160],command[STARTUP_TEXT],saved[STARTUP_TEXT],other[32];
    static char managed[MAX_PATH],managed_directory[MAX_PATH],marker[MAX_PATH],blocked[MAX_PATH],foreign[MAX_PATH],pattern[MAX_PATH];
    StartupConfig config;
    StartupConfig managed_config;
    BOOL copied,migrated;
    HANDLE locked,search; WIN32_FIND_DATAA found;
    char alias[MAX_PATH],source_dir[MAX_PATH],source_exe[MAX_PATH];
    static const BYTE boot_image[32]={'M','Z',1,2,3};
    int index; BOOL ascii; DWORD result;
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
    config.boot_directory=NULL; config.boot_image=NULL; config.boot_size=0;
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
    wsprintfA(source_dir,"%s\\\320\340\341\356\367\350\351 \361\362\356\353",directory);
    wsprintfA(source_exe,"%s\\welcome95.exe",source_dir);
    if(!CreateDirectoryA(source_dir,NULL) || !CopyFileA(executable,source_exe,TRUE)) ExitProcess(2);
    config.executable=source_exe;
    GetShortPathNameA(source_exe,alias,sizeof(alias)); ascii=TRUE;
    for(index=0;alias[index];++index) if((BYTE)alias[index]>=127) ascii=FALSE;
    result=startup_configure(&config,TRUE);
    check(ascii ? result==0 : result==ERROR_INVALID_NAME,"legacy direct shell requires an ASCII alias rather than assuming every short path is ASCII");
    if(!result) startup_configure(&config,FALSE);
    /* Exact migration scenario: old metadata/Run entries, with shell manually restored. */
    value(backup,"Owner",source_exe); value(backup,"OriginalShell","Explorer.exe"); value(backup,"HadShell","0"); value(backup,"Valid","1");
    GetShortPathNameA(source_exe,command,sizeof(command)); lstrcatA(command," /shell"); value(backup,"Gate",command);
    wsprintfA(saved,"\"%s\" /welcome",source_exe); value(service,"Welcome95",saved);
    wsprintfA(saved,"\"%s\" /session-ready",source_exe); value(run,"Welcome95",saved);
    wsprintfA(saved,"\"%s\" /startup",source_exe); value(once,"Welcome95",saved);
    config.boot_directory=directory; config.boot_image=boot_image; config.boot_size=sizeof(boot_image);
    fail_ini=1;
    check(startup_configure(&config,TRUE)!=0 && equal_value(backup,"Gate",command) && equal_shell(ini,NULL),
        "failed migration restores the legacy gate and the manually recovered shell");
    check(startup_configure(&config,TRUE)==0 && startup_installed(&config),"install from an ANSI non-ASCII desktop path through an ASCII bootstrap");
    GetPrivateProfileStringA("boot","shell","",command,sizeof(command),ini);
    ascii=TRUE; for(index=0;command[index];++index) if((BYTE)command[index]>=127 || command[index]==' ') ascii=FALSE;
    check(ascii && lstrlenA(command)<128 && GetFileAttributesA(command)!=INVALID_FILE_ATTRIBUTES,
        "SYSTEM.INI receives only an existing ASCII 8.3 executable with no arguments");
    wsprintfA(saved,"\"%s\" /welcome",command);
    check(equal_value(service,"Welcome95",saved),"RunServices also uses the ASCII trampoline rather than the desktop path");
    wsprintfA(saved,"\"%s\" /startup",command);
    check(startup_rearm(&config)==0 && equal_value(once,"Welcome95",saved),"RunOnce rearming preserves ASCII bootstrap routing");
    check(startup_original(&config,saved,sizeof(saved))==0 && !lstrcmpA(saved,"Explorer.exe"),"bootstrap migration preserves the original shell backup");
    check(startup_configure(&config,TRUE)==0 && equal_shell(ini,command),"repeated Enable reuses the active bootstrap");
    check(startup_configure(&config,FALSE)==0 && equal_shell(ini,NULL) && equal_value(service,"Welcome95",NULL),
        "Disable restores the original shell after bootstrap migration");
    check(startup_prepare_logon(source_exe,directory,managed,&copied)==0 && copied &&
        GetFileAttributesA(managed)!=INVALID_FILE_ATTRIBUTES && equal_shell(ini,NULL),
        "install a non-ASCII source into private WINDOWS/LOGON before touching startup");
    wsprintfA(managed_directory,"%s\\LOGON",directory); wsprintfA(marker,"%s\\W95FILES.INI",managed_directory);
    check(file_text(marker,"Welcome95 LOGON\r\n"),"the managed directory has a validated ownership marker");
    lstrcpyA(alias,managed);
    check(startup_prepare_logon(alias,directory,managed,&copied)==0 && !copied,
        "the installed executable does not copy or relaunch itself repeatedly");
    locked=CreateFileA(managed,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    check(locked!=INVALID_HANDLE_VALUE && startup_prepare_logon(source_exe,directory,managed,&copied)!=0 &&
        !copied && GetFileAttributesA(managed)!=INVALID_FILE_ATTRIBUTES && equal_shell(ini,NULL),
        "a locked installed EXE prevents replacement without touching shell or deleting the working copy");
    if(locked!=INVALID_HANDLE_VALUE) CloseHandle(locked);
    check(startup_prepare_logon(source_exe,directory,managed,&copied)==0 && copied,
        "an unlocked owned EXE can be updated through a staged copy");
    wsprintfA(pattern,"%s\\E95*.BAK",managed_directory); search=FindFirstFileA(pattern,&found);
    check(search!=INVALID_HANDLE_VALUE,"updating retains the previous executable in a unique LOGON backup");
    if(search!=INVALID_HANDLE_VALUE) FindClose(search);
    write_fixture(marker,"FOREIGN MARKER");
    check(startup_prepare_logon(source_exe,directory,managed,&copied)==ERROR_INVALID_DATA && !copied,
        "a changed ownership marker prevents overwriting a foreign installation");
    write_fixture(marker,"Welcome95 LOGON\r\n");
    wsprintfA(blocked,"%s\\BLOCKED",directory); CreateDirectoryA(blocked,NULL);
    wsprintfA(foreign,"%s\\LOGON",blocked); CreateDirectoryA(foreign,NULL);
    wsprintfA(foreign,"%s\\LOGON\\WELCOME95.EXE",blocked); write_fixture(foreign,"FOREIGN EXE");
    check(startup_prepare_logon(source_exe,blocked,pattern,&copied)==ERROR_ALREADY_EXISTS &&
        file_text(foreign,"FOREIGN EXE"),"an unowned destination executable is never replaced");
    config.executable=source_exe;
    check(startup_configure(&config,TRUE)==0,"prepare an enabled legacy desktop installation for managed migration");
    GetPrivateProfileStringA("boot","shell","",command,sizeof(command),ini);
    managed_config=config; managed_config.executable=managed; managed_config.boot_directory=managed_directory;
    value(once,"Welcome95","FOREIGN.EXE");
    check(startup_enable_managed(&managed_config,&migrated)==ERROR_BUSY && !migrated &&
        equal_shell(ini,command) && equal_value(backup,"Owner",source_exe),
        "managed migration preserves foreign startup values and the enabled legacy owner");
    wsprintfA(saved,"\"%s\" /startup",command); value(once,"Welcome95",saved);
    fail_after=2;
    check(startup_enable_managed(&managed_config,&migrated)!=0 && !migrated && startup_installed(&config) &&
        equal_shell(ini,command) && equal_value(backup,"Owner",source_exe) && equal_value(once,"Welcome95",saved),
        "failed managed migration restores the previous owner, exact shell and startup entries");
    check(startup_enable_managed(&managed_config,&migrated)==0 && migrated && startup_installed(&managed_config),
        "managed migration enables the LOGON copy and redirects the ASCII bootstrap safely");
    check(startup_original(&managed_config,saved,sizeof(saved))==0 && !lstrcmpA(saved,"Explorer.exe") &&
        recovery_matches(backup,"Explorer.exe"),"managed migration preserves the actual original shell and readable recovery backup");
    wsprintfA(pattern,"%s\\W95*.BAK",managed_directory); search=FindFirstFileA(pattern,&found);
    check(search!=INVALID_HANDLE_VALUE,"new shell recovery backups are created inside LOGON");
    if(search!=INVALID_HANDLE_VALUE) FindClose(search);
    startup_trace(config.root,config.backup_key,"inert-managed-test",7);
    wsprintfA(pattern,"%s\\W95START.LOG",managed_directory);
    check(GetFileAttributesA(pattern)!=INVALID_FILE_ATTRIBUTES,"bootstrap stage logs use the managed owner directory instead of a root-drive folder");
    check(startup_enable_managed(&managed_config,&migrated)==0 && !migrated,
        "repeated managed Enable preserves the active installation");
    check(startup_configure(&managed_config,FALSE)==0 && equal_shell(ini,NULL),
        "Disable from LOGON restores the original absent shell key");
    RegDeleteKeyA(HKEY_CURRENT_USER,service); RegDeleteKeyA(HKEY_CURRENT_USER,run);
    RegDeleteKeyA(HKEY_CURRENT_USER,once); RegDeleteKeyA(HKEY_CURRENT_USER,backup); RegDeleteKeyA(HKEY_CURRENT_USER,base);
    remove_fixture(directory);
    check(RemoveDirectoryA(directory),"remove only private startup fixtures");
    CloseHandle(output); ExitProcess(failures ? 1 : 0);
}
