/* Real cross-process gates, inert shell, private registry/INI; no OS logoff. */
#include "../src/profiles.c"
#include "../src/fonts.h"
static BOOL single_profile_fixture;
static void fixture_profiles_load(void);
static int fixture_fonts_load(HINSTANCE module);
#define bundled_fonts_load fixture_fonts_load
#define profiles_load fixture_profiles_load
static BOOL WINAPI session_set_event(HANDLE event);
#define SetEvent session_set_event
#include "../src/welcome95.c"
#undef SetEvent
#undef profiles_load
#undef bundled_fonts_load
#include "../src/bootstrap.h"
static HANDLE output;
static int failures,positions,cover_positions,cover_paints;
static DWORD canvas_destroyed_at;
static BOOL consume_creation_damage, visible_during_creation, synchronous_show_painted;
static DWORD premature_paints,closed_publications;
static BOOL early_init_probe,profiles_saw_frame,fonts_saw_frame,init_timer_guarded;
static HDC wait_snapshot;
static HBITMAP wait_frame;
static HGDIOBJ wait_old;
static BOOL wait_background_matches;
static char wait_difference[180];
static BOOL compare_wait_background(HWND window);
static BOOL WINAPI session_set_event(HANDLE event)
{
    if(event && event==closed_event) ++closed_publications;
    return SetEvent(event);
}
static WNDPROC original_password_proc;
static DWORD password_paints;
static DWORD provider_submits;
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
static void fixture_profiles_load(void)
{
    if(wait_snapshot) wait_background_matches=compare_wait_background(screen);
    if(early_init_probe) {
        HDC dc=screen ? GetDC(screen) : NULL;
        COLORREF pixel=dc ? GetPixel(dc,GetSystemMetrics(SM_CXSCREEN)-10,200) : CLR_INVALID;
        if(dc) ReleaseDC(screen,dc);
        profiles_saw_frame=screen && IsWindowVisible(screen) && preparing_canvas && status_view==VIEW_STARTING &&
            !*bundled_fonts_directory() && pixel!=CLR_INVALID && GetBValue(pixel)>200 && GetRValue(pixel)<130;
    }
    if(single_profile_fixture) {
        char key[160];
        wsprintfA(key,"%s\\Profiles",fixture_root);
        load_sources(HKEY_CURRENT_USER,key,fixture_ini); profiles_remember("First");
    } else profiles_load();
}
static int fixture_fonts_load(HINSTANCE module)
{
    if(early_init_probe) {
        fonts_saw_frame=profiles_saw_frame && screen && IsWindowVisible(screen) && preparing_canvas;
        SetEvent(ready_event); SendMessageA(screen,WM_TIMER,TICK,0);
        init_timer_guarded=!finishing && !single_submitted && !next_logon_attempt;
        ResetEvent(ready_event);
    }
    return bundled_fonts_load(module);
}
static void check(BOOL okay,const char *text)
{
    DWORD written;
    const char *prefix=okay ? "PASS: " : "FAIL: ";
    if(!okay) ++failures;
    WriteFile(output,prefix,(DWORD)lstrlenA(prefix),&written,NULL);
    WriteFile(output,text,(DWORD)lstrlenA(text),&written,NULL); WriteFile(output,"\r\n",2,&written,NULL);
}
/* A 16-bit display quantizes each channel by up to eight levels. */
static BOOL pixel_near(COLORREF actual,COLORREF expected)
{
    int channel,difference;
    if(actual==CLR_INVALID) return FALSE;
    for(channel=0;channel<3;++channel) {
        difference=(int)((actual>>(channel*8))&255)-(int)((expected>>(channel*8))&255);
        if(difference < -8 || difference > 8) return FALSE;
    }
    return TRUE;
}
/* Compare actual displayed frames, preserving the old frame before replacement.
   Status text may change; the background, borders and logo must not jump. */
