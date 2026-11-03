/* Isolated registry/INI fixtures; never read a real PWL file or profile. */
#include "../src/profiles.c"
static HANDLE output;
static int failures;
static void check(BOOL condition, const char *text)
{
    DWORD written;
    const char *prefix=condition ? "PASS: " : "FAIL: ";
    if(!condition) ++failures;
    WriteFile(output,prefix,(DWORD)lstrlenA(prefix),&written,NULL);
    WriteFile(output,text,(DWORD)lstrlenA(text),&written,NULL);
    WriteFile(output,"\r\n",2,&written,NULL);
}
static BOOL empty_file(const char *path)
{
    HANDLE file=CreateFileA(path,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE) return FALSE;
    CloseHandle(file); return TRUE;
}
void profiles_test_entry(void)
{
    char temp[MAX_PATH], folder[MAX_PATH], profile[MAX_PATH], upper[MAX_PATH], ini[MAX_PATH];
    char pwl[MAX_PATH], orphan[MAX_PATH], missing[MAX_PATH], key_path[128], child_path[160];
    HKEY root,child;
    DWORD disposition;
    output=CreateFileA("profiles-test.log",GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(output==INVALID_HANDLE_VALUE) ExitProcess(2);
    if(!GetTempPathA(sizeof(temp),temp) || !GetTempFileNameA(temp,"W95",0,folder) ||
        !DeleteFileA(folder) || !CreateDirectoryA(folder,NULL)) ExitProcess(2);
    wsprintfA(profile,"%s\\Daniel",folder); wsprintfA(upper,"%s\\DANIEL",folder);
    wsprintfA(ini,"%s\\fixture.ini",folder); wsprintfA(pwl,"%s\\Daniel.pwl",folder);
    wsprintfA(orphan,"%s\\orphan.pwl",folder); wsprintfA(missing,"%s\\missing.pwl",folder);
    if(!CreateDirectoryA(profile,NULL) || !empty_file(ini) || !empty_file(pwl) || !empty_file(orphan)) ExitProcess(2);
    wsprintfA(key_path,"Software\\Welcome95Tests\\Profiles%lu",GetCurrentProcessId());
    if(RegCreateKeyExA(HKEY_CURRENT_USER,key_path,0,NULL,0,KEY_ALL_ACCESS,NULL,&root,&disposition)!=ERROR_SUCCESS ||
        disposition!=REG_CREATED_NEW_KEY) ExitProcess(2);
    RegCreateKeyExA(root,"DANIEL",0,NULL,0,KEY_ALL_ACCESS,NULL,&child,NULL);
    RegSetValueExA(child,"ProfileImagePath",0,REG_SZ,(const BYTE *)upper,(DWORD)lstrlenA(upper)+1);
    RegCloseKey(child); RegCloseKey(root);
    WritePrivateProfileStringA("Password Lists","DANIEL",pwl,ini);
    WritePrivateProfileStringA("Password Lists","Ghost",orphan,ini);
    WritePrivateProfileStringA("Password Lists","Missing",missing,ini);
    load_sources(HKEY_CURRENT_USER,key_path,ini);
    check(profiles_count()==1,"known profiles do not acquire extra users from unrelated PWL cache aliases");
    check(!lstrcmpA(profiles_name(0),"Daniel"),"preserve actual profile-directory spelling instead of uppercase alias");
    check(profiles_find("DANIEL")==0 && profiles_find("Daniel")==0,"Windows case aliases identify one account");
    check(profiles_remember("DANIEL")==0 && !lstrcmpA(profiles_name(0),"Daniel"),"native uppercase spelling does not overwrite the canonical display name");
    check(profiles_remember("Bad\001Name")<0,"reject control characters in account names");
    check(profiles_remember("Andr\351")>=0,"preserve valid non-ASCII account names");
    wsprintfA(child_path,"%s\\Absent",key_path);
    load_sources(HKEY_CURRENT_USER,child_path,ini);
    check(profiles_count()==2 && profiles_find("DANIEL")>=0,"PWL-only systems still recognize existing cache mappings");
    check(profiles_find("Missing")<0,"a missing PWL mapping does not become a user");
    check(profiles_accept_native("Daniel")==0 && !lstrcmpA(profiles_name(0),"Daniel"),
        "actual native Edit spelling replaces uppercase PWL display casing");
    check(profiles_count()==2 && profiles_find("DANIEL")==0,
        "accepting native spelling does not create a different account");
    wsprintfA(child_path,"%s\\DANIEL",key_path);
    RegDeleteKeyA(HKEY_CURRENT_USER,child_path); RegDeleteKeyA(HKEY_CURRENT_USER,key_path);
    DeleteFileA(ini); DeleteFileA(pwl); DeleteFileA(orphan); RemoveDirectoryA(profile); RemoveDirectoryA(folder);
    CloseHandle(output); ExitProcess(failures ? 1 : 0);
}
