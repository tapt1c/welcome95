/* Real cross-process gates, inert shell, private registry/INI; no OS logoff. */
#include "../src/welcome95.c"
#include "../src/bootstrap.h"
static HANDLE output;
static int failures,positions;
static DWORD canvas_destroyed_at;
static WNDPROC original_password_proc;
static DWORD password_paints;
static WNDPROC system_provider_proc;
static LRESULT CALLBACK legacy_provider_frame(HWND window,UINT message,WPARAM wp,LPARAM lp)
{
    if(message==WM_NCCALCSIZE) {
        RECT *client=wp ? &((NCCALCSIZE_PARAMS *)lp)->rgrc[0] : (RECT *)lp;
        client->left+=3; client->top+=22; client->right-=3; client->bottom-=3;
        if(client->bottom<client->top) client->bottom=client->top;
        return 0;
    }
    return CallWindowProcA(system_provider_proc,window,message,wp,lp);
}
static LRESULT CALLBACK count_password_paints(HWND window,UINT message,WPARAM wp,LPARAM lp)
{
    if(message==WM_PAINT) ++password_paints;
    return CallWindowProcA(original_password_proc,window,message,wp,lp);
}
static char fixture_root[128],fixture_dir[MAX_PATH],fixture_ini[MAX_PATH],fixture_exe[MAX_PATH],source_dir[MAX_PATH];
static const BYTE test_boot[32]={'M','Z',1,2,3};
static char service_key[160],run_key[160],once_key[160],backup_key[160],marker[MAX_PATH];
static void check(BOOL okay,const char *text)
{
    DWORD written;
    const char *prefix=okay ? "PASS: " : "FAIL: ";
    if(!okay) ++failures;
    WriteFile(output,prefix,(DWORD)lstrlenA(prefix),&written,NULL);
    WriteFile(output,text,(DWORD)lstrlenA(text),&written,NULL); WriteFile(output,"\r\n",2,&written,NULL);
}
static void pump(DWORD milliseconds)
{
    DWORD start=GetTickCount(); MSG message;
    do {
        while(PeekMessageA(&message,NULL,0,0,PM_REMOVE)) { TranslateMessage(&message); DispatchMessageA(&message); }
        Sleep(5);
    } while((DWORD)(GetTickCount()-start)<milliseconds);
}
static LRESULT CALLBACK counted_screen(HWND hwnd,UINT message,WPARAM wp,LPARAM lp)
{
    if(message==WM_WINDOWPOSCHANGING) ++positions;
    if(message==WM_DESTROY) canvas_destroyed_at=GetTickCount();
    return welcome_proc(hwnd,message,wp,lp);
}
static LRESULT CALLBACK white_input(HWND hwnd,UINT message,WPARAM wp,LPARAM lp)
{
    if(message==WM_WINDOWPOSCHANGING) ++positions;
    if(message==WM_PAINT) {
        PAINTSTRUCT paint; HDC dc=BeginPaint(hwnd,&paint);
        FillRect(dc,&paint.rcPaint,(HBRUSH)GetStockObject(WHITE_BRUSH)); EndPaint(hwnd,&paint); return 0;
    }
    return DefWindowProcA(hwnd,message,wp,lp);
}
static void fixture_config(void)
{
    wsprintfA(service_key,"%s\\Services",fixture_root); wsprintfA(run_key,"%s\\Run",fixture_root);
    wsprintfA(once_key,"%s\\Once",fixture_root); wsprintfA(backup_key,"%s\\Backup",fixture_root);
    wsprintfA(fixture_ini,"%s\\SYSTEM.INI",fixture_dir); wsprintfA(source_dir,"%s\\\320\340\341\356\367\350\351 \361\362\356\353",fixture_dir);
    wsprintfA(fixture_exe,"%s\\WELCOME.EXE",source_dir);
    wsprintfA(marker,"%s\\shell.started",fixture_dir);
    startup_config.boot_directory=fixture_dir; startup_config.boot_image=test_boot; startup_config.boot_size=sizeof(test_boot);
    startup_config.root=HKEY_CURRENT_USER; startup_config.service_key=service_key; startup_config.run_key=run_key;
    startup_config.once_key=once_key; startup_config.backup_key=backup_key; startup_config.executable=fixture_exe;
    startup_config.ini=fixture_ini; startup_config.write_ini=WritePrivateProfileStringA;
}
static const char *option(void)
{
    const char *text=GetCommandLineA();
    if(*text=='"') { ++text; while(*text && *text!='"') ++text; if(*text) ++text; }
    else while(*text && *text!=' ' && *text!='\t') ++text;
    while(*text==' ' || *text=='\t') ++text;
    return text;
}
static HANDLE worker(const char *mode)
{
    STARTUPINFOA startup; PROCESS_INFORMATION child; char command[MAX_PATH+48];
    zero_memory(&startup,sizeof(startup)); startup.cb=sizeof(startup);
    wsprintfA(command,"\"%s\" %s",exe_path,mode);
    if(!CreateProcessA(exe_path,command,NULL,NULL,FALSE,0,NULL,NULL,&startup,&child)) return NULL;
    CloseHandle(child.hThread); return child.hProcess;
}
static BOOL finished(HANDLE process,DWORD timeout)
{
    DWORD start=GetTickCount(),code;
    if(!process) return FALSE;
    while(WaitForSingleObject(process,0)!=WAIT_OBJECT_0 && (DWORD)(GetTickCount()-start)<timeout) pump(10);
    return GetExitCodeProcess(process,&code) && code==0;
}
static INT_PTR CALLBACK provider_proc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp)
{
    (void)wp; (void)lp;
    if(message==WM_USER+40) {
        HWND field=GetDlgItem(hwnd,23);
        ShowWindow(field,SW_HIDE);
        SetWindowPos(field,NULL,20,50,150,22,SWP_NOZORDER|SWP_NOACTIVATE);
        SetWindowRgn(hwnd,NULL,FALSE);
        return TRUE;
    }
    if(message==WM_USER+41) {
        SetWindowLongPtrA(hwnd,DWLP_MSGRESULT,GetFocus()==GetDlgItem(hwnd,23));
        return TRUE;
    }
    if(message==WM_USER+42) {
        SetWindowLongPtrA(hwnd,DWLP_MSGRESULT,password_paints); return TRUE;
    }
    if(message==WM_USER+43) {
        HWND field=GetDlgItem(hwnd,23); HDC dc=GetDC(field); TEXTMETRICA metrics;
        HFONT font=(HFONT)SendMessageA(field,WM_GETFONT,0,0);
        HGDIOBJ old=SelectObject(dc,font ? font : GetStockObject(SYSTEM_FONT));
        int height=GetTextMetricsA(dc,&metrics) ? metrics.tmHeight : 0;
        if(!old) height=0;
        SelectObject(dc,old); ReleaseDC(field,dc);
        SetWindowLongPtrA(hwnd,DWLP_MSGRESULT,height); return TRUE;
    }
    if(message==WM_COMMAND) return TRUE; /* Inert: a click never proves logon. */
    if(message==WM_CLOSE) { DestroyWindow(hwnd); return TRUE; }
    if(message==WM_DESTROY) PostQuitMessage(0);
    return FALSE;
}
static INT_PTR CALLBACK notice_proc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp)
{
    (void)hwnd; (void)message; (void)wp; (void)lp; return FALSE;
}
static void register_windows(void)
{
    WNDCLASSA cls;
    zero_memory(&cls,sizeof(cls)); cls.hInstance=instance; cls.hCursor=LoadCursor(NULL,IDC_ARROW);
    cls.lpfnWndProc=counted_screen; cls.lpszClassName="Welcome95.Screen"; RegisterClassA(&cls);
    cls.lpfnWndProc=white_input; cls.lpszClassName="Welcome95.WhiteInput"; RegisterClassA(&cls);
    cls.lpfnWndProc=monitor_proc; cls.lpszClassName="Welcome95.Controller"; RegisterClassA(&cls);
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
void session_test_entry(void)
{
    const char *mode=option();
    char temporary[MAX_PATH],original[STARTUP_TEXT],probe_pending[MAX_PATH];
    WNDCLASSA cls;
    HWND competitor,foreground,old_screen,notifier,provider,notice;
    HANDLE startup_worker,shell_worker,notifier_worker,notifier_ready,file,provider_worker;
    DWORD start,written,launched,closed_at;
    HDC dc; COLORREF pixel;
    int i,x,y,width;

    instance=GetModuleHandleA(NULL); GetModuleFileNameA(NULL,exe_path,sizeof(exe_path));
    if(*mode) {
        if(!GetEnvironmentVariableA("WELCOME95_TEST_ROOT",fixture_root,sizeof(fixture_root)) ||
           !GetEnvironmentVariableA("WELCOME95_TEST_DIR",fixture_dir,sizeof(fixture_dir))) ExitProcess(2);
        fixture_config();
        if(!lstrcmpA(mode,"/probe")) {
            wsprintfA(probe_pending,"%s\\probe.tmp",fixture_dir);
            file=CreateFileA(probe_pending,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
            launched=GetTickCount();
            if(file==INVALID_HANDLE_VALUE) ExitProcess(3);
            if(!WriteFile(file,&launched,sizeof(launched),&written,NULL) || written!=sizeof(launched)) ExitProcess(3);
            CloseHandle(file);
            if(!MoveFileA(probe_pending,marker)) ExitProcess(3);
            Sleep(200); ExitProcess(0);
        }
        if(!lstrcmpA(mode,"/boot-shell")) ExitProcess(bootstrap_forward(HKEY_CURRENT_USER,backup_key,"/shell"));
        if(!lstrcmpA(mode,"/boot-startup")) ExitProcess(bootstrap_forward(HKEY_CURRENT_USER,backup_key,"/startup"));
        if(!lstrcmpA(mode,"/shell")) ExitProcess(handoff(TRUE));
        if(!lstrcmpA(mode,"/startup")) ExitProcess(handoff(FALSE));
        if(!lstrcmpA(mode,"/gate-shell")) ExitProcess(handoff(TRUE));
        if(!lstrcmpA(mode,"/gate-startup")) ExitProcess(handoff(FALSE));
        if(!lstrcmpA(mode,"/provider")) {
            DWORD words[32]; DLGTEMPLATE *form=(DLGTEMPLATE *)words; MSG message;
            zero_memory(words,sizeof(words)); form->style=WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME;
            form->cx=250; form->cy=110;
            provider=CreateDialogIndirectParamA(instance,form,NULL,provider_proc,0);
            SetWindowTextA(provider,"Welcome95 synthetic provider");
            system_provider_proc=(WNDPROC)SetWindowLongPtrA(provider,GWLP_WNDPROC,(LONG_PTR)legacy_provider_frame);
            SetWindowPos(provider,NULL,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
            CreateWindowA("EDIT","First",WS_CHILD|WS_VISIBLE,20,20,150,22,provider,(HMENU)21,instance,NULL);
            CreateWindowA("EDIT","synthetic-only",WS_CHILD|WS_VISIBLE|ES_PASSWORD,20,50,150,22,provider,(HMENU)23,instance,NULL);
            original_password_proc=(WNDPROC)SetWindowLongPtrA(GetDlgItem(provider,23),GWLP_WNDPROC,(LONG_PTR)count_password_paints);
            CreateWindowA("BUTTON","OK",WS_CHILD|WS_VISIBLE,180,20,60,22,provider,(HMENU)IDOK,instance,NULL);
            CreateWindowA("BUTTON","Cancel",WS_CHILD|WS_VISIBLE,180,50,60,22,provider,(HMENU)IDCANCEL,instance,NULL);
            ShowWindow(provider,SW_SHOW);
            while(GetMessageA(&message,NULL,0,0)>0) { TranslateMessage(&message); DispatchMessageA(&message); }
            ExitProcess(provider ? 0 : 2);
        }
        if(!lstrcmpA(mode,"/notify")) {
            MSG message;
            zero_memory(&cls,sizeof(cls)); cls.hInstance=instance; cls.lpfnWndProc=monitor_proc;
            cls.lpszClassName="Welcome95.TestNotifier"; RegisterClassA(&cls);
            notifier=CreateWindowA(cls.lpszClassName,"Test notifier",WS_OVERLAPPED,0,0,0,0,NULL,NULL,instance,NULL);
            notifier_ready=OpenEventA(EVENT_MODIFY_STATE,FALSE,"Welcome95.TestNotifierReady");
            if(notifier_ready) { SetEvent(notifier_ready); CloseHandle(notifier_ready); }
            while(GetMessageA(&message,NULL,0,0)>0) DispatchMessageA(&message);
            ExitProcess(notifier ? 0 : 2);
        }
        ExitProcess(2);
    }
    output=CreateFileA("session-test.log",GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(output==INVALID_HANDLE_VALUE) ExitProcess(2);
    if(!GetTempPathA(MAX_PATH,temporary) || !GetTempFileNameA(temporary,"WSE",0,fixture_dir) ||
       !DeleteFileA(fixture_dir) || !CreateDirectoryA(fixture_dir,NULL)) ExitProcess(2);
    wsprintfA(fixture_root,"Software\\Welcome95.Tests\\Session%lu",GetCurrentProcessId()); fixture_config();
    if(!CreateDirectoryA(source_dir,NULL) || !CopyFileA(exe_path,fixture_exe,TRUE)) ExitProcess(2);
    wsprintfA(original,"\"%s\" /probe",exe_path); WritePrivateProfileStringA("boot","shell",original,fixture_ini);
    check(startup_configure(&startup_config,TRUE)==0,"configure only the isolated test shell and startup keys");
    SetEnvironmentVariableA("WELCOME95_TEST_ROOT",fixture_root); SetEnvironmentVariableA("WELCOME95_TEST_DIR",fixture_dir);
    bundled_fonts_load(instance); create_ui_fonts(); register_windows();
    monitor_active=TRUE; preview=TRUE; status_view=VIEW_LOGIN;
    profiles_remember("First"); profiles_remember("Second");
    screen=CreateWindowExA(WS_EX_TOPMOST,"Welcome95.Screen","Priority fixture",WS_POPUP|WS_VISIBLE|WS_CLIPCHILDREN,
        0,0,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN),NULL,NULL,instance,NULL); KillTimer(screen,TICK);
    login_window=CreateWindowExA(WS_EX_TOPMOST,"Welcome95.WhiteInput","Synthetic input",WS_POPUP|WS_VISIBLE,300,250,100,24,NULL,NULL,instance,NULL);
    prefer_native=TRUE; maintain_topmost(screen); pump(120); positions=0;
    for(i=0;i<100;++i) maintain_topmost(screen);
    check(!positions,"100 idle priority checks do not reorder or reposition a single window");
    dc=GetDC(login_window); pixel=GetPixel(dc,10,10); ReleaseDC(login_window,dc);
    check(pixel==RGB(255,255,255),"stable priority preserves the visible input pixels");
    SetForegroundWindow(login_window); foreground=GetForegroundWindow();
    competitor=CreateWindowExA(WS_EX_TOPMOST,"STATIC","Competing splash",WS_POPUP|WS_VISIBLE,10,10,60,40,NULL,NULL,instance,NULL);
    SetForegroundWindow(foreground); maintain_topmost(screen);
    check(window_above(login_window,screen) && window_above(screen,competitor),"one batch restores input above welcome above a competing splash");
    check(GetForegroundWindow()==foreground,"batching priority keeps keyboard focus intact");
    pump(60); positions=0; for(i=0;i<100;++i) maintain_topmost(screen);
    check(!positions,"priority becomes idle again immediately after recovering from a competitor");
    {
        DWORD words[32]; DLGTEMPLATE *form=(DLGTEMPLATE *)words;
        zero_memory(words,sizeof(words)); form->style=WS_POPUP|WS_CAPTION; form->cx=150; form->cy=60;
        notice=CreateDialogIndirectParamA(instance,form,NULL,notice_proc,0); ShowWindow(notice,SW_SHOW);
        provider_process=GetCurrentProcessId(); SetForegroundWindow(notice); foreground=GetForegroundWindow();
        maintain_topmost(screen);
        check(window_above(notice,screen) && window_above(notice,login_window),
            "an unowned Windows provider message remains accessible above canvas and input");
        check(GetForegroundWindow()==foreground,"provider notice priority preserves its existing focus");
        DestroyWindow(notice); provider_process=0;
    }
    DestroyWindow(competitor); DestroyWindow(login_window); login_window=NULL; DestroyWindow(screen);
    resident_mode=TRUE; preview=FALSE; prefer_native=FALSE;
    ready_event=CreateEventA(NULL,TRUE,FALSE,READY_EVENT); closed_event=CreateEventA(NULL,TRUE,FALSE,CLOSED_EVENT);
    logoff_event=CreateEventA(NULL,TRUE,FALSE,LOGOFF_EVENT); ended_event=CreateEventA(NULL,TRUE,FALSE,ENDED_EVENT); controller_event=CreateEventA(NULL,TRUE,FALSE,CONTROLLER_EVENT);
    stop_event=CreateEventA(NULL,TRUE,FALSE,STOP_EVENT);
    ResetEvent(ready_event); ResetEvent(closed_event); ResetEvent(logoff_event); ResetEvent(ended_event); ResetEvent(stop_event);
    controller=CreateWindowA("Welcome95.Controller","Test controller",WS_OVERLAPPED,0,0,0,0,NULL,NULL,instance,NULL); KillTimer(controller,TICK);
    begin_logon_cycle(FALSE); KillTimer(screen,TICK); SetEvent(controller_event);
    startup_worker=worker("/boot-startup"); shell_worker=worker("/boot-shell");
    check(startup_worker && shell_worker,"start real, separate startup and shell gate processes");
    start=GetTickCount();
    while(WaitForSingleObject(ready_event,0)!=WAIT_OBJECT_0 && (DWORD)(GetTickCount()-start)<5000) pump(10);
    check(WaitForSingleObject(ready_event,0)==WAIT_OBJECT_0,"the new session gate publishes readiness to the resident controller");
    SendMessageA(screen,WM_TIMER,TICK,0); start=GetTickCount();
    check(finishing && status_view!=VIEW_LOGIN,"readiness enters the actual welcome loading state");
    {
        HWND boot=FindWindowA("Welcome95.Bootstrap",NULL); DWORD_PTR reply=0;
        check(boot && SendMessageTimeoutA(boot,WM_QUERYENDSESSION,0,0,SMTO_ABORTIFHUNG,500,&reply) && reply,
            "the bootstrap pumps Windows messages while waiting for its frontend");
    }
    pump(250); SendMessageA(screen,WM_TIMER,TICK,0);
    check(IsWindow(screen) && GetFileAttributesA(marker)==INVALID_FILE_ATTRIBUTES &&
        WaitForSingleObject(startup_worker,0)==WAIT_TIMEOUT && WaitForSingleObject(shell_worker,0)==WAIT_TIMEOUT,
        "both gates remain blocked while welcome is visible, before the one-second delay");
    while((DWORD)(GetTickCount()-start)<1050) pump(10);
    SendMessageA(screen,WM_TIMER,TICK,0); closed_at=canvas_destroyed_at;
    check(!screen && session_running && WaitForSingleObject(closed_event,0)==WAIT_OBJECT_0,"acknowledge handoff only after the canvas has been destroyed");
    check(finished(startup_worker,5000) && finished(shell_worker,5000),"both gate processes complete after the close acknowledgement");
    CloseHandle(startup_worker); CloseHandle(shell_worker);
    start=GetTickCount();
    while(GetFileAttributesA(marker)==INVALID_FILE_ATTRIBUTES && (DWORD)(GetTickCount()-start)<5000) pump(10);
    file=CreateFileA(marker,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    launched=0; written=0;
    if(file!=INVALID_HANDLE_VALUE) { ReadFile(file,&launched,sizeof(launched),&written,NULL); CloseHandle(file); }
    check(written==sizeof(launched) && (LONG)(launched-closed_at)>=0,"the inert original shell actually starts after welcome closes");
    SendMessageA(controller,WM_TIMER,TICK,0);
    check(!screen && session_running,"shell exit alone is not misidentified as a new logon");
    notifier_ready=CreateEventA(NULL,TRUE,FALSE,"Welcome95.TestNotifierReady");
    notifier_worker=worker("/notify"); start=GetTickCount();
    while(WaitForSingleObject(notifier_ready,0)!=WAIT_OBJECT_0 && (DWORD)(GetTickCount()-start)<5000) pump(10);
    notifier=FindWindowA("Welcome95.TestNotifier",NULL);
    check(notifier && SendMessageA(notifier,WM_QUERYENDSESSION,0,(LPARAM)ENDSESSION_LOGOFF_95),"a normal per-session process approves a synthetic logoff query");
    SendMessageA(notifier,WM_ENDSESSION,TRUE,(LPARAM)ENDSESSION_LOGOFF_95);
    check(finished(notifier_worker,5000) && WaitForSingleObject(logoff_event,0)==WAIT_OBJECT_0,"the normal notifier exits and signals committed logoff to the persistent process");
    CloseHandle(notifier_worker); CloseHandle(notifier_ready);
    automatic_attempted=TRUE; pending_profile=TRUE;
    SendMessageA(controller,WM_TIMER,TICK,0); KillTimer(screen,TICK);
    check(IsWindow(controller) && screen && status_view==VIEW_LOGIN && !session_running,"committed logoff creates another welcome without exiting the controller");
    check(!finishing && !automatic_attempted && !pending_profile && selected_profile==0,"new logon resets per-session authentication and selection state");
    check(WaitForSingleObject(ready_event,0)==WAIT_TIMEOUT && WaitForSingleObject(closed_event,0)==WAIT_TIMEOUT,
        "new logon clears the previous session's readiness and close acknowledgements");
    old_screen=screen; SetEvent(logoff_event); SendMessageA(controller,WM_TIMER,TICK,0);
    check(screen==old_screen && WaitForSingleObject(closed_event,0)==WAIT_TIMEOUT,"duplicate logoff notifications do not restart an already waiting login");
    SendMessageA(controller,WM_ENDSESSION,FALSE,(LPARAM)ENDSESSION_LOGOFF_95);
    check(screen==old_screen && IsWindow(screen),"cancelled logoff preserves the current login canvas");
    SendMessageA(screen,WM_QUERYENDSESSION,0,0);
    SendMessageA(screen,WM_ENDSESSION,FALSE,0);
    check(screen==old_screen && status_view==VIEW_LOGIN,"cancelled shutdown during login preserves the resident login canvas");
    SendMessageA(controller,WM_ENDSESSION,TRUE,0); SendMessageA(controller,WM_TIMER,TICK,0);
    check(IsWindow(controller) && !screen && awaiting_logon && !body_font,
        "ambiguous session end keeps the controller and releases temporary font resources");
    startup_worker=worker("/gate-startup"); start=GetTickCount();
    while(!screen && (DWORD)(GetTickCount()-start)<5000) pump(10);
    if(screen) KillTimer(screen,TICK);
    check(screen && IsWindow(screen) && !awaiting_logon,"a new OS startup gate also restores welcome after a reasonless session end");
    check(WaitForSingleObject(startup_worker,0)==WAIT_TIMEOUT && WaitForSingleObject(closed_event,0)==WAIT_TIMEOUT,
        "creating a new canvas never acknowledges its own startup gate prematurely");
    SendMessageA(screen,WM_TIMER,TICK,0); finish_time=GetTickCount()-1001; SendMessageA(screen,WM_TIMER,TICK,0);
    check(finished(startup_worker,5000),"the new gate waits until its own welcome has actually closed"); CloseHandle(startup_worker);
    SendMessageA(controller,WM_ENDSESSION,TRUE,0); SendMessageA(controller,WM_TIMER,TICK,0);
    provider_worker=worker("/provider"); provider=NULL; start=GetTickCount();
    while((!provider || !IsWindowVisible(provider)) && (DWORD)(GetTickCount()-start)<5000) {
        pump(10); provider=FindWindowA("#32770","Welcome95 synthetic provider");
    }
    SendMessageA(controller,WM_TIMER,TICK,0); if(screen) KillTimer(screen,TICK);
    check(provider && screen && status_view==VIEW_LOGIN && !finishing && WaitForSingleObject(ready_event,0)==WAIT_TIMEOUT,
        "a separate provider's new password dialog restores login without inventing session readiness");
    allow_logon();
    check(native_logon_attached() && login_window==provider,
        "the repeated login attaches real controls owned by a separate synthetic provider process");
    {
        HWND field=GetDlgItem(provider,23); POINT hit; RECT rectangle; DWORD_PTR reply=0;
        check(SendMessageTimeoutA(provider,WM_USER+43,0,0,SMTO_ABORTIFHUNG,200,&reply) && reply>=16 && reply<=22,
            "the provider can render the shared larger font within the original 22px client height");
        SendMessageA(provider,WM_USER+40,0,0);
        allow_logon(); maintain_topmost(screen); pump(100);
        /* The provider can queue activation of the canvas when hiding its Edit.
           Service the following priority pass as the real 100ms timer does. */
        maintain_topmost(screen); pump(20);
        GetWindowRect(field,&rectangle); hit.x=rectangle.left+8; hit.y=rectangle.top+8;
        check(IsWindowVisible(field) && WindowFromPoint(hit)==field,
            "a refreshed cross-process password Edit is visible and receives mouse hit tests above the canvas");
        login_geometry(&x,&y,&width);
        check(rectangle.left==x && rectangle.top==y && rectangle.right-rectangle.left==width-60 && rectangle.bottom-rectangle.top==22,
            "the repaired cross-process Edit occupies the actual welcome password rectangle");
        SetForegroundWindow(screen);
        SendMessageA(screen,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(x-1,y+8)); pump(60);
        check(SendMessageTimeoutA(provider,WM_USER+41,0,0,SMTO_ABORTIFHUNG,200,&reply) && reply,
            "the provider thread owns keyboard focus in its repaired native password Edit");
        {
            DWORD_PTR modified=0; HDC field_dc; COLORREF ink; int px,py,ink_pixels=0;
            /* Only this owned synthetic fixture is cleared. Real input remains
               in the provider; verify actual system mouse/keyboard delivery. */
            SendMessageA(field,WM_SETTEXT,0,(LPARAM)"");
            SetCursorPos(x+8,y+8); mouse_event(MOUSEEVENTF_LEFTDOWN,0,0,0,0); mouse_event(MOUSEEVENTF_LEFTUP,0,0,0,0);
            pump(40); keybd_event('Q',0,0,0); keybd_event('Q',0,KEYEVENTF_KEYUP,0); pump(100);
            check(SendMessageTimeoutA(field,EM_GETMODIFY,0,0,SMTO_ABORTIFHUNG,200,&modified) && modified,
                "real system keyboard input reaches the external provider's Edit after a real mouse click");
            field_dc=GetDC(field);
            for(py=1;py<21;++py) for(px=1;px<24;++px) {
                ink=GetPixel(field_dc,px,py);
                if(ink!=CLR_INVALID && GetRValue(ink)<100 && GetGValue(ink)<100 && GetBValue(ink)<100) ++ink_pixels;
            }
            ReleaseDC(field,field_dc);
            check(ink_pixels>3,"a typed synthetic password paints visible mask pixels inside the welcome field");
        }
        {
            DWORD_PTR before=0;
            SendMessageTimeoutA(provider,WM_USER+42,0,0,SMTO_ABORTIFHUNG,200,&before);
            InvalidateRect(screen,NULL,FALSE); UpdateWindow(screen); pump(60);
            reply=0;
            check(SendMessageTimeoutA(provider,WM_USER+42,0,0,SMTO_ABORTIFHUNG,200,&reply) && reply>before,
                "repainting the canvas also queues a real paint on the external provider's password Edit");
        }
    }
    SendMessageA(controller,WM_HOTKEY,ID_RESCUE_HOTKEY,0);
    check(rescue_mode && !IsWindowVisible(screen) && IsWindowVisible(provider) &&
        (GetWindowLongA(provider,GWL_STYLE)&WS_CAPTION) && !native_logon_attached(),
        "the hidden controller shortcut restores the real native dialog and hides the canvas");
    check(WaitForSingleObject(ready_event,0)==WAIT_TIMEOUT && WaitForSingleObject(closed_event,0)==WAIT_TIMEOUT,
        "native rescue never submits credentials or marks authentication complete");
    startup_worker=worker("/gate-startup"); start=GetTickCount();
    while(WaitForSingleObject(ready_event,0)!=WAIT_OBJECT_0 && (DWORD)(GetTickCount()-start)<5000) pump(10);
    SendMessageA(screen,WM_TIMER,TICK,0);
    check(finishing && WaitForSingleObject(startup_worker,0)==WAIT_TIMEOUT,"a second session waits on its own loading state rather than stale signals");
    SetEvent(stop_event); SendMessageA(controller,WM_TIMER,TICK,0);
    check(!IsWindow(controller) && !screen && finished(startup_worker,5000),"Disable releases a pending startup gate and removes the canvas");
    CloseHandle(startup_worker);
    if(provider && IsWindow(provider)) SendMessageA(provider,WM_CLOSE,0,0);
    check(finished(provider_worker,5000),"the external provider fixture restores and exits normally");
    CloseHandle(provider_worker);
    check(startup_configure(&startup_config,FALSE)==0,"restore the isolated shell configuration after lifecycle tests");
    if(shell_process) CloseHandle(shell_process);
    CloseHandle(ready_event); CloseHandle(closed_event); CloseHandle(logoff_event); CloseHandle(ended_event); CloseHandle(controller_event); CloseHandle(stop_event);
    RegDeleteKeyA(HKEY_CURRENT_USER,service_key); RegDeleteKeyA(HKEY_CURRENT_USER,run_key);
    RegDeleteKeyA(HKEY_CURRENT_USER,once_key); RegDeleteKeyA(HKEY_CURRENT_USER,backup_key); RegDeleteKeyA(HKEY_CURRENT_USER,fixture_root);
    remove_fixture(fixture_dir);
    check(RemoveDirectoryA(fixture_dir),"remove only owned session fixtures");
    SetEnvironmentVariableA("WELCOME95_TEST_ROOT",NULL); SetEnvironmentVariableA("WELCOME95_TEST_DIR",NULL);
    release_ui_fonts(); CloseHandle(output); ExitProcess(failures ? 1 : 0);
}
