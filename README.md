# ocarina-3ds

*[Leia em português](README.pt-BR.md)*

A Luma3DS plugin for **The Legend of Zelda: Ocarina of Time 3D** that lets you play the ocarina songs with your
own voice, a whistle or a real instrument. Take out the ocarina in the game, whistle or hum a song into the 3DS
microphone, and Link plays it.

- A clear phrase: the plugin recognises one of the 12 songs and plays it by pressing the song's buttons for you
  (the presses are added to the game's own input, so the game reacts exactly as if you had played it by hand).
- An unclear phrase that looks like one or more songs: the plugin asks with the **game's own Yes/No text box**,
  `<song>?`. Yes plays that song; No asks about the next candidate (up to 3), one at a time.
- Nothing is drawn on screen by the plugin, and the buttons keep working as usual.

This is a fan project. It is not affiliated with or endorsed by Nintendo. No game code, assets or data are
included. See [Legal](#legal).

## Contents

- [Requirements](#requirements)
- [Install](#install)
- [How to use](#how-to-use)
- [The 12 songs](#the-12-songs)
- [The log file](#the-log-file)
- [Known limitations](#known-limitations)
- [Troubleshooting](#troubleshooting)
- [How it works](#how-it-works)
- [Building](#building)
- [Project layout](#project-layout)
- [Contributing](#contributing)
- [FAQ](#faq)
- [Legal](#legal)

## Requirements

- **Console:** developed and tested end to end on an original ("Old") Nintendo 3DS: every mechanism was first
  checked with a dedicated probe build (see [docs/HISTORY.md](docs/HISTORY.md)) and the combined plugin was then
  confirmed working on that console. The 2DS has the same hardware family but was not tested; the
  New 3DS / New 2DS were **not tested**. Reports from other consoles are welcome.
- **Custom firmware:** [Luma3DS](https://github.com/LumaTeam/Luma3DS) with its **plugin loader** enabled
  (Rosalina menu: `L + Down + SELECT` → *Plugin Loader* → *Enabled*; the setting is kept after a reboot).
- **Game:** Ocarina of Time 3D, **USA** release, title ID `0004000000033500`, from your own legally obtained
  copy. The plugin was tested with a copy whose code matches the built-in signatures; the exact title version /
  update of that copy was not recorded. Other regions (EUR, JPN, ...) and other versions are untested. The plugin's
  target list contains only the USA title ID, so Luma does not even load it for other regions.
  If the code does not match, the plugin **refuses to patch anything** and writes why in its log; the game then
  runs untouched.

## Install

1. Power off the console and put the SD card in your computer.
2. Create the folder `/luma/plugins/0004000000033500/` on the SD card (if it does not exist).
3. Copy `ocarina.3gx` (from the [releases](../../releases) page, or your own build) into that folder.
   Keep **only one** `.3gx` file there: with several plugins in the folder Luma may load the wrong one.
4. Put the SD card back, power on, make sure the plugin loader is enabled (see above) and start the game.
   Nothing appears on screen; that is normal.

Result on the SD card:

```
/luma/plugins/0004000000033500/ocarina.3gx
/ocarina/log.txt                    (created by the plugin each time the game starts)
```

To uninstall, delete `ocarina.3gx`.

You can check the download with `dist/SHA256SUMS.txt` from the release (`sha256sum -c SHA256SUMS.txt`).

## How to use

1. Load a save that has the ocarina and go somewhere Link can play it (an open field is fine).
2. **Take out the ocarina** and release the button.
3. Wait about 1 second, then **whistle, hum or play** the song close to the microphone (it is below the bottom
   screen). Use firm notes of about 0.3-0.5 s with a short pause between them. After the last note, **stay quiet**.
4. What can happen:
   - **Clear phrase:** about 1.5 s later Link plays the song by himself and the game reacts.
   - **The plugin is unsure:** the game's text box asks `<song name>?` with **Yes / No**. Pick with the D-pad and
     confirm with **A**. Yes plays the song (about 1.5 s later); No asks about the next candidate, if any (at most
     3 questions). Refusing all of them does nothing.
   - **Nothing looked like a song:** nothing happens; just try again.
5. You can still play with the buttons. While you hold an ocarina button the microphone is ignored, and it starts
   listening again 1 second after you let go. The microphone is also ignored while the question box is open, while
   Link plays a song and for 1 second after.

Tips:

- **Whistling works best** (that is what the detector was tuned on).
- **Repeated notes need a clear pause** of about 0.3 s (Serenade of Water and Nocturne of Shadow have two equal
  notes in a row); without the pause the plugin hears one long note.
- **Any key is fine**: only the intervals between the notes count, so you can whistle higher or hum lower than the
  game's ocarina.
- **Quiet room**: a TV, loud talk or music near the console can produce false notes. At worst a pointless question
  appears; answer No.

## The 12 songs

Notes are given as the game's ocarina plays them (the plugin itself only compares intervals, so any transposition
works). Buttons: D4 = L, F4 = R, A4 = Y, B4 = X, D5 = A.

| Song | Notes | Buttons |
|---|---|---|
| Zelda's Lullaby | B4 D5 A4 B4 D5 A4 | X A Y X A Y |
| Epona's Song | D5 B4 A4 D5 B4 A4 | A X Y A X Y |
| Saria's Song | F4 A4 B4 F4 A4 B4 | R Y X R Y X |
| Sun's Song | A4 F4 D5 A4 F4 D5 | Y R A Y R A |
| Song of Time | A4 D4 F4 A4 D4 F4 | Y L R Y L R |
| Song of Storms | D4 F4 D5 D4 F4 D5 | L R A L R A |
| Minuet of Forest | D4 D5 B4 A4 B4 A4 | L A X Y X Y |
| Bolero of Fire | F4 D4 F4 D4 A4 F4 A4 F4 | R L R L Y R Y R |
| Serenade of Water | D4 F4 A4 A4 B4 | L R Y Y X |
| Nocturne of Shadow | B4 A4 A4 D4 B4 A4 F4 | X Y Y L X Y R |
| Requiem of Spirit | D4 F4 D4 A4 F4 D4 | L R L Y R L |
| Prelude of Light | D5 A4 D5 A4 B4 D5 | A Y A Y X A |

(Source of truth: [src/recog/catalog.c](src/recog/catalog.c).)

## The log file

The plugin writes `/ocarina/log.txt` on the SD card. It is recreated (emptied) every time the game starts, so
copy it before you start the game again.

While the ocarina is out, log lines are kept in memory; they are written to the SD card only **after you put the
ocarina away**, because each SD write stalls the game for about 25 ms.

What it contains:

- **Header:** plugin version; the addresses the signature scan found; the hook status; the pad hook line
  (`pad=... padscan=... padhook=...`); the microphone setup (`mic=... stage=0` means it works); and the
  `plan: ... -> ACTIVE` line, or `-> REFUSED (nothing patched)` with the reason codes on the lines above.
- `sess#N open` / `sess#N close ...`: each time the ocarina was opened and closed, with per-session statistics:
  notes heard, phrases, songs, questions, answers, analysis time (`ana_max=...us`) and noise level
  (`floor=`, `peak=`).
- `phrase ... c/ms@ms= 7726/500@0 ... -> SONG|AMBIG|TIMEOUT|CLOSED`: the notes the plugin heard in each phrase
  (pitch in cents / duration in ms @ start in ms) and what it decided. This is the most useful line when tuning.
- `ask` / `dlg ...` / `answer ... res=YES|NO`: every question asked in the game's box and your answer.
- `play ...`, `replay start` / `replay end`: the song the plugin played.
- `overflow`, `anomaly ...` / `PANIC ...`: something unexpected (see [Troubleshooting](#troubleshooting)).

The log stops growing at 2 MiB (the plugin keeps working). It contains no audio.

## Known limitations

- **All 12 songs are always enabled**, whether or not Link has learned them. If the plugin recognises a song Link
  does not know, Link plays the notes and the game does nothing.
- **The game's own music and sound effects can leak into the microphone.** The plugin mutes itself while it is
  replaying a song and while you press ocarina buttons, but background music can still produce false notes,
  especially with the speaker loud. Headphones or a lower volume help.
- **Humming and singing are untested on the console.** The detector was tuned on whistling recordings of one person
  and checked end to end with synthetic tones.
- **Very high notes** (a thin whistle above about 1400 Hz) may be heard an octave lower. A medium whistle is safer.
- **No Scarecrow's Song** (it is user-defined in the game; only the 12 fixed songs are recognised).
- Repeated notes need a pause of about 0.3 s.
- The question box always shows **"Yes" / "No" in English**, and the song names are in English.
- Only the USA release is supported (see [Requirements](#requirements)).

## Troubleshooting

- **The console freezes when I press HOME.** You are probably running a different (older or experimental) plugin:
  this build sets the 3GX header flag *EventsSelfManaged*, without which Luma's loader waits for a reply the plugin
  never sends. Make sure only this `ocarina.3gx` is in the plugin folder. (The fix follows from Luma's loader
  source; a clean HOME/exit with this exact build has not been confirmed on the console yet. If it still freezes,
  please report it.)
- **Nothing happens at all and there is no `/ocarina/log.txt`.** The plugin did not load: check that the plugin
  loader is enabled in Rosalina, that the file is in `/luma/plugins/0004000000033500/` (exact folder name), that it
  is the only `.3gx` there, and that you have the USA release.
- **There is a log, but the `plan:` line ends with `REFUSED`.** Your game's code does not match the signatures
  (different version or region) or a check failed; nothing was patched. Please open an issue with the log.
- **There is a log, `ACTIVE`, but no `phrase` lines.** The microphone did not hear firm enough notes: whistle
  closer and louder, hold each note longer, and check the `floor=`/`peak=` values in the `sess#` lines.
- **It plays the wrong song.** Send the log and say which song you meant.
- **The question box stays on screen.** Wait about 20 seconds: a watchdog closes it and restores the game's state.
- **`PANIC` in the log.** After 3 anomalies or 2 dialog failures in a session the plugin turns itself off until the
  game restarts. The game keeps running normally, just without the microphone. After one dialog failure no more
  questions are asked until you put the ocarina away and take it out again.
- **The game crashed.** Take a photo of both screens (with any error text), note what was happening (opening the
  ocarina, whistling, box open, after Yes/No, Link replaying), restart **without** starting the game, and send
  `/ocarina/log.txt` (it ends at the last time the ocarina was put away).

## How it works

This section is for developers. All addresses below are **found at run time by signature**; where a value is
given it is only "on the tested build it was ...".

### Plugin loading, and why it is so minimal

Luma3DS's plugin loader maps a `.3gx` file at `0x07000000` in the game process (the first `0x100` bytes are the
loader's header) and jumps to its entry point before the game starts. The first attempt used
CTRPluginFramework (CTRPF): even CTRPF's blank template made Ocarina of Time 3D crash at start-up on the Old 3DS
(a thread creation in the game failed with *InvalidState*). A 3GX with an empty entry point worked fine, so this
plugin is written without any framework: **no libc, no CTRPF, no libctru initialisation**, only system-call stubs
and raw IPC. It keeps every variable in `.data` (the linker script asserts `.bss` is empty), uses no floating
point (the hooks run inside game functions that do not expect VFP registers to change) and no division
instructions (ARM11 has none; a small `__aeabi_idiv` is provided).

The header flag **EventsSelfManaged** (`ocarina.plgInfo`) is required: without it Luma notifies the plugin of HOME
/ exit events and waits for an answer, and the whole console froze on HOME.

Start-up (`ocarina_main` in [src/ocarina.c](src/ocarina.c)) runs once, before the title screen: scan, validate,
set up the microphone and the log, then write the two hooks. Any failure means nothing is patched.

### Signature scan

[src/sigs.h](src/sigs.h) holds five short patterns (18-24 ARM instructions each) of the game's own code with all
position-dependent bits masked (branch offsets, pc-relative load offsets). The plugin scans the game's code region
(from `0x00100000`) for each pattern and requires **exactly one** match:

| Name | Role | Tested build |
|---|---|---|
| InputOwner | a function called once per frame (pause/UI update); the per-frame clock hook | `0x0041E944` |
| MessageGetState | message-system getter; its literal pool gives the message-context pointer | `0x003769D8` (context `0x005C1878`) |
| MessageLoader | loads a message by text ID into the context (used to open the native box) | `0x002CD2B4` |
| StartTextbox | the game's "start a text box" function (found, logged, deliberately not called) | `0x00367C7C` |
| PadHold | the end of the game's pad reader, where the hold word is finalised | `pad=` in the log |

Then the hook sites are validated word by word (the expected prologue / the two expected instructions, and no
branch in the game's code that targets the middle of the patched pair), the message context is checked to be
mapped read/write, and the process memory is switched to RWX through Luma's `svcControlProcess`. The scan takes
about 3.3 s at start-up on the Old 3DS (it happens while the game is still loading).

The patterns were generated with [tools/make_signatures.py](tools/make_signatures.py) from a code image of the
game. That image is not distributed; the tool needs your own (see [Building](#building)).

### Hooks

Both hooks overwrite two instructions with `ldr pc,[pc,#-4]; .word trampoline`. The trampolines
([src/start.s](src/start.s)) save every register and the flags, align the stack, call C, restore everything,
replay the two displaced instructions and jump back. The second word is written before the first, so a function
is never half-redirected; caches are flushed with the same two Luma SVCs its loader uses.

1. **Per-frame clock hook at InputOwner's entry.** Its prologue is `ldr r2,[pc,#imm]; push {r4,r5,lr}`; the
   trampoline re-executes both (reading the literal value captured at start-up). `hook_tick` copies new
   microphone samples, runs the recogniser state machine (`ig_frame` in [src/integ.c](src/integ.c)), publishes the
   button mask for the pad hook, and writes the RAM log to the SD card only when the ocarina is closed.
2. **Mid-function pad hook inside the game's pad reader.** The two instructions `ldr r0,[r5]; bic r0,r0,#0x2000`
   just after the newest HID entry is copied into the pad status are replaced. The C code ORs the injected buttons
   into the hold word *before* the game derives the "pressed"/"released" edges from it, so injected presses look
   like ordinary presses, with consistent edges, in the same frame and before the game logic runs. It only ever
   adds bits (real presses are never removed), only A/X/Y/L/R, and only from a mask at most ~150 ms old. It also
   counts real A presses (for the Yes/No answer) and notes when the user holds ocarina buttons (to mute the mic).

   Why not inject at InputOwner? InputOwner runs during drawing, *after* that frame's input was read and the logic
   ran, so presses written there arrive a frame late or not at all; writing into the pad reader is the one place
   where the game sees a press exactly like a physical one.

### Microphone

The plugin talks to the `mic:u` service with raw IPC (no libctru service wrappers): it gets the service handle
from `srv:`, hands it an 8 KiB shared memory block, powers the mic and starts looped PCM16 sampling at
**8182 Hz** (the "8180 Hz" rate). The game process is allowed to use `mic:u`; no exheader change is needed.

The shared block is the hard part. `svcCreateMemoryBlock` on memory inside the plugin's own block failed
(`0xE0A01BF5`), and so did a plain allocation without an explicit address (`0xE0E01BF5`). What works is to look
for a **free range in the game's heap region** with `svcQueryMemory` (`freerange.c`), allocate 8 KiB at an explicit
address at the end of the last free range with `svcControlMemory`, and turn that into the shared block. This is
done at plugin entry, before the game starts. Each frame the block's cache is invalidated, the write counter at
the end of the block is read, and the new samples go into a 4096-sample analysis ring.

### Pitch detector ([src/recog/pitch.c](src/recog/pitch.c), [notes.c](src/recog/notes.c))

Integer only, no allocation, deterministic, host-tested.

- Window of 1024 samples (125 ms), analysed every **hop of 512 samples** (62.5 ms), so a 0.25 s note is seen.
- DC removal, then a normalised square difference function (an autocorrelation variant) over periods of 5-82
  samples (about 100-1636 Hz), with parabolic interpolation of the peak. A frame is "voiced" when the clarity
  is at least 0.80.
- Octave handling: the *smallest* period whose peak is at least 26/32 of the best one is chosen, which avoids
  reporting a note one or two octaves too low (a tighter ratio did exactly that on real recordings).
- Noise-floor gate: the RMS of the last 128 hops is kept; the 15th percentile is the noise floor, and analysis
  only runs when the RMS is at least 2.5 times that (and at least an absolute minimum).
- Segmenter: a note starts after 3 consecutive stable voiced hops (within 60 cents), tolerates 1 unvoiced hop,
  and changes only when the pitch moves more than 60 cents from its centre. Repeated notes therefore need a
  short silence between them.

Measured on the console with an earlier version of the detector: about 1.9 ms per analysis. The plugin logs the
real figure per session (`ana_max`).

### Matcher ([src/recog/matcher.c](src/recog/matcher.c))

- Compares **intervals** between notes (cents relative to the first note), so the key does not matter.
- **Assisted mode** (the default): 150-cent tolerance per interval; a song is reported
  as soon as its last note arrives with a score of at least 800/1000 and a lead of 100 over the runner-up.
- Otherwise, after 900 ms of silence, phrases that only *look like* songs are offered as up to 3 candidates,
  best first; nothing that resembles nothing is a TIMEOUT and is dropped.
- **Saria's Song vs Song of Storms**: both are two rising triplets; that look-alike is reported as ambiguous at
  once instead of guessing.
- The candidates are asked **one at a time** in the native box; a No moves to the next one 500 ms later.

### The native Yes/No dialog

The question box is the game's own text box, opened through the game's own message system, so it looks and
behaves exactly like any other in-game question. The message context lives at the pointer found through
MessageGetState; its state byte (`+0xF38`) is 6 while the ocarina is open, 7 while a choice is shown, 14 while
closing and 0 when free, and `+0xFC4` is the highlighted option. The sequence
([src/dlg.c](src/dlg.c), [src/dlgmem.c](src/dlgmem.c), [src/drv.c](src/drv.c)):

1. **Save** the context's core ranges (`[0,0x20)`, `[0x164,0x168)`, `[0xF38,0xFD0)`).
2. **Close the ocarina's message** like the game does (next = 3, state = 14) and wait for state 0.
3. **Patch the text** of an existing two-choice record of the game's message table (text ID `0x1A`, normally
   a "play again?" Yes/No question) with `<song>?` + Yes/No, in the game's own control-code format.
4. **Call MessageLoader**(context, `0x1A`) on the game thread, then set the few state fields the game would
   (state 1, next 2, draw flag) so the box appears.
5. When the state reaches 7, sample the highlighted option every frame; the answer is taken on a **real A press**
   (at least 250 ms after the box appeared) or when the game leaves state 7.
6. Close the box, then **restore** the text and the saved core ranges so the ocarina session is intact.

Why not StartTextbox? It ends by closing the ocarina page, so the ocarina session would be lost. Restoring the
whole context instead of the core ranges crashed the game, and restoring nothing left the ocarina unable to
recognise songs. A 20 s watchdog restores everything if the box never closes.

### Logging

SD access is raw `fs:USER` IPC. One open/write/close costs about 25 ms on the Old 3DS, which is almost a whole
frame at 30 fps, so lines are buffered in RAM (8 KiB) and appended in one go only when the ocarina has been closed
for a while and nothing of the plugin is running. A capped log keeps a long session from filling the card.

### Safety

- **Refusal on mismatch:** any missing/ambiguous signature, unexpected instruction at a hook site, unmapped message
  context, failed RWX switch or microphone failure means **nothing is patched** (`REFUSED` in the log).
- **Watchdogs:** 20 s for the dialog, 15 s for a replay.
- **Anomalies** (dialog failure or watchdog, wrong state after a restore, mic counter invalid or no new samples
  for 2 s, a hook frame of 40 ms or more, repeated ring overflows) are logged. After a dialog failure no more
  questions are asked until the ocarina closes.
- **Panic mode:** 3 anomalies or 2 dialog failures turn the plugin off for the rest of the session (never while
  its own box is on screen, so the state is always restored first). The game keeps running.

## Building

Requirements:

- [devkitPro](https://devkitpro.org/wiki/Getting_Started) with **devkitARM** and **libctru** (`3ds-dev`).
- **3gxtool** (converts the ELF to `.3gx`; MIT licensed, by ThePixellizerOSS:
  <https://gitlab.com/thepixellizeross/3gxtool>). The development machine got it as the pacman package
  `3gxtool` (version 1.3) from ThePixellizerOSS's package repository, added to devkitPro's `pacman.conf` as
  `[thepixellizeross-win]` (`https://thepixellizeross.gitlab.io/packages/x86_64/win`, Windows build); it installs
  `$DEVKITPRO/tools/bin/3gxtool`. Linux/macOS users should check that project for their platform's package or
  build it from source (not verified here). Note: CTRPluginFramework is **not** needed.

```sh
export DEVKITPRO=/opt/devkitpro
export DEVKITARM=$DEVKITPRO/devkitARM
make                 # -> ocarina.3gx
sh tools/check-build.sh            # nm -u must be empty, 0 VFP instructions, section sizes, stack
python3 -I tools/verify_3gx.py ocarina.3gx   # header: 3GX$0002, flags (EventsSelfManaged), target 0x33500
```

On Windows, from PowerShell with devkitPro's MSYS2:

```powershell
$env:MSYSTEM='MSYS'; '' | & "$env:DEVKITPRO\msys2\usr\bin\bash.exe" -lc 'export DEVKITPRO=/opt/devkitpro DEVKITARM=/opt/devkitpro/devkitARM; cd /path/to/ocarina-3ds && make'
```

Host tests (any desktop C compiler + CMake):

```sh
cmake -S . -B build-tests && cmake --build build-tests && ctest --test-dir build-tests --output-on-failure
python3 -I tools/test_make_signatures.py
```

On Windows with Visual Studio 2022 Build Tools: `run-tests.cmd`.

Optional data, for people who own the game (never commit these files):

- `OOT3D_CANONICAL_CODE=/path/to/code.bin` - a decompressed code image you extracted from your own copy (loaded at
  `0x00100000`). Enables the signature and hook-site checks in `test_hook` and `test_make_signatures.py`, and is
  needed to regenerate `src/sigs.h` (`python3 -I tools/make_signatures.py src/sigs.h`). Without it these checks
  print `SKIP` and the tests still pass.
- `OCARINA_RECORDINGS_DIR=/path` - a folder with raw 8182 Hz PCM16 microphone takes `rec1.raw`/`rec2.raw` used by
  the recording checks of `test_ocarina`. The project's original recordings are private and not distributed.
  `tools/raw_to_wav.py` converts such takes to WAV for listening.

## Project layout

```
Makefile, plugin.ld, ocarina.plgInfo   plugin build (devkitARM + 3gxtool)
CMakeLists.txt, run-tests.cmd          host tests
src/start.s        entry point, SVC wrappers, the two hook trampolines
src/ocarina.c      console glue: scan, mic:u, fs:USER log, hooks
src/integ.c        the whole per-frame state machine (pure, host-tested)
src/hook.c scan.c  hook planning and signature scan;  src/sigs.h  the signatures
src/dlg.c dlgmem.c drv.c dlglog.c   native Yes/No dialog
src/inject.c       button replay;  ringcopy.c freerange.c fslog.c logbuf.c  helpers
src/recog/         pitch detector, note segmenter, song matcher, song catalog
tests/             host tests (synthetic; optional private data, see above)
tools/             build checks, 3GX header check, signature generator, raw->wav
docs/HISTORY.md    how the approach was found (probes and findings)
```

## Contributing

Issues and pull requests are welcome. Some ground rules:

- Never commit game data (code images, ROMs, CIAs, dumps) or audio recordings; `.gitignore` blocks the usual
  extensions.
- Keep the plugin freestanding: no libc, no floats, no `.bss`, no new libraries. `tools/check-build.sh` must show
  an empty `nm -u` and 0 VFP instructions.
- Put logic in pure C files with host tests (like `integ.c`), and keep console-only code in `src/ocarina.c`.
- Logs from real sessions (with a sentence on what you tried) are the most useful bug reports, especially for
  other regions/versions, the New 3DS and humming/singing.

## FAQ

**Does it work on Citra / other emulators?** Not tested. It needs Luma3DS's plugin loader and the 3DS microphone
service.

**Does it work with the EUR/JPN version?** Not as built: the plugin targets only the USA title ID, and the
signatures were only checked on the USA code. Support would need someone with that version to test (and maybe
regenerate signatures).

**Does it record or upload audio?** No. Audio is analysed in memory and discarded; the log has only numbers
(pitches, durations, timings). Nothing is sent anywhere.

**Will it break my save?** It does not touch save data. It changes two places in the game's code in memory while
the game runs, and only after every check passed.

**Why not draw a nice overlay?** The point was to use the game's own UI, and drawing needs a framework that did not
work with this game on the Old 3DS.

## Legal

This is an unofficial fan project, not affiliated with, endorsed or sponsored by Nintendo. "The Legend of Zelda",
"Ocarina of Time" and "Nintendo 3DS" are trademarks of Nintendo.

No game code, assets or data are distributed. `src/sigs.h` contains short masked ARM instruction patterns taken
from the game's code, used only to *find* functions in your own game at run time; the plugin and the tools work only
with a copy of the game you supply yourself. Bring your own legally obtained game.

Use at your own risk. Custom firmware and plugins modify how your console runs software; this may affect your
warranty and online services. The software is provided "as is", without warranty of any kind (see
[LICENSE](LICENSE)).

The repository's own code is MIT licensed ([LICENSE](LICENSE), "OcarinaMod contributors"). Third-party tools
used to build or run it are not included; see the NOTICE section of the license file.
