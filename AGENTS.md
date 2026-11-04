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


## Legacy shell path and rescue

Never assume GetShortPathName removes non-ASCII characters. Production SYSTEM.INI
must contain only an ASCII 8.3 bootstrap executable, with no arguments; it forwards
the owner's full ANSI path through Win32. Embed/rebuild/audit build/w95boot.exe as
resource 300. The same bootstrap routes Run/RunServices/RunOnce. Retain recovery
helpers, use unique owned directories and preserve the original shell on migration.

Ctrl+Alt+Shift+L is deliberately undisclosed in the product UI. Restore an existing
native form, hide the canvas and stop z-order enforcement; do not manufacture a
new credential dialog, submit input or signal readiness/closure as authentication.
Only start a new welcome cycle on real session signals/provider appearance.
Stage logs may contain constant stage tags, ticks, PIDs and numeric codes only.
No paths, command lines, usernames or edit contents in W95START.LOG.


## Native field and owner-draw repaint

A cached layout must validate the actual native Edit visibility/rectangle/styles
and clipping region, not just the dialog bounds. Repairs must preserve typed input
and Windows-disabled states. Queue cross-process painting; do not synchronously
send WM_PAINT to the provider. Idle layouts must stay mutation-free.

After presenting a WS_CLIPCHILDREN parent buffer, explicitly repaint its custom
buttons. Preserve the caller's GDI DC state and honor DRAWITEMSTRUCT's item origin.
Check real pixels after shade cancellation and parent damage, not only commands.


The user's Windows 95 USER32 does not export GetWindowRgnBox. Never statically
import it or add it back to the PE whitelist. Use GetWindowRgn + GDI32.GetRgnBox,
with a separate reusable probe region; preserve the original restoration region.
MinGW headers/import libraries and a passing Wine run do not establish Win95 API
availability. Check new imports against target evidence before allowing them.


The target Win95 dialog retained a (3,22) client origin after removing chrome.
Shrinking the outer window to 22px clipped the entire Edit outside its parent.
Keep original native dialog dimensions and frame/Edit styles; crop only the
actual Edit client pixels. Measure both nonclient offsets, preserve the Edit's
native client anchor, and verify containment in the parent's client rectangle.
Test a legacy WM_NCCALCSIZE fixture that retains (3,22), both reported control
inventories (14 controls / Edit 23 and 8 controls / Edit 31), real system mouse
and keyboard events, and visible synthetic mask pixels. Focus alone is inadequate.


Use src/ui_text.h for CP1251 Russian/English UI selection. Keep account identity,
registry values, CLI switches and metadata identifiers independent of UI language.
WELCOME95_TEST_RUSSIAN is a test-build flag only. Measure translated button text
as single-line text; long explanatory panels may wrap.

For larger native password glyphs, WM_SETFONT(NULL) selects the provider's own
system font. Preserve/restore its original WM_GETFONT value; do not pass a custom
font owned by another process or resize the 167x22 client for a font change.


## Quiet startup before single-account authentication

Keep VIEW_STARTING separate from authenticated Welcome states. A quiet native
probe may snapshot/classify roles but must not embed, resize, restyle, focus or
change the font/visibility of native children. Reject modified input by metadata.
Preserve one-attempt eligibility: disabled OK, notice, replacement form or bounded
timeout returns manual input without retrying, dismissing native errors or
clearing subsequent input. Posted OK is not session readiness. Keep the real OS
ready event and post-DestroyWindow close acknowledgement. /preview-starting is
inert. Test probing-to-manual reattachment, including shutdown cancellation.


## Managed LOGON installation and manual single-account welcome

Interactive no-argument launches on Win9x prepare %WINDIR%\LOGON\WELCOME95.EXE
and relaunch settings there before loading fonts. Helper and preview modes must
not install. Validate the ownership marker, stage copies, retain unique old EXE
backups and preserve the destination on failure/lock/foreign collision.
Known Welcome95 relocation is authorized through the recorded legacy Owner,
original-shell backup and owned startup entries; never take over foreign entries
or externally changed shell. Roll back exact old metadata/entries/shell on
failed migration. Test this against private HKCU/files, not actual Wine startup.
All newly managed diagnostics, stage logs, shell/EXE backups, font and DOS/PIF
files belong under LOGON. Keep cached old bootstraps/recovery files intact.

After manual VIEW_LOGIN, confirmed loading must show the selected account even
if it is the only account. Only quiet automatic VIEW_STARTING may proceed to
the lone greeting. Derive this from the UI phase, never by reading password
contents. Preserve real OS-ready gating, loading delay and close acknowledgement.

## Installer access contracts and early diagnostics

RegFlushKey requires KEY_QUERY_VALUE even when the key was opened for writes.
Do not rely on Wine's permissive implementation; keep the strict API-contract
startup fixture. Allocate shell recovery BAKs with CREATE_NEW and bounded unique
names. Retry ACCESS_DENIED only for a confirmed existing destination; retain
its contents and never mask a genuine denied write/flush as success.
Setup diagnostics must work before Owner exists, remain in the managed LOGON
directory through legacy migration, preserve LastError and contain only fixed
operation tags and numeric codes. Preserve the actual failure stage through
successful rollback. A target screenshot with code 5 alone does not establish
which API failed; state reproduced defects separately from target confirmation.


## Logoff account choice and Control Panel launcher

Keep require_account_choice across cycles after committed logoff, ambiguous
end-session and provider/shell fallback. Do not rearm empty submission in
begin_logon_cycle or a ready-event-only recovery. Initially collapse even one
card at every resolution and defer native attachment until explicit selection.
Cancelled end-session must preserve the initial startup policy.

The native Control Panel item is a CPL launcher only; Settings stays in the EXE.
Keep CPL and all its versions under LOGON. Registration owns only Welcome95
in the standard Control Panel\Cpls key and the private ControlPanel Entry record.
Validate owner/path/size/hash, preserve foreign values and loaded modules, and
roll registration back when Enable fails. Disable restores startup independently
of the applet and keeps the settings icon available for reenabling.
Production must export only undecorated CPlApplet; never ship PanelTestHost.
Audit the native CPL with tests/check_pe.py --panel in addition to main/boot.
Use private HKCU/files plus inert launch callbacks for lifecycle tests, never
the host's actual Control Panel registration or a real settings/shell launch.


## Canvas presentation after logoff

Committed logoff must retire the old canvas/fonts and wait for a new native
provider or real OS startup handshake before allocating the next canvas.
Retirement must never signal CanvasClosed, even briefly before resetting it;
only authenticated handoff after DestroyWindow may publish that acknowledgement.
Create the canvas hidden, publish its HWND, then show and explicitly invalidate
and synchronously redraw its full client and children. UpdateWindow alone cannot
restore an already consumed update region. Keep the session regression with
construction/show damage consumed, a contrasting background and real GDI pixels.


For Win95 CPL compatibility, keep real icon/name/description resources returned
by CPL_INQUIRE; do not require CPL_NEWINQUIRE. Audit the actual x86 resources.
Register the same owned LOGON module in CONTROL.INI [MMCPL] and the existing Cpls
value, with ownership checks, exact rollback/readback and foreign-entry protection.
The documented zero BOOL on an INI cache flush is not itself an error; clear/check
LastError and verify the entry. Test first install, registry-only upgrade,
loaded-version rollback and an INI replacement made after the snapshot.
Explorer95's simplified Control Panel is a fixed list; its all-options view uses
the system namespace. Do not claim to have changed that other product's list.
