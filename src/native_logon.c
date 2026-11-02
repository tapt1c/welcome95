#include "native_logon.h"

#define MAX_CONTROLS 32

typedef struct {
    HWND window;
    RECT rectangle;
    BOOL visible;
    BOOL enabled;
    LONG style, exstyle;
} CONTROL_STATE;

static struct {
    HWND dialog, password, username, submit;
    DWORD process;
    LONG style, exstyle;
    RECT rectangle;
    HRGN region;
    BOOL had_region;
    BOOL visible;
    BOOL attached;
    BOOL unsupported;
    int count, edits, passwords;
    CONTROL_STATE children[MAX_CONTROLS];
    char name[128];
} saved;

static BOOL same_dialog(void)
{
    DWORD process = 0;
    if (!saved.dialog || !IsWindow(saved.dialog)) return FALSE;
    GetWindowThreadProcessId(saved.dialog, &process);
    return process == saved.process;
}

static BOOL set_style(HWND hwnd, int index, LONG value)
{
    SetLastError(0);
    return SetWindowLongA(hwnd, index, value) != 0 || GetLastError() == 0;
}

static BOOL CALLBACK capture_control(HWND child, LPARAM parameter)
{
    CONTROL_STATE *control;
    char name[32];
    LONG style, id;
    (void)parameter;
    if (GetParent(child) != saved.dialog) return TRUE;
    if (saved.count == MAX_CONTROLS) {
        saved.unsupported = TRUE;
        return FALSE;
    }
    control = &saved.children[saved.count++];
    control->window = child;
    control->visible = IsWindowVisible(child);
    control->enabled = IsWindowEnabled(child);
    control->style = GetWindowLongA(child, GWL_STYLE);
    control->exstyle = GetWindowLongA(child, GWL_EXSTYLE);
    if (!GetWindowRect(child, &control->rectangle)) {
        saved.unsupported = TRUE;
        return FALSE;
    }
    MapWindowPoints(NULL, saved.dialog, (POINT *)&control->rectangle, 2);
    GetClassNameA(child, name, sizeof(name));
    style = GetWindowLongA(child, GWL_STYLE);
    id = GetWindowLongA(child, GWL_ID);
    if (!lstrcmpiA(name, "Edit")) {
        ++saved.edits;
        if (style & ES_PASSWORD) {
            ++saved.passwords;
            saved.password = child;
        } else saved.username = child;
    } else if (!lstrcmpiA(name, "Button")) {
        if (id == IDOK || (style & 0x0f) == BS_DEFPUSHBUTTON) saved.submit = child;
        else if ((style & 0x0f) != BS_PUSHBUTTON && (style & 0x0f) != BS_GROUPBOX)
            saved.unsupported = TRUE;
    } else if (lstrcmpiA(name, "Static")) {
        /* Do not hide domain selectors, password-change fields or options. */
        saved.unsupported = TRUE;
    }
    return TRUE;
}

BOOL native_logon_attached(void)
{
    return saved.attached && same_dialog();
}

const char *native_logon_name(void)
{
    return saved.name[0] ? saved.name : "Windows user";
}

