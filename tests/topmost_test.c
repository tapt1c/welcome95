/* Exercise the actual welcome priority function against real desktop windows.
   Including the implementation keeps production and tested behavior identical. */
#include "../src/welcome95.c"

static HANDLE test_output;
static int test_failures;

static void test_check(BOOL condition, const char *description)
{
    DWORD written;
    const char *prefix = condition ? "PASS: " : "FAIL: ";
    if (!condition) ++test_failures;
    WriteFile(test_output, prefix, (DWORD)lstrlenA(prefix), &written, NULL);
    WriteFile(test_output, description, (DWORD)lstrlenA(description), &written, NULL);
    WriteFile(test_output, "\r\n", 2, &written, NULL);
}

static BOOL above(HWND first, HWND second)
{
    HWND window = GetTopWindow(NULL);
    while (window) {
        if (window == first) return TRUE;
        if (window == second) return FALSE;
        window = GetWindow(window, GW_HWNDNEXT);
    }
    return FALSE;
}

void topmost_test_entry(void)
{
    HWND canvas, competitor, popup, foreground;
    HINSTANCE current = GetModuleHandleA(NULL);
    test_output = CreateFileA("topmost-test.log", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL, NULL);
    if (test_output == INVALID_HANDLE_VALUE) ExitProcess(2);
    canvas = CreateWindowExA(WS_EX_TOPMOST, "STATIC", "Test welcome canvas", WS_POPUP | WS_VISIBLE,
        0, 0, 500, 400, NULL, NULL, current, NULL);
    competitor = CreateWindowExA(WS_EX_TOPMOST, "STATIC", "Competing topmost window", WS_POPUP | WS_VISIBLE,
        10, 10, 200, 100, NULL, NULL, current, NULL);
    login_window = CreateWindowExA(WS_EX_TOPMOST, "STATIC", "Synthetic native controls", WS_POPUP | WS_VISIBLE,
        300, 250, 200, 30, NULL, NULL, current, NULL);
    test_check(canvas && competitor && login_window, "create competing desktop windows");
    if (!canvas || !competitor || !login_window) { CloseHandle(test_output); ExitProcess(1); }
    maintain_topmost(canvas);
    test_check(above(canvas, login_window), "unsupported native dialog stays covered until explicitly requested");
    prefer_native = TRUE;
    SetForegroundWindow(login_window);
    foreground = GetForegroundWindow();
    SetWindowPos(competitor, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    maintain_topmost(canvas);
    test_check(above(canvas, competitor), "welcome returns above another topmost window");
    test_check(above(login_window, canvas), "real password controls remain above the welcome");
    test_check(GetForegroundWindow() == foreground, "restoring priority does not steal keyboard focus");
    popup = CreateWindowExA(0, "STATIC", "Synthetic owned Windows error", WS_POPUP | WS_VISIBLE,
        200, 180, 250, 80, login_window, NULL, current, NULL);
    test_check(popup != NULL, "create an owned Windows-message fixture");
    SetForegroundWindow(popup);
    foreground = GetForegroundWindow();
    SetWindowPos(competitor, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    maintain_topmost(canvas);
    test_check(above(canvas, competitor) && above(popup, canvas), "Windows logon error stays accessible above the canvas");
    test_check(GetForegroundWindow() == foreground, "restoring priority preserves error-dialog focus");
    profiles_remember("First"); profiles_remember("Second");
    selected_profile = 0; profile_page = 0; pending_profile = FALSE; preview = FALSE;
    welcome_proc(canvas, WM_LBUTTONDOWN, 0, MAKELPARAM(sx(440), sy(profile_top(1)+10)));
    test_check(selected_profile == 1 && pending_profile,
        "account click queues selection even when native attachment is unavailable");
    DestroyWindow(popup);
    DestroyWindow(login_window); login_window = NULL;
    DestroyWindow(competitor);
    DestroyWindow(canvas);
    CloseHandle(test_output);
    ExitProcess(test_failures ? 1 : 0);
}
