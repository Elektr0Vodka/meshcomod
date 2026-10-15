# filter/

Header-only building blocks of the companion receive filter (`../CompanionFilter.*`).

They are copied from the repeater packet filter of
[Dutch-MeshCore/MeshCore](https://github.com/Dutch-MeshCore/MeshCore)
(`examples/simple_repeater/`, branch `enhancement/dmc-dev-companion-filter` at `31702c2f`),
so the `filter` commands and their replies match the DMC repeater and companion firmware.
Prefer fixing things there first and copying the files over again, so the two do not drift.

| File | What |
| --- | --- |
| `SenderRules.h` | sender / text rule matching, ordered evaluation, throttle and dosing, list editing |
| `PathBlock.h` | blocked path-prefix list |
| `MessageAge.h` | timestamp sanity window and the `filter age` limit |
| `MessageContent.h` | malformed group text scan (timestamp, empty text, UTF-8) |
| `FilterStats.h` | drop counters and bounded reply formatting |
