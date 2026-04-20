# fpp-mask

Falcon Player plugin that applies a separate `.fseq` sequence as a per-channel
brightness mask on whatever is currently playing. Useful for masking out
specific pixels (windows, fixtures the neighbors complain about, etc.) at
night without re-rendering every show sequence.

## How it works

The plugin hooks `ChannelDataPlugin::modifyChannelData()` — which fires after
overlays/effects but before output processors — and multiplies each output
channel by the matching channel from the mask sequence:

    output[ch] = output[ch] * mask[ch] / 255

- Mask byte `0`   → channel forced to black.
- Mask byte `255` → channel passes through unchanged.
- Mask byte `128` → channel at ~50% brightness.

The mask sequence is loaded fully into memory at startup and looped
continuously based on wall-clock time (independent of what's playing). 1:1
channel mapping — mask channel N modulates output channel N. Channels beyond
the mask's channel count pass through unchanged.

## Installing

In the FPP web UI: **Help → Plugin Manager**, then paste this URL into the
"Find a Plugin or Enter a plugininfo.json URL" field:

```
https://raw.githubusercontent.com/joeflateau/fpp-mask/main/pluginInfo.json
```

(A `https://github.com/joeflateau/fpp-mask/blob/main/pluginInfo.json` URL also
works — the UI rewrites it to raw.) Click Install. FPP clones the repo,
runs `scripts/fpp_install.sh` (which calls `make`), and the plugin is loaded
on the next `fppd` restart.

### Manual install (development)

```
cd /home/fpp/media/plugins
git clone https://github.com/joeflateau/fpp-mask.git
cd fpp-mask && make
sudo systemctl restart fppd
```

## Configuration

Visit **Status Pages → Mask Plugin** in the FPP UI to pick the mask sequence
file and toggle the mask on/off. State persists across restarts in
`/home/fpp/media/config/plugin.fpp-mask` (a `key=value` file). The plugin
watches that file and applies changes within ~1 second of save.

## MQTT (Home Assistant)

Topics are relative to FPP's configured MQTT prefix (Settings → MQTT). FPP
exposes plugin events under `<prefix>/event/...`.

| Topic                  | Payload         |
| ---------------------- | --------------- |
| `event/Mask/Set`       | `on` or `off`   |
| `event/Mask/Toggle`    | (any)           |
| `event/Mask/Load`      | `filename.fseq` |

Example Home Assistant MQTT switch:

```yaml
mqtt:
  switch:
    - name: "FPP Mask"
      command_topic: "fpp/event/Mask/Set"
      payload_on: "on"
      payload_off: "off"
      state_topic: "fpp/event/Mask/Set"
      optimistic: true
```

## Producing a mask sequence

Use xLights (or any sequencer) and render to `.fseq` against the same model
layout as your show. Solid white (255) on regions you want visible, solid
black (0) on the windows / fixtures you want masked off. A single static
frame works fine — set the sequence length to one step.

Output `.fseq` to `/home/fpp/media/sequences/` so the dropdown picks it up.

## Notes

- The mask file is decompressed into RAM at load time. A 32-channel single-frame
  mask is 32 bytes; a multi-thousand-frame animated mask could be tens of MB.
- Multi-sync-aware: enabling/disabling on the master propagates to remotes
  that have the same plugin installed.
- Plugin only modifies channels that are actually configured for output (via
  `GetOutputRanges()`).
