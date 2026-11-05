# History: how the approach was found

The plugin was built step by step with small throwaway "probe" plugins, each testing one assumption on a real
Old 3DS running Luma3DS and the USA release of Ocarina of Time 3D. Early probes reported their result through an
intentional error screen (`ERRF_ThrowResultWithMessage`), because SD access and drawing were not yet known to
work inside the game. The probes themselves are not part of this repository; this is what they established.

Before the 3DS work the same idea was prototyped on PC (microphone -> pitch -> song matcher -> native Yes/No box ->
button replay); the detector, matcher and dialog sequence here are integer-only ports of that prototype.

## Milestone 0: can a plugin run at all?

1. **CTRPluginFramework is not usable with this game on the Old 3DS.** A CTRPF-based plugin, and even CTRPF's
   blank template with no code of ours, crashed the game at start-up (ErrDisp `0xE0A01839` at `0x004660D4`,
   the return of a `nn::os` thread start: the system refused a thread creation with *InvalidState*). With the
   plugin loader enabled and no plugin, the game ran fine, and a romfs translation patch made no difference. So
   the loader is fine; the framework is not.
2. **A minimal 3GX works.** A plugin whose entry point is just `bx lr` (MemorySize 2 MiB, no framework) ran
   normally.
3. **HOME froze the console** with that empty plugin. Luma's loader (`plgloader.c`) notifies the plugin of
   HOME/exit and waits for a reply unless the 3GX header flag *eventsSelfManaged* (bit 6) is set. Every build since
   sets `EventsSelfManaged: true`. (A dedicated clean-HOME test of the flag was still pending when this was
   written.)
4. **`mic:u` is reachable from inside the game.** Raw `srv:` IPC got a `mic:u` handle inside the game process; no
   exheader change is needed.
5. **The signatures match the real game.** All four first signatures were unique on the console. The input owner
   was at `0x0041E944` on the console (`0x0041E968` in the code image the patterns came from); the message
   functions and the message context (`0x005C1878`) were at the same addresses. Scanning the 0x4C7000-byte code
   region took 3.26 s.

## Milestone 0, continued: hook, memory, timing

6. **Per-frame hook.** Redirecting the input owner's two-instruction prologue to a trampoline that saves
   everything, calls C and replays the displaced instructions ran stable on the console. The process is made RWX
   with Luma's `svcControlProcess` and caches are synced with the same SVC pair Luma's loader uses. The hook's own
   cost was measured on the console by that probe (the exact figures were read from the error screen and are not
   recorded here); the integrated plugin logs its per-frame cost (`idle_max`, `ana_max`).
7. **Microphone shared memory.** `svcCreateMemoryBlock` on memory inside the plugin block failed (`0xE0A01BF5`);
   allocating without an explicit address failed too (`0xE0E01BF5`). Allocating 8 KiB at an explicit address in a
   **free range of the game's heap region** (found with `svcQueryMemory`), at plugin entry, worked; samples
   arrive at 8182 Hz.
8. **Detector cost.** The first integer detector measured about 1.9 ms per 1024-sample analysis on the Old 3DS
   (out of a 33 ms frame; the game logic runs at 30 fps).
9. **SD card.** Raw `fs:USER` IPC works from the game thread, but one append (open + write + close) costs about
   **25 ms**, almost a frame. Hence: buffer in RAM, write only when the ocarina has been put away.
10. **Recordings.** Two 20 s microphone takes recorded on the console (noise, clicks, six whistled notes) were
    used to tune the detector: a 512-sample hop so short notes are seen, an octave-pick ratio of 26/32 (a stricter
    one reported a note two octaves low), a percentile noise floor. The recordings are private and not included.

## Milestone 1: playing and asking

11. **Button injection.** Writing buttons at the input owner is too late (it runs while drawing, after the logic
    read the input). The working place is a mid-function hook in the game's pad reader, right after the newest HID
    entry is copied: ORing bits into the hold word there gives consistent pressed/released edges in the same frame.
    A replay of a song through it was recognised by the game on the console.
12. **Native Yes/No dialog.** Opening the box through the message loader plus a few state writes, with a text
    record patched to `<song>?`, and saving/restoring only the context's core ranges, showed the game's own box on
    the console with the ocarina still usable afterwards. `Message_StartTextbox` was avoided because it closes the
    ocarina page.
13. **Integration.** `ocarina.3gx` combines all of the above with the state machine in `src/integ.c` (host-tested
    end to end with synthetic audio: all 12 songs, ambiguous phrases, Yes/No paths, mute, failures and panic).
