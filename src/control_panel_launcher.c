/* Win95 Control Panel loads this CPL launcher. Settings stay entirely in the
   adjacent WELCOME95.EXE; the applet only supplies an icon and launch verb. */
#include <windows.h>
#include <cpl.h>
#include "ui_text.h"
#include "control_panel.h"
static HINSTANCE module;
static HKEY storage_root=HKEY_LOCAL_MACHINE;
static const char *cpls_key=PANEL_CPLS;
typedef BOOL (WINAPI *Launch)(LPCSTR,LPSTR,LPSECURITY_ATTRIBUTES,LPSECURITY_ATTRIBUTES,BOOL,DWORD,LPVOID,LPCSTR,LPSTARTUPINFOA,LPPROCESS_INFORMATION);
static Launch launch_process=CreateProcessA;
#ifdef W95_PANEL_TEST
static char test_key[160];
__declspec(dllexport) void WINAPI PanelTestHost(HKEY root,const char *key,Launch launch)
{
    storage_root=root; lstrcpynA(test_key,key,sizeof(test_key)); cpls_key=test_key;
    launch_process=launch;
}
#endif
static void clear_bytes(void *data,UINT size)
{
    volatile BYTE *p=(volatile BYTE *)data;
    while(size--) *p++=0;
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID reserved)
{
    (void)reserved;
    if(reason==DLL_PROCESS_ATTACH) module=instance;
    return TRUE;
}
static BOOL registered(char *path)
{
    HKEY key; char entry[MAX_PATH],a[MAX_PATH],b[MAX_PATH];
    DWORD size=sizeof(entry),type=0,error;
    DWORD length=GetModuleFileNameA(module,path,MAX_PATH);
    if(!length || length>=MAX_PATH || RegOpenKeyExA(storage_root,cpls_key,0,KEY_QUERY_VALUE,&key)) return FALSE;
    error=RegQueryValueExA(key,PANEL_VALUE,NULL,&type,(BYTE *)entry,&size); RegCloseKey(key);
    if(error || type!=REG_SZ || !size || size>sizeof(entry) || entry[size-1]) return FALSE;
    if(!lstrcmpiA(path,entry)) return TRUE;
    return GetShortPathNameA(path,a,sizeof(a)) && GetShortPathNameA(entry,b,sizeof(b)) && !lstrcmpiA(a,b);
}
__declspec(dllexport) LONG CALLBACK CPlApplet(HWND owner,UINT message,LPARAM index,LPARAM parameter)
{
    char path[MAX_PATH];
    (void)index;
    if(message==CPL_INIT) return TRUE;
    if(message==CPL_GETCOUNT) return registered(path) ? 1 : 0;
    if(message==CPL_INQUIRE) {
        CPLINFO *info=(CPLINFO *)parameter;
        info->idIcon=1; info->idName=2; info->idInfo=3; info->lData=0;
    }
    if(message==CPL_NEWINQUIRE) {
        NEWCPLINFOA *info=(NEWCPLINFOA *)parameter;
        clear_bytes(info,sizeof(*info)); info->dwSize=sizeof(*info);
        info->hIcon=LoadIconA(module,MAKEINTRESOURCEA(1));
        lstrcpyA(info->szName,"Welcome95");
        lstrcpyA(info->szInfo,ui_text("\315\340\361\362\360\356\351\352\350 \375\352\360\340\355\340 \357\360\350\342\345\362\361\362\342\350\377 Welcome95.","Welcome95 welcome screen settings."));
    }
    if((message==CPL_DBLCLK || message==CPL_STARTWPARMSA) && registered(path)) {
        STARTUPINFOA startup; PROCESS_INFORMATION child;
        char command[MAX_PATH+4],error_text[180]; int end=lstrlenA(path); DWORD error;
        while(end && path[end-1]!='\\' && path[end-1]!='/') --end;
        if(!end || end+14>=MAX_PATH) return 0;
        lstrcpyA(path+end,"WELCOME95.EXE"); wsprintfA(command,"\"%s\"",path);
        clear_bytes(&startup,sizeof(startup)); startup.cb=sizeof(startup);
        if(launch_process(path,command,NULL,NULL,FALSE,0,NULL,NULL,&startup,&child)) {
            CloseHandle(child.hThread); CloseHandle(child.hProcess);
        } else {
            error=GetLastError();
            wsprintfA(error_text,ui_text("\315\345 \363\344\340\353\356\361\374 \356\362\352\360\373\362\374 \355\340\361\362\360\356\351\352\350 Welcome95 (\356\370\350\341\352\340 %lu).","Cannot open Welcome95 settings (error %lu)."),error);
            MessageBoxA(owner,error_text,"Welcome95",MB_OK|MB_ICONERROR);
        }
        return TRUE;
    }
    return 0;
}
