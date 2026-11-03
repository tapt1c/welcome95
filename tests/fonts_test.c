/* Test only the supplied embedded fonts, using the normal GDI font loader. */
#include <windows.h>
#include "../src/fonts.h"
static HANDLE log_file;
static int failures;
static void check(BOOL condition, const char *text)
{
    DWORD written;
    const char *prefix = condition ? "PASS: " : "FAIL: ";
    if (!condition) ++failures;
    WriteFile(log_file, prefix, (DWORD)lstrlenA(prefix), &written, NULL);
    WriteFile(log_file, text, (DWORD)lstrlenA(text), &written, NULL);
    WriteFile(log_file, "\r\n", 2, &written, NULL);
}
static DWORD be32(const BYTE *data)
{
    return ((DWORD)data[0]<<24) | ((DWORD)data[1]<<16) | ((DWORD)data[2]<<8) | data[3];
}
static BOOL matches_embedded_face(HDC dc, int resource_id)
{
    static BYTE table[65536];
    HINSTANCE instance = GetModuleHandleA(NULL);
    HRSRC resource = FindResourceA(instance, MAKEINTRESOURCEA(resource_id), MAKEINTRESOURCEA(10));
    HGLOBAL memory = resource ? LoadResource(instance, resource) : NULL;
    const BYTE *data = memory ? (const BYTE *)LockResource(memory) : NULL;
    DWORD size = resource ? SizeofResource(instance, resource) : 0;
    DWORD index, count, at, offset, length;
    if (!data || size < 12) return FALSE;
    count = ((DWORD)data[4]<<8) | data[5];
    if (count > (size-12)/16) return FALSE;
    for (index = 0; index < count; ++index) {
        at = 12 + index*16;
        if (be32(data+at) != 0x6e616d65UL) continue;
        offset = be32(data+at+8); length = be32(data+at+12);
        if (offset > size || length > size-offset || length > sizeof(table)) return FALSE;
        if (GetFontData(dc, 0x656d616eUL, 0, table, length) != length) return FALSE;
        for (index = 0; index < length; ++index)
            if (table[index] != data[offset+index]) return FALSE;
        return TRUE;
    }
    return FALSE;
}
void fonts_test_entry(void)
{
    HINSTANCE instance = GetModuleHandleA(NULL);
    HDC dc;
    HFONT font;
    HGDIOBJ old;
    TEXTMETRICA metrics;
    char directory[MAX_PATH], face[64];
    log_file = CreateFileA("fonts-test.log", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (log_file == INVALID_HANDLE_VALUE) ExitProcess(2);
    check(bundled_fonts_load(instance) == 4, "extract and register all four supplied TTF resources");
    lstrcpynA(directory, bundled_fonts_directory(), sizeof(directory));
    check(*directory && GetFileAttributesA(directory) != INVALID_FILE_ATTRIBUTES, "use a dedicated temporary font directory");
    check(bundled_fonts_load(instance) == 0, "duplicate initialization does not overwrite existing font files");
    dc = CreateCompatibleDC(NULL);
    font = CreateFontA(-48, 0, 0, 0, FW_BOLD, TRUE, FALSE, FALSE, ANSI_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, "Arial");
    old = SelectObject(dc, font);
    GetTextFaceA(dc, sizeof(face), face); GetTextMetricsA(dc, &metrics);
    check(!lstrcmpiA(face, "Arial") && metrics.tmItalic && metrics.tmWeight >= FW_BOLD,
          "GDI selects Arial Bold Italic for the welcome lettering");
    check(matches_embedded_face(dc, 1004), "selected Arial Bold Italic matches the supplied XP font name table");
    SelectObject(dc, old); DeleteObject(font);
    font = CreateFontA(-19, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, "Tahoma");
    old = SelectObject(dc, font); GetTextFaceA(dc, sizeof(face), face);
    check(!lstrcmpiA(face, "Tahoma"), "GDI selects Tahoma for account text");
    check(matches_embedded_face(dc, 1001), "selected Tahoma matches the supplied XP font name table");
    SelectObject(dc, old); DeleteObject(font); DeleteDC(dc);
    bundled_fonts_release();
    check(GetFileAttributesA(directory) == INVALID_FILE_ATTRIBUTES, "unregister fonts and remove owned temporary files");
    check(bundled_fonts_load(instance) == 4, "font setup can be repeated after cleanup");
    bundled_fonts_release(); CloseHandle(log_file);
    ExitProcess(failures ? 1 : 0);
}
