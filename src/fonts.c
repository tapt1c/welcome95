#include "fonts.h"

static char directory[MAX_PATH];
static struct {
    const char *name;
    char path[MAX_PATH];
    BOOL created, loaded;
} fonts[] = {
    { "TAHOMA.TTF", "", FALSE, FALSE },
    { "TAHOMABD.TTF", "", FALSE, FALSE },
    { "ARIAL.TTF", "", FALSE, FALSE },
    { "ARIALBI.TTF", "", FALSE, FALSE }
};

const char *bundled_fonts_directory(void) { return directory; }

void bundled_fonts_release(void)
{
    unsigned int index;
    for (index = 0; index < sizeof(fonts)/sizeof(fonts[0]); ++index) {
        if (fonts[index].loaded && RemoveFontResourceA(fonts[index].path)) fonts[index].loaded = FALSE;
        if (fonts[index].created && !fonts[index].loaded && DeleteFileA(fonts[index].path)) fonts[index].created = FALSE;
    }
    if (*directory && RemoveDirectoryA(directory)) directory[0] = '\0';
}

int bundled_fonts_load(HINSTANCE instance)
{
    char temp[MAX_PATH];
    DWORD length, size, written;
    unsigned int index;
    int loaded = 0;
    HRSRC resource;
    HGLOBAL memory;
    const void *data;
    HANDLE file;
    int end,start;
    BOOL managed=FALSE;
    if(*directory) return 0;
    length=GetModuleFileNameA(NULL,temp,sizeof(temp));
    if(length && length<sizeof(temp)) {
        end=(int)length;
        while(end && temp[end-1]!='\\' && temp[end-1]!='/') --end;
        if(end) {
            temp[end-1]=0; start=end-1;
            while(start && temp[start-1]!='\\' && temp[start-1]!='/') --start;
            managed=!lstrcmpiA(temp+start,"LOGON"); length=(DWORD)(end-1);
        }
    }
    if(!managed) length=GetTempPathA(sizeof(temp),temp);
    if(!length || length>=sizeof(temp)-32) return 0;
    if (!GetTempFileNameA(temp, "W95", 0, directory)) return 0;
    /* GetTempFileName creates an owned placeholder. Never reuse or overwrite
       existing font files; each process creates its own fresh directory. */
    if (!DeleteFileA(directory) || !CreateDirectoryA(directory, NULL)) { directory[0] = '\0'; return 0; }
    for (index = 0; index < sizeof(fonts)/sizeof(fonts[0]); ++index) {
        resource = FindResourceA(instance, MAKEINTRESOURCEA(1001+index), MAKEINTRESOURCEA(10));
        if (!resource) continue;
        size = SizeofResource(instance, resource);
        memory = LoadResource(instance, resource);
        data = memory ? LockResource(memory) : NULL;
        if (!data || !size || lstrlenA(directory)+lstrlenA(fonts[index].name)+2 >= MAX_PATH) continue;
        wsprintfA(fonts[index].path, "%s\\%s", directory, fonts[index].name);
        file = CreateFileA(fonts[index].path, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
        if (file == INVALID_HANDLE_VALUE) continue;
        fonts[index].created = TRUE;
        written = 0;
        if (!WriteFile(file, data, size, &written, NULL)) written = 0;
        CloseHandle(file);
        if (written != size) continue;
        if (AddFontResourceA(fonts[index].path)) { fonts[index].loaded = TRUE; ++loaded; }
    }
    return loaded;
}
