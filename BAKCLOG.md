# Welcome95 backlog

This filename (`BAKCLOG.md`) is intentional, as requested by the user.

## Current bug: selecting another user appears to freeze

Evidence: the user's 39-second Windows 95 recording shows responsive shutdown
but permanent “Preparing sign-in…” and inactive account selection. This establishes
that native attachment did not complete; it does not establish the exact reason.
The previous click handler refused to change selection unless native attachment
and the synchronous username update had already succeeded.

In this update:

- Card/page clicks update selection immediately and queue the native change.
- A blank initial native username no longer prevents attachment.
- The native edit is cleared before changing the username.
- Cross-process messages allow callbacks; timer servicing has a reentrancy guard.
- Identical layout calls avoid repeating style, region and geometry mutations.
- Failed attachment writes welcome95-diagnostics.txt beside the EXE. It contains
  reason codes and control metadata, never edit contents or passwords.

Confirmed diagnosis from the user's uploaded diagnostics: the native dialog has
14 controls, including active username Edit 21 and password Edit 23, plus hidden,
disabled Edit 25/29 and checkbox Button 26/30. The old classifier counted all
four Edit controls and rejected the dormant checkbox styles. It therefore never
attached despite the visible form being a simple username/password login.

Fixed: classify active roles after recording snapshots, ignoring controls only
when both hidden and disabled. A visible disabled or hidden enabled extra field
still rejects integration. Restore all original controls and states normally.
The window fixture reproduces all 14 class/ID/style combinations from the log.

Validation: 45 real-window checks passed under Wine64 (36 native integration,
9 canvas/priority checks), including account selection, input clearing, dormant
control preservation and rejection of extra fields that are not fully dormant.
The production x86 PE audit passed. Actual boot/login on the user's Win95 system
remains pending; later provider-specific failures are still possible.

## Required target-system checks

- [ ] Boot the updated EXE on Windows 95 and verify a blank/saved initial username.
- [ ] Select each account before typing; check actual native username and profile.
- [ ] Switch repeatedly: no old password input may carry into another account.
- [ ] Check Enter and green arrow with hidden original OK.
- [ ] Wrong password: accessible Windows error, then retry in the welcome screen.
- [ ] Confirm account discovery from actual ProfileList / SYSTEM.INI mappings.
- [ ] Verify 640×480, 800×600, multiple pages, and different ANSI/OEM localizations.
- [ ] Investigate the garbled first account name visible in the user's recording;
      determine its source and code page before applying any conversion.
- [ ] Verify shutdown, Disable, Trouble signing in? and safe-mode recovery.
- [ ] Cross-process integration with each supported Windows 95 network provider.

## Known architectural limits

- RunServices is asynchronous; appearing before the native dialog is not guaranteed.
- Run signals session startup, not authenticated success; Cancel may start a 95 session.
- No guarantee of covering Ctrl+Alt+Del, critical system screens or fullscreen DOS.
- Profiles are selected before login; live session switching is not implemented.
- Unknown provider forms require explicit native-dialog fallback.
- Up to 32 names and four cards per page; account creation/deletion belongs to Windows.
- All cards currently share the supplied chess avatar.

## Follow-up work

- Add a separate-process automated logon fixture with a temporarily unresponsive
  provider; current native tests use same-process fixtures under Wine64.
- Validate PWL mapping truncation/encoding and account enumeration errors.
- Add keyboard navigation and better visible feedback for failed profile changes.
- Confirm native restoration after resolution changes and provider-driven resizing.

## Welcome/shutdown state reconstruction and supplied fonts

Implemented loading states: one account has a lone greeting; multiple accounts
show only the selected account with loading text. The login/password phase remains
separate. Run's /session-ready now keeps a hidden session observer for ordinary
Windows shutdown notifications. Cancel restores the desktop; successful shutdown
exits through the message loop for font cleanup. Preview-only modes expire in 5 s.

Supplied Arial/Tahoma TTFs are embedded and temporarily registered via Win95 GDI.
XP resources identify Arial Bold Italic 36 pt with 2×3 shadow for welcome. Supplied
files are versions 7.x, so identical historical XP rasterization is not claimed.

Latest validation: 65 checks under Wine64: 36 native, 22 priority/state/observer,
7 font extraction/selection/cleanup checks. Three actual preview screenshots
were inspected. No real OS shutdown was triggered by tests.

