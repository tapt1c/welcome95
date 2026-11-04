#ifndef WELCOME95_CONTROL_PANEL_H
#define WELCOME95_CONTROL_PANEL_H
#include <windows.h>
#include "startup.h"
#define PANEL_CPLS "Software\\Microsoft\\Windows\\CurrentVersion\\Control Panel\\Cpls"
#define PANEL_STORAGE "Software\\Welcome95\\ControlPanel"
#define PANEL_VALUE "Welcome95"
typedef struct {
    DWORD version,size,checksum;
    char owner[MAX_PATH],module[MAX_PATH];
} PanelRecord;
typedef struct {
    HKEY root;
    const char *cpls_key,*storage_key,*executable;
    const BYTE *image;
    DWORD size;
} PanelConfig;
typedef struct {
    BOOL record_present,entry_present,applied,enabled;
    PanelRecord old_record,expected;
    char old_entry[MAX_PATH],created[MAX_PATH];
} PanelChange;
DWORD panel_configure(const PanelConfig *config,BOOL enable,PanelChange *change);
DWORD panel_rollback(const PanelConfig *config,PanelChange *change);
const char *panel_failure_stage(void);
DWORD panel_startup_configure(const PanelConfig *panel,const StartupConfig *startup,BOOL enable,BOOL *migrated,const char **stage);
#endif
