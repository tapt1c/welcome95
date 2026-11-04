/* UI and dispatch tests: all power and DOS callbacks below are inert. */
#include "../src/welcome95.c"
static HANDLE output;
static int failures,actions,standbys,dos_calls;
static WNDPROC original_shade,original_power_paint;
static int legacy_erases,power_paints,shade_paints,restores;
static BOOL slow_shade,inspect_restore,restore_covered;
static struct { HWND window; WNDPROC previous; } legacy_buttons[5];
static int legacy_count;
static LRESULT CALLBACK legacy_button(HWND window,UINT message,WPARAM wp,LPARAM lp)
{
    int i;
    if(message==WM_ERASEBKGND || message==WM_PAINT) {
        RECT rect; HDC dc=GetDC(window);
        GetClientRect(window,&rect); FillRect(dc,&rect,(HBRUSH)GetStockObject(WHITE_BRUSH)); ReleaseDC(window,dc);
        ++legacy_erases;
        if(message==WM_ERASEBKGND) return TRUE;
    }
    for(i=0;i<legacy_count;++i) if(legacy_buttons[i].window==window)
        return CallWindowProcA(legacy_buttons[i].previous,window,message,wp,lp);
    return DefWindowProcA(window,message,wp,lp);
}
static void install_legacy_button(HWND window)
{
    PaintedButtonState *state=(PaintedButtonState *)GetWindowLongPtrA(window,GWLP_USERDATA);
    legacy_buttons[legacy_count].window=window;
    legacy_buttons[legacy_count++].previous=state ? state->previous : (WNDPROC)GetWindowLongPtrA(window,GWLP_WNDPROC);
    if(state) state->previous=legacy_button;
    else SetWindowLongPtrA(window,GWLP_WNDPROC,(LONG_PTR)legacy_button);
}
static LRESULT CALLBACK count_power_paints(HWND window,UINT message,WPARAM wp,LPARAM lp)
{
    if(message==WM_PAINT) ++power_paints;
    return CallWindowProcA(original_power_paint,window,message,wp,lp);
}
static LRESULT CALLBACK delayed_shade(HWND window,UINT message,WPARAM wp,LPARAM lp)
{
    if(message==WM_PAINT) { ++shade_paints; if(slow_shade) Sleep(100); }
    return CallWindowProcA(original_shade,window,message,wp,lp);
}
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
static BOOL power_pixels(void)
{
    HWND button=GetDlgItem(screen,ID_POWER);
    HDC dc=GetDC(button);
    COLORREF icon=GetPixel(dc,6,18),background=GetPixel(dc,230,5);
    ReleaseDC(button,dc);
    return icon!=CLR_INVALID && GetRValue(icon)>150 && GetGValue(icon)<180 &&
        background!=CLR_INVALID && GetBValue(background)>100 && GetRValue(background)<80;
}
static LRESULT CALLBACK power_owner(HWND window,UINT message,WPARAM wp,LPARAM lp)
{
    if(message==WM_ENABLE && wp && inspect_restore) {
        HWND cover=FindWindowA("Welcome95.PowerShade",NULL);
        ++restores;
        restore_covered=cover && IsWindowVisible(cover) && power_dialog_active();
        SendMessageA(window,WM_TIMER,TICK,0);
        restore_covered=restore_covered && above(cover,window);
    }
    return welcome_proc(window,message,wp,lp);
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
    int i,before;
    instance=GetModuleHandleA(NULL);
    output=CreateFileA("power-test.log",GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(output==INVALID_HANDLE_VALUE) ExitProcess(2);
    dispatcher_tests();
    bundled_fonts_load(instance);
    zero_memory(&cls,sizeof(cls)); cls.hInstance=instance; cls.lpfnWndProc=power_owner; cls.lpszClassName="Welcome95.PowerOwnerTest";
    cls.hCursor=LoadCursor(NULL,IDC_ARROW); RegisterClassA(&cls);
    preview=TRUE; monitor_active=TRUE; status_view=VIEW_LOGIN;
    screen=CreateWindowA(cls.lpszClassName,"Welcome95",WS_POPUP|WS_VISIBLE|WS_CLIPCHILDREN,0,0,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN),NULL,NULL,instance,NULL);
    login_window=CreateWindowExA(WS_EX_TOPMOST,"STATIC","Synthetic native input",WS_POPUP|WS_VISIBLE,400,350,100,25,NULL,NULL,instance,NULL);
    UpdateWindow(screen);
    UpdateWindow(GetDlgItem(screen,ID_POWER));
    check(power_pixels(),"the real welcome power button paints its red icon and blue background");
    dc=GetDC(GetDlgItem(screen,ID_POWER));
    PatBlt(dc,0,0,245,29,BLACKNESS); ReleaseDC(GetDlgItem(screen,ID_POWER),dc);
    InvalidateRect(screen,NULL,FALSE); UpdateWindow(screen);
    check(power_pixels(),"a parent repaint also repairs stale black pixels in its owner-drawn power button");
    install_legacy_button(GetDlgItem(screen,ID_POWER)); install_legacy_button(GetDlgItem(screen,ID_SUBMIT));
    dc=GetDC(GetDlgItem(screen,ID_POWER));
    legacy_button(GetDlgItem(screen,ID_POWER),WM_ERASEBKGND,(WPARAM)dc,0);
    check(GetPixel(dc,230,5)==RGB(255,255,255),"the strict old-USER fixture visibly exposes its white erase-before-draw frame");
    ReleaseDC(GetDlgItem(screen,ID_POWER),dc);
    before=legacy_erases;
    for(i=0;i<8;++i) {
        HWND button=GetDlgItem(screen,i&1 ? ID_SUBMIT : ID_POWER);
        dc=GetDC(button); SendMessageA(button,WM_ERASEBKGND,(WPARAM)dc,0); ReleaseDC(button,dc);
        InvalidateRect(button,NULL,TRUE); UpdateWindow(button);
    }
    check(legacy_erases==before && power_pixels(),"welcome power and submit buttons bypass native erasure and present complete buffered pixels");
    original_power_paint=(WNDPROC)SetWindowLongPtrA(GetDlgItem(screen,ID_POWER),GWLP_WNDPROC,(LONG_PTR)count_power_paints);
    before=power_paints; SetRect(&rect,5,100,20,115); InvalidateRect(screen,&rect,FALSE); UpdateWindow(screen);
    check(power_paints==before,"damage away from footer buttons does not repaint an unrelated power button");
    InvalidateRect(screen,NULL,FALSE); UpdateWindow(screen);
    check(power_paints>before && power_pixels(),"full parent damage still repaints and repairs its custom buttons");

    fixture=CreateWindowExA(WS_EX_TOPMOST,"STATIC","White fixture",WS_POPUP|WS_VISIBLE|SS_WHITERECT,0,0,40,40,NULL,NULL,instance,NULL); UpdateWindow(fixture);
    SendDlgItemMessageA(screen,ID_POWER,BM_CLICK,0,0);
    dialog=FindWindowA("Welcome95.PowerChoice",NULL); shade=FindWindowA("Welcome95.PowerShade",NULL);
    check(dialog && shade,"the welcome power button opens a separate choice and dimming layer");
    check(!IsWindowEnabled(screen) && !IsWindowEnabled(login_window),"choice temporarily disables welcome and native input");
    check(IsWindowEnabled(fixture),"unrelated windows are not disabled");
    GetWindowRect(dialog,&rect);
    check(rect.right-rect.left==314 && rect.bottom-rect.top==200 && rect.left>=0 && rect.top>=0 && rect.right<=GetSystemMetrics(SM_CXSCREEN),"dialog keeps the reference's compact dimensions within the screen");
    for(i=0;i<3;++i) {
        GetWindowRect(GetDlgItem(dialog,POWER_STANDBY+i),&child);
        check(child.left>=rect.left && child.right<=rect.right && child.bottom<=rect.bottom,"action icon and label fit inside the choice");
        install_legacy_button(GetDlgItem(dialog,POWER_STANDBY+i)); before=legacy_erases;
        dc=GetDC(GetDlgItem(dialog,POWER_STANDBY+i));
        SendDlgItemMessageA(dialog,POWER_STANDBY+i,WM_ERASEBKGND,(WPARAM)dc,0); ReleaseDC(GetDlgItem(dialog,POWER_STANDBY+i),dc);
        InvalidateRect(GetDlgItem(dialog,POWER_STANDBY+i),NULL,TRUE); UpdateWindow(GetDlgItem(dialog,POWER_STANDBY+i));
        check(legacy_erases==before,"power-choice action buttons also bypass the legacy white erase path");

    }
    dc=GetDC(shade); initial=GetPixel(dc,10,10); ReleaseDC(shade,dc);
    original_shade=(WNDPROC)SetWindowLongPtrA(shade,GWLP_WNDPROC,(LONG_PTR)delayed_shade);
    slow_shade=TRUE; shade_paints=0;
    start=GetTickCount();
    do { pump(40); dc=GetDC(shade); final=GetPixel(dc,10,10); ReleaseDC(shade,dc); }
    while((final==CLR_INVALID || GetRValue(final)>=160) && (DWORD)(GetTickCount()-start)<2000);
    check(initial!=CLR_INVALID && GetRValue(initial)>200 && GetRValue(final)<160,"snapshot visibly darkens gradually without an alpha API");
    while(GetRValue(final)>110 && (DWORD)(GetTickCount()-start)<1200) {
        pump(20); dc=GetDC(shade); final=GetPixel(dc,10,10); ReleaseDC(shade,dc);
    }
    check(GetRValue(final)<=110 && shade_paints<10 && (DWORD)(GetTickCount()-start)<1300,
        "slow 100ms shade paints skip obsolete fade frames instead of stretching sixteen timer steps");
    slow_shade=FALSE; pump(250); before=shade_paints; pump(250);
    check(shade_paints==before,"the completed fade remains idle without repeated full-screen repaints");

    check(above(dialog,shade) && above(shade,screen),"shade stays above welcome and below the undimmed choice");
    SendMessageA(shade,WM_LBUTTONDOWN,0,0); pump(80);
    check(GetForegroundWindow()==dialog,"background clicks return focus to the choice");
    foreign=CreateWindowExA(WS_EX_TOPMOST,"STATIC","Foreign fixture",WS_POPUP|WS_VISIBLE,10,10,20,20,NULL,NULL,instance,NULL);
    SetForegroundWindow(foreign); pump(200);
    check(GetForegroundWindow()==dialog,"choice regains focus without raising native input over it"); DestroyWindow(foreign);
    inspect_restore=TRUE; restores=0; restore_covered=FALSE;
    SendMessageA(dialog,WM_COMMAND,IDCANCEL,0); inspect_restore=FALSE; pump(30);
    check(restores==1 && restore_covered,"Cancel restores input under the shade and blocks a reentrant welcome timer until teardown completes");
    check(!power_dialog_active() && !IsWindow(dialog) && !IsWindow(shade),"Cancel removes both modal windows");
    check(IsWindowEnabled(screen) && IsWindowEnabled(login_window),"Cancel restores exactly the enabled input windows");
    check(power_pixels(),"Cancel repaints the real power button after removing the dimming snapshot");
    legacy_count=2;
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
    SendDlgItemMessageA(screen,ID_SUBMIT,BM_CLICK,0,0);
    check(finishing && status_view==VIEW_WELCOME_MULTI,"buffered submit buttons retain native BM_CLICK notifications in inert preview");
    finishing=FALSE; set_status_view(screen,VIEW_LOGIN);
    /* Non-topmost status lets another application's native save prompt remain usable. */
    set_status_view(screen,VIEW_SHUTDOWN);
    foreign=CreateWindowExA(0,"STATIC","Save prompt fixture",WS_POPUP|WS_VISIBLE,100,100,80,40,NULL,NULL,instance,NULL);
    SetForegroundWindow(foreign); maintain_topmost(screen);
    check(above(foreign,screen),"shutdown status does not cover native save/cancel prompts"); DestroyWindow(foreign);
    DestroyWindow(fixture); DestroyWindow(login_window); login_window=NULL; DestroyWindow(screen);
    power_operations_release(); bundled_fonts_release(); CloseHandle(output);
    ExitProcess(failures ? 1 : 0);
}
