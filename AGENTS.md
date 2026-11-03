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
Read account names from ProfileList and SYSTEM.INI [Password Lists]; ProfileImagePath
may be queried only for canonical directory-name spelling, never logged or read
for profile contents. Clear
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
uploaded XP executable. Keep one shared layout for rendering, controls and hit
tests. Artwork/control dimensions stay in pixels; use center/right/bottom anchors when resolution changes.
Prefer clear Russian user updates. Report what changed, what was tested, and any
remaining target-system uncertainty. Publish only within user-authorized scope.

## Session status screens and fonts

The lone/multiple-account welcome states follow session-ready; Windows must
authenticate even the single-account blank-password attempt. Never bypass
authentication or retry it automatically. The /welcome controller is a Win9x
registered service; /session-ready is an ordinary per-session notifier. Keep the
controller across logoff and reset every per-session signal/selection/attempt.
Win9x end-session notifications without a reason must not kill the controller:
release the canvas/fonts, then wait for a new provider or OS startup handshake.
Known power actions can exit normally; the OS eventually stops an ambiguous-end
service after its temporary resources have been released. Preserve cancellation.
Preview must never register as a service, modify startup or launch a real shell.

Use the supplied TTF resources via src/fonts.c. Do not install fonts permanently,
overwrite user files, or introduce NT-only private-font APIs. Unique temporary
files belong to the process; unregister only resources it loaded. Keep metadata
and ownership notices intact. Use the user-supplied XP font versions documented in assets/fonts/README.md.

## Names and VGA rendering

Windows usernames are case-insensitive; do not invent case-sensitive identities.
The native Edit spelling outranks PWL casing for display. Known profile names
outrank unrelated cache aliases. Never guess a universal OEM/ANSI conversion.
Use DEFAULT_CHARSET. Keep font sizes independent of screen resolution; system
DPI may still affect point-size fonts.
Use the supported COLORONCOLOR mode for bitmap shrinking; check actual GDI pixels.
Run VGA checks with WELCOME95_TEST_SCREEN=640x480x16 and the default 800x600x24.

For manual updates, stop the old observer with Disable and close its settings
before replacing the EXE; then Enable the new copy and restart Windows.


## Power dialog and single-account login

Close the shade and restore exactly the originally enabled input windows before
calling the OS. Do not force-close applications or cover their save/cancel prompts.
Preview and automated tests must never execute real power, suspend or DOS actions.
Use the inert operation/PIF hosts in the regression tests. Keep dynamic optional
power APIs and the Win95-only gate; SHELL32 ShellExecuteExA is a baseline dependency.
Record Explorer95 source changes in docs/POWER_SOURCE.md.

Single-account auto-login may submit only one empty-password attempt through the
native provider, after account selection and native OK are ready. Windows owns
authentication. Do not read input, cache credentials, cancel a rejected login or
retry automatically; preserve manual input after failure.


Below 800px width or 600px height use the stacked compact composition: fixed logo
above two visible rows, vertical navigation and a greeting above the loading card.
Use page_size/page_first, card_x and row_y consistently; changing resolution must
preserve selection and keep the native field and arrow above the footer. In a
compact multi-account list, attach native input only after a card is opened.
If native OK is initially disabled, consume automatic-login eligibility without
submitting; never wait until newly typed manual input enables it.


## Boot integration and recovery

Enable owns only three Welcome95 values (RunServices, Run, RunOnce) and the
SYSTEM.INI [boot] shell value. Save and flush the original value/presence before
changing it, including a unique DOS-readable W95xxxx.BAK beside the executable.
Never overwrite another install's entries, external shell changes or old backups.
Disable restores the original shell and retains recovery metadata for cached or
in-flight gate invocations. Every install mutation needs rollback/readback tests
against private HKCU/INI fixtures, not actual boot configuration under Wine.

The shell gate inherits the logged-on environment. It may launch the saved shell
only after the controller's close acknowledgement; acknowledge after DestroyWindow
returns. Beginning another canvas must not set that acknowledgement. Serialize a
new OS session handshake with pending end-session notifications. Keep bounded waits
and release startup on Disable/failure; do not suspend/terminate arbitrary apps.
Never read password text or infer successful authentication from a button click.

Idle z-order maintenance must cause no SetWindowPos churn. Batch necessary changes
so the canvas never repeatedly covers/uncover the native Edit. Keep hidden windows
out of the visible-priority comparison and preserve focus and owned OS prompts.
Run startup_test/session_test plus existing suites; distinguish inter-process
Wine fixtures from actual Win95 boot, service survival and startup ordering.

Compare executable ownership by file path identity, including Win95 long/8.3
aliases. GetModuleFileName may return a different path spelling in /shell than
in Settings. A short-path gate must read the long-path backup and startup entries.
