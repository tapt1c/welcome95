#ifndef WELCOME95_EARLY_COVER_H
#define WELCOME95_EARLY_COVER_H
#include <windows.h>
static void early_cover_fill(HDC dc,RECT rect,COLORREF color)
{
    HBRUSH brush=CreateSolidBrush(color); FillRect(dc,&rect,brush); DeleteObject(brush);
}
/* Shared base geometry/colors for the early cover, wait cover and full canvas. */
static void early_cover_background(HDC dc,const RECT *area)
{
    RECT rect; int band;
    early_cover_fill(dc,*area,RGB(90,126,220));
    SetRect(&rect,0,0,area->right,78); early_cover_fill(dc,rect,RGB(0,48,156));
    for(band=0;band<128;++band) {
        SetRect(&rect,area->right*band/128,area->bottom-96,area->right*(band+1)/128,area->bottom);
        early_cover_fill(dc,rect,RGB(57-band*57/127,52-band*4/127,173-band*17/127));
    }
}
static void early_cover_render(HWND window,HDC target,void (*details)(HDC))
{
    RECT area; HDC dc,memory;
    HBITMAP frame; HGDIOBJ old=NULL;
    GetClientRect(window,&area);
    memory=CreateCompatibleDC(target);
    frame=memory ? CreateCompatibleBitmap(target,area.right,area.bottom) : NULL;
    dc=target;
    if(frame) { old=SelectObject(memory,frame); dc=memory; }
    else if(memory) DeleteDC(memory);
    early_cover_background(dc,&area);
    if(details) details(dc);
    if(frame) {
        BitBlt(target,0,0,area.right,area.bottom,memory,0,0,SRCCOPY);
        SelectObject(memory,old); DeleteObject(frame); DeleteDC(memory);
    }
}
static void early_cover_paint(HWND window,void (*details)(HDC))
{
    PAINTSTRUCT paint;
    HDC target=BeginPaint(window,&paint);
    early_cover_render(window,target,details);
    EndPaint(window,&paint);
}
#endif
