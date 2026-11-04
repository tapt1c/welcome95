#include "control_panel.h"
#include "startup.h"
static const char *failure="panel-validation";
const char *panel_failure_stage(void) { return failure; }
static DWORD note(const PanelConfig *c,const char *stage,DWORD error)
{
    if(error) failure=stage;
    startup_trace_executable(c->executable,stage,error); return error;
}
static void clear_bytes(void *data,UINT size)
{
    volatile BYTE *p=(volatile BYTE *)data;
    while(size--) *p++=0;
}
static BOOL same_path(const char *a,const char *b)
{
    char first[MAX_PATH],second[MAX_PATH];
    if(!lstrcmpiA(a,b)) return TRUE;
    return GetShortPathNameA(a,first,sizeof(first)) && GetShortPathNameA(b,second,sizeof(second)) && !lstrcmpiA(first,second);
}
static DWORD read_value(const PanelConfig *c,BOOL record,void *data,DWORD capacity,BOOL *present)
{
    HKEY key; DWORD type=0,size=capacity,error;
    *present=FALSE;
    error=RegOpenKeyExA(c->root,record ? c->storage_key : c->cpls_key,0,KEY_QUERY_VALUE,&key);
    if(error==ERROR_FILE_NOT_FOUND) return 0;
    if(error) return note(c,"panel-reg-open-read",error);
    error=RegQueryValueExA(key,record ? "Entry" : PANEL_VALUE,NULL,&type,(BYTE *)data,&size);
    RegCloseKey(key);
    if(error==ERROR_FILE_NOT_FOUND) return 0;
    if(error) return note(c,"panel-reg-read",error);
    if(record ? (type!=REG_BINARY || size!=capacity) :
        (type!=REG_SZ || !size || size>capacity || ((char *)data)[size-1])) return note(c,"panel-reg-data",ERROR_INVALID_DATA);
    *present=TRUE; return 0;
}
static DWORD write_value(const PanelConfig *c,BOOL record,const void *data,DWORD size)
{
    HKEY key; DWORD error;
    error=RegCreateKeyExA(c->root,record ? c->storage_key : c->cpls_key,0,NULL,0,
        KEY_SET_VALUE|KEY_QUERY_VALUE,NULL,&key,NULL);
    if(error) return note(c,"panel-reg-open-write",error);
    if(data) error=RegSetValueExA(key,record ? "Entry" : PANEL_VALUE,0,record ? REG_BINARY : REG_SZ,(const BYTE *)data,size);
    else {
        error=RegDeleteValueA(key,record ? "Entry" : PANEL_VALUE);
        if(error==ERROR_FILE_NOT_FOUND) error=0;
    }
    if(!error) error=RegFlushKey(key);
    RegCloseKey(key); return note(c,record ? "panel-record-write" : "panel-registration-write",error);
}
/* Win9x's legacy discovery route keeps the applet itself under LOGON. */
static DWORD read_ini(const PanelConfig *c,char *entry,BOOL *present)
{
    DWORD length;
    *present=FALSE;
    if(!c->control_ini || !*c->control_ini || !c->write_ini)
        return note(c,"panel-ini-path",ERROR_INVALID_PARAMETER);
    length=GetPrivateProfileStringA("MMCPL",PANEL_VALUE,"\001",entry,MAX_PATH,c->control_ini);
    if(length>=MAX_PATH-1) return note(c,"panel-ini-data",ERROR_MORE_DATA);
    *present=!(length==1 && entry[0]==1);
    return 0;
}
static DWORD write_ini(const PanelConfig *c,const char *entry)
{
    char actual[MAX_PATH]; BOOL present; DWORD error;
    SetLastError(0);
    if(!c->write_ini("MMCPL",PANEL_VALUE,entry,c->control_ini)) {
        error=GetLastError(); return note(c,"panel-ini-write",error ? error : ERROR_WRITE_FAULT);
    }
    /* A cache flush may return zero on success. Only a reported error is
       fatal; always verify the resulting entry instead of trusting the BOOL. */
    SetLastError(0);
    if(!c->write_ini(NULL,NULL,NULL,c->control_ini)) {
        error=GetLastError(); if(error) return note(c,"panel-ini-flush",error);
    }
    error=read_ini(c,actual,&present); if(error) return error;
    if(entry ? (!present || lstrcmpA(actual,entry)) : present)
        return note(c,"panel-ini-verify",ERROR_WRITE_FAULT);
    return 0;
}
static DWORD checksum(const BYTE *data,DWORD size)
{
    DWORD hash=2166136261UL,index;
    for(index=0;index<size;++index) { hash^=data[index]; hash*=16777619UL; }
    return hash;
}
static BOOL owned_module(const PanelRecord *record)
{
    char owner[MAX_PATH],module_path[MAX_PATH]; int a,b,index; const char *name;
    lstrcpyA(owner,record->owner); lstrcpyA(module_path,record->module);
    a=lstrlenA(owner); b=lstrlenA(module_path);
    while(a && owner[a-1]!='\\' && owner[a-1]!='/') --a;
    while(b && module_path[b-1]!='\\' && module_path[b-1]!='/') --b;
    if(!a || !b) return FALSE;
    name=module_path+b;
    if(lstrlenA(name)!=11 || (name[0]!='W' && name[0]!='w') || name[1]!='9' || name[2]!='5' || lstrcmpiA(name+7,".CPL")) return FALSE;
    for(index=3;index<7;++index) if(!((name[index]>='0' && name[index]<='9') ||
        (name[index]>='A' && name[index]<='F') || (name[index]>='a' && name[index]<='f'))) return FALSE;
    owner[a]=0; module_path[b]=0; return same_path(owner,module_path);
}
static BOOL valid_record(const PanelRecord *record)
{
    return record->version==1 && record->size && record->size<=1024*1024 &&
        record->owner[0] && !record->owner[MAX_PATH-1] && record->module[0] && !record->module[MAX_PATH-1] && owned_module(record);
}
static BOOL same_record(const PanelRecord *a,const PanelRecord *b)
{
    return a->version==b->version && a->size==b->size && a->checksum==b->checksum &&
        same_path(a->owner,b->owner) && same_path(a->module,b->module);
}
static BOOL module_matches(const PanelRecord *record)
{
    static BYTE bytes[1024];
    HANDLE file; DWORD count,total=0,hash=2166136261UL,index; BOOL okay=TRUE;
    file=CreateFileA(record->module,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);
    if(file==INVALID_HANDLE_VALUE) return FALSE;
    for(;;) {
        if(!ReadFile(file,bytes,sizeof(bytes),&count,NULL)) { okay=FALSE; break; }
        if(!count) break;
        total+=count; if(total>record->size) { okay=FALSE; break; }
        for(index=0;index<count;++index) { hash^=bytes[index]; hash*=16777619UL; }
    }
    CloseHandle(file); return okay && total==record->size && hash==record->checksum;
}
static DWORD create_module(const PanelConfig *c,PanelRecord *record,char *created)
{
    char directory[MAX_PATH],temporary[MAX_PATH]; int end,attempt;
    HANDLE file=INVALID_HANDLE_VALUE; DWORD error=ERROR_FILE_EXISTS,written=0,seed=GetTickCount();
    lstrcpynA(directory,c->executable,sizeof(directory)); end=lstrlenA(directory);
    while(end && directory[end-1]!='\\' && directory[end-1]!='/') --end;
    if(!end || end+14>=MAX_PATH) return note(c,"panel-directory",ERROR_PATH_NOT_FOUND);
    directory[end]=0;
    for(attempt=0;attempt<64;++attempt) {
        UINT unique=(UINT)((seed+(DWORD)attempt)%65535)+1;
        /* GetTempFileName can omit leading zeroes for an explicit unique.
           Keep our owned-module name canonical before exclusive reservation. */
        wsprintfA(temporary,"%sW95%04x.CPL",directory,unique);
        file=CreateFileA(temporary,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
        if(file!=INVALID_HANDLE_VALUE) break;
        error=GetLastError();
        if((error!=ERROR_FILE_EXISTS && error!=ERROR_ALREADY_EXISTS && error!=ERROR_ACCESS_DENIED) ||
           GetFileAttributesA(temporary)==INVALID_FILE_ATTRIBUTES) return note(c,"panel-module-create",error);
    }
    if(attempt==64) return note(c,"panel-module-exhausted",error);
    error=WriteFile(file,c->image,c->size,&written,NULL) ? 0 : GetLastError();
    if(!error && written!=c->size) error=ERROR_WRITE_FAULT;
    if(!error && !FlushFileBuffers(file)) { error=GetLastError(); if(!error) error=ERROR_WRITE_FAULT; }
    CloseHandle(file);
    if(error) { DeleteFileA(temporary); return note(c,"panel-module-write",error); }
    lstrcpyA(record->module,temporary); lstrcpyA(created,temporary); return 0;
}
DWORD panel_rollback(const PanelConfig *c,PanelChange *change)
{
    PanelRecord current; char entry[MAX_PATH],ini[MAX_PATH]; BOOL record_present,entry_present,ini_present;
    DWORD error,part;
    if(!change->applied) return 0;
    error=read_value(c,TRUE,&current,sizeof(current),&record_present);
    if(!error) error=read_value(c,FALSE,entry,sizeof(entry),&entry_present);
    if(!error) error=read_ini(c,ini,&ini_present);
    if(error) return error;
    if(record_present && !valid_record(&current)) return note(c,"panel-rollback-data",ERROR_INVALID_DATA);
    if(record_present && !same_record(&current,&change->expected) &&
        (!change->record_present || !same_record(&current,&change->old_record))) return note(c,"panel-rollback-owner",ERROR_BUSY);
    if(entry_present && !same_path(entry,change->expected.module) &&
        (!change->entry_present || !same_path(entry,change->old_entry))) return note(c,"panel-rollback-entry",ERROR_BUSY);
    if(ini_present && !same_path(ini,change->expected.module) &&
        (!change->ini_present || !same_path(ini,change->old_ini))) return note(c,"panel-rollback-ini-owner",ERROR_BUSY);
    error=write_value(c,TRUE,change->record_present ? &change->old_record : NULL,sizeof(change->old_record));
    part=write_value(c,FALSE,change->entry_present ? change->old_entry : NULL,
        change->entry_present ? (DWORD)lstrlenA(change->old_entry)+1 : 0);
    if(!error) error=part;
    part=write_ini(c,change->ini_present ? change->old_ini : NULL);
    if(!error) error=part;
    if(!error) {
        change->applied=FALSE;
        if(change->created[0] && module_matches(&change->expected)) DeleteFileA(change->created);
    }
    return note(c,"panel-rollback-result",error);
}
DWORD panel_configure(const PanelConfig *c,BOOL enable,PanelChange *change)
{
    PanelRecord next; DWORD error,rollback;
    const char *saved_failure;
    clear_bytes(change,sizeof(*change)); failure="panel-validation";
    error=read_value(c,TRUE,&change->old_record,sizeof(change->old_record),&change->record_present);
    if(!error) error=read_value(c,FALSE,change->old_entry,sizeof(change->old_entry),&change->entry_present);
    if(!error) error=read_ini(c,change->old_ini,&change->ini_present);
    if(error) return error;
    if(change->record_present && (!valid_record(&change->old_record) ||
        !same_path(change->old_record.owner,c->executable))) return note(c,"panel-owner",ERROR_BUSY);
    if(change->entry_present && (!change->record_present ||
        !same_path(change->old_entry,change->old_record.module))) return note(c,"panel-registration-owner",ERROR_BUSY);
    if(change->ini_present && (!change->record_present ||
        !same_path(change->old_ini,change->old_record.module))) return note(c,"panel-ini-owner",ERROR_BUSY);
    change->enabled=enable;
    if(enable) {
        if(!c->image || !c->size || c->size>1024*1024) return note(c,"panel-resource",ERROR_RESOURCE_DATA_NOT_FOUND);
        clear_bytes(&next,sizeof(next)); next.version=1; next.size=c->size; next.checksum=checksum(c->image,c->size);
        lstrcpynA(next.owner,c->executable,sizeof(next.owner));
        if(change->record_present && module_matches(&change->old_record) &&
            next.size==change->old_record.size && next.checksum==change->old_record.checksum)
            lstrcpyA(next.module,change->old_record.module);
        else {
            if(change->entry_present && !module_matches(&change->old_record) &&
                GetFileAttributesA(change->old_record.module)!=INVALID_FILE_ATTRIBUTES) return note(c,"panel-module-owner",ERROR_INVALID_DATA);
            error=create_module(c,&next,change->created); if(error) return error;
        }
        change->expected=next; change->applied=TRUE;
        error=write_value(c,TRUE,&next,sizeof(next));
        if(!error) error=write_value(c,FALSE,next.module,(DWORD)lstrlenA(next.module)+1);
        if(!error) error=write_ini(c,next.module);
    } else {
        change->expected=change->old_record; change->applied=TRUE;
        error=write_value(c,FALSE,NULL,0);
        if(!error) error=write_ini(c,NULL);
    }
    if(!error) return 0;
    saved_failure=failure; rollback=panel_rollback(c,change); failure=saved_failure;
    return rollback ? ERROR_CAN_NOT_COMPLETE : error;
}
DWORD panel_startup_configure(const PanelConfig *panel,const StartupConfig *startup,BOOL enable,BOOL *migrated,const char **stage)
{
    static PanelChange change;
    DWORD error;
    *migrated=FALSE;
    /* Disable restores startup without removing the user's route back to
       Settings. It must also work if the applet registration was changed. */
    if(!enable) {
        error=startup_configure(startup,FALSE); *stage=startup_failure_stage(); return error;
    }
    error=panel_configure(panel,enable,&change); *stage=panel_failure_stage();
    if(error) return error;
    error=enable ? startup_enable_managed(startup,migrated) : startup_configure(startup,FALSE);
    *stage=startup_failure_stage();
    if(error && panel_rollback(panel,&change)) {
        *stage=panel_failure_stage(); return ERROR_CAN_NOT_COMPLETE;
    }
    return error;
}
