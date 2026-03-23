# Versioned prebuilts

## TCP repeater (Wi‑Fi / `*_repeater_tcp`)

**Canonical location (Flasher + Repeater app version picker):** companion-style folders **`v1.14.1.0`**, **`v1.14.1.1`**, … under this directory. Each contains **`heltec_v4_repeater_tcp*.bin`**, **`heltec_v4_tft_repeater_tcp*.bin`**, **`Heltec_v3_repeater_tcp*.bin`** next to companion images.

The **meshcomod** client lists these **`v*`** dirs for the repeater product (from **v1.14.1.0** upward). `resolveArtifactUrl` turns `prebuilt/<name>.bin` into `prebuilt/releases/<version>/<name>.bin`.

On-device version strings may read **`meshcomod-v1.14.1.0-repeater-tcp-<gitsha>`** when built with `REPEATER_FIRMWARE_VERSION=v1.14.1.0-repeater-tcp`.

## Companion and extras

Companion **`v1.14.x.x`** folders include USB+TCP (and TFT touch) meshcomod radios and Heltec V4 extras per release notes.

See **[`RELEASES.md`](../../RELEASES.md)** and **[`prebuilt/README.md`](../README.md)** for copy scripts and procedures.
