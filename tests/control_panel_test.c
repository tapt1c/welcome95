/* Private HKCU/INI/files, real DLL loading, inert settings launch callback. */
#include <windows.h>
#include <cpl.h>
#include "../src/control_panel.h"
static HANDLE output;
static int failures,launches,fail_shell,fail_panel_write,fail_panel_flush,drop_panel_write,zero_panel_flush;
static char launched_app[MAX_PATH],launched_command[MAX_PATH+4];
typedef BOOL (WINAPI *Launch)(LPCSTR,LPSTR,LPSECURITY_ATTRIBUTES,LPSECURITY_ATTRIBUTES,BOOL,DWORD,LPVOID,LPCSTR,LPSTARTUPINFOA,LPPROCESS_INFORMATION);
typedef LONG (CALLBACK *Applet)(HWND,UINT,LPARAM,LPARAM);
typedef void (WINAPI *Host)(HKEY,const char *,Launch);
static void check(BOOL okay,const char *text)
{
    DWORD written; const char *prefix=okay ? "PASS: " : "FAIL: ";
    if(!okay) ++failures;
    WriteFile(output,prefix,(DWORD)lstrlenA(prefix),&written,NULL);
    WriteFile(output,text,(DWORD)lstrlenA(text),&written,NULL); WriteFile(output,"\r\n",2,&written,NULL);
}
static BOOL WINAPI inert_launch(LPCSTR app,LPSTR command,LPSECURITY_ATTRIBUTES process_security,
    LPSECURITY_ATTRIBUTES thread_security,BOOL inherit,DWORD flags,LPVOID environment,LPCSTR directory,
    LPSTARTUPINFOA startup,LPPROCESS_INFORMATION child)
{
    (void)process_security; (void)thread_security; (void)inherit; (void)flags;
    (void)environment; (void)directory; (void)startup;
    ++launches; lstrcpynA(launched_app,app,sizeof(launched_app)); lstrcpynA(launched_command,command,sizeof(launched_command));
    child->hProcess=CreateEventA(NULL,TRUE,FALSE,NULL); child->hThread=CreateEventA(NULL,TRUE,FALSE,NULL);
    if(!child->hProcess || !child->hThread) ExitProcess(2);
    return TRUE;
}
static BOOL WINAPI write_ini(LPCSTR section,LPCSTR key,LPCSTR value,LPCSTR file)
{
    if(section && fail_shell) { fail_shell=0; return FALSE; }
    return WritePrivateProfileStringA(section,key,value,file);
}
static BOOL WINAPI write_panel_ini(LPCSTR section,LPCSTR key,LPCSTR data,LPCSTR file)
{
    if(section && fail_panel_write) { fail_panel_write=0; SetLastError(ERROR_ACCESS_DENIED); return FALSE; }
    if(!section && fail_panel_flush) { fail_panel_flush=0; SetLastError(ERROR_WRITE_FAULT); return FALSE; }
    if(section && drop_panel_write) { drop_panel_write=0; return TRUE; }
    if(!section && zero_panel_flush) {
        WritePrivateProfileStringA(section,key,data,file); SetLastError(0); return FALSE;
    }
    return WritePrivateProfileStringA(section,key,data,file);
}
static BOOL ini_is(const char *ini,const char *name,const char *text)
{
    char actual[MAX_PATH]; DWORD length=GetPrivateProfileStringA("MMCPL",name,"\001",actual,sizeof(actual),ini);
    return text ? (!lstrcmpA(actual,text)) : (length==1 && actual[0]==1);
}
static void value(const char *key,const char *name,DWORD type,const void *data,DWORD size)
{
    HKEY handle;
    if(RegCreateKeyExA(HKEY_CURRENT_USER,key,0,NULL,0,KEY_SET_VALUE|KEY_QUERY_VALUE,NULL,&handle,NULL)) ExitProcess(2);
    if(data) RegSetValueExA(handle,name,0,type,(const BYTE *)data,size);
    else RegDeleteValueA(handle,name);
    RegCloseKey(handle);
}
static BOOL record_read(const char *key,PanelRecord *record)
{
    HKEY handle; DWORD type=0,size=sizeof(*record),error;
    if(RegOpenKeyExA(HKEY_CURRENT_USER,key,0,KEY_QUERY_VALUE,&handle)) return FALSE;
    error=RegQueryValueExA(handle,"Entry",NULL,&type,(BYTE *)record,&size); RegCloseKey(handle);
    return !error && type==REG_BINARY && size==sizeof(*record);
}
static BOOL entry_is(const char *key,const char *name,const char *text)
{
    HKEY handle; char data[MAX_PATH]; DWORD type=0,size=sizeof(data),error;
    if(RegOpenKeyExA(HKEY_CURRENT_USER,key,0,KEY_QUERY_VALUE,&handle)) return !text;
    error=RegQueryValueExA(handle,name,NULL,&type,(BYTE *)data,&size); RegCloseKey(handle);
    return text ? (!error && type==REG_SZ && size && size<=sizeof(data) && !data[size-1] && !lstrcmpiA(data,text)) : error==ERROR_FILE_NOT_FOUND;
}
static BOOL shell_is(const char *ini,const char *text)
{
    char actual[STARTUP_TEXT]; GetPrivateProfileStringA("boot","shell","",actual,sizeof(actual),ini);
    return !lstrcmpA(actual,text);
}
static BOOL no_cpl(const char *directory)
{
    char pattern[MAX_PATH]; WIN32_FIND_DATAA data; HANDLE found;
    wsprintfA(pattern,"%s\\W95*.CPL",directory); found=FindFirstFileA(pattern,&data);
    if(found==INVALID_HANDLE_VALUE) return TRUE;
    FindClose(found); return FALSE;
}
static void cleanup(const char *directory)
{
    WIN32_FIND_DATAA data; HANDLE search; char pattern[MAX_PATH],path[MAX_PATH];
    wsprintfA(pattern,"%s\\*",directory); search=FindFirstFileA(pattern,&data);
    if(search==INVALID_HANDLE_VALUE) return;
    do {
        if(!lstrcmpA(data.cFileName,".") || !lstrcmpA(data.cFileName,"..")) continue;
        wsprintfA(path,"%s\\%s",directory,data.cFileName);
        if(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) { cleanup(path); RemoveDirectoryA(path); }
        else DeleteFileA(path);
    } while(FindNextFileA(search,&data));
    FindClose(search);
}
void panel_test_entry(void)
{
    static BYTE image[65536],next_image[65536];
    static char temporary[MAX_PATH],directory[MAX_PATH],logon[MAX_PATH],executable[MAX_PATH],source[MAX_PATH],ini[MAX_PATH],control_ini[MAX_PATH];
    static char base[128],cpls[160],storage[160],services[160],run[160],once[160],backup[160],command[MAX_PATH+4];
    static PanelRecord record,saved;
    static PanelChange change;
    PanelConfig panel; StartupConfig startup;
    static const BYTE boot_image[32]={'M','Z',1,2,3};
    HMODULE first=NULL,second=NULL; HANDLE file; DWORD size=0,error,index,pe; BOOL migrated;
    const char *stage;
    union { FARPROC raw; Applet applet; Host host; } address;
    Applet applet,updated; Host host;
    NEWCPLINFOA info; CPLINFO static_info; char title[32],description[128];
    output=CreateFileA("control-panel-test.log",GENERIC_WRITE,0,NULL,CREATE_ALWAYS,0,NULL);
    if(output==INVALID_HANDLE_VALUE) ExitProcess(2);
    file=CreateFileA("build\\w95panel64.cpl",GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    if(file==INVALID_HANDLE_VALUE || !ReadFile(file,image,sizeof(image),&size,NULL) || size<256 || size>=sizeof(image)) ExitProcess(2);
    CloseHandle(file);
    if(!GetTempPathA(MAX_PATH,temporary) || !GetTempFileNameA(temporary,"WPT",0,directory) ||
        !DeleteFileA(directory) || !CreateDirectoryA(directory,NULL)) ExitProcess(2);
    wsprintfA(logon,"%s\\LOGON",directory); if(!CreateDirectoryA(logon,NULL)) ExitProcess(2);
    wsprintfA(executable,"%s\\WELCOME95.EXE",logon); GetModuleFileNameA(NULL,source,sizeof(source));
    if(!CopyFileA(source,executable,TRUE)) ExitProcess(2);
    wsprintfA(ini,"%s\\SYSTEM.INI",directory); WritePrivateProfileStringA("boot","shell","INERTSHELL.EXE /test",ini);
    wsprintfA(base,"Software\\Welcome95.Tests\\Panel%lu_%lu",GetCurrentProcessId(),GetTickCount());
    wsprintfA(cpls,"%s\\Cpls",base); wsprintfA(storage,"%s\\Panel",base);
    wsprintfA(services,"%s\\Services",base); wsprintfA(run,"%s\\Run",base);
    wsprintfA(once,"%s\\Once",base); wsprintfA(backup,"%s\\Backup",base);
    panel.root=HKEY_CURRENT_USER; panel.cpls_key=cpls; panel.storage_key=storage;
    panel.executable=executable; panel.image=image; panel.size=size;
    wsprintfA(control_ini,"%s\\CONTROL.INI",directory);
    panel.control_ini=control_ini; panel.write_ini=write_panel_ini;
    WritePrivateProfileStringA("MMCPL","Unrelated","KEEP.CPL",control_ini);
    startup.root=HKEY_CURRENT_USER; startup.service_key=services; startup.run_key=run; startup.once_key=once;
    startup.backup_key=backup; startup.executable=executable; startup.ini=ini; startup.write_ini=write_ini;
    startup.boot_directory=logon; startup.boot_image=boot_image; startup.boot_size=sizeof(boot_image);
    value(cpls,"Unrelated",REG_SZ,"KEEP",5);
    fail_panel_write=1; error=panel_startup_configure(&panel,&startup,TRUE,&migrated,&stage);
    check(error==ERROR_ACCESS_DENIED && !lstrcmpA(stage,"panel-ini-write") &&
        entry_is(cpls,PANEL_VALUE,NULL) && !record_read(storage,&record) && no_cpl(logon) &&
        ini_is(control_ini,PANEL_VALUE,NULL) && ini_is(control_ini,"Unrelated","KEEP.CPL") &&
        shell_is(ini,"INERTSHELL.EXE /test"),
        "a denied legacy INI registration rolls back the DLL and registry before startup changes");
    fail_panel_flush=1; error=panel_startup_configure(&panel,&startup,TRUE,&migrated,&stage);
    check(error && !lstrcmpA(stage,"panel-ini-flush") && entry_is(cpls,PANEL_VALUE,NULL) &&
        ini_is(control_ini,PANEL_VALUE,NULL) && no_cpl(logon),
        "a legacy INI flush failure removes even a partially written entry");
    drop_panel_write=1; error=panel_startup_configure(&panel,&startup,TRUE,&migrated,&stage);
    check(error && !lstrcmpA(stage,"panel-ini-verify") && entry_is(cpls,PANEL_VALUE,NULL) &&
        ini_is(control_ini,PANEL_VALUE,NULL) && no_cpl(logon),
        "INI readback rejects a reported write that did not publish the discovery entry");
    fail_shell=1; error=panel_startup_configure(&panel,&startup,TRUE,&migrated,&stage);
    check(error && entry_is(cpls,PANEL_VALUE,NULL) && !record_read(storage,&record) && no_cpl(logon) &&
        shell_is(ini,"INERTSHELL.EXE /test") && ini_is(control_ini,PANEL_VALUE,NULL),"failed first Enable removes its newly created applet and registration without changing the original shell");
    check(panel_startup_configure(&panel,&startup,TRUE,&migrated,&stage)==0 && startup_installed(&startup),
        "Enable installs the private startup gate and Control Panel registration together");
    check(record_read(storage,&record) && entry_is(cpls,PANEL_VALUE,record.module) &&
        !lstrcmpiA(record.owner,executable) && GetFileAttributesA(record.module)!=INVALID_FILE_ATTRIBUTES,
        "the registered owned CPL resides beside the managed executable");
    check(ini_is(control_ini,PANEL_VALUE,record.module) && ini_is(control_ini,"Unrelated","KEEP.CPL"),
        "CONTROL.INI MMCPL discovers the owned LOGON applet without changing unrelated entries");
    zero_panel_flush=1; error=panel_startup_configure(&panel,&startup,TRUE,&migrated,&stage); zero_panel_flush=0;
    check(!error && ini_is(control_ini,PANEL_VALUE,record.module) && startup_installed(&startup),
        "the documented zero return for a successful INI cache flush is not mistaken for failure");
    saved=record;
    GetPrivateProfileStringA("MMCPL",PANEL_VALUE,"",source,sizeof(source),control_ini);
    first=LoadLibraryA(source);
    address.raw=first ? GetProcAddress(first,"CPlApplet") : NULL; applet=address.applet;
    address.raw=first ? GetProcAddress(first,"PanelTestHost") : NULL; host=address.host;
    check(first && applet && host,"load the actual launcher DLL and its undecorated CPlApplet export");
    if(!first || !applet || !host) ExitProcess(2);
    host(HKEY_CURRENT_USER,cpls,inert_launch);
    check(applet(NULL,CPL_INIT,0,0) && applet(NULL,CPL_GETCOUNT,0,0)==1,"the registered launcher exposes exactly one Control Panel item");
    applet(NULL,CPL_INQUIRE,0,(LPARAM)&static_info); applet(NULL,CPL_NEWINQUIRE,0,(LPARAM)&info);
    check(static_info.idIcon==1 && static_info.idName==2 && static_info.idInfo==3 &&
        LoadIconA(first,MAKEINTRESOURCEA(static_info.idIcon)) &&
        LoadStringA(first,static_info.idName,title,sizeof(title)) && !lstrcmpA(title,"Welcome95") &&
        LoadStringA(first,static_info.idInfo,description,sizeof(description)) && description[0],
        "a legacy host gets the embedded icon name and description using CPL_INQUIRE alone");
    check(info.dwSize==sizeof(info) && info.hIcon && !lstrcmpA(info.szName,"Welcome95") && info.szInfo[0],
        "dynamic-metadata hosts retain the same settings item");
    check(FindResourceExA(first,RT_STRING,MAKEINTRESOURCEA(1),MAKELANGID(LANG_RUSSIAN,SUBLANG_DEFAULT))!=NULL &&
        FindResourceExA(first,RT_STRING,MAKEINTRESOURCEA(1),MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_US))!=NULL,
        "static applet descriptions include both Russian and English resource tables");
    wsprintfA(command,"\"%s\"",executable); applet(NULL,CPL_DBLCLK,0,0);
    check(launches==1 && !lstrcmpiA(launched_app,executable) && !lstrcmpA(launched_command,command),
        "double-click launches only the existing managed EXE with a quoted path");
    check(panel_startup_configure(&panel,&startup,TRUE,&migrated,&stage)==0 && record_read(storage,&record) &&
        !lstrcmpiA(record.module,saved.module),"repeated Enable reuses the same loaded owned launcher");
    fail_shell=1; error=panel_startup_configure(&panel,&startup,FALSE,&migrated,&stage);
    check(error && !lstrcmpA(stage,"install-shell-write") && startup_installed(&startup) && applet(NULL,CPL_GETCOUNT,0,0)==1,
        "failed Disable restores the icon and the previously enabled shell gate");
    check(panel_startup_configure(&panel,&startup,FALSE,&migrated,&stage)==0 && !startup_installed(&startup) &&
        entry_is(cpls,PANEL_VALUE,saved.module) && ini_is(control_ini,PANEL_VALUE,saved.module) &&
        applet(NULL,CPL_GETCOUNT,0,0)==1,
        "Disable keeps the settings icon available for reenabling the welcome screen");
    applet(NULL,CPL_DBLCLK,0,0);
    check(launches==2,"settings still launch from Control Panel while the welcome screen is disabled");
    fail_shell=1; error=panel_startup_configure(&panel,&startup,TRUE,&migrated,&stage);
    check(error && entry_is(cpls,PANEL_VALUE,saved.module) && shell_is(ini,"INERTSHELL.EXE /test") &&
        applet(NULL,CPL_GETCOUNT,0,0)==1,"failed Enable preserves the previous icon and the original shell");
    check(panel_startup_configure(&panel,&startup,TRUE,&migrated,&stage)==0 && applet(NULL,CPL_GETCOUNT,0,0)==1,
        "reenabling restores the same launcher and its existing settings action");
    value(cpls,PANEL_VALUE,REG_SZ,"FOREIGN.CPL",12);
    check(panel_startup_configure(&panel,&startup,TRUE,&migrated,&stage)==ERROR_BUSY &&
        entry_is(cpls,PANEL_VALUE,"FOREIGN.CPL") && startup_installed(&startup),
        "a foreign Control Panel registration is preserved without changing startup");
    check(panel_startup_configure(&panel,&startup,FALSE,&migrated,&stage)==0 &&
        entry_is(cpls,PANEL_VALUE,"FOREIGN.CPL") && shell_is(ini,"INERTSHELL.EXE /test"),
        "foreign applet changes cannot prevent restoring the original shell with Disable");
    value(cpls,PANEL_VALUE,REG_SZ,saved.module,(DWORD)lstrlenA(saved.module)+1);
    check(panel_startup_configure(&panel,&startup,TRUE,&migrated,&stage)==0,
        "restoring the owned registration allows normal Enable again");
    check(panel_configure(&panel,TRUE,&change)==0,"snapshot both discovery entries for an ownership-checked rollback");
    WritePrivateProfileStringA("MMCPL",PANEL_VALUE,"FOREIGN.CPL",control_ini);
    check(panel_rollback(&panel,&change)==ERROR_BUSY && ini_is(control_ini,PANEL_VALUE,"FOREIGN.CPL") &&
        entry_is(cpls,PANEL_VALUE,saved.module),
        "rollback preserves a foreign INI replacement made after its snapshot");
    WritePrivateProfileStringA("MMCPL",PANEL_VALUE,saved.module,control_ini);
    check(panel_rollback(&panel,&change)==0 && ini_is(control_ini,PANEL_VALUE,saved.module),
        "rollback completes only after the expected owned discovery entry is restored");
    WritePrivateProfileStringA("MMCPL",PANEL_VALUE,"FOREIGN.CPL",control_ini);
    check(panel_startup_configure(&panel,&startup,TRUE,&migrated,&stage)==ERROR_BUSY &&
        ini_is(control_ini,PANEL_VALUE,"FOREIGN.CPL") && startup_installed(&startup),
        "a foreign legacy discovery entry cannot be overwritten by Enable");
    check(panel_startup_configure(&panel,&startup,FALSE,&migrated,&stage)==0 &&
        ini_is(control_ini,PANEL_VALUE,"FOREIGN.CPL") && shell_is(ini,"INERTSHELL.EXE /test"),
        "foreign CONTROL.INI changes never block restoring the original shell");
    WritePrivateProfileStringA("MMCPL",PANEL_VALUE,NULL,control_ini);
    check(panel_startup_configure(&panel,&startup,TRUE,&migrated,&stage)==0 &&
        record_read(storage,&record) && !lstrcmpiA(record.module,saved.module) &&
        ini_is(control_ini,PANEL_VALUE,saved.module),
        "a registry-only previous install gains legacy discovery without replacing its loaded DLL");
    record=saved; ++record.checksum; value(storage,"Entry",REG_BINARY,&record,sizeof(record));
    check(panel_configure(&panel,TRUE,&change)==ERROR_INVALID_DATA && entry_is(cpls,PANEL_VALUE,saved.module),
        "changed launcher contents cannot be silently overwritten or trusted");
    record=saved; lstrcpyA(record.module,"C:\\FOREIGN.CPL"); value(storage,"Entry",REG_BINARY,&record,sizeof(record));
    check(panel_configure(&panel,TRUE,&change)==ERROR_BUSY,"malformed ownership cannot redirect file operations outside LOGON");
    value(storage,"Entry",REG_BINARY,&saved,sizeof(saved));
    for(index=0;index<size;++index) next_image[index]=image[index];
    pe=(DWORD)image[60]|((DWORD)image[61]<<8)|((DWORD)image[62]<<16)|((DWORD)image[63]<<24);
    if(pe+9>=size) ExitProcess(2);
    next_image[pe+8]^=1; panel.image=next_image;
    check(panel_startup_configure(&panel,&startup,FALSE,&migrated,&stage)==0 &&
        ini_is(control_ini,PANEL_VALUE,saved.module),
        "prepare the normal disabled-state update while retaining the loaded settings icon");
    fail_shell=1; error=panel_startup_configure(&panel,&startup,TRUE,&migrated,&stage);
    check(error && entry_is(cpls,PANEL_VALUE,saved.module) && ini_is(control_ini,PANEL_VALUE,saved.module) &&
        record_read(storage,&record) && !lstrcmpiA(record.module,saved.module) &&
        GetFileAttributesA(saved.module)!=INVALID_FILE_ATTRIBUTES,
        "failed version update restores both discovery routes and the previously loaded module");
    check(panel_startup_configure(&panel,&startup,TRUE,&migrated,&stage)==0 && record_read(storage,&record) &&
        lstrcmpiA(record.module,saved.module) && GetFileAttributesA(saved.module)!=INVALID_FILE_ATTRIBUTES,
        "an updated launcher gets a fresh file without overwriting the still-loaded old module");
    second=LoadLibraryA(record.module); address.raw=second ? GetProcAddress(second,"CPlApplet") : NULL; updated=address.applet;
    address.raw=second ? GetProcAddress(second,"PanelTestHost") : NULL; host=address.host;
    check(second && updated && host,"load the updated launcher beside the old live module");
    if(!second || !updated || !host) ExitProcess(2);
    host(HKEY_CURRENT_USER,cpls,inert_launch);
    check(!applet(NULL,CPL_GETCOUNT,0,0) && updated(NULL,CPL_GETCOUNT,0,0)==1 && ini_is(control_ini,PANEL_VALUE,record.module),
        "only the currently registered module advertises an icon after an update");
    check(panel_startup_configure(&panel,&startup,FALSE,&migrated,&stage)==0 && updated(NULL,CPL_GETCOUNT,0,0)==1 &&
        entry_is(cpls,"Unrelated","KEEP") && shell_is(ini,"INERTSHELL.EXE /test"),
        "final Disable preserves unrelated items and restores the exact original shell");
    check(panel_configure(&panel,FALSE,&change)==0 && !updated(NULL,CPL_GETCOUNT,0,0) &&
        ini_is(control_ini,PANEL_VALUE,NULL) && ini_is(control_ini,"Unrelated","KEEP.CPL"),
        "owned applet unregistration works even with its DLL still loaded");
    updated(NULL,CPL_DBLCLK,0,0);
    check(launches==2,"an unregistered stale module never launches settings");
    FreeLibrary(second); FreeLibrary(first);
    RegDeleteKeyA(HKEY_CURRENT_USER,cpls); RegDeleteKeyA(HKEY_CURRENT_USER,storage); RegDeleteKeyA(HKEY_CURRENT_USER,services);
    RegDeleteKeyA(HKEY_CURRENT_USER,run); RegDeleteKeyA(HKEY_CURRENT_USER,once); RegDeleteKeyA(HKEY_CURRENT_USER,backup); RegDeleteKeyA(HKEY_CURRENT_USER,base);
    cleanup(directory); check(RemoveDirectoryA(directory),"remove only private Control Panel fixtures");
    CloseHandle(output); ExitProcess(failures ? 1 : 0);
}
