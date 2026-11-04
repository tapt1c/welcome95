#ifndef WELCOME95_BOOTSTRAP_H
#define WELCOME95_BOOTSTRAP_H
#include <windows.h>
#define BOOTSTRAP_READY_EVENT "Welcome95.ControllerReady"
DWORD bootstrap_forward(HKEY root,const char *key,const char *mode);
#endif
