## Reality Desync

A time and pitch fracture effect (VST3): different parts of the sound run at different speeds, then everything snaps back into alignment on a bar line.

### Downloads

| Platform | File | Notes |
|---|---|---|
| Windows 10/11 (64-bit) | `RealityDesync-*-Windows-x64.zip` | Copy `Reality Desync.vst3` to `C:\Program Files\Common Files\VST3\` |
| macOS 10.13+ (Apple silicon and Intel) | `RealityDesync-*-macOS-universal.zip` | Copy to `/Library/Audio/Plug-Ins/VST3/`. Not notarised, see below |
| Linux (x86-64) | `RealityDesync-*-Linux-x64.zip` | Copy to `~/.vst3/` |

Every zip contains the plugin and an `INSTALL.txt`. Checksums are in `SHA256SUMS.txt`.

### What's in it

* Custom interface: live delay lanes, per-band slice clocks, an alignment ring, a preset browser and Save for your own presets.
* The 32 factory presets are also listed in your host's own preset menu.
* 55 automatable parameters. Reports its 2048-sample latency to the host.
* Full guide: [GUIDE.md](https://github.com/atruenut/RealityDesync/blob/main/GUIDE.md)

### macOS note

The plugin is ad-hoc signed but not notarised. If macOS refuses to load it, run:

```
xattr -dr com.apple.quarantine "/Library/Audio/Plug-Ins/VST3/Reality Desync.vst3"
```

### Fixes in this version

* The Windows download is now the full edition with the custom interface (the earlier prebuilt DLL used the host's generic parameter screen).
* Presets are exposed to the host as named programs.
