/* Russian UI text is CP1251; Wine-only wide drawing measures its real glyphs. */
#include "../src/welcome95.c"
static HANDLE output;
static int failures;
static int measured_width,measured_height;
static char fit_diagnostic[180];
static void check(BOOL okay,const char *description)
{
    DWORD written; const char *prefix=okay ? "PASS: " : "FAIL: ";
    if(!okay) ++failures;
    WriteFile(output,prefix,(DWORD)lstrlenA(prefix),&written,NULL);
    WriteFile(output,description,(DWORD)lstrlenA(description),&written,NULL);
    WriteFile(output,"\r\n",2,&written,NULL);
}
static BOOL same_text(HWND window,const char *expected)
{
    char text[1024]; GetWindowTextA(window,text,sizeof(text)); return !lstrcmpA(text,expected);
}
static BOOL fits(HFONT font,const char *text,int width,int height,BOOL wrap)
{
    WCHAR wide[1024]; RECT area; HDC dc=GetDC(NULL); HGDIOBJ old;
    int length=MultiByteToWideChar(1251,0,text,-1,wide,1024);
    BOOL okay;
    if(!dc || !length) { if(dc) ReleaseDC(NULL,dc); return FALSE; }
    old=SelectObject(dc,font ? font : GetStockObject(SYSTEM_FONT));
    SetRect(&area,0,0,width,height);
    DrawTextW(dc,wide,-1,&area,DT_CALCRECT|DT_NOPREFIX|(wrap ? DT_WORDBREAK : DT_SINGLELINE));
    measured_width=area.right; measured_height=area.bottom;
    okay=area.right<=width && area.bottom<=height;
    SelectObject(dc,old); ReleaseDC(NULL,dc); return okay;
}
static BOOL controls_fit(HWND window)
{
    HWND child=GetWindow(window,GW_CHILD); char text[1024],name[24]; RECT area;
    while(child) {
        int available_width,available_height;
        GetWindowTextA(child,text,sizeof(text)); GetClientRect(child,&area);
        GetClassNameA(child,name,sizeof(name));
        available_width=area.right-8; available_height=area.bottom;
        if(!lstrcmpiA(name,"BUTTON") && (GetWindowLongA(child,GWL_STYLE)&0x0f)==BS_OWNERDRAW) {
            available_width=area.right; available_height=area.bottom-47;
        }
        if(text[0] && !fits((HFONT)SendMessageA(child,WM_GETFONT,0,0),text,available_width,available_height,!lstrcmpiA(name,"STATIC"))) {
            wsprintfA(fit_diagnostic,"Russian power choices fit (id=%d needed=%dx%d available=%dx%d)",
                GetDlgCtrlID(child),measured_width,measured_height,available_width,available_height);
            return FALSE;
        }
        child=GetWindow(child,GW_HWNDNEXT);
    }
    return TRUE;
}
void ui_test_entry(void)
{
    WNDCLASSA cls; HWND settings,dialog; int width=GetSystemMetrics(SM_CXSCREEN),height=GetSystemMetrics(SM_CYSCREEN);
    instance=GetModuleHandleA(NULL);
    output=CreateFileA("ui-test.log",GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(output==INVALID_HANDLE_VALUE) ExitProcess(2);
    bundled_fonts_load(instance); create_ui_fonts();
    check(russian_ui(),"the Russian UI test selects CP1251 translations independently of Wine's locale");
    zero_memory(&cls,sizeof(cls)); cls.hInstance=instance; cls.lpfnWndProc=settings_proc; cls.lpszClassName="Welcome95.UiSettings";
    RegisterClassA(&cls);
    settings=CreateWindowA(cls.lpszClassName,"Settings fixture",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_VISIBLE,0,0,500,315,NULL,NULL,instance,NULL);
    check(same_text(GetDlgItem(settings,ID_ENABLE),"\302\352\353\376\367\350\362\374") && same_text(GetDlgItem(settings,ID_DISABLE),"\316\362\352\353\376\367\350\362\374"),
        "startup action buttons are fully Russian");
    check(same_text(GetDlgItem(settings,ID_PREVIEW),"\317\360\356\361\354\356\362\360") && same_text(GetDlgItem(settings,ID_EXIT),"\307\340\352\360\373\362\374"),
        "preview and close buttons are fully Russian");
    check(same_text(GetDlgItem(settings,ID_PREVIEW_SINGLE),"\317\360\350\342\345\362\361\362\342\350\345 (1)") &&
        same_text(GetDlgItem(settings,ID_PREVIEW_MULTI),"\317\360\350\342\345\362\361\362\342\350\345 (2+)") && same_text(GetDlgItem(settings,ID_PREVIEW_SHUTDOWN),"\302\373\352\353\376\367\345\355\350\345"),
        "all status preview buttons are fully Russian");
    check(same_text(GetDlgItem(settings,200),"\321\356\361\362\356\377\355\350\345: \356\362\352\353\376\367\345\355\356 \344\353\377 \375\362\356\351 \352\356\357\350\350 \357\360\356\343\360\340\354\354\373"),"startup status is fully Russian");
    check(controls_fit(settings),"Russian settings text fits the original controls and information panel");
    DestroyWindow(settings);
    monitor_active=TRUE; preview=TRUE; profiles_remember("Daniel");
    cls.lpfnWndProc=welcome_proc; cls.lpszClassName="Welcome95.UiLogin"; RegisterClassA(&cls);
    screen=CreateWindowExA(WS_EX_TOPMOST,cls.lpszClassName,"Russian preview",WS_POPUP|WS_VISIBLE|WS_CLIPCHILDREN,0,0,width,height,NULL,NULL,instance,NULL);
    KillTimer(screen,TICK);
    check(same_text(GetDlgItem(screen,ID_SUBMIT),"\302\356\351\362\350") && same_text(GetDlgItem(screen,ID_NATIVE),"\315\345 \363\344\340\270\362\361\377 \342\356\351\362\350?") &&
        same_text(GetDlgItem(screen,ID_DONE),"\307\340\352\360\373\362\374 \357\360\356\361\354\356\362\360"),"login, native fallback and finish-preview controls are fully Russian");
    check(same_text(GetDlgItem(screen,ID_POWER),"\302\373\352\353\376\367\350\362\374 \352\356\354\357\374\376\362\345\360") && fits(power_caption_font,"\302\373\352\353\376\367\350\362\374 \352\356\354\357\374\376\362\345\360",211,29,FALSE),
        "the Russian power caption fits its original button beside the icon");
    check(fits(body_font,"\304\353\377 \342\365\356\344\340 \342\373\341\345\360\350\362\345 \361\342\356\270 \350\354\377",315,26,FALSE) && fits(body_font,"\304\353\377 \342\365\356\344\340 \342\342\345\344\350\362\345 \357\340\360\356\353\374",315,26,FALSE),
        "both Russian sign-in prompts fit the compact single-line area");
    check(fits(small_font,"\317\356\361\353\345 \342\365\356\344\340 \354\356\346\355\356 \350\347\354\345\355\350\362\374 \355\340\361\362\360\356\351\352\350.\n\316\362\352\360\356\351\362\345 Welcome95 \344\353\377 \355\340\361\362\360\356\351\352\350 \375\352\360\340\355\340.",305,43,TRUE),
        "the Russian footer fits without extending into the password or screen edge");
    show_power_dialog(screen); dialog=FindWindowA("Welcome95.PowerChoice",NULL);
    check(dialog && same_text(dialog,"\302\373\352\353\376\367\350\362\374 \352\356\354\357\374\376\362\345\360") && same_text(GetDlgItem(dialog,IDCANCEL),"\316\362\354\345\355\340"),
        "the power dialog title and Cancel are Russian");
    check(same_text(GetDlgItem(dialog,POWER_STANDBY),"\306\344\363\371\350\351 \360\345\346\350\354") && same_text(GetDlgItem(dialog,POWER_OFF),"\302\373\352\353\376\367\345\355\350\345") &&
        same_text(GetDlgItem(dialog,POWER_RESTART),"\317\345\360\345\347\340\343\360\363\347\352\340"),"all power action labels are Russian");
    check(same_text(GetDlgItem(dialog,POWER_DOS),"\317\345\360\345\347\340\343\360\363\347\352\340 \342 MS-DOS"),"the MS-DOS option is Russian");
    { BOOL okay=controls_fit(dialog); check(okay,okay ? "Russian power choices fit their original compact controls" : fit_diagnostic); }
    power_dialog_close(); DestroyWindow(screen); release_ui_fonts(); CloseHandle(output); ExitProcess(failures ? 1 : 0);
}
