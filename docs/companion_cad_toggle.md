# Companion CAD toggle

Hardware Channel Activity Detection (CAD) makes the radio scan for an in-progress
transmission before it sends, and defer if the channel is busy (listen before
talk). On the companion it is on by default and can be turned off at runtime.

> Hardware testing note: only the Heltec V4 and Heltec V3 (OLED, ui-new)
> companion builds have been verified on real hardware. The other variants below
> compile, but their on-device CAD controls have not been tested on hardware.

## Change it from an app

Use the tuning-params frames documented in
[companion_protocol.md](companion_protocol.md#9-radio-tuning-parameters-and-cad).
`CMD_SET_TUNING_PARAMS` carries an optional trailing CAD byte, and
`CMD_GET_TUNING_PARAMS` returns it. The change applies live (no reboot) and
persists. Both additions are backward compatible, so older apps keep working.

## Change it on the device

The on-device control depends on the UI the board uses:

| UI variant | Boards (examples) | CAD control |
|-----------|-------------------|-------------|
| ui-new | Heltec V4, Heltec V3 (OLED) | Dedicated CAD page. Swipe to it, ENTER or long-press toggles. Tested on hardware. |
| ui-touch | Heltec V4 TFT touch | Settings > Experimental > CAD switch, then Save. |
| ui-tiny | LilyGo T-Echo Card | Dedicated CAD page. ENTER toggles. |
| ui-orig | Meshtracker X1, MinewSemi ME25LS01, Muziworks R1 Neo, RAK 3112, and similar | Read-only CAD status on the home screen. No on-device toggle (this UI is gesture-only with no free gesture); change CAD from the app. |

## Default and persistence

CAD defaults to on. The setting is stored in the companion prefs and survives a
reboot. Prefs files written before this setting existed load with CAD on.

## Implementation notes

- `MyMesh::getCADEnabled()` returns the persisted `cad_enabled` pref, and
  `MyMesh::setCADEnabled(bool)` updates the pref, applies it to the radio live,
  and saves.
- The pref is appended to the saved prefs blob in `DataStore`, so old prefs
  files remain readable.