- [ ] Validate one/many count and selected identity at real session-ready startup.
- [ ] Verify Russian CP1251 welcome/status rendering and Tahoma/Arial on Windows 95.
- [ ] Verify shutdown from Start menu, cancellation, Disable and helper exit on 95.
- [ ] Confirm temporary font cleanup on normal exits and observe forced-exit leftovers.
- [ ] Confirm final OS-owned shutdown screen handoff; ordinary EXE coverage ends
      when Windows closes the observer. No VxD or provider replacement is included.

## XP font replacement

The user supplied a second set extracted from Windows XP. Replaced the earlier
7.x font resources with Arial Regular 3.00, Arial Bold Italic 2.90 and Tahoma
Regular/Bold 3.15. Original bytes are preserved. Selected GDI font name tables
match the embedded Arial Bold Italic and Tahoma resources exactly under Wine64.
Latest test total: 67 (36 native, 22 states/priority, 9 font tests). Preview images
regenerated with the XP files. Acceptance/rasterization on real Win95 remains a
target-system check, not established by font-table equality under Wine.

## VGA layout, garbled cache alias and display casing

The user confirmed High/True Color and that the native Windows dialog says
Daniel. The old case-insensitive deduplication preserved an earlier uppercase
PWL alias. Accept the native Edit spelling as authoritative without creating
a separate Windows identity. Registry profile-directory spelling is an earlier
fallback. Windows account matching remains case-insensitive.

The old discovery added every PWL mapping as a user. Known registry profiles now
exclude unrelated cache aliases; PWL-only discovery checks mappings exist without
opening them. Control/undefined characters are rejected and valid non-ASCII names
are preserved. Native UI fonts use DEFAULT_CHARSET. Exact byte provenance of the
garbled entry in the user's screenshot is not independently established; verify
its removal on the target rather than claim its encoding was proven.

Fonts now follow the same 800x600 scaling as geometry. Source bitmaps are drawn
through DIBs with explicit COLORONCOLOR and double buffering; avoid legacy default
BLACKONWHITE and unsupported driver stretch modes. Native field container height
is limited to the Edit height. Experimental custom 8-bit palette code was tested
but discarded; it is not shipped and no pixel-perfect 256-color claim is made.

Validation: 82 checks at 640x480x16 and 800x600x24 under Wine64: 37 native, 23
canvas/states, 9 fonts, 10 profile-source/case checks and 3 actual GDI pixel checks.
VGA High/True Color screenshots inspected. Tests create isolated profile/INI
fixtures and never read real PWL data.

- [ ] Verify actual native Edit placement and removal of the extra strip on Win95.
- [ ] Verify the real user's Daniel spelling and unwanted-card removal after login.
- [ ] Verify Cyrillic source names on real ACP1251 Win95; do not infer success from
      an English Wine locale. Diagnostics now include resolution, depth and ACP only.


## Shutdown choice, fixed-pixel layout and one-account auto-login

Implemented the Explorer95-derived 314x200 shutdown choice with gradual GDI
snapshot dimming, cancellation/input restoration and optional Stand By/MS-DOS.
Shade and choice are removed before non-forced native power operations; shutdown
status no longer raises over other applications' save/cancel prompts. Preview
never dispatches a power action. Source provenance: docs/POWER_SOURCE.md.

Replaced resolution scaling with fixed artwork/control dimensions and shared
center/right/bottom anchors. Header/footer stay 78/96 px; fonts follow system DPI,
not screen size. Native and preview password fields both remain 167x22 px.
One discovered account gets one blank-password native OK at initial provider readiness;
multiple accounts and preview do not. Failed attempts leave manual input intact.
No PWL contents, stored passwords or synthetic success signals are involved.

Validation: 268 checks per resolution under Wine64/Xvfb at 640x480x16, 800x600x24
and 1024x768x24. Power/PIF callbacks are inert. Source compiles with C89/pedantic
warnings as errors; PE audit checks i386/Windows 4.0/no CRT and five baseline DLLs.
New preview screenshots show fixed-size layout at 1024 and the dimmed power choice.
These checks do not establish actual Windows 95 authentication or power behavior.

- [ ] Validate real Win95 single-profile empty-password success and password-required
      failure/manual recovery with each supported network provider.
- [ ] Validate APM suspend/resume, native save/cancel prompts and real shutdown/reboot.
- [ ] Validate the real-mode PIF path before logon and return from DOS on Win95.
- [ ] Check Russian ACP1251 labels and resolution changes with the real native Edit.
- [ ] Successful DOS dispatch retains its private PIF for asynchronous loading;
      consider conservative cleanup after confirmed later boot without deleting
      files that Windows may still need. Failures already clean their own files.


