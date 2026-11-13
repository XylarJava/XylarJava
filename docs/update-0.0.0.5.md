# v0.0.0.5-ALPHA

## Accounts and protected names

Settings → Accounts now supports Microsoft, Ely.by and standalone offline profiles.
Ely.by uses its official HTTPS authentication API, including a separate two-factor
code field. Only the session and client tokens are persisted; passwords are not.
The game uses Ely.by's authlib-injector endpoint and the provider's profile/skin,
including slim and HD previews. Ely.by skins are managed on Ely.by; its account
system does not supply capes. The interactive Ely viewer includes Refresh.

The Xylar skin Worker must be upgraded together with the launcher. `/api/identity`
reserves names case-insensitively and issues an owner key in the same database
transaction. Microsoft identities are checked against Minecraft Services; Ely
identities are checked through Ely.by's authenticated Minecraft Services profile
endpoint, without simulating a Minecraft server join. A supplied username or UUID
is not proof. Further requests require the existing key or the same verified provider
identity. An existing owner key can resume only the same provider and subject already verified
for the reserved name; new devices and changed identities still need provider proof.
Existing owners keep their keys and textures; a second authenticated
device gets a separate key. Offline owners transfer a `.xylar-skin-access` file
using the import action in the rejection dialog or Skins & Capes. Keep this file
private. Offline spelling must match the original reservation because Minecraft's
offline UUID is case-sensitive.

The first registration owns an unreserved Xylar name. This registry does not claim
to authenticate arbitrary offline users of other launchers or reserve every name
on Microsoft/Ely.by in advance. Public game textures remain public. Servers in
Minecraft offline mode need their own authentication to reject modified clients.
Xylar's session endpoint now requires proof of profile ownership for offline joins.

Old case-colliding records are retained. The earliest claimed record (then UUID as
a deterministic tie-breaker) receives the reservation; ambiguous legacy collisions
need administrator review and proof of ownership. Never delete profiles or transfer
them just because somebody asks for the same nickname. Existing unverified MSA
links are not accepted as sign-in proof. Canonical profile bindings are retained across provider username case changes.
New clients refuse an old backend rather
than silently skipping username protection. Already owned offline profiles remain
usable without a connection; an explicit server rejection is not ignored.

Deploy the Worker when the new client builds are ready:

```sh
cd /home/mrmariix/xylarjava-skin-backend
npm test
npm run deploy
```

The Worker creates the additional tables on first use without dropping existing
data. The optional migration file can also be applied through the normal D1 migration
workflow. The Worker needs outbound access to the real identity providers; transport
failures must be resolved on the backend rather than accepting an unverified UUID.
OAuth browser login requires a separately registered Ely.by application; this update
uses the documented login/password/2FA flow and does not embed another launcher's
OAuth credentials.

## Preview, appearance and repair

Skins & Capes retains the OpenGL preview when available and uses an interactive
textured 3D raster renderer otherwise: drag to rotate, scroll to zoom, double-click
to reset. Front/back/reset buttons are also available, with camera panning, arrow-key
rotation, lighting, a floor shadow and correct legacy/HD skin normalization.
No OpenGL dependency or continuously running render timer is needed for
the fallback. Meimei appears in Settings → Appearance with the bundled community
themes. Its original image appears only on Home and Instances; Servers, Settings
and dialogs use the separate forest background. Both assets are local.

Repair checks the current log, recent crash report and JVM `hs_err_pid` report.
It identifies antivirus blocks, architecture/DLL issues, driver crashes, full disks,
permissions, long Windows paths, certificate errors and native/metaspace exhaustion.
External problems are explained with redacted evidence instead of triggering
unrelated downloads. Unknown crashes stay unidentified: repair verifies checksums
without promising a diagnosis. Known good libraries are reused; damaged files are
re-downloaded without accepting stale cache fallbacks or HTTP 304 responses.
Failed mod downloads keep the existing mods. JVM/graphics fixes are scoped to the
instance. Completed actions and failures are recorded in `repair-report.json` in
that instance; an unsuccessful restart still reports the remaining problem.

## Windows distribution

The download remains a single `XylarJava.exe`. Packaging embeds a precomputed
payload checksum, verifies it on extraction and avoids hashing the entire archive
on every launch. The outer .NET bundle is no longer compressed, and existing
shortcuts are not rewritten on every start. Installed payloads stay versioned and
updates do not overwrite a running launcher.

Screenshot_486.png shows Windows error 225/226, an antivirus block, rather than a
Minecraft installation failure. The bootstrap now explains that condition and
writes `%LOCALAPPDATA%\XylarJava\bootstrap.log`. This does not prove the detection
is false or guarantee that an unsigned build will be accepted. Submit the exact
new binary and detection to [Microsoft Security Intelligence](https://www.microsoft.com/en-us/wdsi/filesubmission/)
for review when needed. Antivirus protection stays enabled.

For a trusted Authenticode certificate, configure GitHub secrets
`XYLARJAVA_SIGNING_CERT_BASE64` (PFX) and `XYLARJAVA_SIGNING_PASSWORD`.
Packaging signs both the native launcher before embedding and the final wrapper.
Without these secrets it clearly produces an unsigned build. No private signing key
is stored in the repository. The Windows startup smoke job is optional and off by
default; the packaged EXE is uploaded directly as `release-windows`.

Build the new binaries before publishing; old artifacts in dist-single are still
old builds even after the source version is changed:

```sh
cd /home/mrmariix/XylarJava
./scripts/build-windows.sh
XYLARJAVA_BUILD_JOBS=2 XYLARJAVA_APPIMAGE_RUNTIME="$PWD/build/appimage-runtime-x86_64" ./scripts/build-linux-appimage.sh
./scripts/publish-local-release.sh v0.0.0.5-ALPHA
```

Commit and push the update before running the Windows build helper. The last
command publishes only the EXE and AppImage and creates/reuses the tag without
moving existing tags. Publishing is a separate action from editing this update.

Protocol references: [Ely.by authentication](https://docs.ely.by/en/minecraft-auth.html),
[Ely.by authlib-injector](https://docs.ely.by/en/authlib-injector.html),
[Ely.by skins](https://docs.ely.by/en/skins-system.html),
[Windows error codes](https://learn.microsoft.com/en-us/windows/win32/debug/system-error-codes--0-499-).
