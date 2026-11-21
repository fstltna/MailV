# Mail V 6.0

An updated build of **Mail V**, Simon Bell's IMAP/POP/SMTP email client for the
Apple Newton (Newton OS 2.x). It works on current versions of the
[Einstein](https://github.com/pguyot/Einstein) emulator and with modern mail
servers, including **Citadel**.

Mail V is copyright © 1997–2008 Simon Bell and is licensed under the Open
Software License 2.1 (see [LICENSE.md](LICENSE.md)). This repository is a
derivative of the source he published at newtonresearch.org.

## What changed since 5.2.x

**Receiving mail (IMAP)**
- **Citadel compatibility.** Citadel replies to a `BODY` request with
  `BODYSTRUCTURE`. The parser didn't recognise it, left the message structure
  empty, and every message failed at "Decoding". The parser now accepts
  `BODYSTRUCTURE` and skips its extension data (disposition, parameters,
  trailing `NIL`s) instead of misreading it as message parts.
- **Accounts that have never received.** The per-folder UID record is nil until
  the first successful receive, and several steps required it to exist. That
  made a new account fail every time, with the error appearing at "Decoding".
- A server folder the Newton can't add is skipped instead of aborting the
  receive. A part that can't be decoded is filed with a note instead of
  aborting. A message whose structure can't be read is treated as plain text.

**Native decoders (`SM.ntkc`)**
- `DecodeHeader`: bounded charset copy. An unterminated `=?` in a subject, for
  example "Re: 2+2=?", overflowed a 32-byte stack buffer. Also adds RFC 2231
  `charset*lang` support.
- `DecodeQuotedPrintable`: no longer writes a NUL past the end of the buffer
  (heap corruption); handles bare-LF soft breaks; keeps malformed escapes
  literally.
- `DecodeBase64`: rewritten. The old version read outside its lookup table and
  mishandled CR/LF inside a 4-character group.
- UTF-8: characters outside the BMP (emoji) and invalid bytes become `?`.
  Newton strings are UCS-2.

**Build**
- `Global Functions` is reconstructed, since Simon's wasn't published:
  text-encoding registry functions, `kTCP`, endpoint event codes, and a stub for
  the unpublished `GIF.ntkc`. Sending a Note as a GIF image is disabled in this
  build.
- `NCTCompat.h` supplies definitions that the NewtonDev NCT headers lack.

## Repository layout

| Path | Contents |
|---|---|
| `source/` | The MPW/NTK source tree, laid out for a Basilisk II / SheepShaver shared ("Unix") folder. The hidden `.finf/` and `.rsrc/` folders hold each file's Mac type/creator and resource fork. **Copy this folder into the emulator through the Unix volume, not with a plain download**, or NTK won't recognise its files. |
| `docs/final-scripts.txt` | The final NewtonScript for every changed NTK method, labelled by file and method. These changes live inside NTK's binary layout files. Paste them into a fresh copy of the source to reproduce the build. |
| `docs/native-changes.diff` | Readable diff of all native (C++) changes against Simon's original. |
| `release/MailV-6.0.0.pkg` | Ready-to-install package. Drag it onto Einstein, or install it with NCU/NewtonInstaller. Delete any older Mail V from Storage first. |
| `tests/` | Host-side tests for the rewritten decoders. Run `python3 tests/extract.py && g++ -o dt tests/decoder-tests.cpp && ./dt`. |

The NTK files in `source/Projects/Mail/MailV/` are the **edited 6.0 versions**
from a working build (version 6.0.0, About link `NewtonCity.org/mailv`).
`docs/final-scripts.txt` documents what changed in them, in readable form.

## Building

Requirements (all run under Basilisk II or SheepShaver, Mac OS 7.5.5–9):
- **NewtonDev 1.1** (NTK 1.6.4 and Newton C++ Tools with Paul Guyot's additions)
- **MPW 3.5 GM**, including its `Interfaces:RIncludes` folder

1. **Native module.** Put `source/NCT_Projects/SM_NCT` in NCT's projects folder.
   In MPW, select the `SM_NCT:SM` project from the Newton C++ Tools menu and
   choose Build. This produces `Objects:SM.ntkc`. If Rez can't find
   `Types.r`, copy MPW's `Interfaces` folder into the MPW folder.
2. **Package.** Open `source/Projects/Mail/MailV/MailV` in NTK.
   - Make sure the project points at the new `SM.ntkc` and that `SM.ntkc` comes
     right after `Global Functions` in the build order (Project → Process
     Earlier). **Quit and relaunch NTK after adding a new `.ntkc`**, because NTK
     only loads native modules when it opens a project.
   - Build Package. The project already includes the 6.0 changes.

## Debugging tips

- Project Settings → **Compile for Debugging** enables Mail V's debug log. In
  its preferences, "To Notes" with "Log standard" and "Log I/O" writes the full
  server conversation to a Note.
- **Turn the log off when you're done.** It grows with every session and can
  run the emulated Newton out of memory (it reboots with the startup chime).
- Mail V marks a message as fetched as soon as it reads the envelope. A message
  that fails while decoding won't be fetched again, so test with fresh messages.