static BOOL compare_wait_background(HWND window)
{
    HDC device=GetDC(window),dc=device ? CreateCompatibleDC(device) : NULL;
    int x,y,width=GetSystemMetrics(SM_CXSCREEN),height=GetSystemMetrics(SM_CYSCREEN);
    HBITMAP frame=device ? CreateCompatibleBitmap(device,width,height) : NULL;
    HGDIOBJ old=NULL;
    BOOL matches=dc && frame;
    /* Capture both frames in the same display format. Wine may expose a 24-bit
       window DC even on RGB565; comparing it directly to a saved 16-bit bitmap
       otherwise reports conversion rounding as a visible transition. */
    if(matches) {
        old=SelectObject(dc,frame);
        matches=BitBlt(dc,0,0,width,height,device,0,0,SRCCOPY);
    }
    for(y=0;matches && y<height;y+=4) for(x=0;x<width;x+=8) {
        if(y>=cy(334) && y<cy(385) && x>=cx(208) && x<cx(552)) continue;
        if(GetPixel(dc,x,y)!=GetPixel(wait_snapshot,x,y)) {
            wsprintfA(wait_difference,"wait/start frame mismatch at %d,%d: before=%lu after=%lu",x,y,
                (DWORD)GetPixel(wait_snapshot,x,y),(DWORD)GetPixel(dc,x,y));
            matches=FALSE; break;
        }
    }
    if(old) SelectObject(dc,old);
    if(frame) DeleteObject(frame);
    if(dc) DeleteDC(dc);
    if(device) ReleaseDC(window,device);
    SelectObject(wait_snapshot,wait_old); DeleteObject(wait_frame); DeleteDC(wait_snapshot);
    wait_snapshot=NULL;
    return matches;
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
    if(consume_creation_damage && message==WM_CREATE) {
        LRESULT result;
        visible_during_creation=(GetWindowLongA(hwnd,GWL_STYLE)&WS_VISIBLE)!=0;
        result=welcome_proc(hwnd,message,wp,lp);
        /* Model Win9x consuming construction-time damage before first display. */
        ValidateRect(hwnd,NULL);
        return result;
    }
    if(consume_creation_damage && message==WM_WINDOWPOSCHANGED) {
        LRESULT result;
        BOOL showing=preparing_canvas && screen==hwnd && IsWindowVisible(hwnd) &&
            (((WINDOWPOS *)lp)->flags&SWP_SHOWWINDOW);
        if(showing) {
            HDC dc=GetDC(hwnd); RECT area; HBRUSH brush=CreateSolidBrush(RGB(0,128,128));
            GetClientRect(hwnd,&area); FillRect(dc,&area,brush); DeleteObject(brush); ReleaseDC(hwnd,dc);
        }
        result=welcome_proc(hwnd,message,wp,lp);
        if(showing) {
            HDC dc=GetDC(hwnd);
            synchronous_show_painted=pixel_near(GetPixel(dc,GetSystemMetrics(SM_CXSCREEN)-10,10),RGB(0,48,156)) &&
                pixel_near(GetPixel(dc,GetSystemMetrics(SM_CXSCREEN)-10,200),RGB(90,126,220));
            ReleaseDC(hwnd,dc);
        }
        if(IsWindowVisible(hwnd)) ValidateRect(hwnd,NULL);
        return result;
    }
    if(consume_creation_damage && message==WM_PAINT && screen!=hwnd && IsWindowVisible(hwnd)) ++premature_paints;
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
    if(message==WM_USER+44) {
        SetWindowLongPtrA(hwnd,DWLP_MSGRESULT,provider_submits); return TRUE;
    }
    if(message==WM_COMMAND) {
        if(LOWORD(wp)==IDOK) ++provider_submits;
        return TRUE; /* Inert: a click never proves logon. */
    }
    if(message==WM_CLOSE) { DestroyWindow(hwnd); return TRUE; }
    if(message==WM_DESTROY) PostQuitMessage(0);
    return FALSE;
}
static INT_PTR CALLBACK notice_proc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp)
{
    (void)hwnd; (void)message; (void)wp; (void)lp; return FALSE;
}
static LRESULT CALLBACK counted_controller(HWND hwnd,UINT message,WPARAM wp,LPARAM lp)
{
    if(controller_cover && message==WM_WINDOWPOSCHANGING) ++cover_positions;
    if(controller_cover && message==WM_PAINT) ++cover_paints;
    return monitor_proc(hwnd,message,wp,lp);
}
static void register_windows(void)
{
    WNDCLASSA cls;
    zero_memory(&cls,sizeof(cls)); cls.hInstance=instance; cls.hCursor=LoadCursor(NULL,IDC_ARROW);
    cls.lpfnWndProc=counted_screen; cls.lpszClassName="Welcome95.Screen"; RegisterClassA(&cls);
    cls.lpfnWndProc=white_input; cls.lpszClassName="Welcome95.WhiteInput"; RegisterClassA(&cls);
    cls.lpfnWndProc=counted_controller; cls.lpszClassName="Welcome95.Controller"; RegisterClassA(&cls);
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
    HWND competitor,foreground,old_screen,notifier,provider,notice,cover;
    HANDLE startup_worker,shell_worker,notifier_worker,notifier_ready,file,provider_worker;
    DWORD start,written,launched,closed_at,publications_before,child_code;
    HDC dc; COLORREF pixel; BOOL cover_painted=FALSE;
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
            Sleep(500); ExitProcess(0);
        }
        if(!lstrcmpA(mode,"/boot-welcome")) ExitProcess(bootstrap_forward(HKEY_CURRENT_USER,backup_key,"/welcome"));
        if(!lstrcmpA(mode,"/welcome")) {
            HANDLE prepared; HWND canvas; char fail[4];
            if(FindWindowA("Welcome95.Controller",NULL)) ExitProcess(0);
            Sleep(600); /* Simulate mapping/initializing the larger frontend. */
            if(GetEnvironmentVariableA("WELCOME95_TEST_COVER_FAIL",fail,sizeof(fail))) ExitProcess(3);
            prepared=CreateEventA(NULL,TRUE,FALSE,CONTROLLER_EVENT);
            canvas=CreateWindowExA(WS_EX_TOPMOST,"STATIC","Inert prepared frontend",WS_POPUP|WS_VISIBLE|SS_WHITERECT,
                0,0,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN),NULL,NULL,instance,NULL);
            if(!prepared || !canvas) ExitProcess(2);
            UpdateWindow(canvas); SetEvent(prepared); Sleep(200);
            DestroyWindow(canvas); CloseHandle(prepared); ExitProcess(0);
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
    shell_event=CreateEventA(NULL,TRUE,FALSE,SHELL_EVENT); ResetEvent(shell_event);
    ResetEvent(ready_event); ResetEvent(closed_event); ResetEvent(logoff_event); ResetEvent(ended_event); ResetEvent(stop_event);
    ResetEvent(controller_event);
    shell_worker=worker("/boot-welcome"); cover=NULL; start=GetTickCount();
    while((!cover || !IsWindowVisible(cover)) && (DWORD)(GetTickCount()-start)<5000) {
        pump(10); cover=FindWindowA("Welcome95.Bootstrap",NULL);
    }
    check(shell_worker && cover && IsWindowVisible(cover),"the small bootstrap covers the desktop before a delayed frontend is initialized");
    do {
        dc=cover && IsWindow(cover) ? GetDC(cover) : NULL;
        cover_painted=dc && pixel_near(GetPixel(dc,10,10),RGB(0,48,156)) &&
            pixel_near(GetPixel(dc,GetSystemMetrics(SM_CXSCREEN)-10,200),RGB(90,126,220));
        if(dc) ReleaseDC(cover,dc);
        if(cover_painted || !IsWindow(cover)) break;
        pump(10);
    } while((DWORD)(GetTickCount()-start)<5000);
    check(cover_painted,"the early cover paints the welcome bands and background without relying on frontend resources");
    check(finished(shell_worker,5000) && !IsWindow(cover) && WaitForSingleObject(controller_event,0)==WAIT_OBJECT_0,
        "the bootstrap removes its cover after the inert frontend has actually painted and announced initialization");
    CloseHandle(shell_worker); pump(250);
    check(WaitForSingleObject(ready_event,0)==WAIT_TIMEOUT && WaitForSingleObject(closed_event,0)==WAIT_TIMEOUT,
        "cover handoff never publishes authentication or a shell close acknowledgement");
    ResetEvent(controller_event); SetEnvironmentVariableA("WELCOME95_TEST_COVER_FAIL","1");
    shell_worker=worker("/boot-welcome"); start=GetTickCount();
    while(shell_worker && WaitForSingleObject(shell_worker,0)!=WAIT_OBJECT_0 && (DWORD)(GetTickCount()-start)<5000) pump(10);
    child_code=0;
    check(shell_worker && GetExitCodeProcess(shell_worker,&child_code) && child_code==3 &&
        !FindWindowA("Welcome95.Bootstrap",NULL) && WaitForSingleObject(ready_event,0)==WAIT_TIMEOUT &&
        WaitForSingleObject(closed_event,0)==WAIT_TIMEOUT,
        "a failing frontend releases the early cover without cancelling native login or fabricating readiness");
    if(shell_worker) CloseHandle(shell_worker);
    SetEnvironmentVariableA("WELCOME95_TEST_COVER_FAIL",NULL); ResetEvent(controller_event);
    controller=CreateWindowA("Welcome95.Controller","Test controller",WS_OVERLAPPED,0,0,0,0,NULL,NULL,instance,NULL); KillTimer(controller,TICK);
    single_profile_fixture=TRUE;
    release_ui_fonts(); early_init_probe=TRUE;
    begin_logon_cycle(FALSE); early_init_probe=FALSE; KillTimer(screen,TICK); SetEvent(controller_event);
    check(profiles_saw_frame && fonts_saw_frame,"the first complete startup frame is visible before profile discovery or embedded font extraction");
    old_screen=screen; shell_worker=worker("/boot-welcome");
    check(finished(shell_worker,5000) && screen==old_screen && IsWindowVisible(screen),
        "a duplicate welcome bootstrap leaves an existing controller and canvas in place");
    if(shell_worker) CloseHandle(shell_worker);

    check(init_timer_guarded && !preparing_canvas,"a reentrant ready timer cannot authenticate or finish the canvas during resource preparation");
    check(profiles_count()==1 && status_view==VIEW_STARTING && !require_account_choice && !automatic_attempted,
        "a fresh controller still permits its initial one-account quiet startup");
    SendMessageA(controller,WM_ENDSESSION,FALSE,(LPARAM)ENDSESSION_LOGOFF_95);
    check(!require_account_choice && status_view==VIEW_STARTING,
        "a cancelled initial logoff does not consume startup automatic-login eligibility");
    check(WaitForSingleObject(shell_event,0)==WAIT_TIMEOUT && !shell_released &&
        WaitForSingleObject(closed_event,0)==WAIT_TIMEOUT,
        "showing login alone cannot permit the shell or release ordinary startup");
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
    SendMessageA(screen,WM_TIMER,TICK,0);
    check(screen && shell_released && WaitForSingleObject(shell_event,0)==WAIT_OBJECT_0 &&
        WaitForSingleObject(closed_event,0)==WAIT_TIMEOUT && WaitForSingleObject(startup_worker,0)==WAIT_TIMEOUT,
        "confirmed welcome permits shell launch behind the canvas while startup remains blocked");
    start=GetTickCount();
    while(GetFileAttributesA(marker)==INVALID_FILE_ATTRIBUTES && (DWORD)(GetTickCount()-start)<5000) pump(10);
    file=CreateFileA(marker,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    launched=0; written=0;
    if(file!=INVALID_HANDLE_VALUE) { ReadFile(file,&launched,sizeof(launched),&written,NULL); CloseHandle(file); }
    check(written==sizeof(launched) && screen && IsWindowVisible(screen) && shell_confirmed &&
        WaitForSingleObject(closed_event,0)==WAIT_TIMEOUT,
        "the inert original shell begins while welcome is still visible and close acknowledgement is withheld");
    start=GetTickCount();
    while(screen && (DWORD)(GetTickCount()-start)<5000) { pump(10); if(screen) SendMessageA(screen,WM_TIMER,TICK,0); }
    closed_at=canvas_destroyed_at;
    check(!screen && session_running && WaitForSingleObject(closed_event,0)==WAIT_OBJECT_0 &&
        (LONG)(closed_at-launched)>=0,"welcome disappears after shell start and acknowledges only completed destruction");
    check(finished(startup_worker,5000) && finished(shell_worker,5000),"both gates finish after their separate shell-launch and close conditions");
    CloseHandle(startup_worker); CloseHandle(shell_worker);
    {
        HANDLE previous_process=shell_process;
        DWORD previous_id=shell_process_id;
        shell_process=OpenProcess(SYNCHRONIZE,FALSE,GetCurrentProcessId());
        shell_process_id=GetCurrentProcessId(); shell_started_at=GetTickCount();
        check(shell_process && !shell_ready_to_reveal(),
            "a confirmed live shell without a window stays covered during initialization");
        competitor=CreateWindowA("STATIC","Inert shell window",WS_POPUP,10,10,80,40,NULL,NULL,instance,NULL);
        check(competitor && !shell_ready_to_reveal(),"a hidden shell window does not reveal the desktop");
        ShowWindow(competitor,SW_SHOWNOACTIVATE); shell_process_id=0xffffffffUL;
        check(!shell_ready_to_reveal(),"an unrelated process window is not shell presentation evidence");
        shell_process_id=GetCurrentProcessId();
        check(shell_ready_to_reveal(),"a visible shell window permits immediate reveal without animation");
        DestroyWindow(competitor); shell_started_at=GetTickCount()-2001;
        check(shell_ready_to_reveal(),"a headless live shell has a bounded two-second presentation wait");
        shell_confirmed=FALSE; shell_released_at=GetTickCount();
        check(!shell_ready_to_reveal(),"a missing launch notification keeps welcome covered initially");
        shell_released_at=GetTickCount()-5001;
        check(shell_ready_to_reveal(),"a missing launch notification cannot trap an authenticated session indefinitely");
        CloseHandle(shell_process); shell_process=previous_process; shell_process_id=previous_id;
        shell_confirmed=TRUE;
    }
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
    publications_before=closed_publications;
    SendMessageA(controller,WM_TIMER,TICK,0);
    check(IsWindow(controller) && !screen && awaiting_logon && !body_font && !session_running,
        "committed logoff retires session GDI resources without creating children during desktop teardown");
    check(controller_cover && IsWindowVisible(controller) && GetWindow(controller,GW_CHILD)==NULL,
        "logoff keeps a complete visual cover in the existing service window without creating credential controls");

    ShowWindow(controller,SW_HIDE);
    check(IsWindowVisible(controller),"an active wait cover cannot be hidden even before queued repairs can run");
    pump(30);
    check(controller_cover && IsWindowVisible(controller),
        "an externally hidden wait cover is restored promptly without waiting for a new provider");
    ShowWindow(controller,SW_SHOWNOACTIVATE); /* Keep the remainder diagnostic on old builds too. */
    SetWindowPos(controller,HWND_BOTTOM,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
    check(GetWindowLongA(controller,GWL_EXSTYLE)&WS_EX_TOPMOST,
        "the wait cover cannot be demoted behind the desktop before queued repair");
    SetWindowPos(controller,NULL,12,16,160,120,SWP_NOZORDER|SWP_NOACTIVATE);
    SendMessageA(controller,WM_DISPLAYCHANGE,0,0); pump(30);
    {
        RECT bounds; GetWindowRect(controller,&bounds);
        check(bounds.left==0 && bounds.top==0 && bounds.right==GetSystemMetrics(SM_CXSCREEN) &&
            bounds.bottom==GetSystemMetrics(SM_CYSCREEN),
            "the waiting cover repairs screen geometry before repainting after display changes");
    }
    SetWindowPos(controller,HWND_TOPMOST,0,0,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN),SWP_NOACTIVATE);
    RedrawWindow(controller,NULL,NULL,RDW_INVALIDATE|RDW_UPDATENOW);
    dc=GetDC(controller); wait_snapshot=CreateCompatibleDC(dc);
    wait_frame=CreateCompatibleBitmap(dc,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN));
    wait_old=SelectObject(wait_snapshot,wait_frame);
    BitBlt(wait_snapshot,0,0,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN),dc,0,0,SRCCOPY);
    ReleaseDC(controller,dc);
    cover_positions=cover_paints=0;
    for(i=0;i<100;++i) SendMessageA(controller,WM_TIMER,TICK,0);
    check(!cover_positions && !cover_paints,"100 stable wait-cover ticks cause no positioning or repaint churn");
    check(!screen && awaiting_logon && closed_publications==publications_before &&
        WaitForSingleObject(closed_event,0)==WAIT_TIMEOUT,
        "without a new provider or OS handshake logoff stays idle and never publishes a close acknowledgement");
    competitor=CreateWindowA("Welcome95.WhiteInput","Teardown background",WS_POPUP|WS_VISIBLE,
        0,0,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN),NULL,NULL,instance,NULL);
    UpdateWindow(competitor);
    consume_creation_damage=TRUE; visible_during_creation=FALSE; synchronous_show_painted=FALSE; premature_paints=0;
    provider_worker=worker("/provider"); provider=NULL; start=GetTickCount();
    while((!provider || !IsWindowVisible(provider)) && (DWORD)(GetTickCount()-start)<5000) {
        pump(10); provider=FindWindowA("#32770","Welcome95 synthetic provider");
    }
    PostMessageA(controller,REPAIR_COVER,0,0); /* Delayed work from the old wait phase. */
    SendMessageA(controller,WM_TIMER,TICK,0); if(screen) KillTimer(screen,TICK);
    check(provider && screen && status_view==VIEW_LOGIN && !awaiting_logon && !session_running,
        "a new real provider triggers the next login canvas without exiting the controller");
    check(synchronous_show_painted,"the replacement paints during its synchronous show notification before ShowWindow returns");
    check(!controller_cover && !IsWindowVisible(controller),"the waiting cover is hidden only after the replacement canvas is fully presented");
    check(wait_background_matches,wait_background_matches ? "Please wait and the first Windows starting frame have identical background, borders and logo pixels" : wait_difference);
    pump(30);
    check(!controller_cover && !IsWindowVisible(controller),"queued cover repairs cannot resurrect it after a completed canvas handoff");

    check(!visible_during_creation && !premature_paints && IsWindowVisible(screen),
        "canvas construction stays hidden and first visible painting has its published window handle");
    dc=GetDC(screen); i=(GetSystemMetrics(SM_CXSCREEN)-10)*128/GetSystemMetrics(SM_CXSCREEN);
    check(pixel_near(GetPixel(dc,GetSystemMetrics(SM_CXSCREEN)-10,10),RGB(0,48,156)) &&
        pixel_near(GetPixel(dc,GetSystemMetrics(SM_CXSCREEN)-10,200),RGB(90,126,220)) &&
        pixel_near(GetPixel(dc,GetSystemMetrics(SM_CXSCREEN)-10,GetSystemMetrics(SM_CYSCREEN)-5),
            RGB(57-i*57/127,52-i*4/127,173-i*17/127)),
        "full top middle and footer pixels appear immediately despite consumed creation damage");
    ReleaseDC(screen,dc); consume_creation_damage=FALSE; DestroyWindow(competitor);
    check(!finishing && automatic_attempted && require_account_choice && !pending_profile && selected_profile==0,
        "new logon resets in-flight state and selection while requiring manual account choice");
    check(profiles_count()==1 && !account_open() && !GetDlgItem(screen,106) && !IsWindowVisible(GetDlgItem(screen,ID_SUBMIT)),
        "even one account is collapsed after logoff and cannot submit before selection");
    check(WaitForSingleObject(ready_event,0)==WAIT_TIMEOUT && WaitForSingleObject(closed_event,0)==WAIT_TIMEOUT &&
        WaitForSingleObject(shell_event,0)==WAIT_TIMEOUT && !shell_confirmed && !shell_released,
        "new logon clears the previous session's readiness and close acknowledgements");
    old_screen=screen; SetEvent(logoff_event); SendMessageA(controller,WM_TIMER,TICK,0);
    check(screen==old_screen && WaitForSingleObject(closed_event,0)==WAIT_TIMEOUT,"duplicate logoff notifications do not restart an already waiting login");
    SendMessageA(controller,WM_ENDSESSION,FALSE,(LPARAM)ENDSESSION_LOGOFF_95);
    check(screen==old_screen && IsWindow(screen),"cancelled logoff preserves the current login canvas");
    SendMessageA(screen,WM_QUERYENDSESSION,0,0);
    SendMessageA(screen,WM_ENDSESSION,FALSE,0);
    check(screen==old_screen && status_view==VIEW_LOGIN,"cancelled shutdown during login preserves the resident login canvas");
    PostMessageA(provider,WM_CLOSE,0,0);
    check(finished(provider_worker,5000),"the first inert provider closes before reasonless-session recovery");
    CloseHandle(provider_worker);
    publications_before=closed_publications;
    SendMessageA(controller,WM_ENDSESSION,TRUE,0); SendMessageA(controller,WM_TIMER,TICK,0);
    check(closed_publications==publications_before && WaitForSingleObject(closed_event,0)==WAIT_TIMEOUT,
        "retiring a visible canvas never briefly publishes a shell handoff acknowledgement");
    check(IsWindow(controller) && !screen && awaiting_logon && !body_font,
        "ambiguous session end keeps the controller and releases temporary font resources");
    startup_worker=worker("/gate-startup"); start=GetTickCount();
    while(!screen && (DWORD)(GetTickCount()-start)<5000) pump(10);
    if(screen) KillTimer(screen,TICK);
    check(screen && IsWindow(screen) && !awaiting_logon,"a new OS startup gate also restores welcome after a reasonless session end");
    check(require_account_choice && automatic_attempted && !account_open() && status_view==VIEW_LOGIN,
        "the ready-event fallback after an ambiguous session end cannot rearm automatic login");
    check(WaitForSingleObject(startup_worker,0)==WAIT_TIMEOUT && WaitForSingleObject(closed_event,0)==WAIT_TIMEOUT,
        "creating a new canvas never acknowledges its own startup gate prematurely");
    SendMessageA(screen,WM_TIMER,TICK,0); finish_time=GetTickCount()-1001; SendMessageA(screen,WM_TIMER,TICK,0);
    shell_released_at=GetTickCount()-5001; SendMessageA(screen,WM_TIMER,TICK,0);
    check(finished(startup_worker,5000),"the new gate waits until its own welcome has actually closed"); CloseHandle(startup_worker);
    SendMessageA(controller,WM_ENDSESSION,TRUE,0); SendMessageA(controller,WM_TIMER,TICK,0);
    provider_worker=worker("/provider"); provider=NULL; start=GetTickCount();
    while((!provider || !IsWindowVisible(provider)) && (DWORD)(GetTickCount()-start)<5000) {
        pump(10); provider=FindWindowA("#32770","Welcome95 synthetic provider");
    }
    SendMessageA(controller,WM_TIMER,TICK,0); if(screen) KillTimer(screen,TICK);
    check(provider && screen && status_view==VIEW_LOGIN && !finishing && WaitForSingleObject(ready_event,0)==WAIT_TIMEOUT,
        "a separate provider's new password dialog restores login without inventing session readiness");
    {
        DWORD_PTR submits=0;
        allow_logon(); SendMessageA(screen,WM_TIMER,TICK,0); pump(60);
        check(!native_logon_attached() && !account_open() &&
            SendMessageTimeoutA(provider,WM_USER+44,0,0,SMTO_ABORTIFHUNG,200,&submits) && !submits,
            "a repeated one-account login sends no native submit while waiting for a card click");
        SendMessageA(screen,WM_LBUTTONDOWN,0,MAKELPARAM(card_x(440),row_y(profile_top(0)+10)));
        SendMessageA(screen,WM_TIMER,TICK,0); pump(60);
        check(account_open() && !pending_profile && native_logon_attached() && require_account_choice &&
            SendMessageTimeoutA(provider,WM_USER+44,0,0,SMTO_ABORTIFHUNG,200,&submits) && !submits,
            "clicking the sole account exposes real input without triggering automatic authentication");
        SendMessageA(screen,WM_COMMAND,MAKEWPARAM(ID_SUBMIT,BN_CLICKED),(LPARAM)GetDlgItem(screen,ID_SUBMIT)); pump(60);
        check(SendMessageTimeoutA(provider,WM_USER+44,0,0,SMTO_ABORTIFHUNG,200,&submits) && submits==1 &&
            !finishing && WaitForSingleObject(ready_event,0)==WAIT_TIMEOUT,
            "an explicit submit still reaches Windows once and does not fabricate session readiness");
    }
    check(native_logon_attached() && login_window==provider,
        "the repeated login attaches real controls owned by a separate synthetic provider process");
    {
        RECT original,after; LONG style; DWORD_PTR modified=0;
        require_account_choice=FALSE; /* A separate fresh-controller startup fixture. */
        native_logon_restore(FALSE); GetWindowRect(provider,&original); style=GetWindowLongA(provider,GWL_STYLE);
        profiles_remember("First"); pending_profile=FALSE; automatic_attempted=FALSE;
        single_submitted=FALSE; single_started=GetTickCount();
        SendMessageA(GetDlgItem(provider,23),EM_SETMODIFY,FALSE,0);
        check(native_logon_probe(provider),"a separate provider supports the quiet single-user probe");
        set_status_view(screen,VIEW_STARTING); try_single_account_logon(); pump(60); maintain_topmost(screen);
        GetWindowRect(provider,&after);
        check(single_submitted && status_view==VIEW_STARTING && !finishing &&
            !IsWindowVisible(GetDlgItem(screen,ID_SUBMIT)) && EqualRect(&original,&after) &&
            (GetWindowLongA(provider,GWL_STYLE)&WS_CAPTION)==(style&WS_CAPTION) && window_above(screen,provider),
            "one-account startup shows only startup status while the untouched real provider stays behind it");
        check(WaitForSingleObject(ready_event,0)==WAIT_TIMEOUT && WaitForSingleObject(closed_event,0)==WAIT_TIMEOUT,
            "a posted blank attempt never authenticates or releases shell startup by itself");
        single_started=GetTickCount()-4001; service_single_pending();
        check(status_view==VIEW_LOGIN && automatic_attempted && IsWindowVisible(GetDlgItem(screen,ID_POWER)),
            "an unconfirmed quiet attempt returns to manual login without retrying or hiding recovery controls");
        SendMessageTimeoutA(GetDlgItem(provider,23),EM_GETMODIFY,0,0,SMTO_ABORTIFHUNG,200,&modified);
        allow_logon();
        check(native_logon_attached() && login_window==provider,"manual fallback embeds the same real provider normally");
        native_logon_restore(FALSE); native_logon_probe(provider);
        set_status_view(screen,VIEW_STARTING); set_status_view(screen,VIEW_SHUTDOWN); set_status_view(screen,VIEW_LOGIN);
        allow_logon();
        check(native_logon_attached() && native_logon_layout(card_x(501),row_y(profile_top(selected_profile)+52),227),
            "cancelled shutdown during a quiet probe still embeds a real manual password field");
        require_account_choice=TRUE;
    }
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
    check(rescue_mode && !IsWindowVisible(screen) && !GetDlgItem(screen,106) && IsWindowVisible(provider) &&
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
    controller=CreateWindowA("Welcome95.Controller","Inert stop-cover fixture",WS_OVERLAPPED,0,0,0,0,NULL,NULL,instance,NULL);
    KillTimer(controller,TICK); show_controller_cover(controller);
    SendMessageA(controller,WM_CLOSE,0,0);
    check(IsWindow(controller) && controller_cover && IsWindowVisible(controller),
        "a generic close cannot remove an active wait cover before handoff");
    SendMessageA(controller,WM_TIMER,TICK,0);
    check(!IsWindow(controller) && !controller_cover,
        "explicit Disable still destroys an active cover despite hide and close guards");
    check(startup_configure(&startup_config,FALSE)==0,"restore the isolated shell configuration after lifecycle tests");
    if(shell_process) CloseHandle(shell_process);
    CloseHandle(shell_event); CloseHandle(ready_event); CloseHandle(closed_event); CloseHandle(logoff_event); CloseHandle(ended_event); CloseHandle(controller_event); CloseHandle(stop_event);
    RegDeleteKeyA(HKEY_CURRENT_USER,service_key); RegDeleteKeyA(HKEY_CURRENT_USER,run_key);
    RegDeleteKeyA(HKEY_CURRENT_USER,once_key); RegDeleteKeyA(HKEY_CURRENT_USER,backup_key); RegDeleteKeyA(HKEY_CURRENT_USER,fixture_root);
    remove_fixture(fixture_dir);
    check(RemoveDirectoryA(fixture_dir),"remove only owned session fixtures");
    SetEnvironmentVariableA("WELCOME95_TEST_ROOT",NULL); SetEnvironmentVariableA("WELCOME95_TEST_DIR",NULL);
    release_ui_fonts(); CloseHandle(output); ExitProcess(failures ? 1 : 0);
}
