/* Never calls a real shell/PIF manager; all callbacks operate on private fixtures. */
#include <windows.h>
static HANDLE output;
static int failures;
static void check(BOOL okay,const char *description)
{
    DWORD written;
    const char *prefix=okay ? "PASS: " : "FAIL: ";
    if(!okay) ++failures;
    WriteFile(output,prefix,(DWORD)lstrlenA(prefix),&written,NULL);
    WriteFile(output,description,(DWORD)lstrlenA(description),&written,NULL); WriteFile(output,"\r\n",2,&written,NULL);
}
static void zero_bytes(void *memory,unsigned int size)
{
    BYTE *p=(BYTE *)memory;
    while(size--) *p++=0;
}
#include "power_dos_cases.h"
void dos_test_entry(void)
{
    char temporary[MAX_PATH],directory[MAX_PATH];
    output=CreateFileA("dos-test.log",GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(output==INVALID_HANDLE_VALUE) ExitProcess(2);
    if(!GetTempPathA(MAX_PATH,temporary) || !GetTempFileNameA(temporary,"W9T",0,directory) ||
        !DeleteFileA(directory) || !CreateDirectoryA(directory,NULL)) ExitProcess(2);
    ShutdownDosTests(directory);
    check(RemoveDirectoryA(directory),"remove the owned DOS test directory");
    CloseHandle(output); ExitProcess(failures ? 1 : 0);
}
