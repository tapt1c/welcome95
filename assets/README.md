# Reference assets

These bitmaps were extracted without executing the user-supplied Windows XP
`logonui.exe` (bitmap resources 100, 103, 107, 112, 114, 123–126, language 1033).
They retain their original dimensions and pixels. Windows artwork belongs to
Microsoft; it is not original Welcome95 artwork. The uploaded executable itself
is not bundled or executed. The Windows XP wordmark's suffix is covered and
rendered as “95” by the application. No XP executable code is used.

`power-logo.bmp` comes from Explorer95 resource Bitmap143 at the owner's request.
Source and commit: [docs/POWER_SOURCE.md](../docs/POWER_SOURCE.md).

`welcome95.ico` repackages the same 16x16, 4-bit pixels and palette from
`power-logo.bmp` as an icon resource with an opaque AND mask. It supplies the
cacheable CPL_INQUIRE icon; it introduces no new artwork or external icon source.
