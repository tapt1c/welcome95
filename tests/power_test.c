/* UI and dispatch tests: all power and DOS callbacks below are inert. */
#include "../src/welcome95.c"
static HANDLE output;
static int failures,actions,standbys,dos_calls;
static UINT last_flags;
static DWORD exit_error,dos_error;
static BOOL exit_result=TRUE;
static void check(BOOL okay,const char *text)
{
    DWORD written;
    const char *prefix=okay ? "PASS: " : "FAIL: ";
    if(!okay) ++failures;
    WriteFile(output,prefix,(DWORD)lstrlenA(prefix),&written,NULL);
    WriteFile(output,text,(DWORD)lstrlenA(text),&written,NULL); WriteFile(output,"\r\n",2,&written,NULL);
}
static BOOL WINAPI inert_exit(UINT flags,DWORD reason)
{
    ++actions; last_flags=flags; check(!reason && !(flags&EWX_FORCE),"session requests never force other applications closed");
    SetLastError(exit_error); return exit_result;
}
static BOOL inert_standby(void) { ++standbys; return TRUE; }
static DWORD inert_dos(HWND owner) { (void)owner; ++dos_calls; return dos_error; }
static void inert_perform(PowerAction action,BOOL dos)
{
    ++actions; last_flags=(UINT)action;
    check(!FindWindowA("Welcome95.PowerChoice",NULL) && !FindWindowA("Welcome95.PowerShade",NULL),"remove the choice and shade before dispatching a power command");
    check(IsWindowEnabled(screen) && IsWindowEnabled(login_window),"restore welcome and native input before OS save prompts");
    check(action==POWER_RESTART && dos,"MS-DOS checkbox dispatches restart with DOS rather than normal shutdown");
}
static void pump(DWORD milliseconds)
{
    DWORD started=GetTickCount(); MSG message;
    while((DWORD)(GetTickCount()-started)<milliseconds) {
        while(PeekMessageA(&message,NULL,0,0,PM_REMOVE)) {
            if(!power_dialog_translate(&message)) { TranslateMessage(&message); DispatchMessageA(&message); }
        }
        Sleep(5);
    }
}
static BOOL above(HWND first,HWND second)
{
    HWND window=GetTopWindow(NULL);
    while(window) { if(window==first) return TRUE; if(window==second) return FALSE; window=GetWindow(window,GW_HWNDNEXT); }
    return FALSE;
}
static void dispatcher_tests(void)
{
    PowerOperations ops;
    ops.version=0x80000004UL; ops.standby_available=TRUE; ops.dos_available=TRUE;
    ops.end_session=inert_exit; ops.standby=inert_standby; ops.dos=inert_dos;
    actions=standbys=dos_calls=0;
    check(power_request(POWER_OFF,FALSE,NULL,&ops)==0 && last_flags==EWX_SHUTDOWN,"Turn Off maps to the native shutdown flag");
    check(power_request(POWER_RESTART,FALSE,NULL,&ops)==0 && last_flags==EWX_REBOOT,"Restart maps to the native reboot flag");
    check(power_request(POWER_STANDBY,FALSE,NULL,&ops)==0 && standbys==1,"Stand By reaches the optional suspend callback");
    check(power_request(POWER_RESTART,TRUE,NULL,&ops)==0 && dos_calls==1 && actions==2,"MS-DOS never falls through to normal ExitWindowsEx");
    dos_error=ERROR_WRITE_FAULT;
    check(power_request(POWER_RESTART,TRUE,NULL,&ops)==ERROR_WRITE_FAULT && actions==2,"a DOS preparation failure remains a DOS error without power fallback");
    check(power_request(POWER_OFF,TRUE,NULL,&ops)==ERROR_INVALID_PARAMETER,"DOS cannot be combined with Turn Off");
    ops.standby_available=FALSE;
    check(power_request(POWER_STANDBY,FALSE,NULL,&ops)==ERROR_NOT_SUPPORTED && standbys==1,"unavailable Stand By does not invoke an API");
    ops.dos_available=FALSE;
    check(power_request(POWER_RESTART,TRUE,NULL,&ops)==ERROR_NOT_SUPPORTED && dos_calls==2,"unavailable DOS does not launch a program");
    ops.version=10;
    check(power_request(POWER_OFF,FALSE,NULL,&ops)==ERROR_NOT_SUPPORTED && actions==2,"NT and Wine cannot perform real power operations through this backend");
    ops.version=0x80000a04UL;
    check(power_request(POWER_RESTART,FALSE,NULL,&ops)==ERROR_NOT_SUPPORTED && actions==2,"Windows 98 is not mistaken for the Win95 DOS/power target");
    ops.version=0x80000004UL; exit_result=FALSE; exit_error=ERROR_ACCESS_DENIED;
    check(power_request(POWER_OFF,FALSE,NULL,&ops)==ERROR_ACCESS_DENIED,"native failures preserve their error code");
    exit_error=0;
    check(power_request(POWER_OFF,FALSE,NULL,&ops)==ERROR_CANCELLED,"a veto without an error is cancellation, not successful shutdown");
    actions=0;
}
void power_test_entry(void)
{
    WNDCLASSA cls;
    HWND dialog,shade,fixture,foreign;
    RECT rect,child;
    HDC dc;
    COLORREF initial,final;
    PowerDialogHost host;
    MSG message;
    DWORD start;
    int i;
    instance=GetModuleHandleA(NULL);
    output=CreateFileA("power-test.log",GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(output==INVALID_HANDLE_VALUE) ExitProcess(2);
    dispatcher_tests();
    bundled_fonts_load(instance);
    zero_memory(&cls,sizeof(cls)); cls.hInstance=instance; cls.lpfnWndProc=welcome_proc; cls.lpszClassName="Welcome95.PowerOwnerTest";
    cls.hCursor=LoadCursor(NULL,IDC_ARROW); RegisterClassA(&cls);
    preview=TRUE; monitor_active=TRUE; status_view=VIEW_LOGIN;
    screen=CreateWindowA(cls.lpszClassName,"Welcome95",WS_POPUP|WS_VISIBLE,0,0,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN),NULL,NULL,instance,NULL);
    login_window=CreateWindowExA(WS_EX_TOPMOST,"STATIC","Synthetic native input",WS_POPUP|WS_VISIBLE,400,350,100,25,NULL,NULL,instance,NULL);
    UpdateWindow(screen);
    fixture=CreateWindowExA(WS_EX_TOPMOST,"STATIC","White fixture",WS_POPUP|WS_VISIBLE|SS_WHITERECT,0,0,40,40,NULL,NULL,instance,NULL); UpdateWindow(fixture);
    SendMessageA(screen,WM_COMMAND,ID_POWER,0);
    dialog=FindWindowA("Welcome95.PowerChoice",NULL); shade=FindWindowA("Welcome95.PowerShade",NULL);
    check(dialog && shade,"the welcome power button opens a separate choice and dimming layer");
    check(!IsWindowEnabled(screen) && !IsWindowEnabled(login_window),"choice temporarily disables welcome and native input");
    check(IsWindowEnabled(fixture),"unrelated windows are not disabled");
    GetWindowRect(dialog,&rect);
    check(rect.right-rect.left==314 && rect.bottom-rect.top==200 && rect.left>=0 && rect.top>=0 && rect.right<=GetSystemMetrics(SM_CXSCREEN),"dialog keeps the reference's compact dimensions within the screen");
    for(i=0;i<3;++i) {
        GetWindowRect(GetDlgItem(dialog,POWER_STANDBY+i),&child);
        check(child.left>=rect.left && child.right<=rect.right && child.bottom<=rect.bottom,"action icon and label fit inside the choice");
    }
    dc=GetDC(shade); initial=GetPixel(dc,10,10); ReleaseDC(shade,dc);
    start=GetTickCount();
    do { pump(40); dc=GetDC(shade); final=GetPixel(dc,10,10); ReleaseDC(shade,dc); }
    while((final==CLR_INVALID || GetRValue(final)>=160) && (DWORD)(GetTickCount()-start)<2000);
    check(initial!=CLR_INVALID && GetRValue(initial)>200 && GetRValue(final)<160,"snapshot visibly darkens gradually without an alpha API");
    check(above(dialog,shade) && above(shade,screen),"shade stays above welcome and below the undimmed choice");
    SendMessageA(shade,WM_LBUTTONDOWN,0,0); pump(80);
    check(GetForegroundWindow()==dialog,"background clicks return focus to the choice");
    foreign=CreateWindowExA(WS_EX_TOPMOST,"STATIC","Foreign fixture",WS_POPUP|WS_VISIBLE,10,10,20,20,NULL,NULL,instance,NULL);
    SetForegroundWindow(foreign); pump(200);
    check(GetForegroundWindow()==dialog,"choice regains focus without raising native input over it"); DestroyWindow(foreign);
    SendMessageA(dialog,WM_COMMAND,IDCANCEL,0); pump(30);
    check(!power_dialog_active() && !IsWindow(dialog) && !IsWindow(shade),"Cancel removes both modal windows");
    check(IsWindowEnabled(screen) && IsWindowEnabled(login_window),"Cancel restores exactly the enabled input windows");
    dc=GetDC(fixture); final=GetPixel(dc,10,10); ReleaseDC(fixture,dc);
    check(final==RGB(255,255,255),"Cancel leaves original desktop pixels unchanged");
    SendMessageA(screen,WM_COMMAND,ID_POWER,0);
    SendMessageA(screen,WM_HOTKEY,1,0);
    check(!power_dialog_active() && IsWindow(screen),"the welcome's Escape hotkey cancels only the power dialog");
    SendMessageA(screen,WM_COMMAND,ID_POWER,0); dialog=FindWindowA("Welcome95.PowerChoice",NULL);
    zero_memory(&message,sizeof(message)); message.hwnd=dialog; message.message=WM_KEYDOWN; message.wParam=VK_ESCAPE;
    check(power_dialog_translate(&message) && !power_dialog_active(),"Escape also cancels through the normal message translator");
    SendMessageA(screen,WM_COMMAND,ID_POWER,0); dialog=FindWindowA("Welcome95.PowerChoice",NULL);
    SendMessageA(dialog,WM_COMMAND,POWER_OFF,0);
    check(!power_dialog_active() && !actions,"preview Turn Off closes the choice without dispatching power");
    host.instance=instance; host.owner=screen; host.native_input=login_window; host.russian=FALSE; host.preview=FALSE;
    host.standby_available=FALSE; host.dos_available=TRUE; host.perform=inert_perform; host.restore_focus=NULL;
    check(power_dialog_show(&host),"open a choice with an inert action provider");
    dialog=FindWindowA("Welcome95.PowerChoice",NULL);
    check(!IsWindowEnabled(GetDlgItem(dialog,POWER_STANDBY)),"unsupported Stand By is greyed out");
    SendDlgItemMessageA(dialog,POWER_DOS,BM_CLICK,0,0);
    check(!actions && SendDlgItemMessageA(dialog,POWER_DOS,BM_GETCHECK,0,0)==BST_CHECKED,"checking MS-DOS never executes a command");
    check(!IsWindowEnabled(GetDlgItem(dialog,POWER_OFF)),"DOS option cannot accidentally turn the computer off");
    SendMessageA(dialog,WM_COMMAND,POWER_RESTART,0);
    check(actions==1 && !power_dialog_active(),"Restart with DOS dispatches exactly one inert request");
    EnableWindow(login_window,FALSE); host.dos_available=FALSE;
    check(power_dialog_show(&host),"reopen without a DOS provider"); dialog=FindWindowA("Welcome95.PowerChoice",NULL);
    check(!IsWindowEnabled(GetDlgItem(dialog,POWER_DOS)),"unavailable MS-DOS checkbox is disabled");
    SendMessageA(dialog,WM_DISPLAYCHANGE,0,0);
    check(!power_dialog_active() && !IsWindowEnabled(login_window),"display changes cancel and preserve an input window that was already disabled");
    EnableWindow(login_window,TRUE);
    /* Non-topmost status lets another application's native save prompt remain usable. */
    set_status_view(screen,VIEW_SHUTDOWN);
    foreign=CreateWindowExA(0,"STATIC","Save prompt fixture",WS_POPUP|WS_VISIBLE,100,100,80,40,NULL,NULL,instance,NULL);
    SetForegroundWindow(foreign); maintain_topmost(screen);
    check(above(foreign,screen),"shutdown status does not cover native save/cancel prompts"); DestroyWindow(foreign);
    DestroyWindow(fixture); DestroyWindow(login_window); login_window=NULL; DestroyWindow(screen);
    power_operations_release(); bundled_fonts_release(); CloseHandle(output);
    ExitProcess(failures ? 1 : 0);
}
