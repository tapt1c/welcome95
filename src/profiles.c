#include <windows.h>
#include "profiles.h"
#define MAX_PROFILES 32
static char names[MAX_PROFILES][128];
static int count;
int profiles_count(void) { return count; }
const char *profiles_name(int index) { return index >= 0 && index < count ? names[index] : "Windows user"; }
int profiles_remember(const char *name)
{
    int index;
    if (!name || !*name || lstrlenA(name) >= 128) return -1;
    for (index = 0; index < count; ++index)
        if (!lstrcmpiA(names[index], name)) return index;
    if (count == MAX_PROFILES) return -1;
    lstrcpynA(names[count], name, sizeof(names[0]));
    return count++;
}
void profiles_load(void)
{
    HKEY key;
    DWORD index;
    char name[128], *entry, *equals;
    static char section[8192];
    count = 0;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
        "Software\\Microsoft\\Windows\\CurrentVersion\\ProfileList", 0, KEY_READ, &key) == ERROR_SUCCESS) {
        for (index = 0; RegEnumKeyA(key, index, name, sizeof(name)) == ERROR_SUCCESS; ++index)
            profiles_remember(name);
        RegCloseKey(key);
    }
    /* Read name-to-file mappings only. Never open, parse or decrypt PWL files. */
    GetPrivateProfileSectionA("Password Lists", section, sizeof(section), "system.ini");
    for (entry = section; *entry; entry += lstrlenA(entry) + 1) {
        equals = entry;
        while (*equals && *equals != '=') ++equals;
        if (*equals == '=') {
            int length = (int)(equals - entry), i;
            if (length > 0 && length < (int)sizeof(name)) {
                for (i = 0; i < length; ++i) name[i] = entry[i];
                name[length] = '\0'; profiles_remember(name);
            }
        }
    }
}