The user's additional 640x480 reference now has its own stacked composition:
logo/prompt above a two-row account viewport with a native vertical scrollbar.
A compact multi-account list starts collapsed; clicking even the initially
selected index opens its password card. Loading places the greeting above the
selected identity. Navigation and display changes use the same card anchors;
scroll selection queues provider work. Avatars use their native 48px bitmap
inside a 52px frame. The scrollbar uses the installed Windows theme.

Blank-login eligibility is consumed on the initial attached single-account
provider even if native OK is disabled. It stays manual in that case; enabling
OK after the user begins typing cannot trigger a delayed password-clearing attempt.
An already modified native Edit also remains manual (EM_GETMODIFY metadata only).
The current suite has 268 checks per resolution, including 37 layout/login checks.


## Recorded boot flicker, shell timing and logoff (current change)

Evidence: the supplied 27.35s recording shows a shell splash/desktop appearing
before welcome completes; the 9.3s logoff recording returns to the native network
password dialog without Welcome95. Existing code raised the canvas and then its
native input every 100ms, and exited both startup canvas and per-session observer.
This supports the fixes; a video alone does not prove every display-driver cause.

Implemented stable-priority checks and batched z-order updates. Added a registered
Win9x service controller, ordinary session-end notifier and per-cycle event reset.
Explicit logoff recreates login; reasonless session end releases fonts/canvas and
waits for the next provider or OS handshake. Shutdown cancellation preserves an
active login. Preview has its own instance lock and remains inert.

Enable installs a saved-shell proxy plus synchronous RunOnce gate. Both wait for
post-DestroyWindow acknowledgement and the existing >=1s loading interval. Pending
end-session notifications are serialized with new startup requests. The previous
shell command/presence is stored in the registry and a flushed, unique W95xxxx.BAK
beside the EXE before changing SYSTEM.INI. Readback failures roll back owned entries
and shell. Disable restores the shell, keeps recovery data and releases waiters.
Recursive shell configurations and foreign ownership are rejected. No process
suspension, password handling replacement or unrelated startup deletion is used.

Validation: 332 checks per resolution under Wine64/Xvfb: previous 268 plus 31 private startup
configuration/rollback/recovery cases and 33 real inter-process lifecycle checks.
The child shell is inert and only writes a time marker; logoff is synthetic.
Production remains CRT-free i386/Windows 4.0 with baseline system imports.
Implementation and DOS recovery: docs/BOOT_FLOW.md.

- [ ] Real Windows 95: confirm RegisterServiceProcess survival, actual MPR logoff
      notification behavior (including no reason), repeated logon and profile reload.
- [ ] Confirm SYSTEM.INI proxy + HKLM RunOnce order with both native Explorer and
      Explorer95; ordinary user startup must follow welcome closure.
- [ ] Recheck flicker with the actual display driver, password errors and topmost apps.
- [ ] Check shutdown cancellation, temporary-font cleanup and DOS recovery on the VM.

The old Run-only limitations above describe earlier versions; the current startup
model uses RunOnce plus a shell proxy. System services and unrelated RunOnce jobs
are not deferred by the welcome gate.


## Win95 login stall after shell replacement; non-ASCII desktop (current)

Evidence: a 40.75s recording shows responsive power UI but login never reaches the
welcome loading state. The user restored the previous SYSTEM.INI shell and login
worked again while delay disappeared. This localizes the regression to the shell
startup path, not a proven password-handler freeze. Restored shell makes enabled()
false and reactivates the old Run readiness signal, explaining loss of delay.
The main EXE was in Russian "Рабочий стол". The old gate assumed a short filename
was safe for the legacy loader; 8.3 is not an ASCII guarantee. Video alone does
not distinguish all OEM/ANSI, argument or loader-wait details on the actual VM.

Implemented an embedded i386 bootstrap deployed to a unique ASCII 8.3 directory
on the Windows drive. SYSTEM.INI points only to that executable, with no args.
It reads the owner through Win32 registry APIs and launches its full ANSI path
through CreateProcessA. All three startup entries also use the ASCII trampoline.
The bootstrap pumps messages during synchronous startup/shell waits. Original
shell backups, migration/rollback and post-close delay remain intact. Startup
errors release the overlay and show a visible error; bounded stage metadata is
written beside the bootstrap. No credentials or paths are logged.

