#ifndef WELCOME95_STARTUP_H
#define WELCOME95_STARTUP_H
#include <windows.h>
#define STARTUP_TEXT 1024
#define STARTUP_SERVICE "Software\\Microsoft\\Windows\\CurrentVersion\\RunServices"
#define STARTUP_RUN "Software\\Microsoft\\Windows\\CurrentVersion\\Run"
#define STARTUP_ONCE "Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce"
#define STARTUP_BACKUP "Software\\Welcome95\\Startup"
typedef struct {
    HKEY root;
    const char *service_key,*run_key,*once_key,*backup_key;
    const char *executable,*ini;
    BOOL (WINAPI *write_ini)(LPCSTR,LPCSTR,LPCSTR,LPCSTR);
} StartupConfig;
DWORD startup_configure(const StartupConfig *config,BOOL enable);
BOOL startup_installed(const StartupConfig *config);
DWORD startup_original(const StartupConfig *config,char *command,DWORD capacity);
DWORD startup_rearm(const StartupConfig *config);
#endif
