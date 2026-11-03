#include "ui_text.h"
/* Win95 loads this ASCII 8.3 image without arguments from SYSTEM.INI.
   The full ANSI owner path is passed only to Win32 CreateProcessA. */
#include "bootstrap.h"
#include "startup.h"
static BOOL ending;
static LRESULT CALLBACK boot_proc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp)
{
    if(message==WM_QUERYENDSESSION) return TRUE;
    if(message==WM_ENDSESSION && wp) { ending=TRUE; return 0; }
    return DefWindowProcA(hwnd,message,wp,lp);
}
DWORD bootstrap_forward(HKEY root,const char *key,const char *mode)
{
    HKEY handle; HWND window;
    WNDCLASSA cls={0}; STARTUPINFOA startup={0}; PROCESS_INFORMATION child;
    char owner[MAX_PATH],command[MAX_PATH+40],valid[8];
    DWORD size,type,error,waited,exit_code=0;
    MSG message;
    if(!mode || (lstrcmpA(mode,"/shell") && lstrcmpA(mode,"/startup") &&
       lstrcmpA(mode,"/welcome") && lstrcmpA(mode,"/session-ready"))) return ERROR_INVALID_PARAMETER;
    startup_trace(root,key,"bootstrap-enter",0);
    error=RegOpenKeyExA(root,key,0,KEY_QUERY_VALUE,&handle); if(error) return error;
    size=sizeof(valid); type=0;
    error=RegQueryValueExA(handle,"Valid",NULL,&type,(BYTE *)valid,&size);
    if(!error && (type!=REG_SZ || size!=2 || valid[0]!='1' || valid[1])) error=ERROR_INVALID_DATA;
    size=sizeof(owner); type=0;
    if(!error) error=RegQueryValueExA(handle,"Owner",NULL,&type,(BYTE *)owner,&size);
    RegCloseKey(handle);
    if(error) return error;
    if(type!=REG_SZ || !size || size>=sizeof(owner) || owner[size-1] || !owner[0]) return ERROR_INVALID_DATA;
    cls.hInstance=GetModuleHandleA(NULL); cls.lpfnWndProc=boot_proc; cls.lpszClassName="Welcome95.Bootstrap";
    if(!RegisterClassA(&cls) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) return GetLastError();
    window=CreateWindowA(cls.lpszClassName,ui_text("\307\340\343\360\363\347\367\350\352 Welcome95","Welcome95 bootstrap"),WS_OVERLAPPED,0,0,0,0,NULL,NULL,cls.hInstance,NULL);
    if(!window) return GetLastError();
    startup.cb=sizeof(startup); wsprintfA(command,"\"%s\" %s",owner,mode);
    if(!CreateProcessA(owner,command,NULL,NULL,FALSE,0,NULL,NULL,&startup,&child)) {
        error=GetLastError(); DestroyWindow(window); startup_trace(root,key,"bootstrap-create-failed",error); return error;
    }
    CloseHandle(child.hThread); startup_trace(root,key,"bootstrap-owner-started",child.dwProcessId);
    if(!lstrcmpA(mode,"/shell") || !lstrcmpA(mode,"/startup")) {
        ending=FALSE;
        while(!ending) {
            waited=MsgWaitForMultipleObjects(1,&child.hProcess,FALSE,INFINITE,QS_ALLINPUT);
            if(waited==WAIT_OBJECT_0) break;
            if(waited==WAIT_FAILED) { exit_code=GetLastError(); break; }
            while(PeekMessageA(&message,NULL,0,0,PM_REMOVE)) {
                if(message.message==WM_QUIT) { ending=TRUE; break; }
                TranslateMessage(&message); DispatchMessageA(&message);
            }
        }
        if(!ending && !exit_code) GetExitCodeProcess(child.hProcess,&exit_code);
    }
    CloseHandle(child.hProcess); DestroyWindow(window);
    startup_trace(root,key,"bootstrap-return",exit_code); return exit_code;
}
#ifndef WELCOME95_BOOTSTRAP_NO_ENTRY
void boot_entry(void)
{
    const char *text=GetCommandLineA(),*mode="/shell";
    DWORD error;
    char message[240]; HANDLE stop;
    if(*text=='"') { ++text; while(*text && *text!='"') ++text; if(*text) ++text; }
    else while(*text && *text!=' ' && *text!='\t') ++text;
    while(*text==' ' || *text=='\t') ++text;
    if(*text) mode=text;
    error=bootstrap_forward(HKEY_LOCAL_MACHINE,STARTUP_BACKUP,mode);
    if(error && error!=ERROR_CANCELLED) {
        stop=OpenEventA(EVENT_MODIFY_STATE,FALSE,"Welcome95.Stop");
        if(stop) { SetEvent(stop); CloseHandle(stop); }
        wsprintfA(message,ui_text("\315\345 \363\344\340\353\356\361\374 \347\340\357\363\361\362\350\362\374 \347\340\343\360\363\347\367\350\352 Welcome95 (\356\370\350\341\352\340 %lu).\r\n\315\340\346\354\350\362\345 \253\316\362\352\353\376\367\350\362\374\273 \342 \355\340\361\362\360\356\351\352\340\365 Welcome95 \350\353\350 \342\356\361\361\362\340\355\356\342\350\362\345 \361\356\365\360\340\355\270\355\355\373\351 \357\340\360\340\354\345\362\360 shell \342 SYSTEM.INI.","Welcome95 bootstrap could not start (error %lu).\r\nUse the main Welcome95 settings to Disable or restore the saved SYSTEM.INI shell."),error);
        MessageBoxA(NULL,message,"Welcome95",MB_OK|MB_ICONERROR|MB_TOPMOST);
    }
    ExitProcess(error);
}
#endif
