/* Adapted from tapt1c/explorer95/src/shutdown.c. No layered windows/alpha API. */
#include "power_dialog.h"
#include "painted_button.h"
#define FADE_STEPS 16
#define FADE_INTERVAL 35
static HWND shade, choice, previous_focus;
static PowerDialogHost context;
static BOOL owner_enabled, input_enabled, executing, closing;
static HBITMAP snapshot;
static BYTE *pixels;
static BITMAPINFO image;
static int screen_width,screen_height,step,choice_width,choice_height;
static DWORD fade_started;
static HFONT title_font,button_font;
static HBRUSH footer_brush;

static void clear_bytes(void *data,unsigned int size)
{
    BYTE *bytes=(BYTE *)data;
    while(size--) *bytes++=0;
}
static void color_rect(HDC dc,const RECT *rect,COLORREF color)
{
    HBRUSH brush=CreateSolidBrush(color); FillRect(dc,rect,brush); DeleteObject(brush);
}
static void paint_body(HDC dc,const RECT *rect,int origin)
{
    int band,light;
    RECT strip;
    for(band=0;band<32;++band) {
        light=band<16 ? band : 31-band;
        SetRect(&strip,choice_width*band/32-origin,rect->top,choice_width*(band+1)/32-origin,rect->bottom);
        color_rect(dc,&strip,RGB(91+light*3,132+light*3,210+light*2));
    }
}
static BOOL capture_screen(void)
{
    HDC desktop=GetDC(NULL),memory;
    HGDIOBJ old;
    DWORD size;
    BOOL okay=FALSE;
    memory=desktop ? CreateCompatibleDC(desktop) : NULL;
    if(!desktop || !memory || screen_width<=0 || screen_height<=0 || screen_width>0x1000000/screen_height) goto done;
    size=(DWORD)screen_width*screen_height*4;
    pixels=(BYTE *)HeapAlloc(GetProcessHeap(),0,size);
    snapshot=CreateCompatibleBitmap(desktop,screen_width,screen_height);
    if(!pixels || !snapshot) goto done;
    old=SelectObject(memory,snapshot);
    okay=BitBlt(memory,0,0,screen_width,screen_height,desktop,0,0,SRCCOPY);
    SelectObject(memory,old);
    clear_bytes(&image,sizeof(image)); image.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    image.bmiHeader.biWidth=screen_width; image.bmiHeader.biHeight=screen_height;
    image.bmiHeader.biPlanes=1; image.bmiHeader.biBitCount=32; image.bmiHeader.biCompression=BI_RGB;
    if(okay) okay=GetDIBits(desktop,snapshot,0,screen_height,pixels,&image,DIB_RGB_COLORS)==screen_height;
done:
    if(memory) DeleteDC(memory);
    if(desktop) ReleaseDC(NULL,desktop);
    if(!okay) {
        if(snapshot) DeleteObject(snapshot);
        snapshot=NULL;
        if(pixels) HeapFree(GetProcessHeap(),0,pixels);
        pixels=NULL;
    }
    return okay;
}
static void fade(void)
{
    DWORD index,count,elapsed;
    UINT brightness=65536,color=65536,gray_weight;
    int target,advance;
    HDC dc;
    if(!pixels || step>=FADE_STEPS) return;
    elapsed=GetTickCount()-fade_started;
    target=elapsed>=FADE_STEPS*FADE_INTERVAL ? FADE_STEPS : (int)(elapsed/FADE_INTERVAL);
    if(target<=step) return;
    /* Skip obsolete frames on slow Win9x drivers. Apply the combined transform
       once per pixel, keeping the fade bounded by elapsed time, not timer count. */
    for(advance=step;advance<target;++advance) {
        brightness=brightness*15U/16U; color=color*105U/128U;
    }
    gray_weight=brightness-color;
    count=(DWORD)screen_width*screen_height;
    for(index=0;index<count;++index) {
        BYTE *p=pixels+index*4;
        UINT gray=(p[0]+2U*p[1]+p[2])>>2;
        p[0]=(BYTE)((p[0]*color+gray*gray_weight)>>16);
        p[1]=(BYTE)((p[1]*color+gray*gray_weight)>>16);
        p[2]=(BYTE)((p[2]*color+gray*gray_weight)>>16);
    }
    dc=GetDC(shade); SetDIBits(dc,snapshot,0,screen_height,pixels,&image,DIB_RGB_COLORS); ReleaseDC(shade,dc);
    step=target; InvalidateRect(shade,NULL,FALSE);
    if(step==FADE_STEPS) { HeapFree(GetProcessHeap(),0,pixels); pixels=NULL; SetTimer(choice,1,100,NULL); }
}
BOOL power_dialog_active(void) { return shade!=NULL || choice!=NULL || executing || closing; }
void power_dialog_focus(void)
{
    if(!choice) return;
    SetWindowPos(choice,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE);
    SetWindowPos(shade,choice,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
    SetForegroundWindow(GetLastActivePopup(choice));
}
void power_dialog_close(void)
{
    HWND old;
    if(closing) return;
    closing=TRUE; old=choice; choice=NULL;
    if(old) KillTimer(old,1);
    /* Restore input while still covered. Keep the modal guard through teardown
       so a reentrant welcome timer cannot raise the canvas above the shade. */
    if(owner_enabled && IsWindow(context.owner)) EnableWindow(context.owner,TRUE);
    if(input_enabled && IsWindow(context.native_input)) EnableWindow(context.native_input,TRUE);
    owner_enabled=input_enabled=FALSE;
    if(old) DestroyWindow(old);
    if(shade) DestroyWindow(shade);
    shade=NULL;
    if(snapshot) DeleteObject(snapshot);
    snapshot=NULL;
    if(pixels) HeapFree(GetProcessHeap(),0,pixels);
    pixels=NULL;
    if(title_font) DeleteObject(title_font);
    title_font=NULL;
    if(button_font) DeleteObject(button_font);
    button_font=NULL;
    if(footer_brush) DeleteObject(footer_brush);
    footer_brush=NULL;
    if(old && IsWindow(context.owner))
        RedrawWindow(context.owner,NULL,NULL,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_UPDATENOW);
    if(old && previous_focus && IsWindowVisible(previous_focus) && IsWindowEnabled(previous_focus)) SetForegroundWindow(previous_focus);
    if(old && context.restore_focus) context.restore_focus();
    closing=FALSE;
}
static LRESULT CALLBACK shade_proc(HWND window,UINT message,WPARAM wp,LPARAM lp)
{
    if(message==WM_SETCURSOR && LOWORD(lp)==HTCLIENT) { SetCursor(LoadCursor(NULL,IDC_ARROW)); return TRUE; }
    if(message==WM_ERASEBKGND) return TRUE;
    if(message==WM_PAINT) {
        PAINTSTRUCT paint;
        HDC dc=BeginPaint(window,&paint),memory=CreateCompatibleDC(dc);
        if(snapshot && memory) {
            HGDIOBJ old=SelectObject(memory,snapshot);
            BitBlt(dc,0,0,screen_width,screen_height,memory,0,0,SRCCOPY);
            SelectObject(memory,old);
        } else FillRect(dc,&paint.rcPaint,(HBRUSH)(COLOR_DESKTOP+1));
        if(memory) DeleteDC(memory);
        EndPaint(window,&paint); return 0;
    }
    if(message==WM_LBUTTONDOWN || message==WM_RBUTTONDOWN || message==WM_MBUTTONDOWN || message==WM_SETFOCUS) { power_dialog_focus(); return 0; }
    if(message==WM_CLOSE) { power_dialog_close(); return 0; }
    return DefWindowProcA(window,message,wp,lp);
}
static void paint_flag(HDC dc)
{
    HRSRC resource=FindResourceA(context.instance,MAKEINTRESOURCEA(143),MAKEINTRESOURCEA(2));
    HGLOBAL memory=resource ? LoadResource(context.instance,resource) : NULL;
    const BITMAPINFO *info=memory ? (const BITMAPINFO *)LockResource(memory) : NULL;
    const BYTE *bits;
    HBRUSH brushes[16];
    int x,y,index,stride;
    RECT pixel;
    if(!info || info->bmiHeader.biBitCount!=4 || info->bmiHeader.biWidth!=16 || info->bmiHeader.biHeight!=16) return;
    bits=(const BYTE *)info+info->bmiHeader.biSize+16*sizeof(RGBQUAD); stride=8;
    for(index=0;index<16;++index) brushes[index]=CreateSolidBrush(RGB(info->bmiColors[index].rgbRed,info->bmiColors[index].rgbGreen,info->bmiColors[index].rgbBlue));
    for(y=0;y<16;++y) for(x=0;x<16;++x) {
        BYTE value=bits[(15-y)*stride+x/2];
        index=(x&1) ? value&15 : value>>4;
        if(info->bmiColors[index].rgbRed==192 && info->bmiColors[index].rgbGreen==192 && info->bmiColors[index].rgbBlue==192) continue;
        SetRect(&pixel,choice_width-40+x*2,5+y*2,choice_width-38+x*2,7+y*2);
        FillRect(dc,&pixel,brushes[index]);
    }
    for(index=0;index<16;++index) DeleteObject(brushes[index]);
}
static void draw_action(const DRAWITEMSTRUCT *item)
{
    RECT rect=item->rcItem,tile,text,bounds;
    HDC dc=item->hDC;
    POINT position;
    BOOL enabled=!(item->itemState&ODS_DISABLED),down=(item->itemState&ODS_SELECTED)!=0;
    int x=(rect.right-32)/2+(down ? 1 : 0),y=5+(down ? 1 : 0),i;
    HPEN pen;
    HGDIOBJ old_pen,old_brush,old_font;
    char label[64];
    COLORREF color=!enabled ? RGB(135,135,135) : item->CtlID==POWER_STANDBY ? RGB(223,161,20) : item->CtlID==POWER_OFF ? RGB(206,68,38) : RGB(52,147,54);
    static const int rays[8][4]={{16,5,16,10},{24,8,21,11},{27,16,22,16},{24,24,21,21},{16,27,16,22},{8,24,11,21},{5,16,10,16},{8,8,11,11}};
    GetWindowRect(item->hwndItem,&bounds); position.x=bounds.left; position.y=bounds.top; ScreenToClient(choice,&position);
    paint_body(dc,&rect,position.x);
    SetRect(&tile,x,y,x+32,y+32); color_rect(dc,&tile,color);
    DrawEdge(dc,&tile,down ? EDGE_SUNKEN : EDGE_RAISED,BF_RECT);
    pen=CreatePen(PS_SOLID,2,RGB(255,255,255)); old_pen=SelectObject(dc,pen); old_brush=SelectObject(dc,GetStockObject(NULL_BRUSH));
    if(item->CtlID==POWER_STANDBY || item->CtlID==POWER_OFF) {
        Arc(dc,x+7,y+8,x+25,y+26,x+12,y+7,x+20,y+7);
        MoveToEx(dc,x+16,y+5,NULL); LineTo(dc,x+16,y+17);
    } else for(i=0;i<8;++i) { MoveToEx(dc,x+rays[i][0],y+rays[i][1],NULL); LineTo(dc,x+rays[i][2],y+rays[i][3]); }
    SelectObject(dc,old_brush); SelectObject(dc,old_pen); DeleteObject(pen);
    GetWindowTextA(item->hwndItem,label,sizeof(label)); old_font=SelectObject(dc,button_font);
    SetBkMode(dc,TRANSPARENT); SetTextColor(dc,enabled ? RGB(255,255,255) : RGB(220,220,220));
    SetRect(&text,0,44+(down ? 1 : 0),rect.right,rect.bottom-3); DrawTextA(dc,label,-1,&text,DT_CENTER|DT_SINGLELINE|DT_NOPREFIX);
    if(item->itemState&ODS_FOCUS) { InflateRect(&rect,-3,-3); DrawFocusRect(dc,&rect); }
    SelectObject(dc,old_font);
}
static void perform_action(int id)
{
    void (*perform)(PowerAction,BOOL)=context.perform;
    BOOL preview=context.preview;
    BOOL dos=SendDlgItemMessageA(choice,POWER_DOS,BM_GETCHECK,0,0)==BST_CHECKED;
    if(dos && id!=POWER_RESTART) return;
    if(!IsWindowEnabled(GetDlgItem(choice,id))) return;
    executing=TRUE;
    power_dialog_close();
    /* Restore input before the OS can show native save/cancellation prompts. */
    if(!preview && perform) perform((PowerAction)id,dos);
    executing=FALSE;
}
static LRESULT CALLBACK choice_proc(HWND window,UINT message,WPARAM wp,LPARAM lp)
{
    switch(message) {
    case WM_SETCURSOR:
        if(LOWORD(lp)==HTCLIENT) { SetCursor(LoadCursor(NULL,IDC_ARROW)); return TRUE; }
        break;
    case WM_ERASEBKGND: return TRUE;
    case WM_PAINT: {
        PAINTSTRUCT paint; RECT rect,body,title;
        HDC target=BeginPaint(window,&paint),dc,memory;
        HBITMAP frame; HGDIOBJ old,old_frame=NULL;
        GetClientRect(window,&rect); memory=CreateCompatibleDC(target);
        frame=memory ? CreateCompatibleBitmap(target,rect.right,rect.bottom) : NULL;
        dc=target;
        if(frame) { old_frame=SelectObject(memory,frame); dc=memory; }
        else if(memory) DeleteDC(memory);
        color_rect(dc,&rect,RGB(0,45,140));
        SetRect(&body,0,43,rect.right,choice_height-43); paint_body(dc,&body,0);
        SetRect(&title,12,0,rect.right-45,43); old=SelectObject(dc,title_font);
        SetBkMode(dc,TRANSPARENT); SetTextColor(dc,RGB(255,255,255));
        DrawTextA(dc,context.russian ? "\302\373\352\353\376\367\350\362\374\040\352\356\354\357\374\376\362\345\360" : "Turn off computer",-1,&title,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
        SelectObject(dc,old); paint_flag(dc);
        if(frame) {
            BitBlt(target,0,0,rect.right,rect.bottom,memory,0,0,SRCCOPY);
            SelectObject(memory,old_frame); DeleteObject(frame); DeleteDC(memory);
        }
        EndPaint(window,&paint); return 0;
    }
    case WM_DRAWITEM: draw_action((const DRAWITEMSTRUCT *)lp); return TRUE;
    case WM_CTLCOLORSTATIC: case WM_CTLCOLORBTN:
        if((HWND)lp==GetDlgItem(window,POWER_DOS)) {
            SetBkMode((HDC)wp,TRANSPARENT); SetBkColor((HDC)wp,RGB(0,45,140));
            SetTextColor((HDC)wp,IsWindowEnabled((HWND)lp) ? RGB(255,255,255) : RGB(180,180,180));
            return (LRESULT)footer_brush;
        }
        break;
    case WM_COMMAND:
        if(LOWORD(wp)==IDCANCEL) power_dialog_close();
        else if(LOWORD(wp)==POWER_DOS) {
            BOOL dos=SendDlgItemMessageA(window,POWER_DOS,BM_GETCHECK,0,0)==BST_CHECKED;
            EnableWindow(GetDlgItem(window,POWER_OFF),!dos);
            EnableWindow(GetDlgItem(window,POWER_STANDBY),!dos && context.standby_available);
        } else if(LOWORD(wp)>=POWER_STANDBY && LOWORD(wp)<=POWER_RESTART) perform_action(LOWORD(wp));
        return 0;
    case WM_TIMER: {
        HWND foreground,owner;
        if(closing) return 0;
        fade(); foreground=GetForegroundWindow(); owner=foreground;
        while(owner && owner!=window) owner=GetWindow(owner,GW_OWNER);
        if(IsWindowVisible(window) && !owner && !IsChild(window,foreground)) power_dialog_focus();
        return 0;
    }
    case WM_DISPLAYCHANGE: case WM_CLOSE: power_dialog_close(); return 0;
    }
    return DefWindowProcA(window,message,wp,lp);
}
BOOL power_dialog_show(const PowerDialogHost *host)
{
    WNDCLASSA cls;
    RECT client;
    int i,column;
    static BOOL registered;
    static const char *english[]={"Stand By","Turn Off","Restart"};
    static const char *russian[]={"\306\344\363\371\350\351\040\360\345\346\350\354","\302\373\352\353\376\367\345\355\350\345","\317\345\360\345\347\340\343\360\363\347\352\340"};
    if(power_dialog_active()) { power_dialog_focus(); return TRUE; }
    if(!host || !IsWindow(host->owner)) return FALSE;
    context=*host; previous_focus=GetForegroundWindow(); step=0;
    screen_width=GetSystemMetrics(SM_CXSCREEN); screen_height=GetSystemMetrics(SM_CYSCREEN);
    if(!registered) {
        clear_bytes(&cls,sizeof(cls)); cls.hInstance=host->instance; cls.hCursor=LoadCursor(NULL,IDC_ARROW);
        cls.lpfnWndProc=shade_proc; cls.lpszClassName="Welcome95.PowerShade";
        if(!RegisterClassA(&cls) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) return FALSE;
        cls.lpfnWndProc=choice_proc; cls.lpszClassName="Welcome95.PowerChoice";
        if(!RegisterClassA(&cls) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) return FALSE;
        registered=TRUE;
    }
    if(!capture_screen()) { power_dialog_close(); return FALSE; }
    shade=CreateWindowExA(WS_EX_TOPMOST|WS_EX_TOOLWINDOW,"Welcome95.PowerShade","",WS_POPUP,
        0,0,screen_width,screen_height,NULL,NULL,host->instance,NULL);
    choice=CreateWindowExA(WS_EX_TOPMOST,"Welcome95.PowerChoice",context.russian ? "\302\373\352\353\376\367\350\362\374\040\352\356\354\357\374\376\362\345\360" : "Turn off computer",
        WS_POPUP|WS_SYSMENU|WS_BORDER|WS_CLIPCHILDREN,(screen_width-314)/2,(screen_height-200)/3,314,200,shade,NULL,host->instance,NULL);
    if(!shade || !choice) { power_dialog_close(); return FALSE; }
    GetClientRect(choice,&client); choice_width=client.right; choice_height=client.bottom;
    title_font=CreateFontA(-16,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,"Tahoma");
    button_font=CreateFontA(host->russian ? -10 : -11,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,"Tahoma");
    footer_brush=CreateSolidBrush(RGB(0,45,140)); column=(choice_width-48)/3;
    for(i=0;i<3;++i) {
        HWND button=CreateWindowA("BUTTON",host->russian ? russian[i] : english[i],WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,
            24+i*column,73,column,64,choice,(HMENU)(UINT_PTR)(POWER_STANDBY+i),host->instance,NULL);
        if(!button) { power_dialog_close(); return FALSE; }
        painted_button_install(button,draw_action);
        SendMessageA(button,WM_SETFONT,(WPARAM)button_font,TRUE);
    }
    CreateWindowA("BUTTON",context.russian ? "\316\362\354\345\355\340" : "Cancel",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_DEFPUSHBUTTON,
        choice_width-69,choice_height-30,58,20,choice,(HMENU)IDCANCEL,host->instance,NULL);
    CreateWindowA("BUTTON",context.russian ? "\317\345\360\345\347\340\343\360\363\347\352\340\040\342\040\115\123\055\104\117\123" : "Restart in MS-DOS mode",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_AUTOCHECKBOX,
        12,choice_height-30,choice_width-90,20,choice,(HMENU)POWER_DOS,host->instance,NULL);
    if(!GetDlgItem(choice,IDCANCEL) || !GetDlgItem(choice,POWER_DOS)) { power_dialog_close(); return FALSE; }
    SendDlgItemMessageA(choice,IDCANCEL,WM_SETFONT,(WPARAM)button_font,TRUE);
    SendDlgItemMessageA(choice,POWER_DOS,WM_SETFONT,(WPARAM)button_font,TRUE);
    EnableWindow(GetDlgItem(choice,POWER_STANDBY),host->standby_available);
    EnableWindow(GetDlgItem(choice,POWER_DOS),host->dos_available);
    owner_enabled=IsWindowEnabled(host->owner); if(owner_enabled) EnableWindow(host->owner,FALSE);
    input_enabled=host->native_input && IsWindowEnabled(host->native_input);
    if(input_enabled) EnableWindow(host->native_input,FALSE);
    ShowWindow(shade,SW_SHOWNOACTIVATE); UpdateWindow(shade);
    ShowWindow(choice,SW_SHOW); UpdateWindow(choice); power_dialog_focus(); SetFocus(GetDlgItem(choice,IDCANCEL));
    fade_started=GetTickCount();
    SetTimer(choice,1,FADE_INTERVAL,NULL); return TRUE;
}
BOOL power_dialog_translate(MSG *message)
{
    if(!choice) return FALSE;
    if(message->hwnd==choice || IsChild(choice,message->hwnd)) {
        if(message->message==WM_KEYDOWN && message->wParam==VK_ESCAPE) { power_dialog_close(); return TRUE; }
        if(message->message==WM_KEYDOWN && message->wParam==VK_RETURN) {
            HWND focus=GetFocus(); int id=GetDlgCtrlID(focus);
            if(focus && IsChild(choice,focus) && IsWindowEnabled(focus)) {
                if(id==POWER_DOS) SendMessageA(focus,BM_CLICK,0,0);
                else if(id==IDCANCEL || (id>=POWER_STANDBY && id<=POWER_RESTART)) SendMessageA(choice,WM_COMMAND,id,0);
            }
            return TRUE;
        }
        return IsDialogMessageA(choice,message);
    }
    return FALSE;
}
