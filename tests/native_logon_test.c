/* Real Win32-window tests. Run in a test Windows desktop; not a Win95 boot test.
   All text is synthetic. No actual Windows logon or registry changes are used. */
#include <windows.h>
#include "../src/native_logon.h"

static HANDLE output;
static int failures;
static WNDPROC original_dialog_proc;
static WNDPROC system_dialog_proc;
static int layout_changes;
static LRESULT CALLBACK legacy_nonclient(HWND window,UINT message,WPARAM wp,LPARAM lp)
{
    if(message==WM_NCCALCSIZE) {
        RECT *client=wp ? &((NCCALCSIZE_PARAMS *)lp)->rgrc[0] : (RECT *)lp;
        client->left+=3; client->top+=22; client->right-=3; client->bottom-=3;
        if(client->bottom<client->top) client->bottom=client->top;
        return 0;
    }
    return CallWindowProcA(system_dialog_proc,window,message,wp,lp);
}
static void client_rect(HWND window,RECT *rectangle)
{
    GetClientRect(window,rectangle); MapWindowPoints(window,NULL,(POINT *)rectangle,2);
}
static LRESULT CALLBACK count_layout(HWND window,UINT message,WPARAM wp,LPARAM lp)
{
    if(message==WM_WINDOWPOSCHANGING || message==WM_STYLECHANGING) ++layout_changes;
    return CallWindowProcA(original_dialog_proc,window,message,wp,lp);
}

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
    HWND dialog, password, username, submit, cancel, hidden, extra, dormant[9];
    RECT original, rectangle, password_rectangle, live_password_rectangle;
    HRGN region, shape, expected_shape;
    LONG style;
    HFONT original_font, applied_font;
    LOGFONTA font_info;
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
    /* The target log retains a 3,22 client origin after removing the frame.
       Model that legacy behavior rather than Wine's modern frame recalculation. */
    system_dialog_proc=(WNDPROC)SetWindowLongPtrA(dialog,GWLP_WNDPROC,(LONG_PTR)legacy_nonclient);
    SetWindowPos(dialog,NULL,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
    username = CreateWindowA("EDIT", "Daniel", 0x50010480UL,
        20, 20, 150, 24, dialog, (HMENU)21, instance, NULL);
    password = CreateWindowExA(WS_EX_CLIENTEDGE,"EDIT", "synthetic-test-only", 0x500104a0UL,
        20, 55, 150, 24, dialog, (HMENU)23, instance, NULL);
    original_font=CreateFontA(-11,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,"Tahoma");
    SendMessageA(password,WM_SETFONT,(WPARAM)original_font,FALSE);
    submit = CreateWindowA("BUTTON", "OK", 0x50010000UL,
        190, 20, 70, 24, dialog, (HMENU)IDOK, instance, NULL);
    cancel = CreateWindowA("BUTTON", "Cancel", 0x50010000UL,
        190, 55, 70, 24, dialog, (HMENU)IDCANCEL, instance, NULL);
    hidden = CreateWindowA("STATIC", "Originally hidden", 0x48000000UL,
        20, 90, 120, 20, dialog, (HMENU)28, instance, NULL);
    /* Exact class/ID/style inventory from the user's Win95 diagnostics.
       Text and coordinates are synthetic; no password data is in the fixture. */
    dormant[0] = CreateWindowA("STATIC", "", 0x50000803UL, 0, 0, 8, 8, dialog, (HMENU)32, instance, NULL);
    dormant[1] = CreateWindowA("STATIC", "User", 0x50020000UL, 0, 20, 20, 20, dialog, (HMENU)20, instance, NULL);
    dormant[2] = CreateWindowA("STATIC", "Password", 0x50000000UL, 0, 55, 20, 20, dialog, (HMENU)22, instance, NULL);
    dormant[3] = CreateWindowA("STATIC", "Inactive", 0x48020000UL, 0, 80, 20, 20, dialog, (HMENU)24, instance, NULL);
    dormant[4] = CreateWindowA("EDIT", "inactive-domain", 0x48010480UL, 20, 80, 100, 20, dialog, (HMENU)25, instance, NULL);
    dormant[5] = CreateWindowA("BUTTON", "Inactive", 0x48010003UL, 20, 100, 100, 20, dialog, (HMENU)26, instance, NULL);
    dormant[6] = CreateWindowA("BUTTON", "Inactive", 0x48010003UL, 20, 120, 100, 20, dialog, (HMENU)30, instance, NULL);
    dormant[7] = CreateWindowA("STATIC", "Information", 0x50000000UL, 20, 145, 100, 20, dialog, (HMENU)27, instance, NULL);
    dormant[8] = CreateWindowA("EDIT", "inactive-extra", 0x48010880UL, 20, 165, 100, 20, dialog, (HMENU)29, instance, NULL);
    check(username && password && submit && cancel && hidden, "create native controls");
    for (index = 0; index < 9; ++index)
        if (!dormant[index]) { CloseHandle(output); ExitProcess(1); }
    ShowWindow(dialog, SW_SHOWNA);
    GetWindowRect(dialog, &original);
    GetWindowRect(password, &password_rectangle);
    original_font=(HFONT)SendMessageA(password,WM_GETFONT,0,0);
    style = GetWindowLongA(dialog, GWL_STYLE);
    SendMessageA(password,EM_SETMODIFY,FALSE,0); /* Known fixture default, not user input. */
    check(native_logon_probe(dialog),"single-user probing classifies the real controls without embedding them");
    GetWindowRect(dialog,&rectangle); GetWindowRect(password,&live_password_rectangle);
    check(EqualRect(&rectangle,&original) && EqualRect(&live_password_rectangle,&password_rectangle) &&
          IsWindowVisible(username) && IsWindowVisible(cancel) && IsWindowEnabled(cancel),
          "a quiet probe preserves original geometry, visibility and Cancel state");
    native_logon_restore(FALSE);
    SendMessageA(password,EM_SETMODIFY,TRUE,0);
    check(!native_logon_probe(dialog),"automatic probing rejects input already edited by the user");
    GetWindowTextA(password,text,sizeof(text));
    check(!lstrcmpA(text,"synthetic-test-only"),"a rejected quiet probe preserves the existing synthetic input");
    SendMessageA(password,EM_SETMODIFY,FALSE,0);
    check(native_logon_attach(dialog, 345, 250, 260), "attach the reported 14-control Win95 network logon template");
    check(native_logon_attached(), "attachment remains valid");
    applied_font=(HFONT)SendMessageA(password,WM_GETFONT,0,0);
    check(!applied_font && GetObjectA(GetStockObject(SYSTEM_FONT),sizeof(font_info),&font_info)==sizeof(font_info) &&
          font_info.lfHeight>=16 && font_info.lfHeight<=22,
          "the native password uses a larger font inside its unchanged field");
    check(GetParent(password) == dialog && GetParent(submit) == dialog,
          "password and OK keep their original native parent");
    check(!lstrcmpA(native_logon_name(), "Daniel"), "display the remembered username");
    GetWindowTextA(password, text, sizeof(text));
    check(!lstrcmpA(text, "synthetic-test-only"), "existing synthetic input is preserved");
    check(IsWindowVisible(password) && !IsWindowVisible(submit), "native password is visible and the original OK is hidden");
    check(!IsWindowVisible(username) && !IsWindowVisible(cancel), "original account edit and Cancel are hidden");
    check(!IsWindowEnabled(cancel), "hidden Cancel is disabled");
    check((GetWindowLongA(dialog, GWL_STYLE)&WS_CAPTION)==(style&WS_CAPTION),
          "the native frame style remains intact while its pixels are clipped");
    region = CreateRectRgn(0, 0, 0, 0);
    check(GetWindowRgn(dialog, region) != ERROR, "native dialog has an explicit clipping region");
    GetWindowRect(dialog,&rectangle); client_rect(password,&live_password_rectangle);
    OffsetRect(&live_password_rectangle,-rectangle.left,-rectangle.top);
    check(PtInRegion(region,live_password_rectangle.left+5,live_password_rectangle.top+5) &&
          !PtInRegion(region,10,10) && !PtInRegion(region,live_password_rectangle.right+2,live_password_rectangle.top+5),
          "only the actual password field is exposed; all native dialog chrome is clipped");
    check(native_logon_layout(320, 280, 280), "relayout after screen geometry changes");
    GetWindowRect(dialog, &rectangle);
    client_rect(password,&live_password_rectangle);
    check(live_password_rectangle.left==320 && live_password_rectangle.top==280 &&
          live_password_rectangle.right-live_password_rectangle.left==220 && live_password_rectangle.bottom-live_password_rectangle.top==22,
          "native controls follow requested welcome coordinates");
    check(rectangle.right-rectangle.left==original.right-original.left && rectangle.bottom-rectangle.top==original.bottom-original.top,
        "the provider keeps its full original window size rather than a zero-height legacy client");
    {
        POINT hit; hit.x=325; hit.y=285;
        check(WindowFromPoint(hit)==password,"the actual Edit receives hit tests with the reported Win95 3,22 nonclient offset");
    }

    /* A provider may refresh its controls without moving the outer dialog. */
    ShowWindow(password, SW_HIDE);
    SetWindowPos(password, NULL, 20, 55, 150, 24, SWP_NOZORDER | SWP_NOACTIVATE);
    check(native_logon_layout(320, 280, 280) && IsWindowVisible(password),
          "repair a provider-hidden password even when the outer layout is cached");
    client_rect(password,&rectangle);
    check(rectangle.left==320 && rectangle.top==280 && rectangle.right-rectangle.left==220 && rectangle.bottom-rectangle.top==22,
          "repair a provider-moved password without changing its parent");
    SetWindowRgn(dialog, NULL, FALSE);
    native_logon_layout(320,280,280);
    GetWindowRect(dialog,&rectangle); client_rect(password,&live_password_rectangle);
    OffsetRect(&live_password_rectangle,-rectangle.left,-rectangle.top);
    check(GetWindowRgn(dialog, region) != ERROR &&
          PtInRegion(region,live_password_rectangle.left+5,live_password_rectangle.top+5) && !PtInRegion(region,10,10),
          "rebuild a clipping region removed by a provider refresh");
    GetWindowTextA(password, text, sizeof(text));
    check(!lstrcmpA(text, "synthetic-test-only"), "repair preserves synthetic input instead of clearing it");
    EnableWindow(dialog, FALSE);
    EnableWindow(password, FALSE);
    check(native_logon_layout(320, 280, 280) && !IsWindowEnabled(dialog) && !IsWindowEnabled(password),
          "layout repair never enables input disabled by a native modal prompt");
    EnableWindow(password, TRUE);
    EnableWindow(dialog, TRUE);
    original_dialog_proc=(WNDPROC)SetWindowLongPtrA(dialog,GWLP_WNDPROC,(LONG_PTR)count_layout);
    layout_changes=0;
    for(index=0;index<100;++index) native_logon_layout(320,280,280);
    SetWindowLongPtrA(dialog,GWLP_WNDPROC,(LONG_PTR)original_dialog_proc);
    check(!layout_changes,"100 stable native-field checks do not mutate styles, move windows or cause flicker");

    check(native_logon_set_user("Alex"), "select another account through the native username field");
    GetWindowTextA(username, text, sizeof(text));
    check(!lstrcmpA(text, "Alex"), "Windows receives the selected account name");
    GetWindowTextA(password, text, sizeof(text));
    check(!text[0], "switching accounts clears the previous synthetic password");
    native_logon_restore(FALSE);
    check((HFONT)SendMessageA(password,WM_GETFONT,0,0)==original_font,
          "native fallback restores the provider's original password font");
    GetWindowRect(dialog, &rectangle);
    check(EqualRect(&rectangle, &original), "restore the original dialog rectangle");
    check((GetWindowLongA(dialog, GWL_STYLE) & WS_CAPTION) == (style & WS_CAPTION), "restore native chrome");
    GetWindowRect(password, &rectangle);
    check(EqualRect(&rectangle, &password_rectangle), "restore the original password rectangle");
    check(IsWindowVisible(username) && IsWindowVisible(cancel) && IsWindowEnabled(cancel), "restore account and Cancel state");
    check(!IsWindowVisible(hidden), "preserve an originally hidden control");
    check(!IsWindowVisible(dormant[4]) && !IsWindowEnabled(dormant[4]) &&
          !IsWindowVisible(dormant[8]) && !IsWindowEnabled(dormant[8]),
          "dormant Edit fields retain their hidden disabled state");
    check(!IsWindowVisible(dormant[5]) && !IsWindowEnabled(dormant[5]) &&
          !IsWindowVisible(dormant[6]) && !IsWindowEnabled(dormant[6]),
          "dormant checkbox options retain their hidden disabled state");
    ShowWindow(dormant[4], SW_SHOWNA);
    check(!native_logon_attach(dialog, 345, 250, 260),
          "a visible additional Edit still rejects integration even when disabled");
    ShowWindow(dormant[4], SW_HIDE);
    EnableWindow(dormant[4], TRUE);
    check(!native_logon_attach(dialog, 345, 250, 260),
          "a hidden but enabled additional Edit still rejects integration");
    EnableWindow(dormant[4], FALSE);
    check(GetWindowRgn(dialog, region) == ERROR, "remove our clipping when original dialog had none");
    check(!native_logon_attach(dialog, 320, 200, 100), "reject an unusably narrow layout");
    check(IsWindowVisible(username) && (GetWindowLongA(dialog, GWL_STYLE) & WS_CAPTION),
          "failed layout rolls back to accessible native controls");
    SetWindowTextA(username, "");
    check(native_logon_attach(dialog, 345, 250, 260), "attach when the initial native username is blank");
    check(native_logon_set_user("Daniel"), "select a known account after blank-name startup");
    native_logon_restore(FALSE);
    SetWindowTextA(username, "Daniel");
    extra = CreateWindowA("COMBOBOX", "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
        20, 85, 120, 100, dialog, (HMENU)1004, instance, NULL);
    check(extra != NULL, "create an unsupported provider control");
    check(!native_logon_attach(dialog, 345, 250, 260) && IsWindowVisible(extra),
          "a provider's domain selector is never silently hidden");
    DestroyWindow(extra);
    shape=CreateRectRgn(0,0,180,200); expected_shape=CreateRectRgn(180,0,360,80);
    CombineRgn(expected_shape,shape,expected_shape,RGN_OR);
    CombineRgn(shape,expected_shape,NULL,RGN_COPY);
    SetWindowRgn(dialog,shape,TRUE); /* Windows owns shape now. */
    check(native_logon_attach(dialog,345,250,260),"attach while preserving an existing nonrectangular provider region");
    for(index=0;index<100;++index) native_logon_layout(345,250,260);
    native_logon_layout(320,280,280); native_logon_restore(FALSE);
    check(GetWindowRgn(dialog,region)==COMPLEXREGION && EqualRgn(region,expected_shape),
          "reusable region probes never overwrite the original provider-region backup");
    DeleteObject(expected_shape); SetWindowRgn(dialog,NULL,FALSE);
    check(native_logon_attach(dialog, 345, 250, 260), "reattach after fallback");
    ShowWindow(dialog, SW_HIDE);
    native_logon_restore(FALSE);
    check(!IsWindowVisible(dialog), "cleanup does not resurrect a dialog hidden by Windows");
    native_logon_restore(FALSE);
    for(index=0;index<9;++index) DestroyWindow(dormant[index]);
    DestroyWindow(hidden);
    SetWindowLongA(username,GWL_ID,30); SetWindowLongA(password,GWL_ID,31);
    CreateWindowA("STATIC","",WS_CHILD|WS_VISIBLE,0,0,8,8,dialog,(HMENU)32,instance,NULL);
    CreateWindowA("STATIC","User",WS_CHILD|WS_VISIBLE,0,20,20,20,dialog,(HMENU)200,instance,NULL);
    CreateWindowA("STATIC","Password",WS_CHILD|WS_VISIBLE,0,55,20,20,dialog,(HMENU)201,instance,NULL);
    CreateWindowA("STATIC","Information",WS_CHILD|WS_VISIBLE,0,80,20,20,dialog,(HMENU)202,instance,NULL);
    ShowWindow(dialog,SW_SHOWNA);
    check(native_logon_attach(dialog,345,250,227),"attach the second reported 8-control form with Edit IDs 30 and 31");
    {
        POINT hit; hit.x=350; hit.y=255;
        check(WindowFromPoint(hit)==password && GetParent(password)==dialog,
            "the second native form exposes its own password Edit at the same welcome anchor");
    }
    native_logon_restore(FALSE);
    DestroyWindow(dialog);
    DeleteObject(original_font);
    DeleteObject(region);
    CloseHandle(output);
    ExitProcess(failures ? 1 : 0);
}
