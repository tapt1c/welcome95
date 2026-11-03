/* Check actual GDI downscaling; no image processing substitutes for drawing. */
#include "../src/welcome95.c"
void render_test_entry(void)
{
    HRSRC resource;
    HGLOBAL data;
    const BITMAPINFOHEADER *info;
    const BYTE *pixels, *pixel;
    HDC device, dc;
    HBITMAP frame;
    HGDIOBJ old;
    HANDLE log;
    DWORD written;
    int x,y,c,stride,minimum[3]={255,255,255},maximum[3]={0,0,0},value;
    BOOL clean=TRUE, drew=TRUE, restored;
    COLORREF color;
    instance=GetModuleHandleA(NULL);
    log=CreateFileA("render-test.log",GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(log==INVALID_HANDLE_VALUE) ExitProcess(2);
    resource=FindResourceA(instance,MAKEINTRESOURCEA(100),MAKEINTRESOURCEA(2));
    data=resource ? LoadResource(instance,resource) : NULL;
    info=data ? (const BITMAPINFOHEADER *)LockResource(data) : NULL;
    if(!info || info->biBitCount!=24) ExitProcess(2);
    stride=(info->biWidth*3+3)&~3; pixels=(const BYTE *)info+info->biSize;
    for(y=0;y<info->biHeight;++y) for(x=0;x<info->biWidth;++x) {
        pixel=pixels+y*stride+x*3;
        for(c=0;c<3;++c) { if(pixel[c]<minimum[c])minimum[c]=pixel[c]; if(pixel[c]>maximum[c])maximum[c]=pixel[c]; }
    }
    device=GetDC(NULL); dc=CreateCompatibleDC(device); frame=CreateCompatibleBitmap(device,175,165);
    if(!dc || !frame || GetDeviceCaps(device,BITSPIXEL)*GetDeviceCaps(device,PLANES)<16) ExitProcess(2);
    old=SelectObject(dc,frame);
    { RECT area; SetRect(&area,0,0,175,165); fill(dc,area,RGB(255,0,255)); }
    SetStretchBltMode(dc,BLACKONWHITE);
    bitmap(dc,100,0,0,175,165);
    restored=GetStretchBltMode(dc)==BLACKONWHITE;
    for(y=0;y<165;y+=2) for(x=0;x<175;x+=2) {
        color=GetPixel(dc,x,y);
        if(color==CLR_INVALID || color==RGB(255,0,255)) drew=FALSE;
        for(c=0;c<3;++c) {
            value=c==0 ? GetBValue(color) : (c==1 ? GetGValue(color) : GetRValue(color));
            if(value<minimum[c]-8 || value>maximum[c]+8) clean=FALSE;
        }
    }
    {
        const char *a=drew ? "PASS: draw every sampled gradient pixel\r\n" : "FAIL: missing gradient pixels\r\n";
        const char *b=clean ? "PASS: shrinking the gradient introduces no out-of-range colored rings\r\n" : "FAIL: colored outliers after shrinking\r\n";
        const char *d=restored ? "PASS: bitmap drawing preserves the caller's stretch mode\r\n" : "FAIL: stretch mode leaked\r\n";
        WriteFile(log,a,(DWORD)lstrlenA(a),&written,NULL); WriteFile(log,b,(DWORD)lstrlenA(b),&written,NULL);
        WriteFile(log,d,(DWORD)lstrlenA(d),&written,NULL);
    }
    SelectObject(dc,old); DeleteObject(frame); DeleteDC(dc); ReleaseDC(NULL,device); CloseHandle(log);
    ExitProcess(drew && clean && restored ? 0 : 1);
}
