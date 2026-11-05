#include "ui_text.h"
/* Welcome95: ANSI Win32 only, no Unicode/NT-only APIs. */
#include <windows.h>
#include "native_logon.h"
#include "profiles.h"
#include "fonts.h"
#include "power_dialog.h"
#include "power_actions.h"
#include "startup.h"
#include "control_panel.h"
#include "painted_button.h"
#include "bootstrap.h"
#include "early_cover.h"

#define APP "Welcome95"
#define REG_SERVICE "Software\\Microsoft\\Windows\\CurrentVersion\\RunServices"
#define REG_RUN "Software\\Microsoft\\Windows\\CurrentVersion\\Run"
#define READY_EVENT "Welcome95.SessionReady"
#define STOP_EVENT "Welcome95.Stop"
#define INSTANCE_EVENT "Welcome95.Running"
#define CLOSED_EVENT "Welcome95.CanvasClosed"
#define SHELL_EVENT "Welcome95.ShellMayStart"
#define LOGOFF_EVENT "Welcome95.LogoffCommitted"
#define ENDED_EVENT "Welcome95.SessionEnded"
#define CONTROLLER_EVENT BOOTSTRAP_READY_EVENT
#define SHOW_POWER (WM_USER+72)
#define REPAIR_COVER (WM_USER+73)
#define RESTORE_LAYERS (WM_USER+74)
#define SHELL_STARTED (WM_USER+70)
#define SESSION_STARTING (WM_USER+71)
#define ENDSESSION_LOGOFF_95 0x80000000UL
#define ID_ENABLE 101
#define ID_DISABLE 102
#define ID_PREVIEW 103
#define ID_DONE 104
#define ID_EXIT 105
#define ID_SUBMIT 107
#define ID_POWER 108
#define ID_PREV 109
#define ID_NEXT 110
#define ID_PREVIEW_SINGLE 111
#define ID_PREVIEW_MULTI 112
#define ID_PREVIEW_SHUTDOWN 113
#define ID_PREVIEW_STARTING 115
#define ID_PROFILE_SCROLL 114
#define ID_RESCUE_HOTKEY 3
#define VIEW_LOGIN 0
#define VIEW_WELCOME_SINGLE 1
#define VIEW_WELCOME_MULTI 2
#define VIEW_SHUTDOWN 3
#define VIEW_STARTING 4
#define SETTINGS_WIDTH 410
#define SETTINGS_HEIGHT 234
#define TICK 1

static HINSTANCE instance;
static HWND screen;
static HFONT title_font, body_font, small_font, welcome_font, status_font, power_caption_font, settings_font, settings_title_font, message_font;
static int status_view;
static BOOL monitor_active, preview_status;
static DWORD preview_started, monitor_started;
static HANDLE monitor_instance;
static const PowerOperations *menu_power;
static BOOL preview_power;
static HANDLE ready_event, stop_event, running_event, closed_event, logoff_event, controller_event, ended_event;
static HANDLE shell_process, shell_event;
static DWORD shell_process_id,shell_started_at,shell_released_at;
static BOOL shell_confirmed,shell_released;
static HWND controller,backdrop;
static BOOL backdrop_active,positioning_backdrop;
static void show_backdrop(void);
static void hide_backdrop(void);
static void destroy_backdrop(void);
static void maintain_backdrop(void);
static BOOL resident_mode, session_running, awaiting_logon, power_requested, ending_unknown, shutdown_only_canvas;
static DWORD provider_process;
typedef DWORD (WINAPI *RegisterService)(DWORD,DWORD);
static RegisterService register_service;
static StartupConfig startup_config;
static PanelConfig panel_config;
static char system_ini[MAX_PATH],control_ini[MAX_PATH],boot_directory[MAX_PATH];
static BOOL rescue_mode;
static BOOL preview, finishing, preparing_canvas, power_pending, controller_cover;
static BOOL cover_repair_pending,repairing_cover,canvas_show_painted;
static HDC first_frame_dc;
static HBITMAP first_frame_bitmap;
static HGDIOBJ first_frame_old;
static int first_frame_width,first_frame_height;
static HWND cached_paint_window;
static BOOL cached_paint_erased;
static DWORD finish_time;
static HWND login_window;
static BOOL login_was_topmost;
static BOOL prefer_native;
static int selected_profile, profile_page;
static BOOL pending_profile, servicing_logon, automatic_attempted, compact_selected;
static BOOL single_submitted;
/* Once a session has ended, require a deliberate choice until Windows restarts. */
static BOOL require_account_choice;
static DWORD single_started;
static DWORD next_logon_attempt, next_profile_attempt;
static BOOL escape_hotkey, close_hotkey;
static char exe_path[MAX_PATH];
static BOOL sign_in_view(void) { return status_view==VIEW_LOGIN || status_view==VIEW_STARTING; }

/* Artwork/control dimensions stay in pixels; only their anchors move. */
static int cx(int value) { return value + (GetSystemMetrics(SM_CXSCREEN)-800)/2; }
static int cy(int value) { return value + (GetSystemMetrics(SM_CYSCREEN)-600)/2; }
static int rx(int value) { return value + GetSystemMetrics(SM_CXSCREEN)-800; }
static int by(int value) { return value + GetSystemMetrics(SM_CYSCREEN)-600; }

static BOOL compact_layout(void)
{
    return GetSystemMetrics(SM_CXSCREEN)<800 || GetSystemMetrics(SM_CYSCREEN)<600;
}
static int compact_x(int value) { return value+(GetSystemMetrics(SM_CXSCREEN)-640)/2; }
static int compact_y(int value) { return value+(GetSystemMetrics(SM_CYSCREEN)-480)/2; }
static int card_x(int value) { return compact_layout() ? compact_x(value-268) : cx(value); }
static int row_y(int value) { return compact_layout() ? compact_y(value) : cy(value); }
static int loading_y(int value) { return compact_layout() ? compact_y(value-15) : cy(value); }
static BOOL account_open(void)
{
    if(require_account_choice && !compact_selected) return FALSE;
    return !compact_layout() || profiles_count()<=1 || compact_selected;
}
static int page_size(void) { return compact_layout() ? 2 : 4; }
static int page_first(void) { return profile_page*page_size(); }
static int visible_profiles(void)
{
    int total=profiles_count() ? profiles_count() : 1;
    int remaining=total-page_first();
    return remaining<page_size() ? remaining : page_size();
}
static int profile_top(int index)
{
    int slot=index-page_first();
    if(compact_layout()) return 233+slot*80-(selected_profile==page_first()+1 ? 6 : 0);
    return 291-(visible_profiles()*72+8)/2+slot*72+
        (index>selected_profile && selected_profile/page_size()==profile_page ? 8 : 0);
}
static void login_geometry(int *x,int *y,int *width)
{
    *x=card_x(501); *y=row_y(profile_top(selected_profile)+52); *width=227;
}
static void position_arrow(HWND hwnd)
{
    SetWindowPos(GetDlgItem(hwnd,ID_SUBMIT),NULL,card_x(685),row_y(profile_top(selected_profile)+51),
        26,26,SWP_NOZORDER|SWP_NOACTIVATE);
    ShowWindow(GetDlgItem(hwnd,ID_SUBMIT),status_view==VIEW_LOGIN && account_open() ? SW_SHOWNA : SW_HIDE);
}
static void position_profile_navigation(HWND hwnd)
{
    BOOL show=status_view==VIEW_LOGIN;
    int count=profiles_count();
    SCROLLINFO range={sizeof(SCROLLINFO),SIF_RANGE|SIF_PAGE|SIF_POS|SIF_DISABLENOSCROLL,0,0,1,0,0};
    HWND scroll=GetDlgItem(hwnd,ID_PROFILE_SCROLL);
    SetWindowPos(scroll,NULL,compact_x(495),compact_y(208),17,176,SWP_NOZORDER|SWP_NOACTIVATE);
    range.nMax=count>0 ? (count-1)/page_size() : 0; range.nPos=profile_page;
    SetScrollInfo(scroll,SB_CTL,&range,TRUE);
    ShowWindow(scroll,show && compact_layout() && count>1 ? SW_SHOWNA : SW_HIDE);
    ShowWindow(GetDlgItem(hwnd,ID_PREV),show && !compact_layout() && count>4 ? SW_SHOWNA : SW_HIDE);
    ShowWindow(GetDlgItem(hwnd,ID_NEXT),show && !compact_layout() && count>4 ? SW_SHOWNA : SW_HIDE);
}

static void zero_memory(void *memory, unsigned int size)
{
    unsigned char *bytes = (unsigned char *)memory;
    while (size--) *bytes++ = 0;
}

static void error_box(HWND owner, const char *message)
{
    MessageBoxA(owner, message, APP, MB_OK | MB_ICONERROR);
}

static void command_line(char *out, const char *argument)
{
    wsprintfA(out, "\"%s\" %s", exe_path, argument);
}

static BOOL enabled(void) { return startup_installed(&startup_config); }
static void initialize_startup(void)
{
    UINT length=GetWindowsDirectoryA(system_ini,sizeof(system_ini));
    if(!length || length+12>=sizeof(system_ini)) system_ini[0]=0;
    else lstrcatA(system_ini,"\\SYSTEM.INI");
    startup_config.boot_directory=NULL; startup_config.boot_image=NULL; startup_config.boot_size=0;
    if(system_ini[0] && length>=3 && system_ini[1]==':' && system_ini[2]=='\\') {
        HRSRC resource=FindResourceA(instance,MAKEINTRESOURCEA(300),MAKEINTRESOURCEA(10));
        HGLOBAL data=resource ? LoadResource(instance,resource) : NULL;
        lstrcpynA(boot_directory,system_ini,length+1); lstrcatA(boot_directory,"\\LOGON");
        startup_config.boot_directory=boot_directory;
        startup_config.boot_image=data ? (const BYTE *)LockResource(data) : NULL;
        startup_config.boot_size=resource ? SizeofResource(instance,resource) : 0;
    }
    startup_config.root=HKEY_LOCAL_MACHINE;
    startup_config.service_key=STARTUP_SERVICE; startup_config.run_key=STARTUP_RUN;
    startup_config.once_key=STARTUP_ONCE; startup_config.backup_key=STARTUP_BACKUP;
    startup_config.executable=exe_path; startup_config.ini=system_ini;
    startup_config.write_ini=WritePrivateProfileStringA;
    lstrcpynA(control_ini,system_ini,sizeof(control_ini));
    if(control_ini[0]) lstrcpyA(control_ini+length+1,"CONTROL.INI");
    panel_config.control_ini=control_ini; panel_config.write_ini=WritePrivateProfileStringA;
    panel_config.root=HKEY_LOCAL_MACHINE; panel_config.cpls_key=PANEL_CPLS;
    panel_config.storage_key=PANEL_STORAGE; panel_config.executable=exe_path;
    {
        HRSRC resource=FindResourceA(instance,MAKEINTRESOURCEA(301),MAKEINTRESOURCEA(10));
        HGLOBAL data=resource ? LoadResource(instance,resource) : NULL;
        panel_config.image=data ? (const BYTE *)LockResource(data) : NULL;
        panel_config.size=resource ? SizeofResource(instance,resource) : 0;
    }
}
static BOOL configure(HWND owner,BOOL enable)
{
    DWORD error;
    HANDLE event;
    BOOL migrated=FALSE;
    char message[512];
    const char *stage;
    if(enable && !(GetVersion()&0x80000000UL)) {
        error_box(owner,ui_text("\300\342\362\356\347\340\357\363\361\352 \344\356\361\362\363\357\345\355 \362\356\353\374\352\356 \342 Windows 95/98/Me. \302 \344\360\363\343\350\365 \361\350\361\362\345\354\340\365 \344\356\361\362\363\357\345\355 \357\360\356\361\354\356\362\360.","Automatic startup is intended for Windows 95/98/Me. Preview is available on other systems.")); return FALSE;
    }
    if(!system_ini[0]) { error_box(owner,ui_text("\315\345 \355\340\351\344\345\355 SYSTEM.INI. \315\340\361\362\360\356\351\352\350 \340\342\362\356\347\340\357\363\361\352\340 \355\345 \350\347\354\345\355\345\355\373.","Cannot locate SYSTEM.INI. No startup changes made.")); return FALSE; }
    if(enable && (!startup_config.boot_directory || !startup_config.boot_image || !startup_config.boot_size)) {
        error_box(owner,ui_text("\315\345 \363\344\340\353\356\361\374 \347\340\343\360\363\347\350\362\374 \354\356\344\363\353\374 \347\340\357\363\361\352\340 Windows 95. \315\340\361\362\360\356\351\352\350 \340\342\362\356\347\340\357\363\361\352\340 \355\345 \350\347\354\345\355\345\355\373.","Cannot load the Windows 95 bootstrap. No startup changes made.")); return FALSE;
    }
    error=panel_startup_configure(&panel_config,&startup_config,enable,&migrated,&stage);
    if(error) {
        wsprintfA(message,ui_text("\315\345 \363\344\340\353\356\361\374 \350\347\354\345\355\350\362\374 \340\342\362\356\347\340\357\363\361\352 Welcome95 (\356\370\350\341\352\340 %lu).\r\n\335\362\340\357: %s\r\n\306\363\360\355\340\353 \363\361\362\340\355\356\342\352\350: LOGON\\W95START.LOG.","Cannot update Welcome95 startup (error %lu).\r\nStage: %s\r\nSetup log: LOGON\\W95START.LOG."),error,stage);
        error_box(owner,message); return FALSE;
    }
    if(!enable || migrated) {
        event=OpenEventA(EVENT_MODIFY_STATE,FALSE,STOP_EVENT);
        if(event) { SetEvent(event); CloseHandle(event); }
        event=OpenEventA(EVENT_MODIFY_STATE,FALSE,CLOSED_EVENT);
        if(event) { SetEvent(event); CloseHandle(event); }
    }
    return TRUE;
}

