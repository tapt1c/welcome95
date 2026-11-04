/* Win95 startup integration. Only our values and SYSTEM.INI [boot] shell change. */
#include "startup.h"
#define VALUE "Welcome95"
/* Configuration runs serially on the UI/controller thread. Static staging
   buffers keep legacy stack frames below one page without a CRT stack probe. */
typedef struct { BOOL present; char text[STARTUP_TEXT]; } Entry;
static DWORD read_string(const StartupConfig *c,const char *key,const char *name,Entry *entry)
{
    HKEY handle;
    DWORD type,size=sizeof(entry->text),error;
    entry->present=FALSE; entry->text[0]=0;
    error=RegOpenKeyExA(c->root,key,0,KEY_QUERY_VALUE,&handle);
    if(error==ERROR_FILE_NOT_FOUND) return 0;
    if(error) return error;
    error=RegQueryValueExA(handle,name,NULL,&type,(BYTE *)entry->text,&size); RegCloseKey(handle);
    if(error==ERROR_FILE_NOT_FOUND) return 0;
    if(error) return error;
    if(type!=REG_SZ || !size || size>sizeof(entry->text) || entry->text[size-1]) return ERROR_INVALID_DATA;
    entry->present=TRUE; return 0;
}
static DWORD put_string(const StartupConfig *c,const char *key,const char *name,const char *value)
{
    HKEY handle;
    DWORD error=RegCreateKeyExA(c->root,key,0,NULL,0,KEY_SET_VALUE,NULL,&handle,NULL);
    if(error) return error;
    if(value) error=RegSetValueExA(handle,name,0,REG_SZ,(const BYTE *)value,(DWORD)lstrlenA(value)+1);
    else {
        error=RegDeleteValueA(handle,name);
        if(error==ERROR_FILE_NOT_FOUND) error=0;
    }
    if(!error && value && !lstrcmpA(name,"Valid")) error=RegFlushKey(handle);
    RegCloseKey(handle); return error;
}
static DWORD read_shell(const StartupConfig *c,Entry *entry)
{
    DWORD length=GetPrivateProfileStringA("boot","shell","\001",entry->text,sizeof(entry->text),c->ini);
    if(length>=sizeof(entry->text)-1) return ERROR_MORE_DATA;
    entry->present=!(length==1 && entry->text[0]==1);
    if(!entry->present) lstrcpyA(entry->text,"Explorer.exe");
    if(!entry->text[0]) return ERROR_INVALID_DATA;
    return 0;
}
static DWORD set_shell(const StartupConfig *c,const char *value)
{
    Entry verify;
    if(!c->write_ini("boot","shell",value,c->ini)) return ERROR_WRITE_FAULT;
    c->write_ini(NULL,NULL,NULL,c->ini);
    if(read_shell(c,&verify)) return ERROR_READ_FAULT;
    if(value ? (!verify.present || lstrcmpA(verify.text,value)) : verify.present) return ERROR_WRITE_FAULT;
    return 0;
}
static DWORD gate_command(const StartupConfig *c,char *text)
{
    char path[MAX_PATH];
    DWORD length=GetShortPathNameA(c->executable,path,sizeof(path));
    int i;
    if(!length || length>=sizeof(path) || length+8>=128) return ERROR_FILENAME_EXCED_RANGE;
    for(i=0;path[i];++i) if((BYTE)path[i]>=127 || path[i]==' ' || path[i]=='"') return ERROR_INVALID_NAME;
    wsprintfA(text,"%s /shell",path); return 0;
}
static BOOL same_executable(const char *first,const char *second);
static BOOL ascii_path(const char *text)
{
    int i;
    if(!text[0]) return FALSE;
    for(i=0;text[i];++i) if((BYTE)text[i]<=32 || (BYTE)text[i]>=127 || text[i]=='"') return FALSE;
    return TRUE;
}
static BOOL boot_path(const StartupConfig *c,char *path)
{
    static Entry owner,valid,gate;
    if(read_string(c,c->backup_key,"Owner",&owner) || !owner.present ||
       !same_executable(owner.text,c->executable) || read_string(c,c->backup_key,"Valid",&valid) ||
       !valid.present || lstrcmpA(valid.text,"1") || read_string(c,c->backup_key,"Gate",&gate) ||
       !gate.present || lstrlenA(gate.text)>=MAX_PATH || !ascii_path(gate.text)) return FALSE;
    lstrcpyA(path,gate.text); return TRUE;
}
static void run_command(const StartupConfig *c,char *text,const char *option)
{
    char boot[MAX_PATH];
    wsprintfA(text,"\"%s\" %s",boot_path(c,boot) ? boot : c->executable,option);
}
static DWORD deploy_boot(const StartupConfig *c,char *path)
{
    char directory[MAX_PATH],short_path[MAX_PATH];
    HANDLE file;
    DWORD length,written,error;
    if(!c->boot_directory || !c->boot_image || !c->boot_size) return ERROR_RESOURCE_DATA_NOT_FOUND;
    if(!GetTempFileNameA(c->boot_directory,"W95",0,directory)) return GetLastError();
    if(!DeleteFileA(directory)) return GetLastError();
    if(!CreateDirectoryA(directory,NULL)) return GetLastError();
    wsprintfA(path,"%s\\W95BOOT.EXE",directory);
    length=GetShortPathNameA(directory,short_path,sizeof(short_path));
    if(!length || length+13>=128 || !ascii_path(short_path)) { error=ERROR_INVALID_NAME; goto failed; }
    file=CreateFileA(path,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE) { error=GetLastError(); goto failed; }
    error=WriteFile(file,c->boot_image,c->boot_size,&written,NULL) && written==c->boot_size && FlushFileBuffers(file) ? 0 : ERROR_WRITE_FAULT;
    CloseHandle(file);
    if(error) goto failed;
    wsprintfA(path,"%s\\W95BOOT.EXE",short_path); return 0;
failed:
    DeleteFileA(path); RemoveDirectoryA(directory); return error;
}
static BOOL same_executable(const char *first,const char *second)
{
    char a[MAX_PATH],b[MAX_PATH];
    if(!lstrcmpiA(first,second)) return TRUE;
    return GetShortPathNameA(first,a,sizeof(a)) && GetShortPathNameA(second,b,sizeof(b)) && !lstrcmpiA(a,b);
}
/* Tests supply a private Windows directory, never the real boot configuration. */
DWORD startup_prepare_logon(const char *source,const char *windows_directory,char *target,BOOL *copied)
{
    static const char signature[]="Welcome95 LOGON\r\n";
    char directory[MAX_PATH],marker_path[MAX_PATH],stage[MAX_PATH],old_file[MAX_PATH],reserve[MAX_PATH],text[32];
    DWORD attributes,error=0,count,written;
    HANDLE marker=INVALID_HANDLE_VALUE;
    BOOL new_directory=FALSE,new_marker=FALSE,same,old_moved=FALSE;
    int length,attempt;
    *copied=FALSE; target[0]=0; stage[0]=old_file[0]=0;
    if(!source || !windows_directory || !*windows_directory || lstrlenA(windows_directory)+24>=MAX_PATH)
        return ERROR_FILENAME_EXCED_RANGE;
    attributes=GetFileAttributesA(source);
    if(attributes==INVALID_FILE_ATTRIBUTES) return GetLastError();
    if(attributes&FILE_ATTRIBUTE_DIRECTORY) return ERROR_DIRECTORY;
    lstrcpyA(directory,windows_directory); length=lstrlenA(directory);
    while(length && (directory[length-1]=='\\' || directory[length-1]=='/')) directory[--length]=0;
    lstrcatA(directory,"\\LOGON"); wsprintfA(target,"%s\\WELCOME95.EXE",directory);
    wsprintfA(marker_path,"%s\\W95FILES.INI",directory);
    attributes=GetFileAttributesA(directory);
    if(attributes==INVALID_FILE_ATTRIBUTES) {
        if(!CreateDirectoryA(directory,NULL)) return GetLastError();
        new_directory=TRUE;
    } else if(!(attributes&FILE_ATTRIBUTE_DIRECTORY)) return ERROR_DIRECTORY;
    same=same_executable(source,target);
    attributes=GetFileAttributesA(target);
    if(attributes!=INVALID_FILE_ATTRIBUTES && (attributes&FILE_ATTRIBUTE_DIRECTORY)) { error=ERROR_ALREADY_EXISTS; goto done; }
    if(!same && attributes!=INVALID_FILE_ATTRIBUTES && GetFileAttributesA(marker_path)==INVALID_FILE_ATTRIBUTES) {
        error=ERROR_ALREADY_EXISTS; goto done;
    }
    /* Marker ownership also serializes installers; a crash releases the handle. */
    marker=CreateFileA(marker_path,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(marker==INVALID_HANDLE_VALUE) { error=GetLastError(); goto done; }
    new_marker=GetLastError()!=ERROR_ALREADY_EXISTS;
    if(new_marker) {
        if(!WriteFile(marker,signature,sizeof(signature)-1,&written,NULL) || written!=sizeof(signature)-1 || !FlushFileBuffers(marker)) {
            error=ERROR_WRITE_FAULT; goto done;
        }
    } else {
        if(!ReadFile(marker,text,sizeof(text)-1,&count,NULL)) { error=GetLastError(); goto done; }
        text[count]=0;
        if(count!=sizeof(signature)-1 || lstrcmpA(text,signature)) { error=ERROR_INVALID_DATA; goto done; }
    }
    if(same) goto done;
    if(!GetTempFileNameA(directory,"W95",0,stage)) { error=GetLastError(); goto done; }
    if(!CopyFileA(source,stage,FALSE)) { error=GetLastError(); goto done; }
    if(attributes!=INVALID_FILE_ATTRIBUTES) {
        for(attempt=0;attempt<64;++attempt) {
            if(!GetTempFileNameA(directory,"E95",0,reserve)) { error=GetLastError(); goto done; }
            lstrcpyA(old_file,reserve); length=lstrlenA(old_file);
            old_file[length-3]='B'; old_file[length-2]='A'; old_file[length-1]='K';
            if(MoveFileA(reserve,old_file)) break;
            error=GetLastError(); DeleteFileA(reserve);
            if(error!=ERROR_ALREADY_EXISTS && error!=ERROR_FILE_EXISTS) goto done;
        }
        if(attempt==64) goto done;
        error=0;
        if(!DeleteFileA(old_file)) { error=GetLastError(); goto done; }
        /* MoveFile refuses collisions and a running destination on Win95. */
        if(!MoveFileA(target,old_file)) { error=GetLastError(); goto done; }
        old_moved=TRUE;
    }
    if(!MoveFileA(stage,target)) {
        error=GetLastError();
        if(old_moved && !MoveFileA(old_file,target)) error=ERROR_CAN_NOT_COMPLETE;
        goto done;
    }
    stage[0]=0; *copied=TRUE;
done:
    if(stage[0]) DeleteFileA(stage);
    if(marker!=INVALID_HANDLE_VALUE) CloseHandle(marker);
    if(error && new_marker) DeleteFileA(marker_path);
    if(error && new_directory) RemoveDirectoryA(directory);
    return error;
}
static BOOL owned_command(const StartupConfig *c,const char *text,const char *option)
{
    char module[MAX_PATH],boot[MAX_PATH];
    int index=0;
    BOOL quoted;
    while(*text==' ' || *text=='\t') ++text;
    quoted=*text=='"'; if(quoted) ++text;
    while(*text && (quoted ? *text!='"' : (*text!=' ' && *text!='\t'))) {
        if(index>=MAX_PATH-1) return FALSE;
        module[index++]=*text++;
    }
    module[index]=0;
    if(quoted) { if(*text!='"') return FALSE; ++text; }
    while(*text==' ' || *text=='\t') ++text;
    return !lstrcmpiA(text,option) && (same_executable(module,c->executable) ||
        (boot_path(c,boot) && same_executable(module,boot)));
}
static BOOL owns_entry(const StartupConfig *c,const Entry *entry,const char *option)
{
    return !entry->present || owned_command(c,entry->text,option);
}
static DWORD backup(const StartupConfig *c,Entry *original,Entry *saved_gate)
{
    static Entry owner,present,valid;
    DWORD error=read_string(c,c->backup_key,"Owner",&owner);
    if(!error) error=read_string(c,c->backup_key,"Valid",&valid);
    if(error) return error;
    if(!owner.present || !valid.present || lstrcmpA(valid.text,"1")) return ERROR_FILE_NOT_FOUND;
    if(!same_executable(owner.text,c->executable)) return ERROR_BUSY;
    error=read_string(c,c->backup_key,"OriginalShell",original);
    if(!error) error=read_string(c,c->backup_key,"HadShell",&present);
    if(!error) error=read_string(c,c->backup_key,"Gate",saved_gate);
    if(error) return error;
    if(!original->present || !original->text[0] || !saved_gate->present || !present.present ||
       (lstrcmpA(present.text,"0") && lstrcmpA(present.text,"1"))) return ERROR_INVALID_DATA;
    original->present=!lstrcmpA(present.text,"1"); return 0;
}
static DWORD recovery_file(const StartupConfig *c,const char *shell,char *path)
{
    static char directory[MAX_PATH],temporary[MAX_PATH],text[STARTUP_TEXT+32];
    int length,attempt;
    HANDLE file;
    DWORD written,error=ERROR_FILE_EXISTS,size;
    lstrcpynA(directory,c->executable,sizeof(directory)); length=lstrlenA(directory);
    while(length && directory[length-1]!='\\' && directory[length-1]!='/') --length;
    if(!length) return ERROR_PATH_NOT_FOUND;
    directory[length]=0;
    for(attempt=0;attempt<64;++attempt) {
        if(!GetTempFileNameA(directory,"W95",0,temporary)) return GetLastError();
        lstrcpyA(path,temporary); length=lstrlenA(path);
        path[length-3]='B'; path[length-2]='A'; path[length-1]='K';
        if(MoveFileA(temporary,path)) break;
        error=GetLastError(); DeleteFileA(temporary);
        if(error!=ERROR_FILE_EXISTS && error!=ERROR_ALREADY_EXISTS) return error;
    }
    if(attempt==64) return error;
    file=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,TRUNCATE_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE) { error=GetLastError(); DeleteFileA(path); return error; }
    wsprintfA(text,"[boot]\r\nshell=%s\r\n",shell); size=(DWORD)lstrlenA(text);
    error=WriteFile(file,text,size,&written,NULL) && written==size && FlushFileBuffers(file) ? 0 : ERROR_WRITE_FAULT;
    CloseHandle(file);
    if(error) DeleteFileA(path);
    return error;
}
static BOOL self_command(const StartupConfig *c,const char *command)
{
    char module[MAX_PATH],short_module[MAX_PATH],short_self[MAX_PATH];
    const char *name=c->executable;
    int i=0; BOOL quoted;
    while(*command==' ' || *command=='\t') ++command;
    quoted=*command=='"'; if(quoted) ++command;
    while(*command && (quoted ? *command!='"' : (*command!=' ' && *command!='\t'))) {
        if(i>=MAX_PATH-1) return TRUE;
        module[i++]=*command++;
    }
    module[i]=0;
    for(i=0;c->executable[i];++i) if(c->executable[i]=='\\' || c->executable[i]=='/') name=c->executable+i+1;
    if(!lstrcmpiA(module,name)) return TRUE;
    return GetShortPathNameA(module,short_module,sizeof(short_module)) &&
        GetShortPathNameA(c->executable,short_self,sizeof(short_self)) && !lstrcmpiA(short_module,short_self);
}
DWORD startup_original(const StartupConfig *c,char *command,DWORD capacity)
{
    Entry original,gate;
    DWORD error=backup(c,&original,&gate);
    if(error) return error;
    if((DWORD)lstrlenA(original.text)>=capacity) return ERROR_MORE_DATA;
    if(!lstrcmpiA(original.text,gate.text) || self_command(c,original.text)) return ERROR_INVALID_DATA;
    lstrcpyA(command,original.text); return 0;
}
DWORD startup_rearm(const StartupConfig *c)
{
    Entry current;
    char command[STARTUP_TEXT];
    DWORD error=read_string(c,c->once_key,VALUE,&current);
    run_command(c,command,"/startup");
    if(error) return error;
    if(!owns_entry(c,&current,"/startup")) return ERROR_BUSY;
    return put_string(c,c->once_key,VALUE,command);
}
BOOL startup_installed(const StartupConfig *c)
{
    static Entry service,run,shell,original,gate;
    if(backup(c,&original,&gate) || read_shell(c,&shell) || lstrcmpiA(shell.text,gate.text) ||
       read_string(c,c->service_key,VALUE,&service) || read_string(c,c->run_key,VALUE,&run)) return FALSE;
    if(ascii_path(gate.text) && GetFileAttributesA(gate.text)==INVALID_FILE_ATTRIBUTES) return FALSE;
    if(!service.present || !owned_command(c,service.text,"/welcome")) return FALSE;
    return run.present && owned_command(c,run.text,"/session-ready");
}
DWORD startup_configure(const StartupConfig *c,BOOL enable)
{
    static Entry old[3],shell,original,saved_gate,owner;
    const char *keys[3];
    const char *options[3]={"/welcome","/session-ready","/startup"};
    static char gate[STARTUP_TEXT],command[STARTUP_TEXT],recovery[MAX_PATH];
    DWORD error,back_error;
    BOOL have_backup,replace_backup=FALSE,empty_entries,changed_gate=FALSE;
    int i,j;
    keys[0]=c->service_key; keys[1]=c->run_key; keys[2]=c->once_key;
    error=read_shell(c,&shell); if(error) return error;
    for(i=0;i<3;++i) {
        error=read_string(c,keys[i],VALUE,&old[i]); if(error) return error;
        run_command(c,command,options[i]);
        if(!owns_entry(c,&old[i],options[i])) return ERROR_BUSY;
    }
    empty_entries=!old[0].present && !old[1].present && !old[2].present;
    back_error=backup(c,&original,&saved_gate); have_backup=back_error==0;
    if(enable && empty_entries && (have_backup || back_error==ERROR_BUSY)) {
        Entry previous_gate;
        error=read_string(c,c->backup_key,"Gate",&previous_gate); if(error) return error;
        if(previous_gate.present && lstrcmpiA(shell.text,previous_gate.text)) {
            /* A disabled copy retains recovery metadata, but a later install
               backs up the shell actually configured now, including a new copy. */
            replace_backup=TRUE; have_backup=FALSE; back_error=ERROR_FILE_NOT_FOUND;
        }
    }
    if(back_error!=0 && back_error!=ERROR_FILE_NOT_FOUND) return back_error;
    if(enable) {
        if(have_backup && lstrcmpiA(shell.text,saved_gate.text) && lstrcmpA(shell.text,original.text)) return ERROR_BUSY;
        if(c->boot_image && c->boot_directory && c->boot_size) {
            if(have_backup && ascii_path(saved_gate.text) && startup_installed(c)) return 0;
            error=deploy_boot(c,gate); if(error) return error;
        } else {
            error=gate_command(c,gate); if(error) return error;
        }
        if(have_backup) {
            changed_gate=TRUE;
            error=put_string(c,c->backup_key,"Gate",gate);
            if(!error) error=put_string(c,c->backup_key,"Valid","1");
            if(error) goto failed;
        } else {
            /* Never back up our own gate as the original shell after a damaged install. */
            if(!lstrcmpiA(shell.text,gate) || self_command(c,shell.text)) return ERROR_INVALID_DATA;
            error=read_string(c,c->backup_key,"Owner",&owner); if(error) return error;
            if(owner.present && !same_executable(owner.text,c->executable) && !replace_backup) return ERROR_BUSY;
            error=recovery_file(c,shell.text,recovery); if(error) return error;
            error=put_string(c,c->backup_key,"Valid",NULL);
            if(!error) error=put_string(c,c->backup_key,"Owner",c->executable);
            if(!error) error=put_string(c,c->backup_key,"OriginalShell",shell.text);
            if(!error) error=put_string(c,c->backup_key,"HadShell",shell.present ? "1" : "0");
            if(!error) error=put_string(c,c->backup_key,"Gate",gate);
            if(!error) error=put_string(c,c->backup_key,"RecoveryFile",recovery);
            if(!error) error=put_string(c,c->backup_key,"Valid","1");
            if(error) return error;
        }
    } else if(!have_backup) {
        error=gate_command(c,gate);
        if(!error && !lstrcmpiA(shell.text,gate)) return ERROR_INVALID_DATA;
    } else if(have_backup && lstrcmpiA(shell.text,saved_gate.text) && lstrcmpA(shell.text,original.text)) return ERROR_BUSY;
    /* Shell is written last during enable, first during disable. Always retain
       the recovery backup until all startup entries and the shell are restored. */
    if(!enable && have_backup) {
        error=set_shell(c,original.present ? original.text : NULL); if(error) return error;
    }
    for(i=0;i<3;++i) {
        run_command(c,command,options[i]);
        error=put_string(c,keys[i],VALUE,enable ? command : NULL);
        if(error) break;
    }
    if(!error && enable) error=set_shell(c,gate);
failed:
    if(error) {
        DWORD rollback=0,part;
        for(j=0;j<3;++j) {
            part=put_string(c,keys[j],VALUE,old[j].present ? old[j].text : NULL);
            if(part) rollback=part;
        }
        part=set_shell(c,shell.present ? shell.text : NULL); if(part) rollback=part;
        if(changed_gate) {
            part=put_string(c,c->backup_key,"Gate",saved_gate.text); if(part) rollback=part;
            part=put_string(c,c->backup_key,"Valid","1"); if(part) rollback=part;
        }
        return rollback ? ERROR_CAN_NOT_COMPLETE : error;
    }
    /* Keep the original shell backup after Disable, including for an in-flight
       /shell invocation whose SYSTEM.INI command was cached by Windows. */
    return 0;
}

