# InfiniSim patches (personal)

Apply these onto a local InfiniSim tree that points at [InfiniTime---Personal](https://github.com/lecrackzor/InfiniTime---Personal):

```bash
INFINSIM=/home/dethbox/Projects/InfiniSim
cp tools/infinisim-patches/queue.cpp               "$INFINSIM/sim/queue.cpp"
cp tools/infinisim-patches/queue.h                 "$INFINSIM/sim/queue.h"
cp tools/infinisim-patches/semphr.cpp              "$INFINSIM/sim/semphr.cpp"
cp tools/infinisim-patches/semphr.h                "$INFINSIM/sim/semphr.h"
cp tools/infinisim-patches/HeartRateTask.h         "$INFINSIM/sim/heartratetask/HeartRateTask.h"
cp tools/infinisim-patches/HeartRateController.h   "$INFINSIM/sim/components/heartrate/HeartRateController.h"
cp tools/infinisim-patches/HeartRateController.cpp "$INFINSIM/sim/components/heartrate/HeartRateController.cpp"
```

Also required in InfiniSim (not always copied as files — keep in your InfiniSim working tree):
- `main.cpp`: PPGv2 `HeartRateTask` ctor, `NoInit_Persistence`, no “Screen is OFF” overlay, boot `DisableSleeping`
- `sim/nrfx/hal/nrf_gpio.cpp`: latched button GPIO (`nrf_gpio_sim_set_button`)
- `sim/task.cpp`: `vTaskDelay` converts ticks → ms
- `CMakeLists.txt`: `INFINITIME_SIMULATOR=1`, `MONITOR_ZOOM=2`

## What changed
- **queue / semphr** — thread-safe FreeRTOS shims + recursive mutex for FS locks
- **HeartRate\*** — PPGv2 ctor/states + charging pause messages
- **InfiniTime DisplayApp** — dim/sleep compiled out when `INFINITIME_SIMULATOR` is set
- **InfiniTime SystemTask** — always re-poke DisplayApp on `GoToRunning` (fixes Idle/Running desync)

## Known-good run

```bash
# once: deps
sudo pacman -S cmake ninja nodejs npm sdl2 libpng xorg-server-xvfb

# build (from InfiniSim, pointing at personal InfiniTime)
cmake -G Ninja -S /home/dethbox/Projects/InfiniSim -B /home/dethbox/Projects/build_lv_sim \
  -DInfiniTime_DIR=/home/dethbox/Projects/InfiniTime---Personal \
  -DBUILD_RESOURCES=ON -DMONITOR_ZOOM=2
cmake --build /home/dethbox/Projects/build_lv_sim --target infinisim littlefs-do
cd /home/dethbox/Projects/build_lv_sim
./littlefs-do res load resources/infinitime-resources-*.zip

# run (kills stale sim first)
chmod +x /home/dethbox/Projects/InfiniTime---Personal/tools/infinisim-patches/run-infinisim.sh
/home/dethbox/Projects/InfiniTime---Personal/tools/infinisim-patches/run-infinisim.sh
```

Keys: `3` Casio Custom, `1` Digital, `i` screenshot, right-click = side button.

If the window is a **black box**: you are on an old binary, or SDL present was off-thread.
Rebuild InfiniSim (monitor.c present-from-main fix) and use `run-infinisim.sh` above.

One-shot Casio PNG (no interaction):

```bash
cd /home/dethbox/Projects/build_lv_sim
./infinisim --casio-preview /tmp/casio.png
```

Preferred long-term: personal InfiniSim fork so patches are not re-copied.
