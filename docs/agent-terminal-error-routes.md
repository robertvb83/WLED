# Agent Terminal Error Routes

Known recurring issues encountered by AI coding agents in this repo — terminal/PowerShell quirks as
well as build/config gotchas that previously took many back-and-forth attempts to diagnose. Check
this file first whenever a command fails unexpectedly, a build/CI result looks wrong, or the same
kind of error keeps repeating — don't retry the same approach blindly.

## Symptom: common cmdlets suddenly report `CommandNotFoundException`

Example: `Get-ChildItem`, `Write-Host`, `git`, or `Select-String` fail with
`"...wurde nicht als Name eines Cmdlet... erkannt"` (or the English equivalent), even though
they are always available in a normal PowerShell session.

- **Root cause**: the terminal is still busy running a previous long-running command (e.g. a
  multi-environment `pio run`, or one that emits a huge amount of output) when the next command is
  sent. The new input gets fed into whatever the still-running process is doing (or its output
  buffer), not into a fresh PowerShell prompt — producing nonsensical "not found" errors that look
  like a broken shell.
- **Fix**:
  1. Don't keep retrying the same command in that terminal — this repeats the failure.
  2. Run a small, harmless probe command (e.g. `Write-Host CHECK`) once; if it also fails oddly,
     the terminal is still wedged, not just the original command.
  3. Spawn an isolated fresh process instead of reusing the wedged terminal:
     `powershell -NoProfile -Command "<command>"`. This sidesteps whatever the old terminal is
     stuck on.
  4. Prefer verifying results via on-disk artifacts (e.g. file timestamps/sizes) over parsing
     possibly-interleaved or truncated console text.

## Symptom: `pio run` output looks truncated / cuts off mid-build, unclear if it succeeded

- **Root cause**: very long build output can be summarized/truncated by the tooling, or multiple
  `-e` environments queued in one call make it hard to tell which environment's result you're
  looking at.
- **Fix**:
  1. Build one environment per `pio run -e <env>` call when you need an unambiguous result.
  2. Confirm success by checking `.pio/build/<env>/firmware.bin` exists and has a fresh
     `LastWriteTime` (newer than when the build started), rather than relying only on scrollback
     text.
  3. If genuinely unsure, re-run the same `-e <env>` again — PlatformIO's object cache makes a
     repeat run fast and it will print a clean `[SUCCESS]`/`[FAILED]` summary line at the end.

## Symptom: bogus linker error, e.g. `libUpdate.a: No such file or directory`

Took many turns to resolve in a past session (root cause was only found after cleaning the build).

- **Root cause**: a corrupted or partially-written `.pio/build/<env>` directory — usually from an
  interrupted build, or two `pio run` processes writing to the same env's build dir concurrently.
  The missing `.a` file is a symptom, not the actual cause; don't try to chase why that specific
  library is "missing".
- **Fix**: delete the affected env's build dir (`Remove-Item -Recurse .pio\build\<env>`) or run
  `pio run --target clean -e <env>`, then rebuild. Never run `pio run` for the same environment
  from two terminals/processes at once.

## Symptom: a usermod is silently missing from the compiled firmware (`"um":{}` in JSON info)

This happened twice for two different reasons in past sessions — check both.

1. **`extra_scripts` overwrite**: setting `extra_scripts = pre:usermods/<mod>/<mod>_build.py` in
   `platformio_override.ini` (locally) or in a CI workflow YAML **replaces** WLED's own
   `extra_scripts` list instead of extending it, so `pio-scripts/load_usermods.py` never runs and
   no usermods get built in. Always append to the existing list
   (e.g. `extra_scripts = ${env:esp32dev.extra_scripts} pre:usermods/<mod>/<mod>_build.py`) rather
   than assigning a single script.
2. **Forgot to add the usermod name to `custom_usermods`**: a usermod can compile fine and even be
   referenced elsewhere, but if its name isn't listed under `custom_usermods` in the relevant
   `[env:...]` section (local `platformio_override.ini` _and_ any external CI workflow file, which
   may regenerate its own `platformio_override.ini` independently of the local one), it's silently
   left out of that specific build. Check both the local override file and any external CI YAML
   whenever a usermod is "missing" from a build someone else produced.

## Symptom: a build-time-generated header is missing only in CI (`fatal error: <mod>_xxx.h: No such file or directory`)

- **Root cause**: the CI workflow enabled the usermod (added it to `custom_usermods`) but never
  invoked its code-generation script (e.g. `usermods/info_provider/info_provider_build.py`, which
  generates `info_provider_birthdays.h`). Enabling a usermod in CI is not enough if it relies on a
  generated header — the CI YAML's `extra_scripts`/build steps must run the same generator script
  used locally.
- **Fix**: add the usermod's `pre:` build script to CI's `extra_scripts` list (see the
  `extra_scripts` overwrite note above — don't replace the existing list, extend it).

## Symptom: Usermod Settings page breaks (white screen, missing fields/buttons) after `appendConfigData()` changes

A usermod-settings-page rewrite session needed many iterations (white screen, disappearing
headings, a button doing nothing) before landing on a working, simple approach.

- **Root cause**: `appendConfigData()` output is easy to get subtly wrong because it emits raw
  JavaScript/HTML strings that must exactly match how WLED's settings page expects fields to be
  rendered. DOM-manipulation/JS-loop approaches (searching for elements by name and rewriting
  labels/inserting nodes after the fact) are fragile: a single missing brace or wrong selector
  causes the page to fail to load (white screen) or silently drops previously-added
  headings/separators.
- Separately, `requestJson()` (WLED's normal JSON-API helper used on the main UI) is **not**
  available on the Usermod Settings page — a custom button handler there must `fetch()`
  `/json/state` (or `/json/info`) directly instead of assuming that helper exists.
- **Fix**: prefer direct, static `addInfo()` calls for labels/hints over DOM manipulation loops.
  If a settings page ends up in a broken/inconsistent state after several incremental edits,
  reverting to the last known-good version and rebuilding cleanly (rather than patching further)
  resolved it fastest in practice.

## Symptom: a usermod blocks the main loop / other events after a trigger (e.g. Wi-Fi reconnect)

- **Root cause**: triggering an expensive synchronous operation (e.g. a full 253-host subnet ping
  scan) automatically on every occurrence of an event (every Wi-Fi reconnect) causes noticeable
  blocking and delays unrelated status updates (e.g. presence/seen-time info looking stale or
  inconsistent with the configured interval).
- **Fix**: don't auto-trigger expensive network scans on routine events. Make such scans opt-in
  (config checkbox) or manually triggered (a dedicated "Scan now" JSON API flag/button), and keep
  the regular periodic check limited to the small, already-known device list.

## Symptom: HTTP JSON response silently truncated / stale value used

- **Root cause**: a hardcoded response-size guard (e.g. `if (response.length() <= 20000)`) can
  silently reject a valid but larger API response (a 40-entry OpenWeather forecast response can
  exceed 20 KB), causing the code to keep using a stale cached value with no visible error.
- **Fix**: when a value looks wrong/stale despite a fetch "succeeding", check for silent
  size/length guards on the raw HTTP response before assuming a parsing logic bug.
