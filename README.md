# InfiniTime — Personal

Personal [InfiniTime](https://github.com/InfiniTimeOrg/InfiniTime) fork for the PineTime. Built on upstream `main`; this page only covers what differs.

![Casio Custom color cycling](doc/personal/casio-custom-color-cycle.gif)

## Changes vs upstream

### Casio Custom watchface
- Renamed from **Casio Style G7710** to **Casio Custom**
- Weather-focused layout: date and day on the left; icon, temperature, and daily low/high on the right (replaces week number / day-of-year)
- Long-press overlay: cycle face color (paintbrush) and step display brightness

### Apps
Removed from the default app list:
- Paint
- Paddle
- Twos
- Dice
- Metronome

Kept: Stopwatch, Alarm, Timer, Steps, Heart Rate, Music, Navigation, Calculator, Weather

### Heart rate
- Background interval options: Off, Continuous, 30s, 1m, **3m**, 5m, 10m (removed 30m)
- **Start** state persists across reboot (so background HR keeps working after power cycle)
- Pauses while charging; resumes when unplugged
- Watch faces no longer show a bogus `0` BPM while measuring / waiting for a valid sample

### Bug fixes
- Timer: screen can sleep again after tapping **Reset** during the ring ([upstream #2419](https://github.com/InfiniTimeOrg/InfiniTime/issues/2419))
- Timer: keep buzzing for the full 10s after expiry if ringing was interrupted ([upstream #2428](https://github.com/InfiniTimeOrg/InfiniTime/pull/2428))
- Alarm: dismissing a ringing alarm returns to the previous screen instead of the editable alarm config ([upstream #2405](https://github.com/InfiniTimeOrg/InfiniTime/issues/2405))
- Notifications no longer interrupt a ringing alarm/timer or the flashlight ([upstream #1223](https://github.com/InfiniTimeOrg/InfiniTime/issues/1223) / [#610](https://github.com/InfiniTimeOrg/InfiniTime/issues/610))
- Motor: `StopRinging()` also cancels any pending short vibration pulse
- HR: charging pause clears stale BPM; HR task queue send is ISR-safe when called from task context
- BLE FS: `WriteResponse.status` always initialized (success and failure); oversize path early-outs release the file-transfer wake lock ([upstream #2457](https://github.com/InfiniTimeOrg/InfiniTime/issues/2457))
- BLE CTS: initialize `dayofweek` and `reason` on current-time reads ([upstream #2459](https://github.com/InfiniTimeOrg/InfiniTime/pull/2459))
- SPI: chunk EasyDMA reads to ≤255 bytes (same limit as writes) ([upstream #2391](https://github.com/InfiniTimeOrg/InfiniTime/pull/2391))
- FS: serialize littlefs with a recursive mutex across tasks ([upstream #2449](https://github.com/InfiniTimeOrg/InfiniTime/pull/2449))
- Persist time + steps across soft reboots via noinit RAM ([upstream #2400](https://github.com/InfiniTimeOrg/InfiniTime/pull/2400) / [#2293](https://github.com/InfiniTimeOrg/InfiniTime/issues/2293))
- DisplayApp: no-op `LVGL_GUARD` hooks for InfiniSim thread-safety ([upstream #2447](https://github.com/InfiniTimeOrg/InfiniTime/pull/2447))
- Weather: equality compares `minTemperature` to `minTemperature` (was wrongly vs `max`)
- DateTime: compile-time fallback year set after logger init ([upstream #2396](https://github.com/InfiniTimeOrg/InfiniTime/issues/2396))
- DateTime: `to_time_t` cast compatible with libc++ ([upstream #2456](https://github.com/InfiniTimeOrg/InfiniTime/pull/2456))
- GPIOTE: button + touch use high-accuracy sensing so edges are not dropped while both IRQs are live ([upstream #2346](https://github.com/InfiniTimeOrg/InfiniTime/issues/2346))
- DisplayApp: `TouchEvent` queue send is non-blocking (same deadlock avoidance as notifications) ([upstream #2290](https://github.com/InfiniTimeOrg/InfiniTime/issues/2290) / [#2124](https://github.com/InfiniTimeOrg/InfiniTime/issues/2124))
- Stopwatch: rebuild lap list in one buffer instead of repeated `lv_label_ins_text` (reduces LVGL churn under mash)
- Docker: ensure `build.sh` is executable in the image ([upstream #2367](https://github.com/InfiniTimeOrg/InfiniTime/pull/2367))
- Battery icon: low-battery color no longer overwrites the configured base color ([upstream #2208](https://github.com/InfiniTimeOrg/InfiniTime/pull/2208))
- Music: show waiting placeholders until real track metadata arrives ([upstream #1841](https://github.com/InfiniTimeOrg/InfiniTime/pull/1841))
- SPI: chunk `WriteCmdAndBuffer` payloads the same way as reads/writes
- Stopwatch: lap labels wrap in 1..999 (0 is empty-slot sentinel, so `% 1000` made laps vanish)
- Notifications: `GetPrevious` bounds against valid count, not buffer capacity
- CI: set `REF_NAME` in the InfiniSim job so artifacts are not named `infinitisim-` ([upstream #2223](https://github.com/InfiniTimeOrg/InfiniTime/issues/2223))

### InfiniSim
Local InfiniSim patches (queue segfault fix + HR charging message enums) live in [`tools/infinisim-patches/`](tools/infinisim-patches/). Prefer keeping them on a personal InfiniSim fork once created.

## Upstream

Based on [InfiniTimeOrg/InfiniTime](https://github.com/InfiniTimeOrg/InfiniTime). Same GPL-3.0-or-later license. Build, flash, and BLE docs live in upstream’s `doc/` tree.
