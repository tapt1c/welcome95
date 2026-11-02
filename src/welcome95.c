/* Welcome95: ANSI Win32 only, no Unicode/NT-only APIs. */
#include <windows.h>

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
#define TICK 1

static HINSTANCE instance;
static HWND screen;
static HFONT title_font, body_font, small_font;
static HANDLE ready_event, stop_event, running_event;
static BOOL preview, finishing;
static DWORD finish_time;
static HWND login_window;
static BOOL login_was_topmost;
static char exe_path[MAX_PATH];

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

static void flag(HDC dc, int x, int y)
{
    RECT rectangle;
    int row, column;
    COLORREF colors[4];
    colors[0] = RGB(235, 65, 40); colors[1] = RGB(60, 177, 72);
    colors[2] = RGB(48, 112, 218); colors[3] = RGB(250, 207, 42);
    SetRect(&rectangle, x - 3, y - 3, x + 61, y + 61);
    fill(dc, rectangle, RGB(24, 35, 67));
    for (row = 0; row < 2; ++row) {
        for (column = 0; column < 2; ++column) {
            SetRect(&rectangle, x + column * 31, y + row * 31,
                    x + column * 31 + 27, y + row * 31 + 27);
            fill(dc, rectangle, colors[row * 2 + column]);
        }
    }
    for (column = 0; column < 3; ++column) {
        for (row = 0; row < 4; ++row) {
            SetRect(&rectangle, x - 12 - column * 9, y + row * 15 + column * 2,
                    x - 6 - column * 9, y + row * 15 + 6 + column * 2);
            fill(dc, rectangle, RGB(24, 35, 67));
        }
    }
}

static void paint_welcome(HWND hwnd)
{
    PAINTSTRUCT paint;
    RECT area, rectangle;
    HDC dc = BeginPaint(hwnd, &paint);
    int width, height, top, bottom, middle, band;
    GetClientRect(hwnd, &area);
    width = area.right; height = area.bottom;
    top = height / 8; bottom = height * 7 / 8; middle = width / 2;
    fill(dc, area, RGB(87, 121, 209));
    for (band = 0; band < 64; ++band) {
        SetRect(&rectangle, width * band / 64, top, width * (band + 1) / 64, bottom);
        fill(dc, rectangle, RGB(122 - band * 35 / 63, 153 - band * 32 / 63, 225 - band * 16 / 63));
    }
    SetRect(&rectangle, 0, 0, width, top);
    fill(dc, rectangle, RGB(0, 48, 151));
    SetRect(&rectangle, 0, bottom, width, height);
    fill(dc, rectangle, RGB(38, 47, 158));
    SetRect(&rectangle, middle - 1, top + 28, middle, bottom - 28);
    fill(dc, rectangle, RGB(135, 166, 229));
    SetRect(&rectangle, 0, top, width, top + 2);
    fill(dc, rectangle, RGB(175, 195, 242));
    SetRect(&rectangle, 0, bottom, width, bottom + 2);
    fill(dc, rectangle, RGB(244, 174, 70));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    flag(dc, middle - 105, height / 2 - 150);
    SetRect(&rectangle, 20, height / 2 - 75, middle - 30, height / 2);
    label(dc, title_font, "Windows 95", rectangle, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    SetRect(&rectangle, 20, height / 2, middle - 30, height / 2 + 60);
    label(dc, body_font, "Welcome", rectangle, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    SetRect(&rectangle, middle + 30, top + 30, width - 25, top + 100);
    label(dc, body_font, finishing ? "Starting your session..." : "Sign in to Windows", rectangle, DT_LEFT | DT_WORDBREAK);
    SetRect(&rectangle, middle + 30, top + 105, width - 25, top + 175);
    label(dc, small_font, preview ? "Preview only. No startup settings changed." :
          "Use the standard Windows logon dialog.\nYour password stays with Windows.", rectangle, DT_LEFT | DT_WORDBREAK);
    SetRect(&rectangle, 25, bottom + 15, width - 25, height - 8);
    label(dc, small_font, preview ? "Welcome95 preview" : "Welcome95  |  Please wait for Windows logon", rectangle, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    EndPaint(hwnd, &paint);
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
    if (login_window && IsWindow(login_window) && !login_was_topmost)
        SetWindowPos(login_window, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    login_window = NULL;
}

static void allow_logon(void)
{
    HWND found = NULL;
    RECT area;
    int x, y;
    if (preview || finishing) return;
    if (login_window && IsWindow(login_window) && IsWindowVisible(login_window)) return;
    release_logon();
    EnumWindows(find_logon, (LPARAM)&found);
    if (!found) return;
    login_window = found;
    login_was_topmost = (GetWindowLongA(found, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
    GetWindowRect(found, &area);
    x = GetSystemMetrics(SM_CXSCREEN) * 3 / 4 - (area.right - area.left) / 2;
    y = GetSystemMetrics(SM_CYSCREEN) / 2 - (area.bottom - area.top) / 2;
    if (x + area.right - area.left > GetSystemMetrics(SM_CXSCREEN))
        x = GetSystemMetrics(SM_CXSCREEN) - (area.right - area.left);
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    SetWindowPos(found, HWND_TOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
    SetForegroundWindow(found);
}

static LRESULT CALLBACK welcome_proc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp)
{
    switch (message) {
    case WM_CREATE:
        SetTimer(hwnd, TICK, 100, NULL);
        if (preview) {
            HWND button = CreateWindowA("BUTTON", "Finish preview", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                GetSystemMetrics(SM_CXSCREEN) / 2 + 30, GetSystemMetrics(SM_CYSCREEN) / 2 + 50,
                150, 30, hwnd, (HMENU)ID_DONE, instance, NULL);
            SendMessageA(button, WM_SETFONT, (WPARAM)small_font, TRUE);
        }
        return 0;
    case WM_PAINT: paint_welcome(hwnd); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_CLOSE: return 0;
    case WM_SYSCOMMAND:
        if ((wp & 0xfff0) == SC_CLOSE || (wp & 0xfff0) == SC_MINIMIZE) return 0;
        break;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (wp == VK_ESCAPE || wp == VK_F4) return 0;
        break;
    case WM_COMMAND:
        if (preview && LOWORD(wp) == ID_DONE && !finishing) {
            finishing = TRUE; finish_time = GetTickCount(); InvalidateRect(hwnd, NULL, FALSE);
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
        allow_logon();
        return 0;
    case WM_DISPLAYCHANGE:
        SetWindowPos(hwnd, NULL, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), SWP_NOZORDER | SWP_NOACTIVATE);
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    case WM_DESTROY:
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
    title_font = CreateFontA(-42, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, "Arial");
    body_font = CreateFontA(-24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, "Arial");
    small_font = CreateFontA(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
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
