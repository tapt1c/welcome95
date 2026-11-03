# Supplied TrueType fonts

These unchanged files were supplied by the user for this reconstruction:

| File | Family / style | Internal version |
| --- | --- | --- |
| TAHOMA.TTF | Tahoma Regular | 7.05 |
| TAHOMABD.TTF | Tahoma Bold | 7.05 |
| ARIAL.TTF | Arial Regular | 7.07 |
| ARIALBI.TTF | Arial Bold Italic | 7.07 |

Font names and formats were inspected with fontTools. These are version 7.x
fonts, not a claim that they are the original versions shipped with Windows XP.
Font artwork, metadata and ownership remain with their respective authors.
The project does not apply a new license to the supplied Microsoft assets.

The XP UIFILE and string resources specify Arial Bold Italic at 36 pt for welcome,
with a shadow offset of 2×3, Tahoma for accounts/footer, and Arial for instructions.
The application uses GDI point-to-pixel conversion at the current device DPI.
Actual rasterization still depends on OS font mapping and smoothing.

The four files are embedded as RCDATA. The application extracts its own uniquely
named temporary directory, calls the Windows 95 AddFontResource API, creates
its UI fonts, then deletes font objects, unregisters resources and removes owned
temporary files on normal exit. It does not alter the installed-font registry.
Windows 95 has no private-font equivalent to modern AddFontResourceEx flags;
resource availability is session-wide while registered. If font loading fails,
GDI may choose an installed substitute. Forced termination can leave temporary
files behind, and real Windows 95 font acceptance still needs target validation.
