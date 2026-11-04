#!/usr/bin/env python3
"""Audit the actual PE image, including imports; this is not a runtime test."""
import pathlib
import struct
import sys


def check(path, bootstrap=False, panel=False):
    data = pathlib.Path(path).read_bytes()

    def u16(offset):
        return struct.unpack_from('<H', data, offset)[0]

    def u32(offset):
        return struct.unpack_from('<I', data, offset)[0]

    def require(condition, message):
        if not condition:
            raise ValueError(message)

    require(data[:2] == b'MZ', 'DOS signature missing')
    pe = u32(0x3c)
    require(data[pe:pe + 4] == b'PE\0\0', 'PE signature missing')
    require(u16(pe + 4) == 0x14c, 'Must target x86, not x64')
    optional = pe + 24
    require(u16(optional) == 0x10b, 'Must be PE32')
    require((u16(optional + 40), u16(optional + 42)) <= (4, 0), 'OS requirement exceeds Windows 95')
    require((u16(optional + 48), u16(optional + 50)) <= (4, 0), 'Subsystem requirement exceeds Windows 95')
    require(u16(optional + 68) == 2, 'Must be a GUI executable')
    require(u32(optional + 16) != 0, 'No entry point')
    section_table = optional + u16(pe + 20)
    sections = []
    for index in range(u16(pe + 6)):
        at = section_table + index * 40
        sections.append((u32(at + 12), max(u32(at + 8), u32(at + 16)), u32(at + 20)))

    def offset(rva):
        for start, size, raw in sections:
            if start <= rva < start + size:
                return raw + rva - start
        raise ValueError(f'Unmapped RVA {rva:x}')

    def string(at):
        return data[at:data.index(b'\0', at)].decode('ascii')

    # Deliberately small, reviewed Win95 API surface. A new import requires review.
    allowed = {
        'ADVAPI32.dll': set('RegFlushKey RegEnumKeyA RegCloseKey RegCreateKeyExA RegDeleteValueA RegOpenKeyExA RegQueryValueExA RegSetValueExA'.split()),
        'GDI32.dll': set('GetRgnBox SaveDC RestoreDC Arc GetDIBits SetDIBits GetStockObject SetBkColor MoveToEx LineTo StretchDIBits SetStretchBltMode SetBrushOrgEx CreateCompatibleBitmap BitBlt AddFontResourceA RemoveFontResourceA GetDeviceCaps CreatePen RoundRect CreateCompatibleDC DeleteDC GetObjectA StretchBlt CombineRgn CreateRectRgn CreateFontA CreateSolidBrush DeleteObject SelectObject SetBkMode SetTextColor'.split()),
        'KERNEL32.dll': set('CopyFileA ReadFile GetExitCodeProcess SetEndOfFile FlushFileBuffers MoveFileA ResetEvent OpenProcess GetPrivateProfileStringA WritePrivateProfileStringA GetProcessHeap HeapAlloc HeapFree GetSystemDirectoryA GetWindowsDirectoryA GetShortPathNameA GetProcAddress LoadLibraryA FreeLibrary lstrcatA GetStringTypeA GetFileAttributesA FindFirstFileA FindClose ExpandEnvironmentStringsA GetTempPathA GetTempFileNameA DeleteFileA CreateDirectoryA RemoveDirectoryA FindResourceA SizeofResource LoadResource LockResource GetACP GetSystemDefaultLangID CreateFileA WriteFile SetFilePointer lstrcpyA GetPrivateProfileSectionA lstrcpynA MulDiv CloseHandle CreateEventA CreateProcessA ExitProcess GetCommandLineA GetCurrentProcessId GetLastError GetModuleFileNameA GetModuleHandleA GetTickCount GetVersion OpenEventA SetEvent SetLastError Sleep WaitForSingleObject lstrcmpA lstrcmpiA lstrlenA'.split()),
        'USER32.dll': set('CallWindowProcA LoadIconA WindowFromPoint RedrawWindow EqualRect MsgWaitForMultipleObjects PeekMessageA BeginDeferWindowPos DeferWindowPos EndDeferWindowPos GetTopWindow SetScrollInfo DrawEdge DrawFocusRect GetFocus GetForegroundWindow GetWindow GetWindowTextA GetDlgCtrlID SetFocus ScreenToClient InflateRect IsChild OemToCharA GetDC ReleaseDC FindWindowA OffsetRect LoadBitmapA ExitWindowsEx BeginPaint ClientToScreen CreateWindowExA DefWindowProcA DestroyWindow DispatchMessageA DrawTextA EnableWindow EndPaint EnumChildWindows EnumWindows FillRect GetClassNameA GetClientRect GetDlgItem GetLastActivePopup GetMessageA GetParent GetSystemMetrics GetWindowLongA GetWindowRect GetWindowRgn GetWindowThreadProcessId InvalidateRect IsDialogMessageA IsWindow IsWindowEnabled IsWindowVisible KillTimer LoadCursorA MapWindowPoints MessageBoxA PostMessageA PostQuitMessage RegisterClassA RegisterHotKey SendDlgItemMessageA SendMessageA SendMessageTimeoutA SetDlgItemTextA SetForegroundWindow SetRect SetTimer SetWindowLongA SetWindowPos SetWindowRgn ShowWindow TranslateMessage UnregisterHotKey UpdateWindow wsprintfA'.split()),
    }
    allowed['SHELL32.dll']={'ShellExecuteExA'}
    imports = {}
    descriptor = offset(u32(optional + 104))
    while any(data[descriptor:descriptor + 20]):
        lookup, _, _, name_rva, address = struct.unpack_from('<5I', data, descriptor)
        name = string(offset(name_rva))
        require(name in allowed, f'Unsupported DLL dependency: {name}')
        thunk = offset(lookup or address)
        functions = []
        while u32(thunk):
            entry = u32(thunk)
            require(not entry & 0x80000000, f'Unreviewed ordinal import in {name}')
            function = string(offset(entry) + 2)
            require(function != 'GetWindowRgnBox', f'Unavailable on target Windows 95: {name}!{function}')
            require(function in allowed[name], f'Unreviewed import: {name}!{function}')
            functions.append(function)
            thunk += 4
        imports[name] = functions
        descriptor += 20
    require(set(imports) == (set(allowed) - {'GDI32.dll', 'SHELL32.dll'} if bootstrap or panel else set(allowed)), 'Unexpected or missing system DLLs')
    if panel:
        require(u16(pe + 22) & 0x2000, 'Control Panel launcher must be a DLL')
        export = offset(u32(optional + 96))
        count = u32(export + 24)
        names = offset(u32(export + 32))
        exports = {string(offset(u32(names + index * 4))) for index in range(count)}
        require(exports == {'CPlApplet'}, f'Unexpected applet exports: {exports}')
        # Legacy Win95 hosts obtain cacheable metadata through CPL_INQUIRE;
        # validate resources in the actual x86 DLL, not just the Wine fixture.
        resource_root = offset(u32(optional + 112))

        def resource(*ids):
            directory = 0
            for depth, identifier in enumerate(ids):
                at = resource_root + directory
                named, numbered = struct.unpack_from('<HH', data, at + 12)
                entries = at + 16
                found = None
                for index in range(named + numbered):
                    name, target = struct.unpack_from('<II', data, entries + index * 8)
                    if name == identifier:
                        found = target
                        break
                require(found is not None, f'Missing applet resource {ids}')
                if depth + 1 < len(ids):
                    require(found & 0x80000000, f'Invalid resource directory {ids}')
                    directory = found & 0x7fffffff
                else:
                    require(not found & 0x80000000, f'Invalid resource data {ids}')
                    rva, size = struct.unpack_from('<II', data, resource_root + found)
                    return data[offset(rva):offset(rva) + size]

        group = resource(14, 1, 0)
        require(len(group) >= 20 and struct.unpack_from('<HHH', group) == (0, 1, 1),
                'Missing legacy applet icon group')
        resource(3, struct.unpack_from('<H', group, 18)[0], 0)
        for language in (1033, 1049):
            block = resource(6, 1, language)
            strings = []
            at = 0
            for index in range(16):
                length = struct.unpack_from('<H', block, at)[0]
                at += 2
                strings.append(block[at:at + length * 2].decode('utf-16le'))
                at += length * 2
            require(strings[2] == 'Welcome95' and strings[3],
                    f'Missing static applet name/description for language {language}')
    require(u32(optional + 96 + 9 * 8) == 0, 'Unexpected TLS/CRT initialization')
    require(u32(optional + 96 + 14 * 8) == 0, '.NET is not supported')
    print(f'PASS: {path}: x86 GUI, Windows 4.0, {len(imports)} system DLLs, '
          f'{sum(map(len, imports.values()))} reviewed imports, no CRT/.NET/TLS')


if __name__ == '__main__':
    try:
        check(sys.argv[1] if len(sys.argv) > 1 else 'build/welcome95.exe', '--bootstrap' in sys.argv[2:], '--panel' in sys.argv[2:])
    except (ValueError, OSError, struct.error, IndexError) as error:
        sys.exit(f'FAIL: {error}')
