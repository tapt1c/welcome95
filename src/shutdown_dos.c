#define WIN32_LEAN_AND_MEAN
#include "shutdown_dos.h"
#include <shellapi.h>
/* Adapted from tapt1c/explorer95, shutdown_dos.c; see docs/POWER_SOURCE.md. */
static void clear_bytes(void *data, unsigned int length)
{
    BYTE *bytes=(BYTE *)data;
    while(length--) *bytes++=0;
}

#define DOS_REALMODE 0x0008
#define DOS_CLOSE_DISCARD 1
static BOOL IsWindows95(DWORD version)
{
    return (version & 0x80000000UL) && LOBYTE(LOWORD(version)) == 4 && HIBYTE(LOWORD(version)) == 0;
}
static BOOL DosInterfaces(ShutdownDosHost *host)
{
    HMODULE shell = GetModuleHandleA("shell32.dll");
    union {
        FARPROC raw;
        int (WINAPI *open)(const char *,const char *,UINT,UINT);
        int (WINAPI *get)(int,const char *,void *,int,UINT);
        int (WINAPI *set)(int,const char *,const void *,int,UINT);
        int (WINAPI *close)(int,UINT);
    } address;
    address.raw=GetProcAddress(shell,MAKEINTRESOURCEA(9)); host->open=address.open;
    address.raw=GetProcAddress(shell,MAKEINTRESOURCEA(10)); host->get=address.get;
    address.raw=GetProcAddress(shell,MAKEINTRESOURCEA(11)); host->set=address.set;
    address.raw=GetProcAddress(shell,MAKEINTRESOURCEA(13)); host->close=address.close;
    return host->open && host->get && host->set && host->close;
}
BOOL ShutdownDosAvailable(void)
{
    ShutdownDosHost host;
    return IsWindows95(GetVersion()) && DosInterfaces(&host);
}
static DWORD LaunchPif(HWND owner, const char *path)
{
    SHELLEXECUTEINFOA execute; DWORD error;
    clear_bytes(&execute, sizeof(execute)); execute.cbSize = sizeof(execute);
    execute.fMask = SEE_MASK_FLAG_NO_UI; execute.hwnd = owner;
    execute.lpFile = path; execute.nShow = SW_SHOWNORMAL;
    SetLastError(ERROR_SUCCESS);
    if (ShellExecuteExA(&execute)) return ERROR_SUCCESS;
    error = GetLastError(); return error ? error : ERROR_GEN_FAILURE;
}
DWORD ShutdownDosRestart(HWND owner)
{
    ShutdownDosHost host; char windows[MAX_PATH], temporary[MAX_PATH]; UINT length;
    if (!IsWindows95(GetVersion()) || !DosInterfaces(&host)) return ERROR_NOT_SUPPORTED;
    length = GetWindowsDirectoryA(windows, MAX_PATH);
    if (!length || length >= MAX_PATH) return ERROR_PATH_NOT_FOUND;
    length = GetTempPathA(MAX_PATH, temporary);
    if (!length || length >= MAX_PATH) return ERROR_PATH_NOT_FOUND;
    host.launchPif = LaunchPif;
    return ShutdownDosPrepare(owner, GetVersion(), windows, temporary, &host);
}
DWORD ShutdownDosPrepare(HWND owner, DWORD version, const char *windowsDirectory,
                         const char *temporaryDirectory, const ShutdownDosHost *host)
{
    char command[MAX_PATH], directory[MAX_PATH], privateDirectory[MAX_PATH] = "", pif[MAX_PATH] = "";
    ShutdownDosProgram program, verified; int properties = 0, length;
    DWORD error = ERROR_SUCCESS, attributes, verifyIssues = 0; BOOL created = FALSE;
    if (!IsWindows95(version)) return ERROR_NOT_SUPPORTED;
    if (!host || !host->open || !host->get || !host->set || !host->close || !host->launchPif ||
        !windowsDirectory || !temporaryDirectory || !*windowsDirectory || !*temporaryDirectory) return ERROR_INVALID_PARAMETER;
    length = lstrlenA(windowsDirectory);
    if (length + 13 >= MAX_PATH) return ERROR_FILENAME_EXCED_RANGE;
    lstrcpyA(command, windowsDirectory);
    if (command[length-1] != '\\') command[length++] = '\\';
    lstrcpyA(command+length, "COMMAND.COM");
    attributes = GetFileAttributesA(command);
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY)) return ERROR_FILE_NOT_FOUND;
    length = GetShortPathNameA(command, directory, MAX_PATH);
    if (!length || length >= (int)sizeof(program.command)) return ERROR_FILENAME_EXCED_RANGE;
    lstrcpyA(command, directory);
    length = GetShortPathNameA(windowsDirectory, directory, MAX_PATH);
    if (!length || length >= (int)sizeof(program.directory)) return ERROR_FILENAME_EXCED_RANGE;
    /* Reserve a private directory, never overwrite the system's PIF or shell. */

    if (!GetTempFileNameA(temporaryDirectory, "W95", 0, privateDirectory)) { error = GetLastError(); goto done; }
    if (!DeleteFileA(privateDirectory)) { error = GetLastError(); goto done; }
    if (!CreateDirectoryA(privateDirectory, NULL)) { error = GetLastError(); goto done; }
    created = TRUE;
    length = GetShortPathNameA(privateDirectory, pif, MAX_PATH);
    if (!length || length + 13 >= MAX_PATH) { error = ERROR_FILENAME_EXCED_RANGE; pif[0] = 0; goto done; }
    lstrcatA(pif, "\\W95DOS.PIF");
    properties = host->open(command, pif, 0, 0);
    if (!properties) { error = ERROR_NOT_ENOUGH_MEMORY; goto done; }
    clear_bytes(&program, sizeof(program));
    if (host->get(properties, MAKEINTRESOURCEA(1), &program, sizeof(program), 0) != sizeof(program)) { error = ERROR_INVALID_DATA; goto done; }
    /* PRGINIT_REALMODE leaves Windows. Never inherit REALMODESILENT, a private
     * CONFIG/AUTOEXEC, or an arbitrary default command. Keep native prompts. */
    program.flags = 1; program.initial = DOS_REALMODE;
    lstrcpyA(program.title, "Welcome95 MS-DOS");
    lstrcpyA(program.command, command); lstrcpyA(program.directory, directory);
    program.realMode = 0; program.other[0] = 0; lstrcpyA(program.pif, pif);

    if (host->set(properties, MAKEINTRESOURCEA(1), &program, sizeof(program), 0) != sizeof(program)) { error = ERROR_WRITE_FAULT; goto done; }

    if (host->close(properties, 0)) { error = ERROR_WRITE_FAULT; goto done; }
    properties = 0;
    /* Reopen the saved file and verify real mode before dispatching it. */
    attributes = GetFileAttributesA(pif);
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY)) { error = ERROR_WRITE_FAULT; goto done; }
    /* pszPIF is an OUTPUT filename: repeating the creation call would replace
     * our saved settings with COMMAND.COM defaults. Read the PIF as the source
     * instead, without an output filename or automatic INF processing. */
    properties = host->open(pif, NULL, (UINT)-1, 0);
    if (!properties) { error = ERROR_INVALID_DATA; goto done; }
    clear_bytes(&verified, sizeof(verified));
    length = host->get(properties, MAKEINTRESOURCEA(1), &verified, sizeof(verified), 0);
    verified.command[sizeof(verified.command)-1] = 0; verified.directory[sizeof(verified.directory)-1] = 0;
    if (length != sizeof(verified)) verifyIssues |= 1;
    if (!(verified.initial & DOS_REALMODE)) verifyIssues |= 2;
    if (verified.initial & 0x0100) verifyIssues |= 4;
    if (verified.realMode & 0x20) verifyIssues |= 8;
    if (lstrcmpiA(verified.command, command)) verifyIssues |= 16;
    if (lstrcmpiA(verified.directory, directory)) verifyIssues |= 32;
    if (verifyIssues) { error = ERROR_INVALID_DATA; goto done; }
    host->close(properties, DOS_CLOSE_DISCARD); properties = 0;
    error = host->launchPif(owner, pif);
done:
    if (properties) host->close(properties, DOS_CLOSE_DISCARD);
    if (error && created) {
        if (pif[0]) DeleteFileA(pif);
        RemoveDirectoryA(privateDirectory);
    }
    /* Retain a successfully dispatched PIF in TEMP: Windows may still read it
     * asynchronously, including during the subsequent transition to DOS. */
    return error;
}