The requested hidden Ctrl+Alt+Shift+L restores the existing native form and hides
the canvas without a fake logon or sending input. It is intentionally absent from
the UI. Same-provider unowned notices remain above canvas and input too.

Validation: 346 Wine checks per resolution, including 40 install/path cases and
38 real cross-process/bootstrap/rescue checks. Tests use private HKCU/INI paths,
ANSI non-ASCII bytes/spaces and an inert shell. Both production PE images compile
as i386/Windows 4.0 without CRT; the bootstrap imports only three baseline DLLs.

- [ ] Repeat the exact CP1251 Win95 desktop path and inspect W95START.LOG.
- [ ] Confirm real legacy-loader startup, delay, logoff and power cancellation.
- [ ] Verify physical Ctrl+Alt+Shift+L, native wrong-password and provider notices.


## Missing native input and black power button after logoff (current)

Evidence: the new 26.32s recording shows an attached-looking card without a caret;
Windows rejects a blank submit. The user confirms typing works in the restored
standard dialog. A separate screenshot shows a wholly black power-button area.
This establishes an integration/rendering failure, not its exact Win95 message
sequence. The previous bootstrap change remains in place.

The old layout cache checked the outer dialog only. Three new regressions fail
before the fix when the provider hides/moves its Edit or removes the clipping
region. Layout now validates and repairs the real child and clipping too, without
clearing input or enabling a native modal form. Border clicks request native
focus. Canvas paints queue an asynchronous repaint on the provider's Edit.
Metadata diagnostics now include rectangles, region type and hit class/ID only.

Owner-drawn buttons are explicitly repainted after their clipped parent buffer
is presented. Closing the shade invalidates the owner and children. The drawing
callback respects the item origin and saves/restores the caller's GDI state.
No repeated z-order writes or whole-screen timer redraws were added.

Validation: 363 checks per resolution under Wine64/Xvfb at 640x480x16,
800x600x24 and 1024x768x24: 43 native-control checks, 42 lifecycle/cross-process
checks, 7 rendering checks and 48 power checks among the existing suites.
Input in tests is synthetic; OS authentication, logoff and power remain inert.

- [ ] Verify typed stars/caret and mouse focus after real Win95 logoff/relogon.
- [ ] Repeat the native wrong-password prompt and ensure existing input survives.
- [ ] Recheck the power button after uncovering, focus changes and repeated Cancel
      on the user's actual Win95 GDI/display driver.


## Confirmed Win95 loader failure: GetWindowRgnBox (current)

The user's loader screenshot explicitly reports the missing export
USER32.DLL:GetWindowRgnBox. c77ab64 incorrectly added that static dependency;
Wine supplied it and the prior whitelist mistakenly accepted it.

Replaced both diagnostic and layout calls with GetWindowRgn + GDI32.GetRgnBox.
A separate reusable probe is allocated per attachment and released on restore;
it never overwrites the original provider-region snapshot. Added a nonrectangular
original-region restoration regression and a hard rejection of GetWindowRgnBox
in the PE checker. The checker now rejects the exact previous tracked binary.

Validation: rebuilt CRT-free i386 frontend and audited both production PEs;
365 Wine checks per resolution in the three standard screen modes, including
45 native-control cases. Actual Windows 95 launch/login still needs repetition;
the screenshot establishes this missing export precisely, unlike earlier UI
recordings. Password visibility, GDI repaint and boot fixes remain in place.


## Confirmed clipped-out password client on Win95 (current)

The new user log reports a visible/enabled Edit at client rectangle (0,0,167,22),
but the window region is (3,22)-(170,44), and hit-class is Welcome95.Screen.
The implementation set the entire native window height to 22px. On this Win95
provider the nonclient origin remained (3,22) after stripping its frame, leaving
no parent client height for the password. This identifies a concrete geometry
failure, beyond the earlier speculative child-refresh fix. The user also reports
blind typing no longer succeeds; keyboard delivery must be checked explicitly.
Both reported native forms are supported: 14 controls / password Edit 23 and
8 controls / password Edit 31. No typed/password text is included in the log.

Keep the full original native window size and all native dialog/Edit styles.
Preserve the Edit's original client anchor, measure both nonclient offsets, move
the whole dialog and resize only the Edit to expose a 167x22 client. Crop the
window to that actual client rectangle, after verifying it fits in the parent's
client area. Native frames stay outside the visible region; other controls remain
hidden. Attachment no longer hides/shows the whole provider dialog. Restore and
rescue still return the original complete form. Diagnostics now include parent
window/client sizes. Boot/startup, dimming and the import correction are retained.

