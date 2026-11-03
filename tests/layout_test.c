/* Fixed-pixel layout and blank-password handoff to an inert native dialog. */
#include "../src/welcome95.c"
static HANDLE output;
static int failures,submits;
static void check(BOOL okay,const char *description)
{
    DWORD written;
    const char *prefix=okay ? "PASS: " : "FAIL: ";
    if(!okay) ++failures;
    WriteFile(output,prefix,(DWORD)lstrlenA(prefix),&written,NULL);
    WriteFile(output,description,(DWORD)lstrlenA(description),&written,NULL); WriteFile(output,"\r\n",2,&written,NULL);
}
static INT_PTR CALLBACK inert_logon(HWND hwnd,UINT message,WPARAM wp,LPARAM lp)
{
    (void)hwnd; (void)lp;
    if(message==WM_COMMAND && LOWORD(wp)==IDOK) { ++submits; return TRUE; }
    return FALSE;
}
static void pump(void)
{
    MSG message;
    while(PeekMessageA(&message,NULL,0,0,PM_REMOVE)) { TranslateMessage(&message); DispatchMessageA(&message); }
}
static void dimensions(HWND child,int width,int height,const char *description)
{
    RECT rect;
    check(GetWindowRect(child,&rect) && rect.right-rect.left==width && rect.bottom-rect.top==height,description);
}
void layout_test_entry(void)
{
    WNDCLASSA cls;
    DWORD words[32];
    DLGTEMPLATE *form=(DLGTEMPLATE *)words;
    HWND edit,username,submit,dialog,scroll;
    RECT rect;
    HDC dc;
    HGDIOBJ old;
    TEXTMETRICA metrics;
    int x,y,width,w=GetSystemMetrics(SM_CXSCREEN),h=GetSystemMetrics(SM_CYSCREEN),i;
    char text[64];
    instance=GetModuleHandleA(NULL);
    output=CreateFileA("layout-test.log",GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(output==INVALID_HANDLE_VALUE) ExitProcess(2);
    bundled_fonts_load(instance); create_ui_fonts();
    check(cx(398)==w/2-2 && cy(291)==h/2-9,"central composition moves by half the change in resolution");
    check(by(504)==h-96 && by(526)==h-74 && rx(775)==w-25,"footer keeps a fixed height and edge offsets");
    check(cx(380)-cx(243)==137 && cy(318)-cy(232)==86,"logo keeps its 137 by 86 pixel rectangle");
    dc=GetDC(NULL); old=SelectObject(dc,body_font); GetTextMetricsA(dc,&metrics);
    check(metrics.tmHeight-metrics.tmInternalLeading==20,"body font stays 20 pixels high across screen resolutions");
    SelectObject(dc,welcome_font); GetTextMetricsA(dc,&metrics);
    check(metrics.tmHeight-metrics.tmInternalLeading==font_pixels(36),"welcome font keeps the original 36 point size");
    SelectObject(dc,old); ReleaseDC(NULL,dc);
    profiles_remember("Daniel"); selected_profile=0; profile_page=0;
    preview=TRUE; monitor_active=TRUE;
    zero_memory(&cls,sizeof(cls)); cls.hInstance=instance; cls.lpfnWndProc=welcome_proc; cls.lpszClassName="Welcome95.LayoutTest";
    RegisterClassA(&cls);
    screen=CreateWindowA(cls.lpszClassName,"Welcome95 layout fixture",WS_POPUP|WS_VISIBLE,0,0,w,h,NULL,NULL,instance,NULL);
    KillTimer(screen,TICK);
    edit=GetDlgItem(screen,201);
    dimensions(GetDlgItem(screen,ID_SUBMIT),26,26,"sign-in arrow is always 26 by 26 pixels");
    dimensions(edit,167,22,"preview password field is always 167 by 22 pixels");
    dimensions(GetDlgItem(screen,ID_POWER),245,29,"power button keeps its pixel dimensions");
    GetWindowRect(GetDlgItem(screen,ID_POWER),&rect);
    check(rect.left==25 && rect.top==h-74,"power button stays anchored at the lower left");
    login_geometry(&x,&y,&width); GetWindowRect(edit,&rect);
    check(rect.left==x && rect.top==y && width==227,"drawn and native fields use the same center anchor");
    SendMessageA(screen,WM_DISPLAYCHANGE,0,0);
    dimensions(edit,167,22,"display-change handler preserves control sizes");
    try_single_account_logon();
    check(!automatic_attempted,"preview cannot attempt automatic authentication");
    zero_memory(words,sizeof(words));
    form->style=WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME;
    form->cx=250; form->cy=110;
    dialog=CreateDialogIndirectParamA(instance,form,NULL,inert_logon,0);
    username=CreateWindowA("EDIT","DANIEL",WS_CHILD|WS_VISIBLE,20,20,150,22,dialog,(HMENU)21,instance,NULL);
    edit=CreateWindowA("EDIT","synthetic-only",WS_CHILD|WS_VISIBLE|ES_PASSWORD,20,50,150,22,dialog,(HMENU)23,instance,NULL);
    submit=CreateWindowA("BUTTON","OK",WS_CHILD|WS_VISIBLE,180,20,60,22,dialog,(HMENU)IDOK,instance,NULL);
    CreateWindowA("BUTTON","Cancel",WS_CHILD|WS_VISIBLE,180,50,60,22,dialog,(HMENU)IDCANCEL,instance,NULL);
    ShowWindow(dialog,SW_SHOWNA);
    check(native_logon_attach(dialog,x,y,width),"attach inert native dialog for automatic-login tests");
    dimensions(edit,167,22,"native password field matches the fixed preview dimensions");
    preview=FALSE; status_view=VIEW_LOGIN; pending_profile=TRUE;
    try_single_account_logon(); pump();
    check(!automatic_attempted && !submits,"automatic login waits for pending account selection");
    pending_profile=FALSE; EnableWindow(submit,FALSE); try_single_account_logon(); pump();
    check(automatic_attempted && !submits,"a provider that disables blank OK stays manual");
    SetWindowTextA(edit,"synthetic-later-input"); EnableWindow(submit,TRUE);
    try_single_account_logon(); pump(); GetWindowTextA(edit,text,sizeof(text));
    check(!submits && !lstrcmpA(text,"synthetic-later-input"),"later provider readiness cannot erase newly typed manual input");
    automatic_attempted=FALSE; /* A fresh-start scenario for the remaining inert cases. */
    prefer_native=TRUE; try_single_account_logon(); pump();
    check(!automatic_attempted && !submits,"explicit native-dialog fallback disables automatic login");
    prefer_native=FALSE;
    SendMessageA(edit,EM_SETMODIFY,TRUE,0); try_single_account_logon(); pump();
    GetWindowTextA(edit,text,sizeof(text));
    check(automatic_attempted && !submits && !lstrcmpA(text,"synthetic-later-input"),
        "an already edited native field is preserved without reading its text in production");
    SendMessageA(edit,EM_SETMODIFY,FALSE,0); automatic_attempted=FALSE;
    for(i=0;i<5;++i) { try_single_account_logon(); pump(); }
    check(automatic_attempted && submits==1,"one account submits exactly one native authentication command");
    GetWindowTextA(username,text,sizeof(text));
    check(!lstrcmpA(text,"Daniel"),"automatic login selects the canonical account spelling");
    GetWindowTextA(edit,text,sizeof(text));
    check(!text[0],"the inert provider receives blank input, never a stored password");
    SetWindowTextA(edit,"synthetic-retry"); try_single_account_logon(); pump(); GetWindowTextA(edit,text,sizeof(text));
    check(submits==1 && !lstrcmpA(text,"synthetic-retry"),"failure leaves manual input intact without automatic retries");
    profiles_remember("Second"); automatic_attempted=FALSE;
    try_single_account_logon(); pump();
    check(!automatic_attempted && submits==1,"multiple accounts never trigger automatic login");
    selected_profile=0; pending_profile=FALSE; compact_selected=FALSE; position_arrow(screen);
    check(account_open()==!compact_layout() && IsWindowVisible(GetDlgItem(screen,ID_SUBMIT))==!compact_layout(),
        "compact multi-account list starts without an expanded password card");
    SendMessageA(screen,WM_LBUTTONDOWN,0,MAKELPARAM(card_x(440),row_y(profile_top(0)+10)));
    check(account_open() && selected_profile==0 && (!compact_layout() || pending_profile),
        "clicking the first account expands it even when its index was already selected");
    selected_profile=0; pending_profile=FALSE;
    SendMessageA(screen,WM_LBUTTONDOWN,0,MAKELPARAM(card_x(440),row_y(profile_top(1)+10)));
    check(selected_profile==1 && pending_profile,"card hit testing follows the same fixed-pixel center anchor");
    scroll=GetDlgItem(screen,ID_PROFILE_SCROLL); position_profile_navigation(screen);
    GetWindowRect(scroll,&rect);
    check(!compact_layout() || (rect.left==compact_x(495) && rect.top==compact_y(208) && rect.right-rect.left==17 && rect.bottom-rect.top==176),
        "compact scrollbar occupies the reference's vertical list edge");
    check(IsWindowVisible(scroll)==compact_layout(),"vertical scrollbar is visible only for a compact account list");
    check(!compact_layout() || (card_x(433)==compact_x(165) && loading_y(262)==compact_y(247)),
        "compact loading identity is below its greeting at the reference position");
    profiles_remember("Third"); profiles_remember("Fourth"); profiles_remember("Fifth");
    SendMessageA(screen,WM_VSCROLL,SB_BOTTOM,(LPARAM)scroll);
    check(selected_profile==4 && page_first()==4 && pending_profile,
        "scrolling to the bottom reaches the last account without synchronous provider calls");
    GetWindowTextA(username,text,sizeof(text));
    check(!lstrcmpA(text,"Daniel"),"scrolling queues the account without changing native input inside the handler");
    SendMessageA(screen,WM_VSCROLL,SB_TOP,(LPARAM)scroll);
    SendMessageA(screen,WM_LBUTTONDOWN,0,MAKELPARAM(card_x(440),row_y(profile_top(1)+10)));
    login_geometry(&x,&y,&width); GetWindowRect(GetDlgItem(screen,ID_SUBMIT),&rect);
    check(y+22<=by(504) && rect.bottom<=by(504),"the second visible account's field and arrow fit above the footer");
    set_status_view(screen,VIEW_WELCOME_MULTI);
    check(!IsWindowVisible(scroll),"loading status hides account scrolling");
    set_status_view(screen,VIEW_LOGIN);
    check(IsWindowVisible(scroll)==compact_layout(),"returning to login restores navigation for the current layout");
    login_window=dialog;
    set_status_view(screen,VIEW_SHUTDOWN);
    check(!(GetWindowLongA(dialog,GWL_EXSTYLE)&WS_EX_TOPMOST) && IsWindowVisible(dialog),
        "shutdown lowers native input below status without changing provider visibility");
    set_status_view(screen,VIEW_LOGIN); maintain_topmost(screen);
    check((GetWindowLongA(dialog,GWL_EXSTYLE)&WS_EX_TOPMOST) && native_logon_attached(),
        "cancelled shutdown restores the attached native field above welcome");
    native_logon_restore(FALSE); DestroyWindow(dialog); login_window=NULL; DestroyWindow(screen);
    release_ui_fonts(); CloseHandle(output); ExitProcess(failures ? 1 : 0);
}
