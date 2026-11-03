#ifndef WELCOME95_POWER_ACTIONS_H
#define WELCOME95_POWER_ACTIONS_H
#include <windows.h>
typedef enum { POWER_STANDBY=1700, POWER_OFF=1701, POWER_RESTART=1702 } PowerAction;
typedef struct {
    DWORD version;
    BOOL standby_available, dos_available;
    BOOL (WINAPI *end_session)(UINT,DWORD);
    BOOL (*standby)(void);
    DWORD (*dos)(HWND);
} PowerOperations;
const PowerOperations *power_operations(void);
void power_operations_release(void);
DWORD power_request(PowerAction action, BOOL dos, HWND owner, const PowerOperations *operations);
#endif