Regression evidence: a fixed (3,22) WM_NCCALCSIZE fixture reproduces the target
behavior. Compiled against 8d66912's actual native_logon.c, it fails the field hit
test, original-size and second-form exposure checks. The corrected implementation
passes both forms, preserves the original nonrectangular region and remains idle
without window/style churn. The separate provider test now receives real system
mouse/keyboard events, checks EM_GETMODIFY metadata and visible synthetic mask
pixels, and services queued activation before the next priority pass.

Validation: 370 checks per resolution under Wine64 at the three standard modes,
including 48 native and 44 cross-process/lifecycle cases. Production remains x86,
PE 4.0, no CRT, with the existing 173 reviewed imports. This is a reproduction of
the logged Win95 geometry; actual Win95 login still needs a repeat on the VM.

- [ ] Confirm caret, visible stars, correct-password success and wrong-password retry
      on the user's actual network and Windows logon dialogs.


## Larger password masks and complete Russian UI (current)

The user confirms the Win95 geometry fix works and supplies a screenshot of real
masked input. Requested slightly larger stars without resizing the field, then
complete Russian localization.

WM_SETFONT(NULL) selects the provider's own larger default system font, without
passing a process-private HFONT. Original WM_GETFONT metadata is restored on
fallback/cleanup. No font object is created or deleted across process boundaries;
client dimensions remain 167x22. Existing input, native styles and IDs are intact.

Russian CP1251 UI now covers login prompts/footer/buttons, settings/actions and
statuses, previews, errors, generic display names and bootstrap errors. A shared
ANSI-language helper keeps Cyrillic out of non-1251 interfaces. Account names,
startup values, command switches and diagnostic identifiers are unchanged.
The Russian UI test forces language only in the test build, round-trips actual
control captions and measures CP1251 glyph widths using test-only wide APIs.

Validation: 387 checks per resolution in the three standard Wine modes: 50 native,
45 session and 14 Russian UI checks among the existing suites. Tests cover font
restoration, provider-side text height, real typing/mask pixels, unchanged field
geometry and Russian text fitting the original settings, compact login and power
controls. Russian power labels use 10px Tahoma Bold to fit their 88px columns
(the previous 11px standby label needed 90px). Production keeps x86/PE4.0/no CRT;
main/boot imports are audited.

- [ ] Visually confirm the larger default-font stars and full Russian captions on
      the user's Win95; font rasterization and high-DPI variants remain target checks.


## Smaller welcome power caption

Reduced only the footer power-caption Arial height from 20px to 18px using its
own font with normal cleanup. The 245x29 button, 26px icon, anchors, password and
other UI fonts retain their previous geometry. The existing Russian-width test
now measures the actual caption font. Validation: 387 checks per resolution in
the three standard Wine modes and both production PE audits; Win95 rendering
still requires visual confirmation on the user's display.


## XP-style startup, smaller greeting and compact settings (current)

The user requested a smaller Russian greeting, no chooser for one passwordless
account, and denser settings; then chose the existing status-screen composition
for the pending phase. VIEW_STARTING now shows Windows 95 artwork 40px right of
center (latest user annotation) and truthful “Запуск Windows...” / “Windows is starting up...” text with the XP bars
and glow. No invented RPCSS/service stage is displayed. The separate confirmed
Welcome still follows only the real OS readiness signal. Native validation owns
the single empty attempt; no passwords/PWL contents are read or stored.

A quiet probe leaves native controls/geometry/font untouched behind the canvas.
Disabled OK, modified input, errors, provider replacement or a four-second bound
return to manual login, consuming the automatic attempt. Errors stay accessible
and typed manual input survives. Shutdown cancellation also reattaches normally.
Existing session service, rescue, minimum greeting delay and shell close gate
remain in place. Russian greeting is 32pt (was 36); English reference remains 36.
Settings are 410×234 (was 500×315), Tahoma 11px, 10px margins and 24px buttons, with
separate actions and four previews including startup. Both UI languages remain.

Validation: 407 checks per resolution under Wine64/Xvfb at 640×480×16, 800×600×24
and 1024×768×24, including 54 native, 45 layout, 51 session and 16 Russian UI checks.
The tests use synthetic credentials/events and inert power/boot fixtures. New
actual English Wine screenshots are in docs/previews (ACP1252; no WM decorations).
Production main and embedded bootstrap are rebuilt/audited. After the final
40px logo move, the 7 rendering and 16 Russian UI checks were repeated at VGA
and the updated startup screenshot was inspected.

