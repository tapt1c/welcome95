#ifndef WELCOME95_PAINTED_BUTTON_H
#define WELCOME95_PAINTED_BUTTON_H
#include <windows.h>
/* As in Explorer95's LinkProc, retain native BUTTON input while bypassing
   old USER's erase-before-WM_DRAWITEM paint path. Only our own controls use it. */
typedef struct {
    WNDPROC previous;
    void (*draw)(const DRAWITEMSTRUCT *);
} PaintedButtonState;
static LRESULT CALLBACK painted_button_proc(HWND window,UINT message,WPARAM wp,LPARAM lp)
{
    PaintedButtonState *state=(PaintedButtonState *)GetWindowLongPtrA(window,GWLP_USERDATA);
    if(state && message==WM_SETCURSOR && LOWORD(lp)==HTCLIENT) {
        SetCursor(LoadCursor(NULL,IDC_ARROW)); return TRUE;
    }
    if(state && message==WM_ERASEBKGND) return TRUE;
    if(state && message==WM_PAINT) {
        PAINTSTRUCT paint; DRAWITEMSTRUCT item;
        HDC target,dc; HBITMAP frame=NULL; HGDIOBJ old=NULL;
        BYTE *bytes=(BYTE *)&item; UINT remaining=sizeof(item);
        while(remaining--) *bytes++=0;
        item.CtlType=ODT_BUTTON;
        item.CtlID=(UINT)GetDlgCtrlID(window); item.hwndItem=window; item.itemAction=ODA_DRAWENTIRE;
        GetClientRect(window,&item.rcItem); target=BeginPaint(window,&paint);
        dc=CreateCompatibleDC(target);
        if(dc) frame=CreateCompatibleBitmap(target,item.rcItem.right,item.rcItem.bottom);
        if(frame) {
            old=SelectObject(dc,frame);
            if(!old || old==HGDI_ERROR) { DeleteObject(frame); frame=NULL; }
        }
        if(frame) item.hDC=dc;
        else { if(dc) DeleteDC(dc); item.hDC=target; }
        if(GetFocus()==window) item.itemState|=ODS_FOCUS;
        if(!IsWindowEnabled(window)) item.itemState|=ODS_DISABLED;
        if(SendMessageA(window,BM_GETSTATE,0,0)&BST_PUSHED) item.itemState|=ODS_SELECTED;
        state->draw(&item);
        if(frame) {
            BitBlt(target,0,0,item.rcItem.right,item.rcItem.bottom,dc,0,0,SRCCOPY);
            SelectObject(dc,old); DeleteObject(frame); DeleteDC(dc);
        }
        EndPaint(window,&paint); return 0;
    }
    if(state) {
        LRESULT result=CallWindowProcA(state->previous,window,message,wp,lp);
        if(message==WM_NCDESTROY) {
            SetWindowLongPtrA(window,GWLP_USERDATA,0);
            HeapFree(GetProcessHeap(),0,state);
        }
        return result;
    }
    return DefWindowProcA(window,message,wp,lp);
}
static BOOL painted_button_install(HWND window,void (*draw)(const DRAWITEMSTRUCT *))
{
    PaintedButtonState *state;
    if(!window || GetWindowLongPtrA(window,GWLP_USERDATA)) return FALSE;
    state=(PaintedButtonState *)HeapAlloc(GetProcessHeap(),0,sizeof(*state));
    if(!state) return FALSE;
    state->previous=(WNDPROC)GetWindowLongPtrA(window,GWLP_WNDPROC); state->draw=draw;
    SetWindowLongPtrA(window,GWLP_USERDATA,(LONG_PTR)state);
    if(!SetWindowLongPtrA(window,GWLP_WNDPROC,(LONG_PTR)painted_button_proc)) {
        SetWindowLongPtrA(window,GWLP_USERDATA,0); HeapFree(GetProcessHeap(),0,state); return FALSE;
    }
    return TRUE;
}
#endif
