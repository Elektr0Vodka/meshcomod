# Versioned prebuilts

## TCP repeater (Wi‑Fi / `*_repeater_tcp`)

**Current builds** use the same **`v1.14.1.x`** line as the meshcomod companion, with a **`‑repeater-tcp`** build tag. They are published under:

- **`v1.14.1.0/`** (and future **`v1.14.1.1/`**, …) — look for **`heltec_v4_repeater_tcp*.bin`**, **`heltec_v4_tft_repeater_tcp*.bin`**, **`Heltec_v3_repeater_tcp*.bin`**.

On the device, the version string looks like **`meshcomod-v1.14.1.0-repeater-tcp-<gitsha>`**.

## Legacy `repeater-1.0.x/` folders

Directories named **`repeater-1.0.0`** … **`repeater-1.0.11`** were an older naming scheme. **Binaries are no longer kept there** (only **`notes.md`** remains per version). Use **`v1.14.1.0/`** for current TCP repeater downloads, or pull old `.bin` files from **git history** if you need an exact legacy build.

## Companion and extras

Companion **`v1.14.x.x`** folders include USB+TCP (and TFT touch) meshcomod radios, Heltec V4 extras, and — from **`v1.14.1.0` onward** — TCP repeater binaries in the same folder.

See **[`RELEASES.md`](../../RELEASES.md)** for the full changelog and links.
