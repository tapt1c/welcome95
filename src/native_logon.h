#ifndef WELCOME95_NATIVE_LOGON_H
#define WELCOME95_NATIVE_LOGON_H

#include <windows.h>

/* Visually integrate the real dialog's controls without reading the password,
   replacing its window procedure, or changing control ownership. */
BOOL native_logon_attach(HWND dialog, int x, int y, int width);
BOOL native_logon_layout(int x, int y, int width);
void native_logon_restore(BOOL reveal_original);
BOOL native_logon_attached(void);
void native_logon_focus(void);
void native_logon_submit(void);
BOOL native_logon_user_empty(void);
BOOL native_logon_set_user(const char *name);
const char *native_logon_name(void);

#endif
