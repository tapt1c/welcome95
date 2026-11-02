/* Welcome95: ANSI Win32 only, no Unicode/NT-only APIs. */
#include <windows.h>
#include "native_logon.h"
#include "profiles.h"

#define APP "Welcome95"
#define REG_SERVICE "Software\\Microsoft\\Windows\\CurrentVersion\\RunServices"
#define REG_RUN "Software\\Microsoft\\Windows\\CurrentVersion\\Run"
#define READY_EVENT "Welcome95.SessionReady"
#define STOP_EVENT "Welcome95.Stop"
#define INSTANCE_EVENT "Welcome95.Running"
#define ID_ENABLE 101
#define ID_DISABLE 102
#define ID_PREVIEW 103
#define ID_DONE 104
#define ID_EXIT 105
#define ID_NATIVE 106
#define ID_SUBMIT 107
#define ID_POWER 108
#define ID_PREV 109
#define ID_NEXT 110
#define TICK 1

static HINSTANCE instance;
static HWND screen;
static HFONT title_font, body_font, small_font;
static HANDLE ready_event, stop_event, running_event;
static BOOL preview, finishing;
static DWORD finish_time;
static HWND login_window;
static BOOL login_was_topmost;
static BOOL prefer_native;
static int selected_profile, profile_page;
static BOOL pending_profile, servicing_logon;
static DWORD next_logon_attempt, next_profile_attempt;
static BOOL escape_hotkey, close_hotkey;
static char exe_path[MAX_PATH];

static int sx(int value) { return MulDiv(value, GetSystemMetrics(SM_CXSCREEN), 800); }
static int sy(int value) { return MulDiv(value, GetSystemMetrics(SM_CYSCREEN), 600); }

