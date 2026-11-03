# Welcome95 contributor instructions

## Purpose and platform

Welcome95 is a native ANSI Win32/GDI welcome screen for Windows 95. Keep the
production binary x86/i386, PE subsystem 4.0, with no modern CRT or .NET dependency.
Use the existing checkout; do not create a worktree unless requested.
Read README.md, docs/TESTING.md and BAKCLOG.md before changing login integration.
The deliberately requested backlog filename is `BAKCLOG.md`.

## Authentication and profiles

Windows must keep ownership of its native password Edit and authenticate input.
Never read, log, copy or persist a real password. Do not open/decrypt PWL files.
Read only account names from ProfileList and SYSTEM.INI [Password Lists]. Clear
old input before changing the native username. Do not replace authentication
with a successful click, cached-password comparison or synthetic success signal.

Keep native-control parents, IDs and handlers intact. Save and restore geometry,
styles, visibility and Cancel state. Unsupported dialogs stay behind the canvas;
Trouble signing in? explicitly restores their normal view. Keep logon error
messages accessible. Esc/Alt+F4 resistance is not a security boundary.

## Responsive UI

Card selection must update immediately even before native attachment succeeds.
Do not perform cross-process operations inside mouse/button selection handlers.
Queue the requested profile and service it separately. Bound synchronous messages
and permit callbacks; guard the service against reentrant timer messages.
Avoid repeated SetWindowPos/style/region writes when geometry has not changed.
Never claim a Win95 hang's precise cause without evidence or diagnostics.

Diagnostics may record reason codes, control classes, IDs, styles and states.
They must not record edit text, passwords, profile paths or credentials.

## Build and validation

From the repository root:

    sh tools/build.sh all test-helper
    python3 tests/check_pe.py build/welcome95.exe
    sh tools/test-wine.sh
    git diff --check

The prepared cloud machine has local MinGW in /workspace/.tools/mingw and optional
Wine64/Xvfb/x64 MinGW in /workspace/.tools/win-tests. Wine/Xvfb need local sockets;
use the supported sandbox escalation when necessary. Keep TLS, APT signatures
and platform proxy authentication enabled. The Wine runner uses its own profile
and display, not the user's personal Wine profile. Preserve command exit statuses.

The production EXE at build/welcome95.exe is intentionally tracked for download.
Other build outputs are ignored. Commit its rebuilt version with source changes
when preparing a distributable update. Assets are embedded with windres.

Run relevant existing window tests after integration changes; add a regression
for a concrete failure. Separate tests executed under Wine from Windows 95 boot
and cross-process network-provider validation. The latter requires the target OS.
Do not call unexecuted target checks passed. Update BAKCLOG.md with remaining
limitations and evidence. Preserve existing user changes.

## Presentation and delivery

The XP resources in assets came from a user-supplied reference executable; their
origin is documented in assets/README.md. Do not execute or redistribute the
uploaded XP executable. Keep one shared, scaled layout for rendering and hit tests.
Prefer clear Russian user updates. Report what changed, what was tested, and any
remaining target-system uncertainty. Publish only within user-authorized scope.

## Session status screens and fonts

The lone/multiple-account welcome states follow session-ready; they never bypass
password entry. /session-ready is a hidden observer after signalling the startup
canvas. It must approve shutdown queries, handle cancellation, exit normally on
committed end-session, and release its font resources. Do not claim unknown OS
shutdown phases as observed events. Preview states must never call ExitWindowsEx.

Use the supplied TTF resources via src/fonts.c. Do not install fonts permanently,
overwrite user files, or introduce NT-only private-font APIs. Unique temporary
files belong to the process; unregister only resources it loaded. Keep metadata
and ownership notices intact. Font versions 7.x are not the original XP versions.
