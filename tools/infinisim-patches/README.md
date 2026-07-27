# InfiniSim patches (personal)

Apply these onto a local InfiniSim tree that points at [InfiniTime---Personal](https://github.com/lecrackzor/InfiniTime---Personal):

```bash
cp tools/infinisim-patches/queue.cpp              "$INFINSIM/sim/queue.cpp"
cp tools/infinisim-patches/queue.h                "$INFINSIM/sim/queue.h"
cp tools/infinisim-patches/semphr.cpp             "$INFINSIM/sim/semphr.cpp"
cp tools/infinisim-patches/semphr.h               "$INFINSIM/sim/semphr.h"
cp tools/infinisim-patches/HeartRateTask.h        "$INFINSIM/sim/heartratetask/HeartRateTask.h"
cp tools/infinisim-patches/HeartRateController.h  "$INFINSIM/sim/components/heartrate/HeartRateController.h"
cp tools/infinisim-patches/HeartRateController.cpp "$INFINSIM/sim/components/heartrate/HeartRateController.cpp"
```

Also update InfiniSim `main.cpp`:
- Construct `HeartRateTask` after `settingsController` with `(Hrs3300, HeartRateController, Settings, Bma421)`
- Define `NoInit_Persistence` (from `components/persistence/RebootPersist.h`)
- Align keyboard HR simulation with `UpdateState` / `UpdateHeartRate` and PPGv2 states

## What changed
- **queue.cpp / queue.h** — thread-safe FreeRTOS queue shim (`mutex` + `condition_variable`) to stop the sleep-time segfault
- **semphr.cpp / semphr.h** — recursive mutex stubs for personal FS locks
- **HeartRateTask.h** — charging pause messages + PPGv2 four-arg ctor
- **HeartRateController.\*** — PPGv2 states / `UpdateState` + `UpdateHeartRate` API

Preferred long-term: keep a personal InfiniSim fork (`InfiniSim---Personal`) with these commits so you do not re-apply patches.
