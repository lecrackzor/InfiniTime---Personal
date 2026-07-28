# InfiniSim patches (personal)

Apply onto a local InfiniSim tree that points at [InfiniTime---Personal](https://github.com/lecrackzor/InfiniTime---Personal):

```bash
# WSL / Linux — one shot
python3 tools/infinisim-patches/apply-personal-sim.py
# expects InfiniSim at ~/Projects/InfiniSim and InfiniTime at ~/Projects/InfiniTime
# (symlink InfiniSim/InfiniTime -> personal tree)
```

Or copy files manually:

```bash
INFINSIM=$HOME/Projects/InfiniSim
cp tools/infinisim-patches/queue.cpp               "$INFINSIM/sim/queue.cpp"
cp tools/infinisim-patches/queue.h                 "$INFINSIM/sim/queue.h"
cp tools/infinisim-patches/semphr.cpp              "$INFINSIM/sim/semphr.cpp"
cp tools/infinisim-patches/semphr.h                "$INFINSIM/sim/semphr.h"
cp tools/infinisim-patches/HeartRateTask.h         "$INFINSIM/sim/heartratetask/HeartRateTask.h"
cp tools/infinisim-patches/HeartRateTask.cpp       "$INFINSIM/sim/heartratetask/HeartRateTask.cpp"
cp tools/infinisim-patches/HeartRateController.h   "$INFINSIM/sim/components/heartrate/HeartRateController.h"
cp tools/infinisim-patches/HeartRateController.cpp "$INFINSIM/sim/components/heartrate/HeartRateController.cpp"
```

`apply-personal-sim.py` also sets InfiniSim `CMakeLists.txt` / `main.cpp` for personal settings:
- `MONITOR_ZOOM=2`, `INFINITIME_SIMULATOR=1`
- exclude trimmed Paint/Paddle/Twos/Dice/Metronome + Analog/Infineat/PTS/PrideFlag screens
- PPGv2 `HeartRateTask` ctor, face keys `1` Digital / `2` Terminal / `3` Casio, boot `DisableSleeping`

Also required in InfiniSim (not always copied as files — keep in your InfiniSim working tree):
- `sim/displayapp/LittleVgl.cpp`: `MoveScreen` scrolls `monitor.tft_fb` with `memmove` only (no SDL from DisplayApp)
- `sim/nrfx/hal/nrf_gpio.cpp`: latched button GPIO (`nrf_gpio_sim_set_button`)
- `sim/task.cpp`: `vTaskDelay` converts ticks → ms
- `lv_drivers/display/monitor.c`: present only via main-thread `monitor_sdl_ui_update()`

## What changed
- **queue / semphr** — thread-safe FreeRTOS shims + recursive mutex for FS locks
- **HeartRate\*** — PPGv2 ctor/states + charging pause messages
- **InfiniTime DisplayApp** — dim/sleep compiled out when `INFINITIME_SIMULATOR` is set
- **InfiniTime SystemTask** — always re-poke DisplayApp on `GoToRunning` (fixes Idle/Running desync)
- **MoveScreen** — Settings (and Launcher/Notifications) Up/Down scroll used to call `SDL_RenderReadPixels` from DisplayApp and leak surfaces → black window on Wayland/X11. Fixed by scrolling `tft_fb` in memory; Present stays on main.

## Known-good run

```bash
# build
export MAMBA_ROOT_PREFIX=$HOME/nrf52/tools/micromamba/root
ENV=$MAMBA_ROOT_PREFIX/envs/infinisim
export PATH="$ENV/bin:$HOME/nrf52/tools/cmake/bin:$PATH"
export CC=$ENV/bin/x86_64-conda-linux-gnu-gcc
export CXX=$ENV/bin/x86_64-conda-linux-gnu-g++
cd ~/Projects/InfiniSim
cmake -S . -B build -DMONITOR_ZOOM=2 -DInfiniTime_DIR=$HOME/Projects/InfiniTime
cmake --build build -j$(nproc) --target infinisim

# run
~/Projects/run-infinisim.sh
# or: tools/infinisim-patches/run-infinisim.sh
```

Keys: `3` Casio Custom, `1` Digital, `2` Terminal, `i` screenshot, `w` inject weather, right-click = side button.

**README / doc screenshots:** Casio Custom is weather-first. Always press `w` before capturing; otherwise the PNG looks mostly empty on the right.

Preferred long-term: personal InfiniSim fork so patches are not re-copied.
