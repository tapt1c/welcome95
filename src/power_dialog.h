#ifndef WELCOME95_POWER_DIALOG_H
#define WELCOME95_POWER_DIALOG_H
#include <windows.h>
#include "power_actions.h"
#define POWER_DOS 1703
typedef struct {
    HINSTANCE instance;
    HWND owner, native_input;
    BOOL russian, preview, standby_available, dos_available;
    void (*perform)(PowerAction,BOOL);
    void (*restore_focus)(void);
} PowerDialogHost;
BOOL power_dialog_show(const PowerDialogHost *host);
void power_dialog_close(void);
BOOL power_dialog_active(void);
void power_dialog_focus(void);
BOOL power_dialog_translate(MSG *message);
#endif
