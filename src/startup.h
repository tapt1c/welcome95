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
    const char *boot_directory;
    const BYTE *boot_image;
    DWORD boot_size;
    BOOL (WINAPI *write_ini)(LPCSTR,LPCSTR,LPCSTR,LPCSTR);
} StartupConfig;
DWORD startup_configure(const StartupConfig *config,BOOL enable);
DWORD startup_prepare_logon(const char *source,const char *windows_directory,char *target,BOOL *copied);
DWORD startup_enable_managed(const StartupConfig *config,BOOL *migrated);
const char *startup_failure_stage(void);
BOOL startup_installed(const StartupConfig *config);
DWORD startup_original(const StartupConfig *config,char *command,DWORD capacity);
DWORD startup_rearm(const StartupConfig *config);
void startup_trace(HKEY root,const char *backup_key,const char *stage,DWORD code);
#endif