void native_logon_restore(BOOL reveal_original)
{
    int index;
    BOOL visible;
    if (saved.attached && same_dialog()) {
        visible = reveal_original ? saved.visible : IsWindowVisible(saved.dialog);
        ShowWindow(saved.dialog, SW_HIDE);
        /* Remove our clipping before restoring geometry and original children. */
        if (saved.had_region && saved.region) {
            if (SetWindowRgn(saved.dialog, saved.region, FALSE)) saved.region = NULL;
        } else SetWindowRgn(saved.dialog, NULL, FALSE);
        set_style(saved.dialog, GWL_STYLE, saved.style);
        set_style(saved.dialog, GWL_EXSTYLE, saved.exstyle);
        SetWindowPos(saved.dialog, saved.exstyle & WS_EX_TOPMOST ? HWND_TOPMOST : HWND_NOTOPMOST,
            saved.rectangle.left, saved.rectangle.top,
            saved.rectangle.right - saved.rectangle.left, saved.rectangle.bottom - saved.rectangle.top,
            SWP_FRAMECHANGED | SWP_NOACTIVATE);
        for (index = 0; index < saved.count; ++index) {
            CONTROL_STATE *control = &saved.children[index];
            if (!IsWindow(control->window) || GetParent(control->window) != saved.dialog) continue;
            set_style(control->window, GWL_STYLE, control->style);
            set_style(control->window, GWL_EXSTYLE, control->exstyle);
            SetWindowPos(control->window, NULL, control->rectangle.left, control->rectangle.top,
                control->rectangle.right - control->rectangle.left,
                control->rectangle.bottom - control->rectangle.top,
                SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
            EnableWindow(control->window, control->enabled);
            ShowWindow(control->window, control->visible ? SW_SHOWNA : SW_HIDE);
        }
        ShowWindow(saved.dialog, visible ? SW_SHOWNA : SW_HIDE);
    }
    if (saved.region) DeleteObject(saved.region);
    saved.region = NULL;
    saved.dialog = NULL;
    saved.attached = FALSE;
}

BOOL native_logon_layout(int x, int y, int width)
{
    HRGN region;
    RECT rectangle;
    POINT origin;
    int dx, dy, index, gap, height;
    if (!native_logon_attached() || width < 160) return FALSE;
    gap = MulDiv(60, GetSystemMetrics(SM_CXSCREEN), 800);
    height = MulDiv(22, GetSystemMetrics(SM_CYSCREEN), 600);
    set_style(saved.dialog, GWL_STYLE, GetWindowLongA(saved.dialog, GWL_STYLE) &
        ~(WS_CAPTION | WS_BORDER | WS_DLGFRAME | WS_THICKFRAME | WS_SYSMENU | DS_MODALFRAME));
    for (index = 0; index < saved.count; ++index) {
        HWND child = saved.children[index].window;
        if (child != saved.password) ShowWindow(child, SW_HIDE);
    }
    if (!SetWindowPos(saved.dialog, HWND_TOPMOST, x, y, width, 30,
                      SWP_FRAMECHANGED | SWP_NOACTIVATE)) return FALSE;
    if (!SetWindowPos(saved.password, NULL, 0, 0, width - gap, height,
                      SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED)) return FALSE;
    if (!SetWindowPos(saved.submit, NULL, width - 52, 0, 52, 30,
                      SWP_NOZORDER | SWP_NOACTIVATE)) return FALSE;
    origin.x = 0; origin.y = 0;
    if (!ClientToScreen(saved.dialog, &origin) || !GetWindowRect(saved.dialog, &rectangle)) return FALSE;
    dx = origin.x - rectangle.left; dy = origin.y - rectangle.top;
    region = CreateRectRgn(dx, dy, dx + width - gap, dy + height);
    if (!region) return FALSE;
    /* The system owns the region after a successful SetWindowRgn. Outside these
       password rectangle the native dialog is invisible AND does not receive clicks. */
    if (!SetWindowRgn(saved.dialog, region, TRUE)) {
        DeleteObject(region); return FALSE;
    }
    return TRUE;
}

void native_logon_focus(void)
{
    if (!native_logon_attached() || !IsWindowEnabled(saved.dialog)) return;
    SetForegroundWindow(saved.dialog);
    /* The native thread sets focus itself; no cross-thread SetFocus/hook needed. */
    PostMessageA(saved.dialog, WM_NEXTDLGCTL, (WPARAM)saved.password, TRUE);
}

void native_logon_submit(void)
{
    if (native_logon_attached() && IsWindowEnabled(saved.dialog) && IsWindowEnabled(saved.submit))
        PostMessageA(saved.dialog, WM_COMMAND, MAKEWPARAM(GetWindowLongA(saved.submit, GWL_ID), BN_CLICKED), (LPARAM)saved.submit);
}

BOOL native_logon_set_user(const char *name)
{
    DWORD_PTR result;
    if (!native_logon_attached() || !name || !*name || lstrlenA(name) >= (int)sizeof(saved.name) ||
        !IsWindowEnabled(saved.dialog)) return FALSE;
    /* Clear the previous account's input before changing the native username. */
    if (!SendMessageTimeoutA(saved.password, WM_SETTEXT, 0, (LPARAM)"",
        SMTO_ABORTIFHUNG | SMTO_BLOCK, 200, &result) || !result) return FALSE;
    if (!SendMessageTimeoutA(saved.username, WM_SETTEXT, 0, (LPARAM)name,
        SMTO_ABORTIFHUNG | SMTO_BLOCK, 200, &result) || !result) return FALSE;
    lstrcpynA(saved.name, name, sizeof(saved.name));
    native_logon_focus();
    return TRUE;
}

BOOL native_logon_attach(HWND dialog, int x, int y, int width)
{
    int index;
    DWORD_PTR result;
    LONG style, exstyle;
    native_logon_restore(FALSE);
    saved.dialog = dialog;
    saved.count = 0; saved.edits = 0; saved.passwords = 0;
    saved.unsupported = FALSE; saved.name[0] = '\0';
    saved.password = NULL; saved.username = NULL; saved.submit = NULL;
    GetWindowThreadProcessId(dialog, &saved.process);
    saved.visible = IsWindowVisible(dialog);
    if (!saved.visible || !IsWindowEnabled(dialog) || !GetWindowRect(dialog, &saved.rectangle)) return FALSE;
    EnumChildWindows(dialog, capture_control, 0);
    if (saved.unsupported || saved.edits != 2 || saved.passwords != 1 ||
        !saved.username || !saved.submit || !IsWindowVisible(saved.password) ||
        !IsWindowVisible(saved.submit) || !IsWindowEnabled(saved.password) || !IsWindowEnabled(saved.submit))
        return FALSE;
    /* Only the non-password username field is read. A hung provider cannot block
       the welcome thread indefinitely. Password text never enters our process. */
    if (!SendMessageTimeoutA(saved.username, WM_GETTEXT, sizeof(saved.name), (LPARAM)saved.name,
                             SMTO_ABORTIFHUNG | SMTO_BLOCK, 200, &result)) saved.name[0] = '\0';
    saved.name[sizeof(saved.name) - 1] = '\0';
    /* With no remembered account the original username entry is still needed. */
    if (!saved.name[0]) return FALSE;
    saved.style = GetWindowLongA(dialog, GWL_STYLE);
    saved.exstyle = GetWindowLongA(dialog, GWL_EXSTYLE);
    saved.region = CreateRectRgn(0, 0, 0, 0);
    if (!saved.region) return FALSE;
    saved.had_region = GetWindowRgn(dialog, saved.region) != ERROR;
    saved.attached = TRUE;
    ShowWindow(dialog, SW_HIDE);
    style = saved.style & ~(WS_CAPTION | WS_BORDER | WS_DLGFRAME | WS_THICKFRAME |
                             WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | DS_MODALFRAME);
    exstyle = saved.exstyle & ~(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_WINDOWEDGE | WS_EX_STATICEDGE);
    if (!set_style(dialog, GWL_STYLE, style) || !set_style(dialog, GWL_EXSTYLE, exstyle)) {
        native_logon_restore(TRUE); return FALSE;
    }
    for (index = 0; index < saved.count; ++index) {
        HWND child = saved.children[index].window;
        if (child == saved.password) continue;
        ShowWindow(child, SW_HIDE);
        if (GetWindowLongA(child, GWL_ID) == IDCANCEL) EnableWindow(child, FALSE);
    }
    set_style(saved.password, GWL_STYLE, GetWindowLongA(saved.password, GWL_STYLE) & ~WS_BORDER);
    set_style(saved.password, GWL_EXSTYLE, GetWindowLongA(saved.password, GWL_EXSTYLE) & ~WS_EX_CLIENTEDGE);
    if (!native_logon_layout(x, y, width)) {
        native_logon_restore(TRUE); return FALSE;
    }
    ShowWindow(dialog, SW_SHOWNA);
    native_logon_focus();
    return TRUE;
}