static int visible_profiles(void)
{
    int total = profiles_count() ? profiles_count() : 1;
    int remaining = total - profile_page * 4;
    return remaining < 4 ? remaining : 4;
}
static int profile_top(int index)
{
    int slot = index - profile_page * 4;
    return 291 - (visible_profiles() * 72 + 8) / 2 + slot * 72 +
        (index > selected_profile && selected_profile / 4 == profile_page ? 8 : 0);
}
static void login_geometry(int *x, int *y, int *width)
{
    *x = sx(501); *y = sy(profile_top(selected_profile) + 52); *width = sx(227);
}
static void position_arrow(HWND hwnd)
{
    SetWindowPos(GetDlgItem(hwnd, ID_SUBMIT), NULL, sx(685), sy(profile_top(selected_profile) + 51),
        sx(26), sy(26), SWP_NOZORDER | SWP_NOACTIVATE);
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

static LONG read_value(const char *key, char *buffer, DWORD capacity)
{
    HKEY handle;
    DWORD type;
    LONG result = RegOpenKeyExA(HKEY_LOCAL_MACHINE, key, 0, KEY_QUERY_VALUE, &handle);
    if (result != ERROR_SUCCESS) return result;
    result = RegQueryValueExA(handle, APP, NULL, &type, (BYTE *)buffer, &capacity);
    RegCloseKey(handle);
    if (result == ERROR_SUCCESS && (type != REG_SZ || capacity == 0 || buffer[capacity - 1] != '\0'))
        return ERROR_INVALID_DATA;
    return result;
}

static LONG write_value(const char *key, const char *value)
{
    HKEY handle;
    LONG result = RegCreateKeyExA(HKEY_LOCAL_MACHINE, key, 0, NULL, 0,
                                 KEY_SET_VALUE, NULL, &handle, NULL);
    if (result != ERROR_SUCCESS) return result;
    result = RegSetValueExA(handle, APP, 0, REG_SZ, (const BYTE *)value, (DWORD)lstrlenA(value) + 1);
    RegCloseKey(handle);
    return result;
}

static LONG remove_value(const char *key)
{
    HKEY handle;
    LONG result = RegOpenKeyExA(HKEY_LOCAL_MACHINE, key, 0, KEY_SET_VALUE, &handle);
    if (result == ERROR_FILE_NOT_FOUND) return ERROR_SUCCESS;
    if (result != ERROR_SUCCESS) return result;
    result = RegDeleteValueA(handle, APP);
    RegCloseKey(handle);
    return result == ERROR_FILE_NOT_FOUND ? ERROR_SUCCESS : result;
}

static void command_line(char *out, const char *argument)
{
    wsprintfA(out, "\"%s\" %s", exe_path, argument);
}

static BOOL enabled(void)
{
    char actual[MAX_PATH + 40], expected[MAX_PATH + 40];
    command_line(expected, "/welcome");
    if (read_value(REG_SERVICE, actual, sizeof(actual)) != ERROR_SUCCESS || lstrcmpiA(actual, expected))
        return FALSE;
    command_line(expected, "/session-ready");
    return read_value(REG_RUN, actual, sizeof(actual)) == ERROR_SUCCESS && !lstrcmpiA(actual, expected);
}

static BOOL configure(HWND owner, BOOL enable)
{
    char old_service[MAX_PATH + 40], old_run[MAX_PATH + 40], command[MAX_PATH + 40];
    LONG service_result, run_result, result;
    HANDLE event;
    service_result = read_value(REG_SERVICE, old_service, sizeof(old_service));
    run_result = read_value(REG_RUN, old_run, sizeof(old_run));
    if ((service_result != ERROR_SUCCESS && service_result != ERROR_FILE_NOT_FOUND) ||
        (run_result != ERROR_SUCCESS && run_result != ERROR_FILE_NOT_FOUND)) {
        error_box(owner, "Cannot read existing startup settings. No changes made.");
        return FALSE;
    }
    if (enable) {
        if (!(GetVersion() & 0x80000000UL)) {
            error_box(owner, "Automatic startup is intended for Windows 95/98/Me. Preview is available on other systems.");
            return FALSE;
        }
        command_line(command, "/welcome");
        if (service_result == ERROR_SUCCESS && lstrcmpiA(old_service, command)) {
            error_box(owner, "Another Welcome95 installation owns the startup entry. Disable it first.");
            return FALSE;
        }
        command_line(command, "/session-ready");
        if (run_result == ERROR_SUCCESS && lstrcmpiA(old_run, command)) {
            error_box(owner, "Another Welcome95 installation owns the session entry. Disable it first.");
            return FALSE;
        }
        result = write_value(REG_RUN, command);
        if (result == ERROR_SUCCESS) {
            command_line(command, "/welcome");
            result = write_value(REG_SERVICE, command);
            if (result != ERROR_SUCCESS) {
                LONG rollback = run_result == ERROR_SUCCESS ? write_value(REG_RUN, old_run) : remove_value(REG_RUN);
                if (rollback != ERROR_SUCCESS)
                    error_box(owner, "Startup failed and rollback failed. Remove the Welcome95 Run entry manually.");
            }
        }
    } else {
        command_line(command, "/welcome");
        if (service_result == ERROR_SUCCESS && lstrcmpiA(old_service, command)) {
            error_box(owner, "Startup belongs to another copy. Open that copy to disable it.");
            return FALSE;
        }
        command_line(command, "/session-ready");
        if (run_result == ERROR_SUCCESS && lstrcmpiA(old_run, command)) {
            error_box(owner, "Session startup belongs to another copy. Open that copy to disable it.");
            return FALSE;
        }
        result = remove_value(REG_SERVICE);
        if (result == ERROR_SUCCESS) result = remove_value(REG_RUN);
        event = OpenEventA(EVENT_MODIFY_STATE, FALSE, STOP_EVENT);
        if (event) { SetEvent(event); CloseHandle(event); }
    }
    if (result != ERROR_SUCCESS) {
        error_box(owner, "Cannot update startup settings. Check registry permissions and retry.");
        return FALSE;
    }
    return TRUE;
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

static void bitmap(HDC dc, int id, int x, int y, int width, int height)
{
    HBITMAP image = LoadBitmapA(instance, MAKEINTRESOURCEA(id));
    HDC memory;
    HGDIOBJ old;
    BITMAP info;
    if (!image) return;
    memory = CreateCompatibleDC(dc);
    old = SelectObject(memory, image);
    GetObjectA(image, sizeof(info), &info);
    StretchBlt(dc, x, y, width, height, memory, 0, 0, info.bmWidth, info.bmHeight, SRCCOPY);
    SelectObject(memory, old); DeleteDC(memory); DeleteObject(image);
}

static void rounded(HDC dc, RECT area, COLORREF color, COLORREF edge)
{
    HBRUSH brush = CreateSolidBrush(color);
    HPEN pen = CreatePen(PS_SOLID, 1, edge);
    HGDIOBJ old_brush = SelectObject(dc, brush), old_pen = SelectObject(dc, pen);
    RoundRect(dc, area.left, area.top, area.right, area.bottom, sx(5), sy(5));
    SelectObject(dc, old_brush); SelectObject(dc, old_pen);
    DeleteObject(brush); DeleteObject(pen);
}

static void paint_welcome(HWND hwnd)
{
    PAINTSTRUCT paint;
    RECT area, rectangle;
    HDC dc = BeginPaint(hwnd, &paint);
    int band, index, top;
    GetClientRect(hwnd, &area);
    fill(dc, area, RGB(90,126,220));
    bitmap(dc, 100, 0, sy(78), sx(219), sy(207));
    SetRect(&rectangle, 0, 0, area.right, sy(78));
    fill(dc, rectangle, RGB(0,48,156));
    for (band = 0; band < 128; ++band) {
        SetRect(&rectangle, area.right*band/128, sy(504), area.right*(band+1)/128, area.bottom);
        fill(dc, rectangle, RGB(57-band*57/127,52-band*4/127,173-band*17/127));
    }
    bitmap(dc, 125, 0, sy(78), area.right, sy(2));
    bitmap(dc, 126, 0, sy(504), area.right, sy(3));
    bitmap(dc, 124, sx(398), sy(121), sx(1) > 0 ? sx(1) : 1, sy(340));
    bitmap(dc, 123, sx(243), sy(232), sx(137), sy(86));
    /* Retain the supplied artwork, replacing only the XP suffix with 95. */
    SetRect(&rectangle, sx(358), sy(286), sx(384), sy(318));
    fill(dc, rectangle, RGB(90,126,220));
    SetBkMode(dc, TRANSPARENT); SetTextColor(dc, RGB(255,156,33));
    label(dc, body_font, "95", rectangle, DT_LEFT | DT_SINGLELINE);
    SetTextColor(dc, RGB(255,255,255));
    SetRect(&rectangle, sx(40), sy(338), sx(355), sy(389));
    label(dc, body_font, finishing ? "Starting your session..." : "To begin, enter your password", rectangle, DT_RIGHT | DT_WORDBREAK);
    for (index = profile_page * 4; index < profile_page * 4 + visible_profiles(); ++index) {
        top = profile_top(index);
        if (index == selected_profile) bitmap(dc, 112, sx(425), sy(top), sx(350), sy(72));
        SetRect(&rectangle, sx(433), sy(top+8), sx(488), sy(top+63));
        rounded(dc, rectangle, index == selected_profile ? RGB(255,235,94) : RGB(239,247,255), RGB(239,247,255));
        bitmap(dc, 114, sx(437), sy(top+12), sx(47), sy(47));
        SetRect(&rectangle, sx(499), sy(top+9), sx(765), sy(top+36));
        label(dc, index == selected_profile ? title_font : body_font,
            profiles_name(index), rectangle, DT_LEFT | DT_SINGLELINE);
        if (index == selected_profile) {
            SetRect(&rectangle, sx(499), sy(top+35), sx(765), sy(top+50));
            label(dc, small_font, preview ? "Preview" : (pending_profile ? "Switching user..." : (native_logon_attached() ? "Type your password." : "Preparing sign-in...")), rectangle, DT_LEFT | DT_SINGLELINE);
            SetRect(&rectangle, sx(499), sy(top+50), sx(670), sy(top+76));
            rounded(dc, rectangle, RGB(255,255,255), RGB(49,81,181));
        }
    }
    SetRect(&rectangle, sx(470), sy(527), sx(775), sy(570));
    label(dc, small_font, "After you log on, you can change your settings.\nOpen Welcome95 to customize this screen.", rectangle, DT_LEFT | DT_WORDBREAK);
    EndPaint(hwnd, &paint);
}

static void draw_button(DRAWITEMSTRUCT *item)
{
    RECT rectangle = item->rcItem;
    RECT stripe;
    int pixel, band;
    HDC dc = item->hDC;
    if (item->CtlID == ID_SUBMIT) {
        bitmap(dc, 103, 0, 0, rectangle.right, rectangle.bottom);
    } else if (item->CtlID == ID_POWER) {
        for (pixel = 0; pixel < rectangle.right; ++pixel) {
            band = (sx(25) + pixel) * 128 / GetSystemMetrics(SM_CXSCREEN);
            SetRect(&stripe, pixel, 0, pixel + 1, rectangle.bottom);
            fill(dc, stripe, RGB(57-band*57/127,52-band*4/127,173-band*17/127));
        }
        bitmap(dc, 107, 0, 0, sx(26), sy(26));
        rectangle.left = sx(34);
        SetBkMode(dc, TRANSPARENT); SetTextColor(dc, RGB(255,255,255));
        label(dc, body_font, "Turn off computer", rectangle, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
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
    if (screen) { ShowWindow(GetDlgItem(screen, ID_NATIVE), SW_HIDE); EnableWindow(GetDlgItem(screen, ID_SUBMIT), preview); }
}

static void allow_logon(void)
{
    HWND found = NULL;
    RECT area;
    int x, y, width;
    if (preview || finishing) return;
    if (login_window && IsWindow(login_window) && IsWindowVisible(login_window)) {
        if (native_logon_attached()) {
            login_geometry(&x, &y, &width);
            native_logon_layout(x, y, width);
            return;
        }
        if (prefer_native) return;
        /* Retry while the provider is still initializing its controls. */
    }
    if ((LONG)(GetTickCount() - next_logon_attempt) < 0) return;
    next_logon_attempt = GetTickCount() + 1000;
    release_logon();
    EnumWindows(find_logon, (LPARAM)&found);
    if (!found) return;
    login_window = found;
    login_was_topmost = (GetWindowLongA(found, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
    login_geometry(&x, &y, &width);
    if (!prefer_native && native_logon_attach(found, x, y, width)) {
        if (native_logon_user_empty() && !profiles_count()) {
            native_logon_restore(TRUE);
            ShowWindow(GetDlgItem(screen, ID_NATIVE), SW_SHOWNA);
            return;
        }
        if (!pending_profile) {
            if (native_logon_user_empty() && profiles_count()) {
                selected_profile = 0; pending_profile = TRUE;
            } else {
                selected_profile = profiles_remember(native_logon_name());
                if (selected_profile < 0) selected_profile = 0;
            }
            profile_page = selected_profile / 4;
        }
        ShowWindow(GetDlgItem(screen, ID_PREV), profiles_count()>4 ? SW_SHOWNA : SW_HIDE);
        ShowWindow(GetDlgItem(screen, ID_NEXT), profiles_count()>4 ? SW_SHOWNA : SW_HIDE);
        login_geometry(&x, &y, &width); native_logon_layout(x, y, width);
        position_arrow(screen);
        EnableWindow(GetDlgItem(screen, ID_SUBMIT), !pending_profile);
        InvalidateRect(screen, NULL, FALSE);
        return;
    }
    /* An unknown provider must keep its required controls accessible. */
    GetWindowRect(found, &area);
    x = GetSystemMetrics(SM_CXSCREEN) * 3 / 4 - (area.right - area.left) / 2;
    y = GetSystemMetrics(SM_CYSCREEN) / 2 - (area.bottom - area.top) / 2;
    if (x + area.right - area.left > GetSystemMetrics(SM_CXSCREEN))
        x = GetSystemMetrics(SM_CXSCREEN) - (area.right - area.left);
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    SetWindowPos(found, prefer_native ? HWND_TOPMOST : HWND_NOTOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOACTIVATE);
    ShowWindow(GetDlgItem(screen, ID_NATIVE), SW_SHOWNA);
    if (prefer_native) SetForegroundWindow(found);
    InvalidateRect(screen, NULL, FALSE);
}

static void maintain_topmost(HWND hwnd)
{
    HWND popup;
    UINT flags = SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER;
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, flags);
    if (!login_window || !IsWindow(login_window) || !IsWindowVisible(login_window) ||
        (!native_logon_attached() && !prefer_native)) return;
    /* Keep the actual password controls above our own canvas. Owned error
       messages are the sole intentional exception to the welcome's priority. */
    SetWindowPos(login_window, HWND_TOPMOST, 0, 0, 0, 0, flags);
    popup = GetLastActivePopup(login_window);
    if (popup != login_window && IsWindowVisible(popup))
        SetWindowPos(popup, HWND_TOPMOST, 0, 0, 0, 0, flags);
}

static LRESULT CALLBACK welcome_proc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp)
{
    int x, y, width, index;
    char requested_name[128];
    switch (message) {
    case WM_CREATE:
        SetTimer(hwnd, TICK, 100, NULL);
        escape_hotkey = RegisterHotKey(hwnd, 1, 0, VK_ESCAPE);
        close_hotkey = RegisterHotKey(hwnd, 2, MOD_ALT, VK_F4);
        CreateWindowA("BUTTON", "Trouble signing in?", WS_CHILD | BS_PUSHBUTTON,
            sx(625), sy(572), sx(150), sy(23), hwnd, (HMENU)ID_NATIVE, instance, NULL);
        SendDlgItemMessageA(hwnd, ID_NATIVE, WM_SETFONT, (WPARAM)small_font, TRUE);
        CreateWindowA("BUTTON", "Sign in", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            sx(685), sy(304), sx(26), sy(26), hwnd, (HMENU)ID_SUBMIT, instance, NULL);
        EnableWindow(GetDlgItem(hwnd, ID_SUBMIT), preview);
        CreateWindowA("BUTTON", "Turn off computer", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            sx(25), sy(526), sx(245), sy(29), hwnd, (HMENU)ID_POWER, instance, NULL);
        CreateWindowA("BUTTON", "<", WS_CHILD | (profiles_count()>4 ? WS_VISIBLE : 0), sx(720), sy(455), sx(25), sy(24), hwnd, (HMENU)ID_PREV, instance, NULL);
        CreateWindowA("BUTTON", ">", WS_CHILD | (profiles_count()>4 ? WS_VISIBLE : 0), sx(750), sy(455), sx(25), sy(24), hwnd, (HMENU)ID_NEXT, instance, NULL);
        position_arrow(hwnd);
        if (preview) {
            login_geometry(&x, &y, &width);
            CreateWindowA("EDIT", "", WS_CHILD | WS_DISABLED | ES_PASSWORD,
                x, y, width - sx(60), sy(22), hwnd, (HMENU)201, instance, NULL);
            /* A disabled preview edit never accepts a real password. */
            {
            HWND button = CreateWindowA("BUTTON", "Finish preview", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                sx(625), sy(572), sx(150), sy(23), hwnd, (HMENU)ID_DONE, instance, NULL);
            SendMessageA(button, WM_SETFONT, (WPARAM)small_font, TRUE);
            }
        }
        return 0;
    case WM_PAINT: paint_welcome(hwnd); return 0;
    case WM_DRAWITEM: draw_button((DRAWITEMSTRUCT *)lp); return TRUE;
    case WM_ERASEBKGND: return 1;
    case WM_CLOSE: return 0;
    case WM_HOTKEY: return 0;
    case WM_SYSCOMMAND:
        if ((wp & 0xfff0) == SC_CLOSE || (wp & 0xfff0) == SC_MINIMIZE) return 0;
        break;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (wp == VK_ESCAPE || wp == VK_F4) return 0;
        if (wp == VK_RETURN) { if (!pending_profile) native_logon_submit(); return 0; }
        break;
    case WM_LBUTTONDOWN:
        x = LOWORD(lp); y = HIWORD(lp);
        for (index = profile_page*4; index < profile_page*4 + visible_profiles(); ++index) {
            if (x >= sx(425) && x < sx(775) && y >= sy(profile_top(index)) && y < sy(profile_top(index)+64) &&
                index != selected_profile) {
                selected_profile = index; pending_profile = !preview; next_profile_attempt = 0;
                position_arrow(hwnd); EnableWindow(GetDlgItem(hwnd, ID_SUBMIT), preview);
                InvalidateRect(hwnd, NULL, FALSE); break;
            }
        }
        return 0;
    case WM_COMMAND:
        if (LOWORD(wp) == ID_PREV || LOWORD(wp) == ID_NEXT) {
            index = profile_page + (LOWORD(wp) == ID_NEXT ? 1 : -1);
            if (index >= 0 && index*4 < profiles_count()) {
                profile_page = index; selected_profile = index*4; pending_profile = !preview; next_profile_attempt = 0;
                position_arrow(hwnd); EnableWindow(GetDlgItem(hwnd, ID_SUBMIT), preview);
                InvalidateRect(hwnd, NULL, FALSE);
            }
        }
        if (LOWORD(wp) == ID_SUBMIT) {
            if (preview) { finishing = TRUE; finish_time = GetTickCount(); }
            else if (!pending_profile) native_logon_submit();
        }
        if (LOWORD(wp) == ID_POWER && !preview && MessageBoxA(hwnd, "Turn off this computer?", APP, MB_YESNO | MB_ICONQUESTION) == IDYES)
            ExitWindowsEx(EWX_SHUTDOWN, 0);
        if (preview && LOWORD(wp) == ID_DONE && !finishing) {
            finishing = TRUE; finish_time = GetTickCount(); InvalidateRect(hwnd, NULL, FALSE);
        }
        if (LOWORD(wp) == ID_NATIVE && login_window) {
            prefer_native = TRUE;
            native_logon_restore(TRUE);
            ShowWindow(GetDlgItem(hwnd, ID_NATIVE), SW_HIDE);
            SetForegroundWindow(login_window);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_TIMER:
        if (stop_event && WaitForSingleObject(stop_event, 0) == WAIT_OBJECT_0) {
            DestroyWindow(hwnd); return 0;
        }
        if (!finishing && ready_event && WaitForSingleObject(ready_event, 0) == WAIT_OBJECT_0) {
            finishing = TRUE; finish_time = GetTickCount(); InvalidateRect(hwnd, NULL, FALSE);
        }
        if (finishing && (DWORD)(GetTickCount() - finish_time) >= 1000) {
            DestroyWindow(hwnd); return 0;
        }
        if (servicing_logon) return 0;
        servicing_logon = TRUE;
        allow_logon();
        if (pending_profile && native_logon_attached() && (LONG)(GetTickCount()-next_profile_attempt) >= 0) {
            next_profile_attempt = GetTickCount() + 1000;
            index = selected_profile;
            lstrcpynA(requested_name, profiles_name(index), sizeof(requested_name));
            if (native_logon_set_user(requested_name) && selected_profile == index) {
                pending_profile = FALSE;
                login_geometry(&x, &y, &width);
                native_logon_layout(x, y, width);
                EnableWindow(GetDlgItem(hwnd, ID_SUBMIT), TRUE);
                InvalidateRect(hwnd, NULL, FALSE);
            }
        }
        maintain_topmost(hwnd);
        servicing_logon = FALSE;
        return 0;
    case WM_DISPLAYCHANGE:
        SetWindowPos(hwnd, NULL, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), SWP_NOZORDER | SWP_NOACTIVATE);
        login_geometry(&x, &y, &width);
        if (native_logon_attached() && !native_logon_layout(x, y, width)) {
            prefer_native = TRUE;
            native_logon_restore(TRUE);
            ShowWindow(GetDlgItem(hwnd, ID_NATIVE), SW_HIDE);
        }
        SetWindowPos(GetDlgItem(hwnd, ID_NATIVE), NULL, sx(625), sy(572), sx(150), sy(23), SWP_NOZORDER | SWP_NOACTIVATE);
        if (preview) {
            SetWindowPos(GetDlgItem(hwnd, 201), NULL, x, y, width - sx(60), sy(22), SWP_NOZORDER | SWP_NOACTIVATE);
            SetWindowPos(GetDlgItem(hwnd, ID_DONE), NULL, sx(625), sy(572), sx(150), sy(23), SWP_NOZORDER | SWP_NOACTIVATE);
        }
        position_arrow(hwnd);
        SetWindowPos(GetDlgItem(hwnd, ID_POWER), NULL, sx(25), sy(526), sx(245), sy(29), SWP_NOZORDER | SWP_NOACTIVATE);
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    case WM_DESTROY:
        if (escape_hotkey) UnregisterHotKey(hwnd, 1);
        if (close_hotkey) UnregisterHotKey(hwnd, 2);
        KillTimer(hwnd, TICK); release_logon(); PostQuitMessage(0); return 0;
    }
    return DefWindowProcA(hwnd, message, wp, lp);
}

static void update_settings(HWND hwnd)
{
    SetDlgItemTextA(hwnd, 200, enabled() ? "Status: enabled for the next Windows start" : "Status: disabled for this executable");
}

static void button(HWND hwnd, const char *text, int id, int x, int y, int width)
{
    HWND child = CreateWindowA("BUTTON", text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                              x, y, width, 30, hwnd, (HMENU)(UINT_PTR)id, instance, NULL);
    SendMessageA(child, WM_SETFONT, (WPARAM)small_font, TRUE);
}

static LRESULT CALLBACK settings_proc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp)
{
    HWND child;
    STARTUPINFOA startup;
    PROCESS_INFORMATION process;
    char command[MAX_PATH + 40];
    switch (message) {
    case WM_CREATE:
        child = CreateWindowA("STATIC", "Welcome95\r\nWindows XP-inspired welcome for Windows 95.\r\n\r\nKeep this EXE in a permanent local folder before enabling.\r\nWindows retains its own logon dialog and password handling.\r\nStartup timing requires testing on your Windows 95 installation.",
            WS_CHILD | WS_VISIBLE, 20, 20, 460, 115, hwnd, NULL, instance, NULL);
        SendMessageA(child, WM_SETFONT, (WPARAM)small_font, TRUE);
        child = CreateWindowA("STATIC", "", WS_CHILD | WS_VISIBLE, 20, 145, 460, 25,
                              hwnd, (HMENU)200, instance, NULL);
        SendMessageA(child, WM_SETFONT, (WPARAM)small_font, TRUE);
        button(hwnd, "Enable", ID_ENABLE, 20, 185, 100);
        button(hwnd, "Disable", ID_DISABLE, 130, 185, 100);
        button(hwnd, "Preview", ID_PREVIEW, 240, 185, 100);
        button(hwnd, "Close", ID_EXIT, 350, 185, 100);
        update_settings(hwnd); return 0;
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ID_ENABLE:
            if (configure(hwnd, TRUE)) MessageBoxA(hwnd, "Enabled. Restart Windows to test startup.\r\nKeep this executable at its current path.", APP, MB_OK | MB_ICONINFORMATION);
            update_settings(hwnd); break;
        case ID_DISABLE: configure(hwnd, FALSE); update_settings(hwnd); break;
        case ID_PREVIEW:
            zero_memory(&startup, sizeof(startup)); startup.cb = sizeof(startup);
            zero_memory(&process, sizeof(process));
            command_line(command, "/preview");
            if (!CreateProcessA(exe_path, command, NULL, NULL, FALSE, 0, NULL, NULL, &startup, &process))
                error_box(hwnd, "Cannot start preview.");
            else { CloseHandle(process.hThread); CloseHandle(process.hProcess); }
            break;
        case ID_EXIT: DestroyWindow(hwnd); break;
        }
        return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcA(hwnd, message, wp, lp);
}

int WINAPI WinMain(HINSTANCE current, HINSTANCE previous, LPSTR arguments, int show)
{
    WNDCLASSA cls;
    MSG message;
    HWND window;
    DWORD length, version;
    BOOL welcome;
    (void)previous; (void)show;
    instance = current;
    length = GetModuleFileNameA(NULL, exe_path, sizeof(exe_path));
    if (!length || length >= sizeof(exe_path) - 1) return 1;
    if (!lstrcmpiA(arguments, "/session-ready")) {
        ready_event = CreateEventA(NULL, TRUE, FALSE, READY_EVENT);
        if (!ready_event) return 1;
        SetEvent(ready_event);
        /* Preserve the signal if Run races the RunServices process. */
        Sleep(15000); CloseHandle(ready_event); return 0;
    }
    preview = !lstrcmpiA(arguments, "/preview");
    welcome = !lstrcmpiA(arguments, "/welcome");
    if (*arguments && !preview && !welcome) {
        error_box(NULL, "Unknown option. Run without arguments for settings."); return 1;
    }
    version = GetVersion();
    if (welcome && (!(version & 0x80000000UL) || LOBYTE(LOWORD(version)) != 4)) {
        error_box(NULL, "Automatic welcome mode supports Windows 95/98/Me only. Use Preview to inspect the screen."); return 1;
    }
    if (welcome || preview) {
        running_event = CreateEventA(NULL, TRUE, FALSE, INSTANCE_EVENT);
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
    profiles_load();
    title_font = CreateFontA(-sy(20), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, "Arial");
    body_font = CreateFontA(-sy(20), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, "Arial");
    small_font = CreateFontA(-sy(12), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, "Arial");
    zero_memory(&cls, sizeof(cls));
    cls.hInstance = instance; cls.hCursor = LoadCursor(NULL, IDC_ARROW);
    cls.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    cls.lpfnWndProc = welcome || preview ? welcome_proc : settings_proc;
    cls.lpszClassName = welcome || preview ? "Welcome95.Screen" : "Welcome95.Settings";
    if (!RegisterClassA(&cls)) return 1;
    if (welcome || preview) {
        window = CreateWindowExA(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, cls.lpszClassName, APP,
            WS_POPUP | WS_VISIBLE, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
            NULL, NULL, instance, NULL);
        screen = window;
    } else {
        window = CreateWindowA(cls.lpszClassName, "Welcome95 settings", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
            CW_USEDEFAULT, CW_USEDEFAULT, 500, 270, NULL, NULL, instance, NULL);
        ShowWindow(window, SW_SHOW);
    }
    if (!window) return 1;
    UpdateWindow(window);
    while (GetMessageA(&message, NULL, 0, 0) > 0) {
        if ((welcome || preview) || !IsDialogMessageA(window, &message)) {
            TranslateMessage(&message); DispatchMessageA(&message);
        }
    }
    if (ready_event) CloseHandle(ready_event);
    if (stop_event) CloseHandle(stop_event);
    if (running_event) CloseHandle(running_event);
    DeleteObject(title_font); DeleteObject(body_font); DeleteObject(small_font);
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
