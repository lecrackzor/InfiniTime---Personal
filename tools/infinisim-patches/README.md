# InfiniSim patches (personal)

Apply these onto a local InfiniSim tree that points at [InfiniTime---Personal](https://github.com/lecrackzor/InfiniTime---Personal):

```bash
cp tools/infinisim-patches/queue.cpp    "$INFINSIM/sim/queue.cpp"
cp tools/infinisim-patches/queue.h      "$INFINSIM/sim/queue.h"
cp tools/infinisim-patches/HeartRateTask.h "$INFINSIM/sim/heartratetask/HeartRateTask.h"
```

## What changed
- **queue.cpp / queue.h** — thread-safe FreeRTOS queue shim (`mutex` + `condition_variable`) to stop the sleep-time segfault
- **HeartRateTask.h** — `PauseForCharging` / `ResumeFromCharging` message enums matching the InfiniTime fork

Preferred long-term: keep a personal InfiniSim fork (`InfiniSim---Personal`) with these commits so you do not re-apply patches.
