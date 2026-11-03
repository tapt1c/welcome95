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
