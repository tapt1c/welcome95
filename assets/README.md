# Reference assets

These bitmaps were extracted without executing the user-supplied Windows XP
`logonui.exe` (bitmap resources 100, 103, 107, 112, 114, 123–126, language 1033).
They retain their original dimensions and pixels. Windows artwork belongs to
Microsoft; it is not original Welcome95 artwork. The uploaded executable itself
is not bundled or executed. The Windows XP wordmark's suffix is covered and
rendered as “95” by the application. No XP executable code is used.

`power-logo.bmp` comes from Explorer95 resource Bitmap143 at the owner's request.
Source and commit: [docs/POWER_SOURCE.md](../docs/POWER_SOURCE.md).

The owner supplied `welcm95.ico` for release 1.0. `welcm95-original.ico` retains
that upload unchanged. `welcome95.ico` contains its eight native 4/8-bit DIB frames
at 16, 24, 32 and 48 pixels, byte-for-byte with their original palettes and masks.
PNG/ARGB-only frames are omitted for Windows 95 compatibility; no artwork was
redrawn. This icon is embedded in the main EXE and the Control Panel launcher.
The PE audit compares every embedded frame with the legacy ICO and checks the
main EXE's 1.0.0.0 file/product version resources.