/* Relocation restores previous metadata, startup entries and shell on failure. */
DWORD startup_enable_managed(const StartupConfig *c,BOOL *migrated)
{
    static const char *names[]={"Owner","OriginalShell","HadShell","Gate","RecoveryFile","Valid"};
    static Entry owner,valid,metadata[6],entries[3],shell;
    const char *keys[3];
    StartupConfig previous;
    DWORD error,part,rollback=0;
    int i;
    *migrated=FALSE;
    error=startup_configure(c,TRUE);
    if(error!=ERROR_BUSY) return error;
    if(read_string(c,c->backup_key,"Owner",&owner) || !owner.present || same_executable(owner.text,c->executable) ||
       read_string(c,c->backup_key,"Valid",&valid) || !valid.present || lstrcmpA(valid.text,"1")) return ERROR_BUSY;
    keys[0]=c->service_key; keys[1]=c->run_key; keys[2]=c->once_key;
    for(i=0;i<6;++i) { error=read_string(c,c->backup_key,names[i],&metadata[i]); if(error) return error; }
    for(i=0;i<3;++i) { error=read_string(c,keys[i],VALUE,&entries[i]); if(error) return error; }
    error=read_shell(c,&shell); if(error) return error;
    previous=*c; previous.executable=owner.text;
    error=startup_configure(&previous,FALSE); if(error) return error;
    error=startup_configure(c,TRUE);
    if(!error) { *migrated=TRUE; return 0; }
    part=put_string(c,c->backup_key,"Valid",NULL); if(part) rollback=part;
    for(i=0;i<6;++i) {
        part=put_string(c,c->backup_key,names[i],metadata[i].present ? metadata[i].text : NULL);
        if(part) rollback=part;
    }
    for(i=0;i<3;++i) {
        part=put_string(c,keys[i],VALUE,entries[i].present ? entries[i].text : NULL);
        if(part) rollback=part;
    }
    part=set_shell(c,shell.present ? shell.text : NULL); if(part) rollback=part;
    return rollback ? ERROR_CAN_NOT_COMPLETE : error;
}