/* Settings launched from a download/Desktop run their managed copy. Previews
   and startup helper modes never install or change the system configuration. */
static int managed_settings(void)
{
    char windows[MAX_PATH],target[MAX_PATH],command[MAX_PATH+4],message[240];
    STARTUPINFOA startup={0}; PROCESS_INFORMATION child;
    DWORD error,length; BOOL copied=FALSE;
    length=GetWindowsDirectoryA(windows,sizeof(windows));
    error=!length || length>=sizeof(windows) ? ERROR_PATH_NOT_FOUND :
        startup_prepare_logon(exe_path,windows,target,&copied);
    if(!error && !copied) return -1;
    if(!error) {
        startup.cb=sizeof(startup); wsprintfA(command,"\"%s\"",target);
        if(CreateProcessA(target,command,NULL,NULL,FALSE,0,NULL,NULL,&startup,&child)) {
            CloseHandle(child.hThread); CloseHandle(child.hProcess); return 0;
        }
        error=GetLastError();
    }
    wsprintfA(message,ui_text("\315\345 \363\344\340\353\356\361\374 \363\361\362\340\355\356\342\350\362\374 Welcome95 \342 \357\340\357\352\363 Windows\\LOGON (\356\370\350\341\352\340 %lu).\r\n\316\362\352\353\376\367\350\362\345 \361\362\340\360\363\376 \342\345\360\361\350\376 \350 \347\340\352\360\356\351\362\345 Welcome95 \357\345\360\345\344 \356\341\355\356\342\353\345\355\350\345\354.","Cannot install Welcome95 in Windows\\LOGON (error %lu).\r\nDisable the old version and close Welcome95 before updating."),error);
    error_box(NULL,message); return 1;
}

/* A shell/startup gate runs in the newly logged-on user's environment. */
static DWORD handoff(BOOL launch_shell)
{
    HANDLE ready,closed,initialized,stop,permission;
    STARTUPINFOA startup;
    PROCESS_INFORMATION child;
    char original[STARTUP_TEXT],command[MAX_PATH+40];
    DWORD error=0,waited,started=GetTickCount();
    DWORD_PTR result;
    HWND target;
    startup_trace(startup_config.root,startup_config.backup_key,launch_shell ? "handoff-shell" : "handoff-startup",0);
    if(launch_shell) {
        error=startup_original(&startup_config,original,sizeof(original));
        if(error) { startup_trace(startup_config.root,startup_config.backup_key,"original-shell-error",error); return error; }
    }
    if(!enabled()) goto launch;
    ready=CreateEventA(NULL,TRUE,FALSE,READY_EVENT);
    closed=CreateEventA(NULL,TRUE,FALSE,CLOSED_EVENT);
    initialized=CreateEventA(NULL,TRUE,FALSE,CONTROLLER_EVENT);
    stop=CreateEventA(NULL,TRUE,FALSE,STOP_EVENT);
    permission=CreateEventA(NULL,TRUE,FALSE,SHELL_EVENT);
    if(!ready || !closed || !initialized || !stop || !permission) error=ERROR_NOT_ENOUGH_MEMORY;
    target=FindWindowA("Welcome95.Controller",NULL);
    if(!error && !target && WaitForSingleObject(stop,0)!=WAIT_OBJECT_0) {
        zero_memory(&startup,sizeof(startup)); startup.cb=sizeof(startup);
        command_line(command,"/welcome");
        if(CreateProcessA(exe_path,command,NULL,NULL,FALSE,0,NULL,NULL,&startup,&child)) {
            CloseHandle(child.hThread); CloseHandle(child.hProcess);
        } else error=GetLastError();
    }
    while(!error && WaitForSingleObject(initialized,0)!=WAIT_OBJECT_0) {
        if(WaitForSingleObject(stop,0)==WAIT_OBJECT_0) { error=ERROR_CANCELLED; break; }
        if((DWORD)(GetTickCount()-started)>=30000) { error=ERROR_TIMEOUT; break; }
        Sleep(20);
    }
    if(!error) {
        target=FindWindowA("Welcome95.Controller",NULL);
        result=0;
        if(!target || !SendMessageTimeoutA(target,SESSION_STARTING,0,0,SMTO_ABORTIFHUNG,5000,&result) || !result)
            error=ERROR_INVALID_WINDOW_HANDLE;
    }
    if(!error) {
        startup_trace(startup_config.root,startup_config.backup_key,launch_shell ? "handoff-wait-shell-permission" : "handoff-wait-close",0);
        do {
            waited=WaitForSingleObject(launch_shell ? permission : closed,20);
            if(waited==WAIT_OBJECT_0) break;
            if(waited==WAIT_FAILED) { error=GetLastError(); break; }
            if(WaitForSingleObject(stop,0)==WAIT_OBJECT_0) { error=ERROR_CANCELLED; break; }
            if((DWORD)(GetTickCount()-started)>=30000) { error=ERROR_TIMEOUT; break; }
        } while(TRUE);
    }
    /* Fail open on a broken controller, never leave the machine without a shell. */
    startup_trace(startup_config.root,startup_config.backup_key,"handoff-wait-result",error);
    if(error && stop) SetEvent(stop);
    if(ready) CloseHandle(ready);
    if(closed) CloseHandle(closed);
    if(initialized) CloseHandle(initialized);
    if(stop) CloseHandle(stop);
    if(permission) CloseHandle(permission);
launch:
    if(!launch_shell) return 0;
    zero_memory(&startup,sizeof(startup)); startup.cb=sizeof(startup);
    if(!CreateProcessA(NULL,original,NULL,NULL,FALSE,0,NULL,NULL,&startup,&child)) {
        error=GetLastError(); startup_trace(startup_config.root,startup_config.backup_key,"shell-create-error",error); return error;
    }
    startup_trace(startup_config.root,startup_config.backup_key,"shell-started",child.dwProcessId);
    target=FindWindowA("Welcome95.Controller",NULL);
    if(target) SendMessageTimeoutA(target,SHELL_STARTED,child.dwProcessId,0,SMTO_ABORTIFHUNG,200,&result);
    CloseHandle(child.hThread); CloseHandle(child.hProcess);
    if(enabled()) startup_rearm(&startup_config);
    return 0;
}

static void fill(HDC dc, RECT rectangle, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, &rectangle, brush);
    DeleteObject(brush);
}

static void label(HDC dc, HFONT font, const char *text, RECT rectangle, UINT flags)
{
    HFONT old = (HFONT)SelectObject(dc, font);
    DrawTextA(dc, text, -1, &rectangle, flags | DT_NOPREFIX);
    SelectObject(dc, old);
}

static void finish_paint(HWND hwnd, PAINTSTRUCT *paint, HDC dc, HBITMAP frame, HGDIOBJ old)
{
    RECT area;
    HWND button;
    int index;
    static const int custom_buttons[]={ID_POWER,ID_SUBMIT};
    if(frame) {
        GetClientRect(hwnd,&area);
        BitBlt(paint->hdc,0,0,area.right,area.bottom,dc,0,0,SRCCOPY);
        SelectObject(dc,old); DeleteObject(frame); DeleteDC(dc);
    }
    EndPaint(hwnd,paint);
    /* WS_CLIPCHILDREN keeps the parent's buffer out of the button rectangles.
       Win9x can invalidate just the parent after uncovering/recreating it. */
    for(index=0;index<2;++index) {
        button=GetDlgItem(hwnd,custom_buttons[index]);
        if(button && IsWindowVisible(button)) {
            RECT bounds;
            GetWindowRect(button,&bounds); MapWindowPoints(NULL,hwnd,(POINT *)&bounds,2);
            if(bounds.left<paint->rcPaint.right && bounds.right>paint->rcPaint.left &&
               bounds.top<paint->rcPaint.bottom && bounds.bottom>paint->rcPaint.top) {
                InvalidateRect(button,NULL,FALSE); UpdateWindow(button);
            }
        }
    }
    if(status_view==VIEW_LOGIN) native_logon_repaint();
}

static void bitmap(HDC dc, int id, int x, int y, int width, int height)
{
    HRSRC resource=FindResourceA(instance, MAKEINTRESOURCEA(id), MAKEINTRESOURCEA(2));
    HGLOBAL memory=resource ? LoadResource(instance, resource) : NULL;
    const BITMAPINFO *info=memory ? (const BITMAPINFO *)LockResource(memory) : NULL;
    const BYTE *pixels;
    int old_mode;
    if (!info || info->bmiHeader.biBitCount!=24 || info->bmiHeader.biCompression!=BI_RGB) return;
    pixels=(const BYTE *)info+info->bmiHeader.biSize;
    old_mode=SetStretchBltMode(dc, COLORONCOLOR);
    SetBrushOrgEx(dc, 0, 0, NULL);
    StretchDIBits(dc, x,y,width,height,0,0,info->bmiHeader.biWidth,info->bmiHeader.biHeight,
        pixels,info,DIB_RGB_COLORS,SRCCOPY);
    if (old_mode) SetStretchBltMode(dc,old_mode);
}

static void rounded(HDC dc, RECT area, COLORREF color, COLORREF edge)
{
    HBRUSH brush = CreateSolidBrush(color);
    HPEN pen = CreatePen(PS_SOLID, 1, edge);
    HGDIOBJ old_brush = SelectObject(dc, brush), old_pen = SelectObject(dc, pen);
    RoundRect(dc, area.left, area.top, area.right, area.bottom, 5, 5);
    SelectObject(dc, old_brush); SelectObject(dc, old_pen);
    DeleteObject(brush); DeleteObject(pen);
}

