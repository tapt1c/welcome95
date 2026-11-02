/* Real Win32-window tests. Run in a test Windows desktop; not a Win95 boot test.
   All text is synthetic. No actual Windows logon or registry changes are used. */
#include <windows.h>
#include "../src/native_logon.h"

static HANDLE output;
static int failures;

static void check(BOOL condition, const char *description)
{
    DWORD written;
    const char *prefix = condition ? "PASS: " : "FAIL: ";
    if (!condition) ++failures;
    WriteFile(output, prefix, (DWORD)lstrlenA(prefix), &written, NULL);
    WriteFile(output, description, (DWORD)lstrlenA(description), &written, NULL);
    WriteFile(output, "\r\n", 2, &written, NULL);
}

static INT_PTR CALLBACK fixture_proc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp)
{
    (void)hwnd; (void)message; (void)wp; (void)lp;
    return FALSE;
}

void test_entry(void)
{
    DWORD template_words[32];
    DLGTEMPLATE *template;
    HWND dialog, password, username, submit, cancel, hidden, extra;
    RECT original, rectangle, password_rectangle;
    HRGN region;
    LONG style;
    int index;
    char text[64];
    HINSTANCE instance = GetModuleHandleA(NULL);
    output = CreateFileA("native-logon-test.log", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, NULL);
    if (output == INVALID_HANDLE_VALUE) ExitProcess(2);
    for (index = 0; index < 32; ++index) template_words[index] = 0;
    template = (DLGTEMPLATE *)template_words;
    template->style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME;
    template->x = 20; template->y = 20; template->cx = 250; template->cy = 110;
    dialog = CreateDialogIndirectParamA(instance, template, NULL, fixture_proc, 0);
    check(dialog != NULL, "create a native dialog fixture");
    if (!dialog) { CloseHandle(output); ExitProcess(1); }
    SetWindowTextA(dialog, "Synthetic logon fixture");
    username = CreateWindowA("EDIT", "Daniel", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP,
        20, 20, 150, 24, dialog, (HMENU)1001, instance, NULL);
    password = CreateWindowA("EDIT", "synthetic-test-only", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_PASSWORD,
        20, 55, 150, 24, dialog, (HMENU)1002, instance, NULL);
    submit = CreateWindowA("BUTTON", "OK", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        190, 20, 70, 24, dialog, (HMENU)IDOK, instance, NULL);
    cancel = CreateWindowA("BUTTON", "Cancel", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        190, 55, 70, 24, dialog, (HMENU)IDCANCEL, instance, NULL);
    hidden = CreateWindowA("STATIC", "Originally hidden", WS_CHILD,
        20, 90, 120, 20, dialog, (HMENU)1003, instance, NULL);
    check(username && password && submit && cancel && hidden, "create native controls");
    ShowWindow(dialog, SW_SHOWNA);
    GetWindowRect(dialog, &original);
    GetWindowRect(password, &password_rectangle);
    style = GetWindowLongA(dialog, GWL_STYLE);
    check(native_logon_attach(dialog, 345, 250, 260), "attach a remembered single-user dialog");
    check(native_logon_attached(), "attachment remains valid");
    check(GetParent(password) == dialog && GetParent(submit) == dialog,
          "password and OK keep their original native parent");
    check(!lstrcmpA(native_logon_name(), "Daniel"), "display the remembered username");
    GetWindowTextA(password, text, sizeof(text));
    check(!lstrcmpA(text, "synthetic-test-only"), "existing synthetic input is preserved");
    check(IsWindowVisible(password) && !IsWindowVisible(submit), "native password is visible and the original OK is hidden");
    check(!IsWindowVisible(username) && !IsWindowVisible(cancel), "original account edit and Cancel are hidden");
    check(!IsWindowEnabled(cancel), "hidden Cancel is disabled");
    check(!(GetWindowLongA(dialog, GWL_STYLE) & WS_CAPTION), "native title bar is removed");
    region = CreateRectRgn(0, 0, 0, 0);
    check(GetWindowRgn(dialog, region) != ERROR, "native dialog has an explicit clipping region");
    check(PtInRegion(region, 10, 10) && !PtInRegion(region, 250, 10) && !PtInRegion(region, 204, 10),
          "only the actual password field is exposed; all native dialog chrome is clipped");
    check(native_logon_layout(320, 280, 280), "relayout after screen geometry changes");
    GetWindowRect(dialog, &rectangle);
    check(rectangle.left == 320 && rectangle.top == 280 && rectangle.right - rectangle.left == 280,
          "native controls follow requested welcome coordinates");
    check(native_logon_set_user("Alex"), "select another account through the native username field");
    GetWindowTextA(username, text, sizeof(text));
    check(!lstrcmpA(text, "Alex"), "Windows receives the selected account name");
    GetWindowTextA(password, text, sizeof(text));
    check(!text[0], "switching accounts clears the previous synthetic password");
    native_logon_restore(FALSE);
    GetWindowRect(dialog, &rectangle);
    check(EqualRect(&rectangle, &original), "restore the original dialog rectangle");
    check((GetWindowLongA(dialog, GWL_STYLE) & WS_CAPTION) == (style & WS_CAPTION), "restore native chrome");
    GetWindowRect(password, &rectangle);
    check(EqualRect(&rectangle, &password_rectangle), "restore the original password rectangle");
    check(IsWindowVisible(username) && IsWindowVisible(cancel) && IsWindowEnabled(cancel), "restore account and Cancel state");
    check(!IsWindowVisible(hidden), "preserve an originally hidden control");
    check(GetWindowRgn(dialog, region) == ERROR, "remove our clipping when original dialog had none");
    check(!native_logon_attach(dialog, 320, 200, 100), "reject an unusably narrow layout");
    check(IsWindowVisible(username) && (GetWindowLongA(dialog, GWL_STYLE) & WS_CAPTION),
          "failed layout rolls back to accessible native controls");
    SetWindowTextA(username, "");
    check(!native_logon_attach(dialog, 345, 250, 260) && IsWindowVisible(username),
          "an empty account stays editable in the standard dialog");
    SetWindowTextA(username, "Daniel");
    extra = CreateWindowA("COMBOBOX", "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
        20, 85, 120, 100, dialog, (HMENU)1004, instance, NULL);
    check(extra != NULL, "create an unsupported provider control");
    check(!native_logon_attach(dialog, 345, 250, 260) && IsWindowVisible(extra),
          "a provider's domain selector is never silently hidden");
    DestroyWindow(extra);
    check(native_logon_attach(dialog, 345, 250, 260), "reattach after fallback");
    ShowWindow(dialog, SW_HIDE);
    native_logon_restore(FALSE);
    check(!IsWindowVisible(dialog), "cleanup does not resurrect a dialog hidden by Windows");
    native_logon_restore(FALSE);
    DestroyWindow(dialog);
    DeleteObject(region);
    CloseHandle(output);
    ExitProcess(failures ? 1 : 0);
}
