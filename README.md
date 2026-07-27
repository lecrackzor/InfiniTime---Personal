# InfiniTime — Personal

Personal [InfiniTime](https://github.com/InfiniTimeOrg/InfiniTime) fork for the PineTime. Built on upstream `main`; this page only covers what differs.

Photos of the watchface / UI can go here later.

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

## Upstream

Based on [InfiniTimeOrg/InfiniTime](https://github.com/InfiniTimeOrg/InfiniTime). Same GPL-3.0-or-later license. Build, flash, and BLE docs live in upstream’s `doc/` tree.