static int font_pixels(int points)
{
    HDC dc = GetDC(NULL);
    int dpi = dc ? GetDeviceCaps(dc, LOGPIXELSY) : 96;
    if (dc) ReleaseDC(NULL, dc);
    {
        int pixels=MulDiv(points, dpi>0 ? dpi : 96, 72);
        return pixels>0 ? pixels : 1;
    }
}
static void create_ui_fonts(void)
{
    welcome_font = CreateFontA(-font_pixels(ui_russian() ? 32 : 36), 0, 0, 0, FW_BOLD, TRUE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, "Arial");
    title_font = CreateFontA(-font_pixels(14), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, "Tahoma");
    body_font = CreateFontA(-20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, "Arial");
    power_caption_font = CreateFontA(-18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, "Arial");
    small_font = CreateFontA(-font_pixels(9), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, "Tahoma");
    status_font = CreateFontA(-font_pixels(8), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, "Tahoma");
    settings_font = CreateFontA(-11,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,"Tahoma");
    settings_title_font = CreateFontA(-13,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,"Tahoma");
    message_font = CreateFontA(-font_pixels(14),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,"Arial");
}
static void release_ui_font_handles(void)
{
    DeleteObject(status_font); DeleteObject(welcome_font); DeleteObject(title_font);
    DeleteObject(body_font); DeleteObject(small_font); DeleteObject(power_caption_font);
    DeleteObject(settings_font); DeleteObject(settings_title_font); DeleteObject(message_font);
    status_font=welcome_font=title_font=body_font=small_font=power_caption_font=NULL;
    settings_font=settings_title_font=message_font=NULL;
}
static BOOL russian_ui(void)
{
    return ui_russian();
}
static const char *welcome_text(void)
{
    return russian_ui() ? "\317\360\350\342\345\362\361\362\342\350\345" : "welcome";
}
static void paint_greeting(HDC dc, int left, int right)
{
    RECT text;
    UINT align = right == 370 ? DT_RIGHT : DT_CENTER;
    if(compact_layout() && status_view==VIEW_WELCOME_MULTI) {
        SetRect(&text,compact_x(82),compact_y(93),compact_x(487),compact_y(158)); align=DT_RIGHT;
    } else SetRect(&text, cx(left+2), cy(267+3), cx(right+2), cy(330+3));
    SetTextColor(dc, RGB(49,81,181));
    label(dc, welcome_font, welcome_text(), text, align | DT_VCENTER | DT_SINGLELINE);
    OffsetRect(&text, -2, -3);
    SetTextColor(dc, RGB(239,247,255));
    label(dc, welcome_font, welcome_text(), text, align | DT_VCENTER | DT_SINGLELINE);
}
static void paint_logo_with_font(HDC dc, int x, int y, HFONT font)
{
    RECT suffix;
    bitmap(dc, 123, x, y, 137, 86);
    SetRect(&suffix, x+115, y+54, x+141, y+86);
    fill(dc, suffix, RGB(90,126,220));
    SetTextColor(dc, RGB(255,156,33));
    label(dc, font, "95", suffix, DT_LEFT | DT_SINGLELINE);
    SetTextColor(dc, RGB(255,255,255));
}
static void paint_logo_at(HDC dc,int x,int y) { paint_logo_with_font(dc,x,y,body_font); }
static void paint_background_artwork(HDC dc,const RECT *area,int view)
{
    if(!compact_layout() || view==VIEW_STARTING || view==VIEW_SHUTDOWN)
        bitmap(dc,100,0,78,219,207);
    bitmap(dc,125,0,78,area->right,2);
    if(view==VIEW_STARTING || view==VIEW_SHUTDOWN)
        bitmap(dc,126,0,area->bottom-96,area->right,2);
    else if(view!=VIEW_WELCOME_SINGLE && !compact_layout())
        bitmap(dc,126,0,area->bottom-96,area->right,3);
}
static void paint_status_panel(HDC dc,const RECT *area,HFONT logo_font,HFONT text_font,const char *text)
{
    RECT rect;
    int x=(area->right-800)/2,y=(area->bottom-600)/2;
    paint_logo_with_font(dc,x+435,y+228,logo_font);
    SetRect(&rect,x+208,y+334,x+552,y+385);
    label(dc,text_font,text,rect,DT_RIGHT|DT_SINGLELINE);
}
static void paint_wait_details(HDC dc)
{
    HFONT logo_font=CreateFontA(-20,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,"Arial");
    HFONT text_font=CreateFontA(-font_pixels(14),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,"Arial");
    RECT area;
    GetClientRect(controller,&area);
    paint_background_artwork(dc,&area,VIEW_STARTING);
    SetBkMode(dc,TRANSPARENT); SetTextColor(dc,RGB(255,255,255));
    paint_status_panel(dc,&area,logo_font,text_font,ui_text("\317\356\344\356\346\344\350\362\345...","Please wait..."));
    DeleteObject(logo_font); DeleteObject(text_font);
}
static void paint_status_message(HDC dc, const char *text)
{
    RECT area;
    /* XP UIFILE: centered 384px panel, product right padding 20px,
       help right padding 40px. VGA bitmap (355,168), text right edge 472. */
    SetRect(&area,0,0,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN));
    paint_status_panel(dc,&area,body_font,message_font,text);
}
static const char *starting_text(void)
{
    return ui_text("\307\340\357\363\361\352 Windows...","Windows is starting up...");
}
static void paint_starting_frame(HDC dc,const RECT *area,HFONT logo_font,HFONT text_font)
{
    early_cover_background(dc,area);
    paint_background_artwork(dc,area,VIEW_STARTING);
    SetBkMode(dc,TRANSPARENT); SetTextColor(dc,RGB(255,255,255));
    paint_status_panel(dc,area,logo_font,text_font,starting_text());
}
static void release_first_frame(void)
{
    if(first_frame_old) SelectObject(first_frame_dc,first_frame_old);
    if(first_frame_bitmap) DeleteObject(first_frame_bitmap);
    if(first_frame_dc) DeleteDC(first_frame_dc);
    first_frame_old=NULL; first_frame_bitmap=NULL; first_frame_dc=NULL;
    first_frame_width=first_frame_height=0;
}
static BOOL prepare_first_frame(void)
{
    HDC device;
    RECT area;
    HFONT logo_font=body_font,text_font=message_font,temporary_logo=NULL,temporary_text=NULL;
    release_first_frame();
    first_frame_width=GetSystemMetrics(SM_CXSCREEN); first_frame_height=GetSystemMetrics(SM_CYSCREEN);
    device=GetDC(NULL);
    if(device) {
        first_frame_dc=CreateCompatibleDC(device);
        if(first_frame_dc) first_frame_bitmap=CreateCompatibleBitmap(device,first_frame_width,first_frame_height);
        ReleaseDC(NULL,device);
    }
    if(first_frame_bitmap) {
        first_frame_old=SelectObject(first_frame_dc,first_frame_bitmap);
        if(first_frame_old==HGDI_ERROR) first_frame_old=NULL;
    }
    if(!first_frame_old) { release_first_frame(); return FALSE; }
    /* Only two temporary status fonts are needed before showing the first
       image. Full UI font creation and profile/TTF work follow presentation. */
    if(!logo_font) logo_font=temporary_logo=CreateFontA(-20,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,"Arial");
    if(!text_font) text_font=temporary_text=CreateFontA(-font_pixels(14),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,"Arial");
    SetRect(&area,0,0,first_frame_width,first_frame_height);
    if(logo_font && text_font) paint_starting_frame(first_frame_dc,&area,logo_font,text_font);
    if(temporary_logo) DeleteObject(temporary_logo);
    if(temporary_text) DeleteObject(temporary_text);
    if(!logo_font || !text_font) { release_first_frame(); return FALSE; }
    return TRUE;
}
static BOOL first_frame_matches(HWND window)
{
    RECT area;
    return first_frame_dc && preparing_canvas && status_view==VIEW_STARTING &&
        GetClientRect(window,&area) && area.right==first_frame_width && area.bottom==first_frame_height;
}
static BOOL copy_first_frame(HWND window,HDC target)
{
    return target && first_frame_matches(window) &&
        BitBlt(target,0,0,first_frame_width,first_frame_height,first_frame_dc,0,0,SRCCOPY);
}
static BOOL erase_first_frame(HWND window,HDC target)
{
    BOOL copied;
    if(!first_frame_matches(window)) return TRUE;
    copied=copy_first_frame(window,target);
    if(copied && cached_paint_window==window) cached_paint_erased=TRUE;
    else if(copied) startup_trace(startup_config.root,startup_config.backup_key,
        window==controller ? "cover-erase-painted" : "canvas-erase-painted",0);
    return copied;
}
static HDC begin_cached_paint(HWND window,PAINTSTRUCT *paint,BOOL *erased)
{
    HDC target;
    HWND previous_window=cached_paint_window;
    BOOL previous_erased=cached_paint_erased;
    cached_paint_window=window; cached_paint_erased=FALSE;
    target=BeginPaint(window,paint);
    *erased=cached_paint_erased;
    cached_paint_window=previous_window; cached_paint_erased=previous_erased;
    return target;
}
static void login_hotkeys(HWND hwnd,BOOL enable);
static void set_status_view(HWND hwnd, int view)
{
    static const int controls[] = { ID_SUBMIT, ID_POWER, ID_PREV, ID_NEXT, ID_PROFILE_SCROLL, ID_DONE, 201 };
    unsigned int index;
    status_view = view;
    if(view==VIEW_SHUTDOWN) hide_backdrop();
    login_hotkeys(hwnd,sign_in_view());
    /* Cover the native field during shutdown without changing its visibility. */
    if(view==VIEW_SHUTDOWN && native_logon_attached())
        SetWindowPos(login_window,HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
    SetWindowPos(hwnd,view==VIEW_SHUTDOWN ? HWND_NOTOPMOST : HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
    if(view!=VIEW_SHUTDOWN && backdrop && !rescue_mode) show_backdrop();
    if (view != VIEW_LOGIN) {
        for (index = 0; index < sizeof(controls)/sizeof(controls[0]); ++index)
            ShowWindow(GetDlgItem(hwnd, controls[index]), SW_HIDE);
    } else {
        position_arrow(hwnd);
        ShowWindow(GetDlgItem(hwnd, ID_POWER), SW_SHOWNA);
        position_profile_navigation(hwnd);
    }
    InvalidateRect(hwnd, NULL, FALSE); UpdateWindow(hwnd);
}

static void login_hotkeys(HWND hwnd,BOOL enable)
{
    if(!enable) {
        if(escape_hotkey) UnregisterHotKey(hwnd,1);
        if(close_hotkey) UnregisterHotKey(hwnd,2);
        escape_hotkey=close_hotkey=FALSE;
    } else {
        if(!escape_hotkey) escape_hotkey=RegisterHotKey(hwnd,1,0,VK_ESCAPE);
        if(!close_hotkey) close_hotkey=RegisterHotKey(hwnd,2,MOD_ALT,VK_F4);
    }
}
static void perform_power(PowerAction action,BOOL dos)
{
    DWORD error;
    char text[160];
    HWND owner=screen;
    login_hotkeys(owner,FALSE);
    power_requested=action!=POWER_STANDBY; ending_unknown=FALSE;
    if(action!=POWER_STANDBY) set_status_view(owner,VIEW_SHUTDOWN);
    error=power_request(action,dos,owner,menu_power);
    if(error || action==POWER_STANDBY) {
        power_requested=FALSE;
        if(IsWindow(owner)) { set_status_view(owner,VIEW_LOGIN); login_hotkeys(owner,TRUE); }
        if(error && error!=ERROR_CANCELLED) {
            wsprintfA(text,ui_text("Windows \355\345 \363\344\340\353\356\361\374 \342\373\357\356\353\355\350\362\374 \342\373\341\360\340\355\355\356\345 \344\345\351\361\362\342\350\345.\r\n\316\370\350\341\352\340: %lu","Windows could not perform the selected power action.\r\nError: %lu"),error);
            error_box(owner,text);
        }
        if(native_logon_attached()) native_logon_focus();
    }
}
static void show_power_dialog(HWND owner)
{
    PowerDialogHost host;
    if(power_dialog_active()) { power_dialog_focus(); return; }
    menu_power=power_operations();
    host.instance=instance; host.owner=owner; host.native_input=login_window;
    host.russian=russian_ui(); host.preview=preview;
    host.standby_available=preview || menu_power->standby_available;
    host.dos_available=preview || menu_power->dos_available;
    host.perform=perform_power; host.restore_focus=native_logon_focus;
    if(!power_dialog_show(&host)) error_box(owner,ui_text("\315\345 \363\344\340\353\356\361\374 \356\362\352\360\373\362\374 \356\352\355\356 \342\373\352\353\376\367\345\355\350\377 \352\356\354\357\374\376\362\345\360\340.","Cannot open the shutdown dialog."));
}

static void paint_welcome(HWND hwnd)
{
    PAINTSTRUCT paint;
    RECT area, rectangle;
    HDC dc, memory;
    HBITMAP frame;
    HGDIOBJ old=NULL;
    BOOL erased;
    int index, top;
    dc=begin_cached_paint(hwnd,&paint,&erased);
    GetClientRect(hwnd,&area);
    if(first_frame_matches(hwnd) && (erased || copy_first_frame(hwnd,dc))) {
        EndPaint(hwnd,&paint); return;
    }
    /* Allocation or display changes may invalidate the prepared image. */
    if(preparing_canvas && status_view==VIEW_STARTING && !body_font) create_ui_fonts();
    memory=CreateCompatibleDC(dc);
    frame=memory ? CreateCompatibleBitmap(dc,area.right,area.bottom) : NULL;
    if(frame) { old=SelectObject(memory,frame); dc=memory; }
    else if(memory) DeleteDC(memory);
    if(status_view==VIEW_STARTING) {
        paint_starting_frame(dc,&area,body_font,message_font);
        finish_paint(hwnd,&paint,dc,frame,old); return;
    }
    early_cover_background(dc,&area);
    paint_background_artwork(dc,&area,status_view);
    SetBkMode(dc, TRANSPARENT); SetTextColor(dc, RGB(255,255,255));
    if (status_view == VIEW_WELCOME_SINGLE) {
        paint_greeting(dc, 100, 800); finish_paint(hwnd,&paint,dc,frame,old); return;
    }
    if (status_view == VIEW_WELCOME_MULTI) {
        bitmap(dc,124,compact_layout() ? compact_x(128) : cx(398),compact_layout() ? compact_y(208) : 96,1,compact_layout() ? 170 : by(486)-96);
        paint_greeting(dc, 40, 370);
        SetRect(&rectangle, card_x(433), loading_y(262), card_x(485), loading_y(314));
        rounded(dc, rectangle, RGB(239,247,255), RGB(239,247,255));
        bitmap(dc, 114, card_x(435), loading_y(264), 48, 48);
        SetRect(&rectangle, card_x(499), loading_y(262), card_x(775), loading_y(285));
        SetTextColor(dc, RGB(255,255,255));
        label(dc, title_font, profiles_name(selected_profile), rectangle, DT_LEFT | DT_SINGLELINE);
        SetRect(&rectangle, card_x(499), loading_y(285), card_x(775), loading_y(320));
        SetTextColor(dc, RGB(0,48,156));
        label(dc, status_font, russian_ui() ? "\307\340\343\360\363\347\352\340\040\353\350\367\355\373\365\040\357\340\360\340\354\345\362\360\356\342\056\056\056" : "Loading your personal settings...", rectangle, DT_LEFT | DT_WORDBREAK);
        finish_paint(hwnd,&paint,dc,frame,old); return;
    }
    if (status_view == VIEW_SHUTDOWN) {
        paint_status_message(dc,ending_unknown ? (russian_ui() ? "\317\356\344\356\346\344\350\362\345..." : "Please wait...") : russian_ui() ? "\307\340\342\345\360\370\345\355\350\345\040\360\340\341\356\362\373\040\127\151\156\144\157\167\163\056\056\056" : "Windows is shutting down...");
        finish_paint(hwnd,&paint,dc,frame,old); return;
    }
    bitmap(dc,124,compact_layout() ? compact_x(128) : cx(398),compact_layout() ? compact_y(208) : 96,1,compact_layout() ? 170 : by(486)-96);
    paint_logo_at(dc,compact_layout() ? compact_x(356) : cx(243),compact_layout() ? compact_y(84) : cy(232));
    if(compact_layout()) SetRect(&rectangle,compact_x(150),compact_y(188),compact_x(467),compact_y(214));
    else SetRect(&rectangle,cx(40),cy(338),cx(355),cy(389));
    label(dc, body_font, finishing ? ui_text("\302\365\356\344 \342 \361\350\361\362\345\354\363...","Starting your session...") : (account_open() ? ui_text("\304\353\377 \342\365\356\344\340 \342\342\345\344\350\362\345 \357\340\360\356\353\374","To begin, enter your password") : ui_text("\304\353\377 \342\365\356\344\340 \342\373\341\345\360\350\362\345 \361\342\356\270 \350\354\377","To begin, click your user name")), rectangle, DT_RIGHT | DT_WORDBREAK);
    for (index = page_first(); index < page_first() + visible_profiles(); ++index) {
        top = profile_top(index);
        if (index == selected_profile && account_open()) bitmap(dc, 112, card_x(425), row_y(top), 350, 72);
        SetRect(&rectangle, card_x(433), row_y(top+12), card_x(485), row_y(top+64));
        rounded(dc, rectangle, index == selected_profile && account_open() ? RGB(255,235,94) : RGB(239,247,255), RGB(239,247,255));
        bitmap(dc, 114, card_x(435), row_y(top+14), 48, 48);
        SetRect(&rectangle, card_x(499), row_y(top+9), card_x(765), row_y(top+36));
        label(dc, index == selected_profile && account_open() ? title_font : body_font,
            profiles_name(index), rectangle, DT_LEFT | DT_SINGLELINE);
        if (index == selected_profile && account_open()) {
            SetRect(&rectangle, card_x(499), row_y(top+35), card_x(765), row_y(top+50));
            label(dc, small_font, preview ? ui_text("\317\360\356\361\354\356\362\360","Preview") : (pending_profile ? ui_text("\321\354\345\355\340 \357\356\353\374\347\356\342\340\362\345\353\377...","Switching user...") : (native_logon_attached() ? ui_text("\302\342\345\344\350\362\345 \357\340\360\356\353\374.","Type your password.") : ui_text("\317\356\344\343\356\362\356\342\352\340 \342\365\356\344\340...","Preparing sign-in..."))), rectangle, DT_LEFT | DT_SINGLELINE);
            SetRect(&rectangle, card_x(499), row_y(top+50), card_x(670), row_y(top+76));
            rounded(dc, rectangle, RGB(255,255,255), RGB(49,81,181));
        }
    }
    SetRect(&rectangle, rx(470), by(527), rx(775), by(570));
    label(dc, small_font, ui_text("\317\356\361\353\345 \342\365\356\344\340 \354\356\346\355\356 \350\347\354\345\355\350\362\374 \355\340\361\362\360\356\351\352\350.\n\316\362\352\360\356\351\362\345 Welcome95 \342 \317\340\355\345\353\350 \363\357\360\340\342\353\345\355\350\377.","After you log on, you can change your settings.\nOpen Welcome95 in Control Panel."), rectangle, DT_LEFT | DT_WORDBREAK);
    finish_paint(hwnd,&paint,dc,frame,old);
}

static void draw_button(const DRAWITEMSTRUCT *item)
{
    RECT rectangle = item->rcItem;
    RECT stripe;
    int state=SaveDC(item->hDC);
    int pixel, band;
    HDC dc = item->hDC;
    SetBkMode(dc,TRANSPARENT); SetTextColor(dc,RGB(255,255,255));
    if (item->CtlID == ID_SUBMIT) {
        bitmap(dc, 103, rectangle.left, rectangle.top, rectangle.right-rectangle.left, rectangle.bottom-rectangle.top);
    } else if (item->CtlID == ID_POWER) {
        for (pixel = rectangle.left; pixel < rectangle.right; ++pixel) {
            band = (25 + pixel-rectangle.left) * 128 / GetSystemMetrics(SM_CXSCREEN);
            SetRect(&stripe, pixel, rectangle.top, pixel + 1, rectangle.bottom);
            fill(dc, stripe, RGB(57-band*57/127,52-band*4/127,173-band*17/127));
        }
        bitmap(dc, 107, rectangle.left, rectangle.top, 26, 26);
        rectangle.left += 34;
        label(dc, power_caption_font, ui_text("\302\373\352\353\376\367\350\362\374 \352\356\354\357\374\376\362\345\360","Turn off computer"), rectangle, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
    if(state) RestoreDC(dc,state);
}

/* Identify an existing native password dialog without reading any edit text.
   No synthetic keystrokes, password collection, or authentication replacement. */
static BOOL CALLBACK password_child(HWND child, LPARAM parameter)
{
    char class_name[32];
    GetClassNameA(child, class_name, sizeof(class_name));
    if (!lstrcmpiA(class_name, "Edit") && (GetWindowLongA(child, GWL_STYLE) & ES_PASSWORD))
        *(BOOL *)parameter = TRUE;
    return TRUE;
}

static BOOL CALLBACK find_logon(HWND hwnd, LPARAM parameter)
{
    BOOL password = FALSE;
    char class_name[32];
    DWORD process;
    if (!IsWindowVisible(hwnd) || hwnd == screen) return TRUE;
    GetWindowThreadProcessId(hwnd, &process);
    if (process == GetCurrentProcessId()) return TRUE;
    GetClassNameA(hwnd, class_name, sizeof(class_name));
    if (lstrcmpA(class_name, "#32770")) return TRUE;
    EnumChildWindows(hwnd, password_child, (LPARAM)&password);
    if (!password) return TRUE;
    *(HWND *)parameter = hwnd;
    return FALSE;
}

static void release_logon(void)
{
    native_logon_restore(FALSE);
    if (login_window && IsWindow(login_window) && !login_was_topmost)
        SetWindowPos(login_window, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    login_window = NULL;
    if (screen) EnableWindow(GetDlgItem(screen, ID_SUBMIT), preview);
}

/* One blank-password attempt, authenticated by Windows, never a retry loop. */
static void manual_single_login(void)
{
    automatic_attempted=TRUE; single_submitted=FALSE; pending_profile=FALSE;
    release_logon(); next_logon_attempt=0;
    if(screen) set_status_view(screen,VIEW_LOGIN);
}
static void try_single_account_logon(void)
{
    BOOL posted;
    if (require_account_choice || automatic_attempted || preview || finishing || prefer_native || pending_profile ||
        !sign_in_view() || profiles_count()!=1 || !native_logon_attached()) return;
    automatic_attempted=TRUE;
    /* A provider that requires input before enabling OK stays manual. Waiting
       until it enables OK could erase a password the user has begun typing. */
    posted=native_logon_ready() && native_logon_try_empty(profiles_name(0));
    if(status_view==VIEW_STARTING) {
        if(posted) { single_submitted=TRUE; single_started=GetTickCount(); }
        else manual_single_login();
    }
}

static void allow_logon(void)
{
    HWND found = NULL;
    RECT area;
    int x, y, width;
    BOOL quiet=status_view==VIEW_STARTING;
    if (preview || finishing || !sign_in_view() || !account_open()) return;
    if (login_window && IsWindow(login_window) && IsWindowVisible(login_window)) {
        if (native_logon_attached()) {
            if(quiet) return;
            login_geometry(&x,&y,&width);
            if(native_logon_layout(x,y,width)) return;
            release_logon(); next_logon_attempt=0;
        }
        if (prefer_native) return;
        /* Retry while the provider is still initializing its controls. */
    }
    if(quiet && single_submitted) return;
    if ((LONG)(GetTickCount() - next_logon_attempt) < 0) return;
    next_logon_attempt = GetTickCount() + 1000;
    release_logon();
    EnumWindows(find_logon, (LPARAM)&found);
    if (!found) return;
    login_window = found;
    GetWindowThreadProcessId(found,&provider_process);
    login_was_topmost = (GetWindowLongA(found, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
    login_geometry(&x, &y, &width);
    if (!prefer_native && (quiet ? native_logon_probe(found) : native_logon_attach(found, x, y, width))) {
        compact_selected=TRUE;
        if (!native_logon_user_empty()) profiles_accept_native(native_logon_name());
        if (native_logon_user_empty() && !profiles_count()) {
            native_logon_restore(TRUE);
            return;
        }
        if (!pending_profile) {
            if (native_logon_user_empty() && profiles_count()) {
                selected_profile = 0; pending_profile = TRUE;
            } else {
                selected_profile = profiles_accept_native(native_logon_name());
                if (selected_profile < 0) selected_profile = 0;
            }
            profile_page = selected_profile / page_size();
        }
        if(quiet) {
            if(profiles_count()!=1) { manual_single_login(); return; }
            SetWindowPos(found,screen,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOOWNERZORDER);
            SetForegroundWindow(screen); return;
        }
        position_profile_navigation(screen);
        login_geometry(&x, &y, &width); native_logon_layout(x, y, width);
        position_arrow(screen);
        EnableWindow(GetDlgItem(screen, ID_SUBMIT), !pending_profile);
        InvalidateRect(screen, NULL, FALSE);
        return;
    }
    if(quiet) { manual_single_login(); return; }
    /* An unknown provider must keep its required controls accessible. */
    GetWindowRect(found, &area);
    x = GetSystemMetrics(SM_CXSCREEN) * 3 / 4 - (area.right - area.left) / 2;
    y = GetSystemMetrics(SM_CYSCREEN) / 2 - (area.bottom - area.top) / 2;
    if (x + area.right - area.left > GetSystemMetrics(SM_CXSCREEN))
        x = GetSystemMetrics(SM_CXSCREEN) - (area.right - area.left);
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    SetWindowPos(found, prefer_native ? HWND_TOPMOST : HWND_NOTOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOACTIVATE);
    if (prefer_native) SetForegroundWindow(found);
    InvalidateRect(screen, NULL, FALSE);
}

static BOOL CALLBACK find_provider_notice(HWND window,LPARAM parameter)
{
    DWORD process=0; char name[24]; BOOL password=FALSE;
    if(window==login_window || !IsWindowVisible(window) || !IsWindowEnabled(window)) return TRUE;
    GetWindowThreadProcessId(window,&process);
    if(!provider_process || process!=provider_process) return TRUE;
    GetClassNameA(window,name,sizeof(name)); if(lstrcmpA(name,"#32770")) return TRUE;
    EnumChildWindows(window,password_child,(LPARAM)&password);
    if(password) return TRUE;
    *(HWND *)parameter=window; return FALSE;
}
static HWND provider_notice(HWND input)
{
    HWND popup=input ? GetLastActivePopup(input) : NULL;
    if(popup && popup!=input && IsWindowVisible(popup)) return popup;
    popup=NULL; if(provider_process) EnumWindows(find_provider_notice,(LPARAM)&popup);
    return popup;
}
static void service_single_pending(void)
{
    HWND found=NULL;
    if(preview || status_view!=VIEW_STARTING) return;
    if(single_submitted && login_window && (!IsWindow(login_window) || !IsWindowVisible(login_window)))
        EnumWindows(find_logon,(LPARAM)&found);
    if(profiles_count()!=1 || provider_notice(login_window) || found ||
        (DWORD)(GetTickCount()-single_started)>=4000) manual_single_login();
}
static BOOL window_above(HWND first,HWND second)
{
    HWND window=GetTopWindow(NULL);
    while(window) {
        if(window==first) return TRUE;
        if(window==second) return FALSE;
        window=GetWindow(window,GW_HWNDNEXT);
    }
    return FALSE;
}
/* The persistent lower layer has no controls or status text. */
static void paint_backdrop_details(HDC dc)
{
    RECT area;
    HFONT font=CreateFontA(-20,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,"Arial");
    GetClientRect(backdrop,&area);
    paint_background_artwork(dc,&area,VIEW_STARTING);
    SetBkMode(dc,TRANSPARENT);
    paint_logo_with_font(dc,(area.right-800)/2+435,(area.bottom-600)/2+228,font);
    if(font) DeleteObject(font);
}
static HWND backdrop_upper(void)
{
    if(screen && IsWindowVisible(screen)) return screen;
    if(controller_cover && controller && IsWindowVisible(controller)) return controller;
    return NULL;
}
static LRESULT CALLBACK backdrop_proc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp)
{
    switch(message) {
    case WM_PAINT:
        {
            PAINTSTRUCT paint; BOOL erased;
            HDC dc=begin_cached_paint(hwnd,&paint,&erased);
            if(!erased) early_cover_render(hwnd,dc,paint_backdrop_details);
            EndPaint(hwnd,&paint); return 0;
        }
    case WM_ERASEBKGND:
        if(wp) {
            early_cover_render(hwnd,(HDC)wp,paint_backdrop_details);
            if(cached_paint_window==hwnd) cached_paint_erased=TRUE;
            return TRUE;
        }
        return FALSE;
    case WM_MOUSEACTIVATE: return MA_NOACTIVATEANDEAT;
    case WM_SETCURSOR: SetCursor(LoadCursor(NULL,IDC_ARROW)); return TRUE;
    case WM_CLOSE: if(backdrop_active) return 0; break;
    case WM_WINDOWPOSCHANGING:
        if(backdrop_active && !positioning_backdrop) {
            WINDOWPOS *position=(WINDOWPOS *)lp;
            HWND upper=backdrop_upper();
            if(position->flags&SWP_HIDEWINDOW) position->flags=(position->flags&~SWP_HIDEWINDOW)|SWP_SHOWWINDOW;
            if(!(position->flags&SWP_NOZORDER)) position->hwndInsertAfter=upper ? upper : HWND_TOPMOST;
        }
        break;
    case WM_DISPLAYCHANGE:
        if(backdrop_active) {
            maintain_backdrop();
            RedrawWindow(hwnd,NULL,NULL,RDW_INVALIDATE|RDW_UPDATENOW);
        }
        return 0;
    case WM_QUERYENDSESSION: return TRUE;
    case WM_ENDSESSION: return 0; /* The registered controller owns session policy. */
    case WM_DESTROY: backdrop=NULL; backdrop_active=FALSE; return 0;
    }
    return DefWindowProcA(hwnd,message,wp,lp);
}
static void maintain_backdrop(void)
{
    HWND upper,below; RECT bounds;
    int width=GetSystemMetrics(SM_CXSCREEN),height=GetSystemMetrics(SM_CYSCREEN);
    BOOL correct;
    if(!backdrop_active || positioning_backdrop || !backdrop || !IsWindow(backdrop) || rescue_mode) return;
    upper=backdrop_upper();
    GetWindowRect(backdrop,&bounds);
    correct=IsWindowVisible(backdrop) && (GetWindowLongA(backdrop,GWL_EXSTYLE)&WS_EX_TOPMOST) &&
        bounds.left==0 && bounds.top==0 && bounds.right==width && bounds.bottom==height;
    if(upper) {
        below=GetWindow(upper,GW_HWNDNEXT);
        while(below && !IsWindowVisible(below)) below=GetWindow(below,GW_HWNDNEXT);
        if(below!=backdrop) correct=FALSE;
    }
    if(correct) return;
    positioning_backdrop=TRUE;
    SetWindowPos(backdrop,upper ? upper : HWND_TOPMOST,0,0,width,height,SWP_NOACTIVATE|SWP_SHOWWINDOW);
    positioning_backdrop=FALSE;
}
static void set_canvas_owner(HWND owner)
{
    if(!screen || !IsWindow(screen) || GetWindow(screen,GW_OWNER)==owner) return;
#ifdef _WIN64
    SetWindowLongPtrA(screen,GWLP_HWNDPARENT,(LONG_PTR)owner);
#else
    SetWindowLongA(screen,GWL_HWNDPARENT,(LONG)owner);
#endif
}
static void show_backdrop(void)
{
    BOOL needs_paint;
    if(rescue_mode || status_view==VIEW_SHUTDOWN) return;
    if(!backdrop || !IsWindow(backdrop)) {
        WNDCLASSA cls;
        zero_memory(&cls,sizeof(cls)); cls.hInstance=instance; cls.lpfnWndProc=backdrop_proc;
        cls.hCursor=LoadCursor(NULL,IDC_ARROW); cls.lpszClassName="Welcome95.Backdrop";
        if(!RegisterClassA(&cls) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) return;
        backdrop=CreateWindowExA(WS_EX_TOPMOST|WS_EX_TOOLWINDOW,cls.lpszClassName,APP,WS_POPUP,
            0,0,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN),NULL,NULL,instance,NULL);
        if(!backdrop) { startup_trace(startup_config.root,startup_config.backup_key,"backdrop-create-error",GetLastError()); return; }
    }
    set_canvas_owner(backdrop);
    needs_paint=!backdrop_active || !IsWindowVisible(backdrop);
    backdrop_active=TRUE;
    maintain_backdrop();
    if(needs_paint) {
        RedrawWindow(backdrop,NULL,NULL,RDW_INVALIDATE|RDW_UPDATENOW);
        maintain_backdrop();
        startup_trace(startup_config.root,startup_config.backup_key,"backdrop-presented",0);
    }
}
static void hide_backdrop(void)
{
    if(backdrop && screen && GetWindow(screen,GW_OWNER)==backdrop) set_canvas_owner(NULL);
    backdrop_active=FALSE;
    if(backdrop && IsWindow(backdrop) && IsWindowVisible(backdrop)) {
        ShowWindow(backdrop,SW_HIDE);
        startup_trace(startup_config.root,startup_config.backup_key,"backdrop-hidden",0);
    }
}
static void destroy_backdrop(void)
{
    hide_backdrop();
    if(backdrop && IsWindow(backdrop)) DestroyWindow(backdrop);
    backdrop=NULL;
}
static void maintain_topmost(HWND hwnd)
{
    HWND input=NULL,popup=NULL,window;
    HDWP batch;
    BOOL correct;
    UINT flags=SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOOWNERZORDER;
    if(rescue_mode || power_dialog_active() || status_view==VIEW_SHUTDOWN) return;
    maintain_backdrop();
    if(status_view==VIEW_LOGIN && login_window && IsWindowVisible(login_window) &&
       (native_logon_attached() || prefer_native)) {
        input=login_window;
    }
    if(sign_in_view()) popup=provider_notice(input);
    correct=(GetWindowLongA(hwnd,GWL_EXSTYLE)&WS_EX_TOPMOST)!=0;
    if(input && (!(GetWindowLongA(input,GWL_EXSTYLE)&WS_EX_TOPMOST) || !window_above(input,hwnd))) correct=FALSE;
    if(popup && (!(GetWindowLongA(popup,GWL_EXSTYLE)&WS_EX_TOPMOST) || !window_above(popup,input))) correct=FALSE;
    for(window=GetTopWindow(NULL);window && window!=hwnd;window=GetWindow(window,GW_HWNDNEXT))
        if(window!=input && window!=popup && IsWindowVisible(window)) correct=FALSE;
    if(correct) return;
    batch=BeginDeferWindowPos(3);
    if(batch) batch=DeferWindowPos(batch,hwnd,HWND_TOPMOST,0,0,0,0,flags);
    if(batch && input) batch=DeferWindowPos(batch,input,HWND_TOPMOST,0,0,0,0,flags);
    if(batch && popup) batch=DeferWindowPos(batch,popup,HWND_TOPMOST,0,0,0,0,flags);
    if(batch && EndDeferWindowPos(batch)) { maintain_backdrop(); return; }
    /* Allocation failure: place the canvas below input instead of raising it
       over the password rectangle and then uncovering it on every timer tick. */
    if(input) SetWindowPos(input,HWND_TOPMOST,0,0,0,0,flags);
    SetWindowPos(hwnd,input ? input : HWND_TOPMOST,0,0,0,0,flags);
    if(popup) SetWindowPos(popup,HWND_TOPMOST,0,0,0,0,flags);
    maintain_backdrop();
}

static BOOL CALLBACK find_shell_window(HWND window,LPARAM parameter)
{
    DWORD process=0;
    if(window==screen || window==controller || window==backdrop || !IsWindowVisible(window)) return TRUE;
    GetWindowThreadProcessId(window,&process);
    if(process!=shell_process_id) return TRUE;
    *(BOOL *)parameter=TRUE; return FALSE;
}
static BOOL shell_ready_to_reveal(void)
{
    BOOL visible=FALSE;
    if(!shell_released) {
        shell_released=TRUE; shell_released_at=GetTickCount();
        if(shell_event) SetEvent(shell_event);
        startup_trace(startup_config.root,startup_config.backup_key,"shell-permitted",0);
        return FALSE;
    }
    if(!shell_confirmed) {
        if((DWORD)(GetTickCount()-shell_released_at)<5000) return FALSE;
        startup_trace(startup_config.root,startup_config.backup_key,"shell-not-confirmed",ERROR_TIMEOUT);
        return TRUE;
    }
    EnumWindows(find_shell_window,(LPARAM)&visible);
    return visible || !shell_process || WaitForSingleObject(shell_process,0)==WAIT_OBJECT_0 ||
        (DWORD)(GetTickCount()-shell_started_at)>=2000;
}
static void begin_logon_cycle(BOOL reset);
static void rescue_native_logon(void);
static void release_ui_fonts(void);
static void close_canvas(void)
{
    /* Keep the old upper frame visible until its lower owner is hidden. */
    hide_backdrop();
    if(screen && IsWindow(screen)) DestroyWindow(screen);
    if(closed_event) SetEvent(closed_event); /* after DestroyWindow has returned */
    startup_trace(startup_config.root,startup_config.backup_key,"canvas-closed",0);
}
static LRESULT CALLBACK welcome_proc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp)
{
    int x, y, width, index;
    char requested_name[128];
    switch (message) {
    case WM_CREATE:
        if (preview_status) preview_started = GetTickCount();
        SetTimer(hwnd, TICK, 100, NULL);
        if(sign_in_view()) login_hotkeys(hwnd,TRUE);
        CreateWindowA("BUTTON", ui_text("\302\356\351\362\350","Sign in"), WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            card_x(685), row_y(profile_top(selected_profile)+51), 26, 26, hwnd, (HMENU)ID_SUBMIT, instance, NULL);
        painted_button_install(GetDlgItem(hwnd,ID_SUBMIT),draw_button);
        EnableWindow(GetDlgItem(hwnd, ID_SUBMIT), preview);
        CreateWindowA("BUTTON", ui_text("\302\373\352\353\376\367\350\362\374 \352\356\354\357\374\376\362\345\360","Turn off computer"), WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            25, by(526), 245, 29, hwnd, (HMENU)ID_POWER, instance, NULL);
        painted_button_install(GetDlgItem(hwnd,ID_POWER),draw_button);
        CreateWindowA("BUTTON", "<", WS_CHILD | (profiles_count()>4 ? WS_VISIBLE : 0), cx(720), cy(455), 25, 24, hwnd, (HMENU)ID_PREV, instance, NULL);
        CreateWindowA("BUTTON", ">", WS_CHILD | (profiles_count()>4 ? WS_VISIBLE : 0), cx(750), cy(455), 25, 24, hwnd, (HMENU)ID_NEXT, instance, NULL);
        CreateWindowA("SCROLLBAR","",WS_CHILD|SBS_VERT,compact_x(495),compact_y(208),17,176,
            hwnd,(HMENU)ID_PROFILE_SCROLL,instance,NULL);
        position_profile_navigation(hwnd);
        position_arrow(hwnd);
        if (preview) {
            login_geometry(&x, &y, &width);
            CreateWindowA("EDIT", "", WS_CHILD | WS_DISABLED | ES_PASSWORD,
                x, y, width - 60, 22, hwnd, (HMENU)201, instance, NULL);
            /* A disabled preview edit never accepts a real password. */
            {
            HWND button = CreateWindowA("BUTTON", ui_text("\307\340\352\360\373\362\374 \357\360\356\361\354\356\362\360","Finish preview"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                rx(625), by(572), 150, 23, hwnd, (HMENU)ID_DONE, instance, NULL);
            SendMessageA(button, WM_SETFONT, (WPARAM)small_font, TRUE);
            }
        }
        if (status_view != VIEW_LOGIN) set_status_view(hwnd, status_view);
        return 0;
    case WM_PAINT: paint_welcome(hwnd); return 0;
    case WM_ENABLE:
        if(wp && backdrop_active) PostMessageA(hwnd,RESTORE_LAYERS,0,0);
        break;
    case RESTORE_LAYERS:
        if(!power_dialog_active()) maintain_topmost(hwnd);
        return 0;
    case WM_WINDOWPOSCHANGED:
        if(preparing_canvas && screen==hwnd && IsWindowVisible(hwnd) &&
           (((WINDOWPOS *)lp)->flags&SWP_SHOWWINDOW)) {
            LRESULT result=DefWindowProcA(hwnd,message,wp,lp);
            /* Complete the first frame inside ShowWindow itself. Waiting until
               it returns can expose an unpainted popup during Win9x logoff. */
            canvas_show_painted=RedrawWindow(hwnd,NULL,NULL,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_UPDATENOW);
            startup_trace(startup_config.root,startup_config.backup_key,"canvas-show-painted",canvas_show_painted ? 0 : GetLastError());
            return result;
        }
        break;
    case WM_DRAWITEM: draw_button((DRAWITEMSTRUCT *)lp); return TRUE;
    case WM_ERASEBKGND:
        /* Older USER/drivers can request the background before WM_PAINT. */
        return erase_first_frame(hwnd,(HDC)wp);
    case WM_CLOSE: return 0;
    case WM_HOTKEY:
        if(power_dialog_active() && (wp==1 || wp==2)) power_dialog_close();
        return 0;
    case WM_QUERYENDSESSION:
        power_dialog_close();
        if (!((DWORD)lp & ENDSESSION_LOGOFF_95)) { ending_unknown=!power_requested; set_status_view(hwnd,VIEW_SHUTDOWN); }
        return TRUE;
    case WM_ENDSESSION:
        if (wp) {
            if(resident_mode && (!power_requested || ((DWORD)lp&ENDSESSION_LOGOFF_95))) {
                SetEvent(((DWORD)lp&ENDSESSION_LOGOFF_95) ? logoff_event : ended_event); return 0;
            }
            DestroyWindow(hwnd); return 0;
        }
        if (!wp && status_view == VIEW_SHUTDOWN) {
            power_requested=FALSE; ending_unknown=FALSE;
            if (monitor_active && (!resident_mode || shutdown_only_canvas)) DestroyWindow(hwnd);
            else { set_status_view(hwnd, VIEW_LOGIN); login_hotkeys(hwnd,TRUE); }
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wp & 0xfff0) == SC_CLOSE || (wp & 0xfff0) == SC_MINIMIZE) return 0;
        break;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (wp == VK_ESCAPE || wp == VK_F4) return 0;
        if (wp == VK_RETURN) { if (status_view == VIEW_LOGIN && account_open() && !pending_profile) native_logon_submit(); return 0; }
        break;
    case WM_LBUTTONDOWN:
        if(power_dialog_active()) return 0;
        if (status_view != VIEW_LOGIN) return 0;
        if (account_open() && !pending_profile && native_logon_attached()) {
            login_geometry(&x, &y, &width);
            /* The painted border belongs to this canvas. Clicking it must focus
               Windows' real Edit too, without forwarding or reading any text. */
            if ((int)LOWORD(lp) >= x-2 && (int)LOWORD(lp) < x+width-58 &&
                (int)HIWORD(lp) >= y-2 && (int)HIWORD(lp) < y+24) {
                native_logon_focus(); return 0;
            }
        }
        x = LOWORD(lp); y = HIWORD(lp);
        for (index = page_first(); index < page_first() + visible_profiles(); ++index) {
            if (x >= card_x(425) && x < card_x(775) && y >= row_y(profile_top(index)) && y < row_y(profile_top(index)+64) &&
                (index != selected_profile || !account_open())) {
                compact_selected=TRUE;
                selected_profile = index; pending_profile = !preview; next_profile_attempt = 0;
                position_arrow(hwnd); position_profile_navigation(hwnd); EnableWindow(GetDlgItem(hwnd, ID_SUBMIT), preview);
                InvalidateRect(hwnd, NULL, FALSE); break;
            }
        }
        return 0;
    case WM_VSCROLL:
        if(status_view==VIEW_LOGIN && !power_dialog_active() && (HWND)lp==GetDlgItem(hwnd,ID_PROFILE_SCROLL)) {
            index=profile_page;
            if(LOWORD(wp)==SB_LINEUP || LOWORD(wp)==SB_PAGEUP) --index;
            else if(LOWORD(wp)==SB_LINEDOWN || LOWORD(wp)==SB_PAGEDOWN) ++index;
            else if(LOWORD(wp)==SB_THUMBPOSITION || LOWORD(wp)==SB_THUMBTRACK) index=HIWORD(wp);
            else if(LOWORD(wp)==SB_TOP) index=0;
            else if(LOWORD(wp)==SB_BOTTOM) index=(profiles_count()-1)/page_size();
            if(index>=0 && index*page_size()<profiles_count() && index!=profile_page) {
                profile_page=index; selected_profile=page_first(); compact_selected=TRUE; pending_profile=!preview; next_profile_attempt=0;
                position_arrow(hwnd); position_profile_navigation(hwnd);
                EnableWindow(GetDlgItem(hwnd,ID_SUBMIT),preview); InvalidateRect(hwnd,NULL,FALSE);
            }
        }
        return 0;
    case WM_COMMAND:
        if(power_dialog_active()) { power_dialog_focus(); return 0; }
        if (status_view != VIEW_LOGIN) return 0;
        if (LOWORD(wp) == ID_PREV || LOWORD(wp) == ID_NEXT) {
            index = profile_page + (LOWORD(wp) == ID_NEXT ? 1 : -1);
            if (index >= 0 && index*page_size() < profiles_count()) {
                profile_page = index; selected_profile = index*page_size(); compact_selected=TRUE; pending_profile = !preview; next_profile_attempt = 0;
                position_arrow(hwnd); position_profile_navigation(hwnd); EnableWindow(GetDlgItem(hwnd, ID_SUBMIT), preview);
                InvalidateRect(hwnd, NULL, FALSE);
            }
        }
        if (LOWORD(wp) == ID_SUBMIT) {
            if (preview) { finishing = TRUE; finish_time = GetTickCount(); set_status_view(hwnd,VIEW_WELCOME_MULTI); }
            else if (!pending_profile) native_logon_submit();
        }
        if (LOWORD(wp) == ID_POWER && !power_pending) {
            power_pending=PostMessageA(hwnd,SHOW_POWER,0,0);
        }
        if (preview && LOWORD(wp) == ID_DONE && !finishing) {
            finishing = TRUE; finish_time = GetTickCount(); set_status_view(hwnd,VIEW_WELCOME_MULTI);
        }
        return 0;
    case SHOW_POWER:
        power_pending=FALSE;
        if(status_view==VIEW_LOGIN && !finishing && !rescue_mode) {
            SetCursor(LoadCursor(NULL,IDC_ARROW)); show_power_dialog(hwnd);
        }
        return 0;
    case WM_SETCURSOR:
        if(LOWORD(lp)==HTCLIENT) { SetCursor(LoadCursor(NULL,IDC_ARROW)); return TRUE; }
        break;
    case WM_TIMER:
        if (wp==2 && preview_power) {
            KillTimer(hwnd,2); show_power_dialog(hwnd); return 0;
        }
        if (stop_event && WaitForSingleObject(stop_event, 0) == WAIT_OBJECT_0) {
            if(resident_mode) close_canvas(); else DestroyWindow(hwnd);
            return 0;
        }
        if(power_dialog_active() || preparing_canvas) return 0;
        if (sign_in_view() && !finishing && ready_event && WaitForSingleObject(ready_event, 0) == WAIT_OBJECT_0) {
            int loading_view=profiles_count()>1 || status_view==VIEW_LOGIN ? VIEW_WELCOME_MULTI : VIEW_WELCOME_SINGLE;
            finishing = TRUE; finish_time = GetTickCount(); set_status_view(hwnd,loading_view);
        }
        if (finishing && (DWORD)(GetTickCount() - finish_time) >= 1000) {
            if(resident_mode) {
                if(shell_ready_to_reveal()) { session_running=TRUE; close_canvas(); }
                else maintain_topmost(hwnd);
            } else DestroyWindow(hwnd);
            return 0;
        }
        if (preview_status && (DWORD)(GetTickCount()-preview_started) >= 5000) { DestroyWindow(hwnd); return 0; }
        if (!sign_in_view()) { maintain_topmost(hwnd); return 0; }
        if (rescue_mode || servicing_logon) return 0;
        servicing_logon = TRUE;
        allow_logon();
        if (pending_profile && native_logon_attached() && (LONG)(GetTickCount()-next_profile_attempt) >= 0) {
            next_profile_attempt = GetTickCount() + 1000;
            index = selected_profile;
            lstrcpynA(requested_name, profiles_name(index), sizeof(requested_name));
            if(status_view==VIEW_STARTING && !native_logon_pristine()) manual_single_login();
            else if (native_logon_set_user(requested_name) && selected_profile == index) {
                pending_profile = FALSE;
                login_geometry(&x, &y, &width);
                if(status_view==VIEW_LOGIN) {
                    native_logon_layout(x, y, width);
                    EnableWindow(GetDlgItem(hwnd, ID_SUBMIT), TRUE);
                }
                InvalidateRect(hwnd, NULL, FALSE);
            }
        }
        try_single_account_logon();
        service_single_pending();
        maintain_topmost(hwnd);
        servicing_logon = FALSE;
        return 0;
    case WM_DISPLAYCHANGE:
        compact_selected=TRUE;
        power_dialog_close();
        profile_page=selected_profile/page_size();
        position_profile_navigation(hwnd);
        SetWindowPos(hwnd, NULL, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), SWP_NOZORDER | SWP_NOACTIVATE);
        login_geometry(&x, &y, &width);
        if (status_view == VIEW_LOGIN && native_logon_attached() && !native_logon_layout(x, y, width)) {
            prefer_native = TRUE;
            native_logon_restore(TRUE);
        }
        if (preview) {
            SetWindowPos(GetDlgItem(hwnd, 201), NULL, x, y, width - 60, 22, SWP_NOZORDER | SWP_NOACTIVATE);
            SetWindowPos(GetDlgItem(hwnd, ID_DONE), NULL, rx(625), by(572), 150, 23, SWP_NOZORDER | SWP_NOACTIVATE);
        }
        position_arrow(hwnd);
        SetWindowPos(GetDlgItem(hwnd, ID_PREV), NULL, cx(720), cy(455), 25, 24, SWP_NOZORDER | SWP_NOACTIVATE);
        SetWindowPos(GetDlgItem(hwnd, ID_NEXT), NULL, cx(750), cy(455), 25, 24, SWP_NOZORDER | SWP_NOACTIVATE);
        SetWindowPos(GetDlgItem(hwnd, ID_POWER), NULL, 25, by(526), 245, 29, SWP_NOZORDER | SWP_NOACTIVATE);
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    case WM_DESTROY:
        power_dialog_close();
        if (escape_hotkey) UnregisterHotKey(hwnd, 1);
        if (close_hotkey) UnregisterHotKey(hwnd, 2);
        KillTimer(hwnd, TICK); power_pending=FALSE; release_logon(); screen = NULL;
        if(!resident_mode) hide_backdrop();
        if (!monitor_active) PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, message, wp, lp);
}

/* Build children while hidden. Win9x may consume the initial update region
   during creation/session teardown; UpdateWindow alone cannot request it again.
   Publish screen before showing, then explicitly paint the entire composition. */
static HWND create_canvas(void)
{
    canvas_show_painted=FALSE;
    if(status_view!=VIEW_SHUTDOWN) show_backdrop();
    release_first_frame();
    if(preparing_canvas && status_view==VIEW_STARTING) {
        BOOL prepared;
        startup_trace(startup_config.root,startup_config.backup_key,"canvas-frame-start",0);
        prepared=prepare_first_frame();
        startup_trace(startup_config.root,startup_config.backup_key,"canvas-frame-ready",prepared ? 0 : ERROR_NOT_ENOUGH_MEMORY);
        if(!prepared && !body_font) create_ui_fonts();
    }
    startup_trace(startup_config.root,startup_config.backup_key,"canvas-create-start",0);
    screen=CreateWindowExA(WS_EX_TOPMOST|WS_EX_TOOLWINDOW,"Welcome95.Screen",APP,
        WS_POPUP|WS_CLIPCHILDREN,0,0,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN),
        backdrop,NULL,instance,NULL);
    if(screen) {
        startup_trace(startup_config.root,startup_config.backup_key,"canvas-show-start",0);
        ShowWindow(screen,SW_SHOWNOACTIVATE);
        if(!canvas_show_painted) RedrawWindow(screen,NULL,NULL,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_UPDATENOW);
        startup_trace(startup_config.root,startup_config.backup_key,"canvas-presented",0);
    }
    release_first_frame();
    if(screen) maintain_backdrop(); else hide_backdrop();
    return screen;
}

/* Reuse the service's existing window across teardown, without creating a new
   credential canvas or retaining session font/control handles in the cover. */
static void maintain_controller_cover(HWND window)
{
    RECT bounds;
    LONG style,extended;
    int width=GetSystemMetrics(SM_CXSCREEN),height=GetSystemMetrics(SM_CYSCREEN);
    if(!controller_cover || repairing_cover || !window || !IsWindow(window)) return;
    maintain_backdrop();
    style=GetWindowLongA(window,GWL_STYLE); extended=GetWindowLongA(window,GWL_EXSTYLE);
    GetWindowRect(window,&bounds);
    if(IsWindowVisible(window) && (style&WS_POPUP) && (extended&WS_EX_TOPMOST) && (extended&WS_EX_TOOLWINDOW) &&
       bounds.left==0 && bounds.top==0 && bounds.right==width && bounds.bottom==height) return;
    repairing_cover=TRUE;
    if(!(style&WS_POPUP)) SetWindowLongA(window,GWL_STYLE,WS_POPUP|(style&WS_VISIBLE));
    if(!(extended&WS_EX_TOOLWINDOW)) SetWindowLongA(window,GWL_EXSTYLE,extended|WS_EX_TOOLWINDOW);
    SetWindowPos(window,HWND_TOPMOST,0,0,width,height,SWP_NOACTIVATE|SWP_SHOWWINDOW);
    RedrawWindow(window,NULL,NULL,RDW_INVALIDATE|RDW_UPDATENOW);
    repairing_cover=FALSE;
    maintain_backdrop();
}
static void queue_cover_repair(HWND window)
{
    if(controller_cover && !repairing_cover && !cover_repair_pending)
        cover_repair_pending=PostMessageA(window,REPAIR_COVER,0,0);
}
static void show_controller_cover(HWND window)
{
    if(!window || !IsWindow(window)) return;
    controller_cover=TRUE;
    show_backdrop();
    maintain_controller_cover(window);
}
static void hide_controller_cover(void)
{
    if(!controller_cover) return;
    /* Disable repairs before our deliberate hide; stale posted work is inert. */
    controller_cover=FALSE; cover_repair_pending=FALSE;
    if(controller && IsWindow(controller)) {
        ShowWindow(controller,SW_HIDE);
        SetWindowLongA(controller,GWL_STYLE,WS_OVERLAPPED);
        SetWindowLongA(controller,GWL_EXSTYLE,0);
        SetWindowPos(controller,HWND_NOTOPMOST,0,0,0,0,SWP_NOACTIVATE);
    }
}

/* A committed end notification precedes completion of Win9x desktop teardown.
   Keep the service, but create no new canvas/children until a new native form
   or a real startup handshake establishes the next session. */
static void await_logon_cycle(void)
{
    require_account_choice=TRUE;
    ResetEvent(ready_event); ResetEvent(closed_event);
    if(shell_event) ResetEvent(shell_event);
    shell_confirmed=shell_released=FALSE; shell_process_id=0;
    show_controller_cover(controller);
    /* Retiring the old session is not an authenticated shell handoff. */
    if(screen && IsWindow(screen)) DestroyWindow(screen);
    release_ui_fonts();
    startup_trace(startup_config.root,startup_config.backup_key,"canvas-retired",0);
    session_running=FALSE; awaiting_logon=TRUE; finishing=FALSE;
    if(shell_process) CloseHandle(shell_process);
    shell_process=NULL;
    if(enabled()) startup_rearm(&startup_config);
}

static void begin_logon_cycle(BOOL reset)
{
    if(reset) require_account_choice=TRUE;
    if(screen && IsWindow(screen)) DestroyWindow(screen);
    if(reset) ResetEvent(ready_event);
    ResetEvent(closed_event); ResetEvent(logoff_event);
    if(shell_event) ResetEvent(shell_event);
    shell_confirmed=shell_released=FALSE; shell_process_id=0; shell_started_at=shell_released_at=0;
    if(ended_event) ResetEvent(ended_event);
    if(shell_process) CloseHandle(shell_process);
    shell_process=NULL; session_running=FALSE; awaiting_logon=FALSE;
    power_requested=FALSE; ending_unknown=FALSE; shutdown_only_canvas=FALSE;
    finishing=FALSE; prefer_native=FALSE; pending_profile=FALSE; servicing_logon=FALSE;
    rescue_mode=FALSE;
    automatic_attempted=require_account_choice; compact_selected=FALSE; selected_profile=profile_page=0;
    next_logon_attempt=next_profile_attempt=0;
    single_submitted=FALSE; preparing_canvas=TRUE; status_view=VIEW_STARTING;
    /* First cover the desktop using OS fonts and embedded artwork. Profile
       discovery and TTF extraction/registration must not delay the first frame. */
    {
        BOOL load_fonts=!body_font;
        if(!create_canvas()) {
            preparing_canvas=FALSE; hide_controller_cover();
            if(stop_event) SetEvent(stop_event);
            if(closed_event) SetEvent(closed_event);
            return;
        }
        if(load_fonts && !body_font) create_ui_fonts();
        profiles_load();
        if(load_fonts && bundled_fonts_load(instance)>0) {
            release_ui_font_handles(); create_ui_fonts();
            SendDlgItemMessageA(screen,ID_DONE,WM_SETFONT,(WPARAM)small_font,FALSE);
        }
    }
    single_started=GetTickCount();
    set_status_view(screen,!preview && !require_account_choice && profiles_count()==1 ? VIEW_STARTING : VIEW_LOGIN);
    preparing_canvas=FALSE; hide_controller_cover();
    if(enabled()) startup_rearm(&startup_config);
}
static void rescue_native_logon(void)
{
    HWND found=NULL,popup;
    power_dialog_close();
    hide_controller_cover();
    if(!screen && awaiting_logon) begin_logon_cycle(TRUE);
    if(screen) {
        rescue_mode=TRUE; prefer_native=TRUE;
        login_hotkeys(screen,FALSE);
        native_logon_restore(FALSE);
        ShowWindow(screen,SW_HIDE);
    }
    hide_backdrop();
    if(login_window && IsWindow(login_window) && IsWindowVisible(login_window)) found=login_window;
    else EnumWindows(find_logon,(LPARAM)&found);
    if(found) {
        GetWindowThreadProcessId(found,&provider_process);
        SetWindowPos(found,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
        popup=provider_notice(found);
        if(popup && popup!=found && IsWindowVisible(popup)) found=popup;
        SetForegroundWindow(found);
    }
    startup_trace(startup_config.root,startup_config.backup_key,"native-rescue",found ? 0 : ERROR_NOT_FOUND);
    /* Never create a fake credential dialog, submit input or mark a session ready. */
}
static LRESULT CALLBACK monitor_proc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp)
{
    switch(message) {
    case WM_PAINT:
        if(controller_cover && resident_mode) {
            if(first_frame_matches(hwnd)) {
                PAINTSTRUCT paint; BOOL erased;
                HDC dc=begin_cached_paint(hwnd,&paint,&erased);
                if(!erased) copy_first_frame(hwnd,dc);
                EndPaint(hwnd,&paint);
            } else early_cover_paint(hwnd,paint_wait_details);
            return 0;
        }
        break;
    case WM_ERASEBKGND:
        if(controller_cover && resident_mode) return erase_first_frame(hwnd,(HDC)wp);
        break;
    case WM_SETCURSOR:
        if(controller_cover && LOWORD(lp)==HTCLIENT) { SetCursor(LoadCursor(NULL,IDC_ARROW)); return TRUE; }
        break;
    case REPAIR_COVER:
        cover_repair_pending=FALSE;
        maintain_controller_cover(hwnd);
        return 0;
    case WM_CLOSE:
        if(controller_cover && resident_mode) {
            startup_trace(startup_config.root,startup_config.backup_key,"cover-close-held",0);
            return 0;
        }
        break;
    case WM_WINDOWPOSCHANGING:
        if(controller_cover && resident_mode && !repairing_cover) {
            WINDOWPOS *position=(WINDOWPOS *)lp;
            /* Keep only our temporary cover present until explicit handoff or
               rescue. Queued repair alone allows a visible hidden interval. */
            if(position->flags&SWP_HIDEWINDOW) {
                position->flags=(position->flags&~SWP_HIDEWINDOW)|SWP_SHOWWINDOW;
                startup_trace(startup_config.root,startup_config.backup_key,"cover-hide-held",0);
            }
            if(!(position->flags&SWP_NOZORDER) && position->hwndInsertAfter!=HWND_TOPMOST) {
                position->hwndInsertAfter=HWND_TOPMOST;
                startup_trace(startup_config.root,startup_config.backup_key,"cover-order-held",0);
            }
        }
        break;
    case WM_SHOWWINDOW:
        if(!wp) queue_cover_repair(hwnd);
        break;
    case WM_WINDOWPOSCHANGED:
        queue_cover_repair(hwnd);
        break;
    case WM_DISPLAYCHANGE:
        if(controller_cover) {
            maintain_controller_cover(hwnd);
            RedrawWindow(hwnd,NULL,NULL,RDW_INVALIDATE|RDW_UPDATENOW);
            return 0;
        }
        break;
    case WM_CREATE:
        SetTimer(hwnd,TICK,resident_mode ? 100 : 250,NULL);
        if(resident_mode) RegisterHotKey(hwnd,ID_RESCUE_HOTKEY,MOD_CONTROL|MOD_ALT|MOD_SHIFT,'L');
        return 0;
    case WM_HOTKEY:
        if(resident_mode && wp==ID_RESCUE_HOTKEY) rescue_native_logon();
        return 0;
    case SESSION_STARTING:
        if(!resident_mode) return FALSE;
        /* Serialize a fast new login with pending end-session notifications.
           Reset old acknowledgements before publishing this cycle's readiness. */
        if(awaiting_logon || WaitForSingleObject(logoff_event,0)==WAIT_OBJECT_0 ||
           (ended_event && WaitForSingleObject(ended_event,0)==WAIT_OBJECT_0)) begin_logon_cycle(TRUE);
        startup_trace(startup_config.root,startup_config.backup_key,"session-starting",0);
        SetEvent(ready_event); return TRUE;
    case SHELL_STARTED:
        if(resident_mode) {
            if(shell_process) CloseHandle(shell_process);
            shell_process=OpenProcess(SYNCHRONIZE,FALSE,(DWORD)wp);
            shell_process_id=(DWORD)wp; shell_confirmed=TRUE; shell_started_at=GetTickCount();
        }
        return 0;
    case WM_QUERYENDSESSION:
        if(controller_cover) return TRUE;
        if(!((DWORD)lp&ENDSESSION_LOGOFF_95) && enabled() && !FindWindowA("Welcome95.Screen",NULL) &&
           (resident_mode || !FindWindowA("Welcome95.Controller",NULL))) {
            if(!body_font) { bundled_fonts_load(instance); create_ui_fonts(); }
            status_view=VIEW_SHUTDOWN; shutdown_only_canvas=TRUE; ending_unknown=!power_requested;
            create_canvas();
        }
        return TRUE;
    case WM_ENDSESSION:
        if(wp && (!resident_mode || !power_requested || ((DWORD)lp&ENDSESSION_LOGOFF_95))) {
            HANDLE event;
            if(resident_mode) show_controller_cover(hwnd);
            event=CreateEventA(NULL,TRUE,FALSE,((DWORD)lp&ENDSESSION_LOGOFF_95) ? LOGOFF_EVENT : ENDED_EVENT);
            if(event) { SetEvent(event); CloseHandle(event); }
            if(resident_mode) return 0;
        }
        if(!wp) { power_requested=FALSE; ending_unknown=FALSE; }
        if(screen && IsWindow(screen) && (wp || status_view==VIEW_SHUTDOWN)) {
            if(!wp && resident_mode && !shutdown_only_canvas) set_status_view(screen,VIEW_LOGIN);
            else DestroyWindow(screen);
        }
        if(wp) DestroyWindow(hwnd);
        return 0;
    case WM_TIMER:
        if(stop_event && WaitForSingleObject(stop_event,0)==WAIT_OBJECT_0) {
            close_canvas(); DestroyWindow(hwnd); return 0;
        }
        if(resident_mode) {
            if(controller_cover) maintain_controller_cover(hwnd);
            if(WaitForSingleObject(logoff_event,0)==WAIT_OBJECT_0) {
                require_account_choice=TRUE;
                ResetEvent(logoff_event);
                if(session_running || awaiting_logon) await_logon_cycle();
            } else if(ended_event && WaitForSingleObject(ended_event,0)==WAIT_OBJECT_0) {
                ResetEvent(ended_event);
                await_logon_cycle();
            } else if(awaiting_logon) {
                HWND found=NULL;
                EnumWindows(find_logon,(LPARAM)&found);
                if(found) begin_logon_cycle(TRUE);
                else if(WaitForSingleObject(ready_event,0)==WAIT_OBJECT_0) begin_logon_cycle(FALSE);
            } else if(session_running && !screen && shell_process &&
                      WaitForSingleObject(shell_process,0)==WAIT_OBJECT_0) {
                /* Recovery if the normal per-session notifier was not started:
                   require the previous provider process's password dialog too. */
                HWND found=NULL; DWORD process=0;
                EnumWindows(find_logon,(LPARAM)&found);
                if(found) GetWindowThreadProcessId(found,&process);
                if(found && process==provider_process) begin_logon_cycle(TRUE);
            }
        } else if(ready_event && (DWORD)(GetTickCount()-monitor_started)>=15000) {
            CloseHandle(ready_event); ready_event=NULL;
        }
        return 0;
    case WM_DESTROY:
        controller_cover=cover_repair_pending=FALSE;
        destroy_backdrop();
        if(resident_mode) UnregisterHotKey(hwnd,ID_RESCUE_HOTKEY);
        KillTimer(hwnd,TICK); PostQuitMessage(0); return 0;
    }
    return DefWindowProcA(hwnd,message,wp,lp);
}

static void update_settings(HWND hwnd)
{
    SetDlgItemTextA(hwnd, 200, enabled() ? ui_text("\321\356\361\362\356\377\355\350\345: \342\352\353\376\367\345\355\356 \361\356 \361\353\345\344\363\376\371\345\343\356 \347\340\357\363\361\352\340 Windows","Status: enabled for the next Windows start") : ui_text("\321\356\361\362\356\377\355\350\345: \356\362\352\353\376\367\345\355\356 \344\353\377 \375\362\356\351 \352\356\357\350\350 \357\360\356\343\360\340\354\354\373","Status: disabled for this executable"));
}

static void button(HWND hwnd, const char *text, int id, int x, int y, int width)
{
    HWND child = CreateWindowA("BUTTON", text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                              x, y, width, 24, hwnd, (HMENU)(UINT_PTR)id, instance, NULL);
    SendMessageA(child, WM_SETFONT, (WPARAM)settings_font, TRUE);
}

static LRESULT CALLBACK settings_proc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp)
{
    HWND child;
    STARTUPINFOA startup;
    PROCESS_INFORMATION process;
    char command[MAX_PATH + 40];
    switch (message) {
    case WM_CREATE:
        child=CreateWindowA("STATIC","Welcome95 1.0",WS_CHILD|WS_VISIBLE,10,8,384,18,hwnd,NULL,instance,NULL);
        SendMessageA(child,WM_SETFONT,(WPARAM)settings_title_font,TRUE);
        child=CreateWindowA("STATIC",ui_text("\317\360\350\342\345\362\361\362\342\350\345 \342 \361\362\350\353\345 Windows XP \344\353\377 Windows 95.\r\n\316\341\356\353\356\367\352\340 \343\356\362\356\342\350\362\361\377 \347\340 \375\352\360\340\355\356\354 \357\360\350\342\345\362\361\362\342\350\377.\r\n\317\360\356\342\345\360\352\363 \357\340\360\356\353\377 \342\373\357\356\353\355\377\345\362 Windows.","Windows XP welcome screen for Windows 95.\r\nYour shell starts behind the welcome screen.\r\nWindows verifies your password."),
            WS_CHILD|WS_VISIBLE,10,30,384,42,hwnd,NULL,instance,NULL);
        SendMessageA(child,WM_SETFONT,(WPARAM)settings_font,TRUE);
        child=CreateWindowA("STATIC","",WS_CHILD|WS_VISIBLE,10,78,384,16,hwnd,(HMENU)200,instance,NULL);
        SendMessageA(child,WM_SETFONT,(WPARAM)settings_font,TRUE);
        button(hwnd,ui_text("\302\352\353\376\367\350\362\374","Enable"),ID_ENABLE,10,101,90);
        button(hwnd,ui_text("\316\362\352\353\376\367\350\362\374","Disable"),ID_DISABLE,108,101,90);
        button(hwnd,ui_text("\317\360\356\361\354\356\362\360","Preview"),ID_PREVIEW,206,101,90);
        button(hwnd,ui_text("\307\340\352\360\373\362\374","Close"),ID_EXIT,304,101,90);
        child=CreateWindowA("STATIC",ui_text("\317\360\356\361\354\356\362\360 \375\352\360\340\355\356\342:","Preview screens:"),WS_CHILD|WS_VISIBLE,10,133,384,14,hwnd,NULL,instance,NULL);
        SendMessageA(child,WM_SETFONT,(WPARAM)settings_font,TRUE);
        button(hwnd,ui_text("\307\340\357\363\361\352","Starting"),ID_PREVIEW_STARTING,10,152,90);
        button(hwnd,ui_text("\301\345\347 \357\340\360\356\353\377","No password"),ID_PREVIEW_SINGLE,108,152,90);
        button(hwnd,ui_text("\321 \352\340\360\362\356\367\352\356\351","With account"),ID_PREVIEW_MULTI,206,152,90);
        button(hwnd,ui_text("\302\373\352\353\376\367\345\355\350\345","Shutdown"),ID_PREVIEW_SHUTDOWN,304,152,90);
        child=CreateWindowA("STATIC",ui_text("\317\356\361\353\345 \342\352\353\376\367\345\355\350\377: \317\340\355\345\353\374 \363\357\360\340\342\353\345\355\350\377 - Welcome95.","After enabling: Control Panel - Welcome95."),WS_CHILD|WS_VISIBLE,10,184,384,14,hwnd,NULL,instance,NULL);
        SendMessageA(child,WM_SETFONT,(WPARAM)settings_font,TRUE);
        update_settings(hwnd); return 0;
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ID_ENABLE:
            if (configure(hwnd, TRUE)) MessageBoxA(hwnd, ui_text("\302\352\353\376\367\345\355\356. \321\356\347\344\340\355\340 \360\345\347\345\360\342\355\340\377 \352\356\357\350\377 \357\360\345\346\355\345\351 \356\341\356\353\356\367\352\350.\r\n\317\345\360\345\347\340\343\360\363\347\350\362\345 Windows \344\353\377 \357\360\356\342\345\360\352\350 \340\342\362\356\347\340\357\363\361\352\340.\r\n\315\340\361\362\360\356\351\352\350: \317\340\355\345\353\374 \363\357\360\340\342\353\345\355\350\377 - Welcome95.","Enabled. The previous shell has been backed up.\r\nRestart Windows to test startup.\r\nSettings: Control Panel - Welcome95."), APP, MB_OK | MB_ICONINFORMATION);
            update_settings(hwnd); break;
        case ID_DISABLE: configure(hwnd, FALSE); update_settings(hwnd); break;
        case ID_PREVIEW_STARTING:
        case ID_PREVIEW_SINGLE:
        case ID_PREVIEW_MULTI:
        case ID_PREVIEW_SHUTDOWN:
        case ID_PREVIEW:
            zero_memory(&startup, sizeof(startup)); startup.cb = sizeof(startup);
            zero_memory(&process, sizeof(process));
            command_line(command, LOWORD(wp)==ID_PREVIEW_STARTING ? "/preview-starting" :
                LOWORD(wp) == ID_PREVIEW_SINGLE ? "/preview-single" :
                LOWORD(wp) == ID_PREVIEW_MULTI ? "/preview-multi" :
                LOWORD(wp) == ID_PREVIEW_SHUTDOWN ? "/preview-shutdown" : "/preview");
            if (!CreateProcessA(exe_path, command, NULL, NULL, FALSE, 0, NULL, NULL, &startup, &process))
                error_box(hwnd, ui_text("\315\345 \363\344\340\353\356\361\374 \347\340\357\363\361\362\350\362\374 \357\360\356\361\354\356\362\360.","Cannot start preview."));
            else { CloseHandle(process.hThread); CloseHandle(process.hProcess); }
            break;
        case ID_EXIT: DestroyWindow(hwnd); break;
        }
        return 0;
    case WM_ENDSESSION: if (wp) DestroyWindow(hwnd); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcA(hwnd, message, wp, lp);
}

static void release_ui_fonts(void)
{
    release_ui_font_handles();
    bundled_fonts_release();
    power_operations_release();
}

int WINAPI WinMain(HINSTANCE current, HINSTANCE previous, LPSTR arguments, int show)
{
    WNDCLASSA cls;
    MSG message;
    HWND window;
    DWORD length, version;
    BOOL welcome;
    BOOL session_monitor;
    (void)previous; (void)show;
    instance = current;
    length = GetModuleFileNameA(NULL, exe_path, sizeof(exe_path));
    if (!length || length >= sizeof(exe_path) - 1) return 1;
    initialize_startup();
    if(!*arguments && (GetVersion()&0x80000000UL)) {
        int result=managed_settings();
        if(result>=0) return result;
    }
    if(!lstrcmpiA(arguments,"/shell") || !lstrcmpiA(arguments,"/startup")) {
        DWORD error;
        if(!(GetVersion()&0x80000000UL)) return 1;
        error=handoff(!lstrcmpiA(arguments,"/shell"));
        if(error) {
            HANDLE event=OpenEventA(EVENT_MODIFY_STATE,FALSE,STOP_EVENT);
            if(event) { SetEvent(event); CloseHandle(event); }
            MessageBoxA(NULL,ui_text("Welcome95 \355\345 \363\344\340\353\356\361\374 \347\340\357\363\361\362\350\362\374 \361\356\365\360\340\355\270\355\355\363\376 \356\341\356\353\356\367\352\363. \302\356\361\361\362\340\355\356\342\350\362\345 \357\340\360\340\354\345\362\360 shell \342 \360\340\347\344\345\353\345 [boot] \364\340\351\353\340 SYSTEM.INI \350\347 \360\345\347\345\360\342\355\356\351 \352\356\357\350\350 Welcome95 \350\353\350 \355\340\346\354\350\362\345 \253\316\362\352\353\376\367\350\362\374\273.","Welcome95 cannot start the saved shell. Restore SYSTEM.INI [boot] shell from the Welcome95 backup or use Disable."),APP,MB_OK|MB_ICONERROR|MB_TOPMOST);
        }
        return error ? 1 : 0;
    }
    session_monitor = !lstrcmpiA(arguments, "/session-ready");
    if (session_monitor) {
        ready_event = CreateEventA(NULL, TRUE, FALSE, READY_EVENT);
        if (!ready_event) return 1;
        if(!enabled()) SetEvent(ready_event);
        monitor_instance = CreateEventA(NULL, TRUE, FALSE, "Welcome95.SessionMonitor");
        if (!monitor_instance) { CloseHandle(ready_event); return 1; }
        if (GetLastError() == ERROR_ALREADY_EXISTS) { CloseHandle(monitor_instance); CloseHandle(ready_event); return 0; }
        monitor_active = TRUE; monitor_started = GetTickCount();
        stop_event = CreateEventA(NULL, TRUE, FALSE, STOP_EVENT);
        if (!stop_event) { CloseHandle(monitor_instance); CloseHandle(ready_event); return 1; }
    }
    preview_status = !lstrcmpiA(arguments, "/preview-single") || !lstrcmpiA(arguments, "/preview-multi") ||
        !lstrcmpiA(arguments, "/preview-shutdown") || !lstrcmpiA(arguments,"/preview-starting");
    preview_power = !lstrcmpiA(arguments,"/preview-power");
    preview = !lstrcmpiA(arguments, "/preview") || preview_status || preview_power;
    if(!lstrcmpiA(arguments,"/preview-starting")) status_view=VIEW_STARTING;
    if (!lstrcmpiA(arguments, "/preview-single")) status_view = VIEW_WELCOME_SINGLE;
    if (!lstrcmpiA(arguments, "/preview-multi")) status_view = VIEW_WELCOME_MULTI;
    if (!lstrcmpiA(arguments, "/preview-shutdown")) status_view = VIEW_SHUTDOWN;
    preview_started = GetTickCount();
    welcome = !lstrcmpiA(arguments, "/welcome");
    if (*arguments && !preview && !welcome && !session_monitor) {
        error_box(NULL, ui_text("\315\345\350\347\342\345\361\362\355\373\351 \357\340\360\340\354\345\362\360. \307\340\357\363\361\362\350\362\345 \357\360\356\343\360\340\354\354\363 \341\345\347 \357\340\360\340\354\345\362\360\356\342 \344\353\377 \356\362\352\360\373\362\350\377 \355\340\361\362\360\356\345\352.","Unknown option. Run without arguments for settings.")); return 1;
    }
    version = GetVersion();
    if (welcome && (!(version & 0x80000000UL) || LOBYTE(LOWORD(version)) != 4)) {
        error_box(NULL, ui_text("\300\342\362\356\347\340\357\363\361\352 \375\352\360\340\355\340 \357\360\350\342\345\362\361\362\342\350\377 \344\356\361\362\363\357\345\355 \362\356\353\374\352\356 \342 Windows 95/98/Me. \304\353\377 \357\360\356\361\354\356\362\360\340 \355\340\346\354\350\362\345 \253\317\360\356\361\354\356\362\360\273.","Automatic welcome mode supports Windows 95/98/Me only. Use Preview to inspect the screen.")); return 1;
    }
    if (welcome || preview) {
        running_event = CreateEventA(NULL, TRUE, FALSE, preview ? "Welcome95.Preview" : INSTANCE_EVENT);
        if (!running_event) return 1;
        if (GetLastError() == ERROR_ALREADY_EXISTS) { CloseHandle(running_event); return 0; }
        if (!preview) {
            ready_event = CreateEventA(NULL, TRUE, FALSE, READY_EVENT);
            stop_event = CreateEventA(NULL, TRUE, FALSE, STOP_EVENT);
            if (!ready_event || !stop_event) {
                if (ready_event) CloseHandle(ready_event);
                if (stop_event) CloseHandle(stop_event);
                CloseHandle(running_event); return 1;
            }
        }
    }
    if(welcome) {
        union { FARPROC raw; RegisterService service; } address;
        closed_event=CreateEventA(NULL,TRUE,FALSE,CLOSED_EVENT);
        logoff_event=CreateEventA(NULL,TRUE,FALSE,LOGOFF_EVENT);
        ended_event=CreateEventA(NULL,TRUE,FALSE,ENDED_EVENT);
        controller_event=CreateEventA(NULL,TRUE,FALSE,CONTROLLER_EVENT);
        shell_event=CreateEventA(NULL,TRUE,FALSE,SHELL_EVENT);
        if(!closed_event || !logoff_event || !controller_event || !ended_event || !shell_event) return 1;
        address.raw=GetProcAddress(GetModuleHandleA("kernel32.dll"),"RegisterServiceProcess");
        register_service=address.service;
        if(!register_service || !register_service(GetCurrentProcessId(),1)) return 1;
        resident_mode=monitor_active=TRUE;
    }
    if(!welcome && (!session_monitor || !FindWindowA("Welcome95.Controller",NULL))) {
        profiles_load(); bundled_fonts_load(instance); create_ui_fonts();
    }
    zero_memory(&cls, sizeof(cls));
    cls.hInstance = instance; cls.hCursor = LoadCursor(NULL, IDC_ARROW);
    cls.hIcon=LoadIconA(instance,MAKEINTRESOURCEA(1));
    cls.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    cls.lpfnWndProc = welcome || preview || session_monitor ? welcome_proc : settings_proc;
    cls.lpszClassName = welcome || preview || session_monitor ? "Welcome95.Screen" : "Welcome95.Settings";
    if (!RegisterClassA(&cls)) { release_ui_fonts(); return 1; }
    if (session_monitor || welcome) {
        cls.lpfnWndProc = monitor_proc; cls.lpszClassName = welcome ? "Welcome95.Controller" : "Welcome95.Monitor";
        if (!RegisterClassA(&cls)) { release_ui_fonts(); return 1; }
        window = CreateWindowA(cls.lpszClassName, APP, WS_OVERLAPPED, 0, 0, 0, 0, NULL, NULL, instance, NULL);
        if(welcome && window) {
            controller=window; begin_logon_cycle(FALSE);
            if(screen) SetEvent(controller_event);
        }
    } else if (welcome || preview) {
        window = create_canvas();
    } else {
        window = CreateWindowA(cls.lpszClassName, ui_text("\315\340\361\362\360\356\351\352\350 Welcome95","Welcome95 settings"), WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
            CW_USEDEFAULT, CW_USEDEFAULT, SETTINGS_WIDTH, SETTINGS_HEIGHT, NULL, NULL, instance, NULL);
        ShowWindow(window, SW_SHOW);
    }
    if (!window) { release_ui_fonts(); return 1; }
    UpdateWindow(window);
    if(preview_power) SetTimer(window,2,200,NULL);
    while (GetMessageA(&message, NULL, 0, 0) > 0) {
        if(power_dialog_translate(&message)) continue;
        if ((welcome || preview) || !IsDialogMessageA(window, &message)) {
            TranslateMessage(&message); DispatchMessageA(&message);
        }
    }
    if(register_service && resident_mode) register_service(GetCurrentProcessId(),0);
    if(shell_process) CloseHandle(shell_process);
    if(shell_event) CloseHandle(shell_event);
    if(closed_event) CloseHandle(closed_event);
    if(logoff_event) CloseHandle(logoff_event);
    if(ended_event) CloseHandle(ended_event);
    if(controller_event) CloseHandle(controller_event);
    if (ready_event) CloseHandle(ready_event);
    if (stop_event) CloseHandle(stop_event);
    if (running_event) CloseHandle(running_event);
    if (monitor_instance) CloseHandle(monitor_instance);
    destroy_backdrop();
    release_ui_fonts();
    return 0;
}

/* Parse only the executable portion; WinMain receives the remaining switches.
   This entry point intentionally avoids linking any C runtime. */
void welcome_entry(void)
{
    char *arguments = GetCommandLineA();
    if (*arguments == '"') {
        ++arguments;
        while (*arguments && *arguments != '"') ++arguments;
        if (*arguments) ++arguments;
    } else {
        while (*arguments && *arguments != ' ' && *arguments != '\t') ++arguments;
    }
    while (*arguments == ' ' || *arguments == '\t') ++arguments;
    ExitProcess((UINT)WinMain(GetModuleHandleA(NULL), NULL, arguments, SW_SHOWDEFAULT));
}
