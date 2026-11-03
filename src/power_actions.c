/* Optional power APIs and non-forced session end, adapted from explorer95. */
#include "power_actions.h"
#include "shutdown_dos.h"
static HMODULE power_library;
typedef BOOLEAN (WINAPI *SuspendAllowed)(void);
typedef BOOLEAN (WINAPI *SuspendState)(BOOLEAN,BOOLEAN,BOOLEAN);
typedef BOOL (WINAPI *SystemPowerState)(BOOL,BOOL);
static SuspendState suspend_state;
static SystemPowerState system_state;
static PowerOperations system_operations;
static BOOL windows95(DWORD version)
{
    return (version&0x80000000UL) && LOBYTE(LOWORD(version))==4 && HIBYTE(LOWORD(version))==0;
}
static BOOL suspend_windows(void)
{
    if(suspend_state) return suspend_state(FALSE,FALSE,FALSE);
    if(system_state) return system_state(TRUE,FALSE);
    SetLastError(ERROR_NOT_SUPPORTED); return FALSE;
}
void power_operations_release(void)
{
    if(power_library) FreeLibrary(power_library);
    power_library=NULL; suspend_state=NULL; system_state=NULL;
}
const PowerOperations *power_operations(void)
{
    char path[MAX_PATH];
    SuspendAllowed allowed=NULL;
    union { FARPROC raw; SuspendAllowed allowed; SuspendState suspend; SystemPowerState system; } address;
    power_operations_release();
    system_operations.version=GetVersion();
    system_operations.end_session=ExitWindowsEx;
    system_operations.standby=suspend_windows;
    system_operations.dos=ShutdownDosRestart;
    system_operations.standby_available=FALSE; system_operations.dos_available=FALSE;
    if(!windows95(system_operations.version)) return &system_operations;
    address.raw=GetProcAddress(GetModuleHandleA("kernel32.dll"),"SetSystemPowerState"); system_state=address.system;
    if(GetSystemDirectoryA(path,sizeof(path)) && lstrlenA(path)+14<MAX_PATH) {
        lstrcatA(path,"\\powrprof.dll"); power_library=LoadLibraryA(path);
        if(power_library) {
            address.raw=GetProcAddress(power_library,"IsPwrSuspendAllowed"); allowed=address.allowed;
            address.raw=GetProcAddress(power_library,"SetSuspendState"); suspend_state=address.suspend;
        }
    }
    system_operations.standby_available=suspend_state && allowed ? allowed() : system_state!=NULL;
    system_operations.dos_available=ShutdownDosAvailable();
    return &system_operations;
}
DWORD power_request(PowerAction action, BOOL dos, HWND owner, const PowerOperations *operations)
{
    BOOL result;
    DWORD error;
    if(!operations || !windows95(operations->version)) return ERROR_NOT_SUPPORTED;
    if(dos && action!=POWER_RESTART) return ERROR_INVALID_PARAMETER;
    if(action==POWER_RESTART && dos)
        return operations->dos_available && operations->dos ? operations->dos(owner) : ERROR_NOT_SUPPORTED;
    SetLastError(ERROR_SUCCESS);
    if(action==POWER_STANDBY) {
        if(!operations->standby_available || !operations->standby) return ERROR_NOT_SUPPORTED;
        result=operations->standby();
    } else if(action==POWER_OFF || action==POWER_RESTART) {
        if(!operations->end_session) return ERROR_NOT_SUPPORTED;
        result=operations->end_session(action==POWER_OFF ? EWX_SHUTDOWN : EWX_REBOOT,0);
    } else return ERROR_INVALID_PARAMETER;
    if(result) return ERROR_SUCCESS;
    error=GetLastError();
    return error ? error : (action==POWER_STANDBY ? ERROR_NOT_SUPPORTED : ERROR_CANCELLED);
}
