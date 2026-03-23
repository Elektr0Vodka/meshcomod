## repeater-1.0.9 — 2026-03-23

**TCP repeater prebuilt** (Heltec WiFi LoRa 32 **V4** + **V3**). Train: **`repeater-X.Y.Z`**, independent of companion **`v1.14.0.x`**.

**Compile-time version string:** `meshcomod-repeater-1.0.9-<gitsha>` (this build: **`98dfca5d`** in `out/` filenames).

**Changes vs repeater-1.0.8**

- **Resources tab parity with companion:** Repeater TCP UI now includes a dedicated **Resources** page with the same metrics/layout as companion: CPU MHz, RAM used %, PSRAM used % (or n/a), and Flash used %.

**Build:** **`DISABLE_DEBUG=1`**. App + **merged** (flash **0x0**) binaries.

| Device | Merged (0x0) | App-only |
|--------|--------------|----------|
| Heltec V4 | [heltec_v4_repeater_tcp-merged.bin](heltec_v4_repeater_tcp-merged.bin) | [heltec_v4_repeater_tcp.bin](heltec_v4_repeater_tcp.bin) |
| Heltec V3 | [Heltec_v3_repeater_tcp-merged.bin](Heltec_v3_repeater_tcp-merged.bin) | [Heltec_v3_repeater_tcp.bin](Heltec_v3_repeater_tcp.bin) |

**Procedure:** [`docs/REPEATER_RELEASE_PROCEDURE.md`](../../../docs/REPEATER_RELEASE_PROCEDURE.md).
