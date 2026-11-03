#include <windows.h>
#include "profiles.h"
#define MAX_PROFILES 32
static char names[MAX_PROFILES][128];
static int count;
int profiles_count(void) { return count; }
const char *profiles_name(int index) { return index >= 0 && index < count ? names[index] : "Windows user"; }
int profiles_find(const char *name)
{
    int index;
    if (!name) return -1;
    for (index = 0; index < count; ++index)
        if (!lstrcmpiA(names[index], name)) return index;
    return -1;
}
static BOOL valid_name(const char *name)
{
    WORD types[128];
    int index, length;
    if (!name || !(length=lstrlenA(name)) || length >= 128) return FALSE;
    if (!lstrcmpA(name, ".") || !lstrcmpA(name, "..")) return FALSE;
    if (!GetStringTypeA(LOCALE_USER_DEFAULT, CT_CTYPE1, name, length, types)) return FALSE;
    for (index=0; index<length; ++index) {
        if ((!types[index] || (types[index]&C1_CNTRL)) || name[index]=='\\' || name[index]=='/' ||
            name[index]=='*' || name[index]=='?' || name[index]=='=' || name[index]=='[' || name[index]==']') return FALSE;
    }
    return TRUE;
}
int profiles_remember(const char *name)
{
    int index;
    if (!valid_name(name)) return -1;
    index=profiles_find(name);
    if (index>=0) return index; /* Keep the first authoritative spelling. */
    if (count == MAX_PROFILES) return -1;
    lstrcpynA(names[count], name, sizeof(names[0]));
    return count++;
}
int profiles_accept_native(const char *name)
{
    int index;
    if (!valid_name(name)) return -1;
    index=profiles_find(name);
    if (index<0) return profiles_remember(name);
    /* The actual ANSI username Edit outranks an uppercase cache alias. */
    lstrcpynA(names[index],name,sizeof(names[0]));
    return index;
}
static void registry_spelling(HKEY root, const char *base, const char *name, char *canonical)
{
    HKEY key;
    char path[MAX_PATH], expanded[MAX_PATH], key_path[512];
    DWORD size=sizeof(path), type, length;
    WIN32_FIND_DATAA found;
    HANDLE search;
    lstrcpynA(canonical, name, 128);
    if (lstrlenA(base)+lstrlenA(name)+2 >= (int)sizeof(key_path)) return;
    wsprintfA(key_path, "%s\\%s", base, name);
    if (RegOpenKeyExA(root, key_path, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return;
    if (RegQueryValueExA(key, "ProfileImagePath", NULL, &type, (BYTE *)path, &size) != ERROR_SUCCESS ||
        (type!=REG_SZ && type!=REG_EXPAND_SZ) || !size || size>sizeof(path) || path[size-1]) { RegCloseKey(key); return; }
    RegCloseKey(key);
    length=ExpandEnvironmentStringsA(path, expanded, sizeof(expanded));
    if (!length || length>sizeof(expanded)) return;
    search=FindFirstFileA(expanded, &found);
    if (search==INVALID_HANDLE_VALUE) return;
    FindClose(search);
    if ((found.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) && !lstrcmpiA(found.cFileName, name))
        lstrcpynA(canonical, found.cFileName, 128);
}
static BOOL pwl_mapping_exists(const char *path)
{
    DWORD attributes;
    int length=lstrlenA(path);
    if (length<4 || lstrcmpiA(path+length-4, ".pwl")) return FALSE;
    attributes=GetFileAttributesA(path);
    return attributes!=INVALID_FILE_ATTRIBUTES && !(attributes&FILE_ATTRIBUTE_DIRECTORY);
}
static void load_sources(HKEY root, const char *profile_key, const char *ini_file)
{
    HKEY key;
    DWORD index, result, length;
    int registry_count;
    char name[128], canonical[128], converted[128], *entry, *equals;
    static char section[8192];
    count=0;
    if (RegOpenKeyExA(root, profile_key, 0, KEY_READ, &key)==ERROR_SUCCESS) {
        for (index=0;; ++index) {
            result=RegEnumKeyA(key, index, name, sizeof(name));
            if (result==ERROR_NO_MORE_ITEMS) break;
            if (result!=ERROR_SUCCESS) { if (result==ERROR_MORE_DATA) continue; break; }
            registry_spelling(root, profile_key, name, canonical);
            profiles_remember(canonical);
        }
        RegCloseKey(key);
    }
    registry_count=count;
    length=GetPrivateProfileSectionA("Password Lists", section, sizeof(section), ini_file);
    if (length>=sizeof(section)-2) return; /* Do not interpret a truncated record. */
    for (entry=section; *entry; entry+=lstrlenA(entry)+1) {
        equals=entry;
        while (*equals && *equals!='=') ++equals;
        if (*equals=='=') {
            int size=(int)(equals-entry), i;
            if (size<=0 || size>=(int)sizeof(name)) continue;
            for (i=0;i<size;++i) name[i]=entry[i];
            name[size]='\0';
            /* Existing profile names outrank cache aliases. OEM conversion is
               accepted only when it matches a known profile, never guessed. */
            if (registry_count) {
                if (profiles_find(name)>=0) continue;
                if (OemToCharA(name, converted) && profiles_find(converted)>=0) continue;
                continue;
            }
            if (valid_name(name) && pwl_mapping_exists(equals+1)) profiles_remember(name);
        }
    }
}
void profiles_load(void)
{
    load_sources(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion\\ProfileList", "system.ini");
}