/* Best-effort bounded metadata journal; no commands, usernames or edit contents. */
void startup_trace(HKEY root,const char *key,const char *stage,DWORD code)
{
    HKEY handle; HANDLE file;
    char path[MAX_PATH],line[160];
    DWORD size=sizeof(path),type,written,length;
    int end;
    if(RegOpenKeyExA(root,key,0,KEY_QUERY_VALUE,&handle)) return;
    type=0;
    if(RegQueryValueExA(handle,"Owner",NULL,&type,(BYTE *)path,&size) || type!=REG_SZ || !size || size>=sizeof(path) || path[size-1]) {
        RegCloseKey(handle); return;
    }
    RegCloseKey(handle); path[sizeof(path)-1]=0;
    end=lstrlenA(path);
    while(end && path[end-1]!='\\' && path[end-1]!='/') --end;
    if(!end || end+13>=MAX_PATH) return;
    lstrcpyA(path+end,"W95START.LOG");
    file=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE) return;
    length=SetFilePointer(file,0,NULL,FILE_END);
    if(length>65536) { SetFilePointer(file,0,NULL,FILE_BEGIN); SetEndOfFile(file); }
    wsprintfA(line,"%lu pid=%lu %s code=%lu\r\n",GetTickCount(),GetCurrentProcessId(),stage,code);
    WriteFile(file,line,(DWORD)lstrlenA(line),&written,NULL); CloseHandle(file);
}