- [ ] Repeat passwordless/protected single-account boot and logoff on real Win95.
- [ ] Confirm manual recovery with slow/error/replaced native providers on Win95.
- [ ] Visually compare Russian 32pt greeting/status screen and compact settings
      using the VM's actual DPI, GDI driver and fonts.


## Status placement from the XP VGA reference (current)

The user supplied an original XP shutdown image beside the previous Welcome95
startup capture. The previous arbitrary 40px logo move left both logo and status
line misplaced. Startup and shutdown now use one paint_status_message function:
At 640×480 the logo bitmap origin is (355,168), text right edge is x=472 and
text-area top is y=274. This moves startup artwork +63px/-4px and raises its
line 4px from 77b245d. Text follows the original right alignment; shutdown uses
the same placement. Coordinates retain center-relative anchors,
137×86 artwork and Arial 14pt from the original UIFILE (rcstr 1 / rcint 45).
The previous 18px status font was narrower than the XP reference. The footer
divider uses the original UIFILE's 2px height rather than 3px. Windows 95 suffix remains deliberate.
Original UIFILE confirms a centered 384px panel with 20px product and 40px help
right padding; text is right-aligned, so different status strings share the
same right edge. Authentication, native controls and session/shell timing are
unchanged.

Validation: the 407-check Wine suite passed at 640×480×24 for the position change.
After the final original-font/right-alignment correction, 7 rendering and 16
Russian UI checks passed again; the UI test now measures the actual 344px status
area. Actual startup/shutdown screenshots were inspected; both visible status
strings end at x=469 under Wine. Main/bootstrap PE audits pass. Real Win95 font
rasterization remains a target-system check.

## Manual single-account loading and managed Windows/LOGON install (current)

The user's 10-second VM recording shows a single Daniel password card followed
by only the lone greeting. Confirmed cause: ready-event handling chose the
greeting solely by profile count. It now preserves the distinction between
quiet VIEW_STARTING and manual VIEW_LOGIN. After manual entry, one account uses
the same named loading card as the XP reference. Quiet passwordless acceptance
still uses a lone greeting. Both await the real OS-ready event; native input,
minimum loading delay and shell/close gates are unchanged. Manual preview also
shows the card. Preview captions now distinguish No password / With account.

No-argument launches on Win9x copy/relaunch settings under %WINDIR%\LOGON before
font loading. Helpers/previews/NT settings do not install. W95FILES.INI validates
ownership and serializes installers via an open file handle. Source copy is
staged; updates retain E95xxxx.BAK, locked/foreign/invalid destinations refuse
replacement, and copy/rename errors preserve the old executable. Enable migrates
a known legacy Welcome95 Owner only after its startup/shell ownership checks.
A failed migration restores six exact metadata values, three startup values and
the previous shell; unrelated entries/external shell changes remain protected.
Successful migration stops legacy observers and takes effect after reboot.

New EXE, diagnostics, W95START.LOG, shell/EXE backups and private font/DOS PIF/boot
subdirectories all live under LOGON. The journal derives its directory from
Owner rather than Gate. Old files remain available for recovery/cached calls.
No recursive deletion or system PWL/font/profile modifications are introduced.
CopyFileA and ReadFile are baseline Win95 KERNEL32 file APIs; both are explicitly
reviewed in the PE whitelist. Production main/boot remain i386/PE4.0/no CRT.

Validation: 426 checks per resolution at 640×480×16, 800×600×24 and 1024×768×24 under
Wine64/Xvfb: 57 startup, 46 layout, 10 font and 51 session checks among existing
suites. Startup tests use private HKCU/INI/files with a non-ASCII source, owned
updates, locked/foreign destinations, rollback and recovery backups. A separate
LOGON font process proves font extraction/cleanup remains inside its own folder.
DOS tests use a private Windows/LOGON fixture and never execute DOS/power. The
manual loading screenshot is an actual inert Wine window with one synthetic
Daniel profile and private ready/closed events; it does not authenticate.

- [ ] Repeat first installation and update from Desktop on real Windows 95.
- [ ] Verify LOGON ownership, both logs, recovery file, DOS/boot/font directories
      and saved original shell on the VM, including disable/re-enable and reboot.
- [ ] Verify one protected account shows the named card after successful input;
      one passwordless automatic account still shows only the greeting.
- [ ] Recheck wrong-password retry, rescue, logoff/relogon and shell/autostart delay
      on the real providers. Wine tests do not establish actual Win95 boot/GDI.
