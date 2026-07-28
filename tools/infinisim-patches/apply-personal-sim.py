#!/usr/bin/env python3
"""Configure a local InfiniSim tree for InfiniTime---Personal."""
from pathlib import Path
import os
import shutil
import sys

HOME = Path.home()
SIM = Path(os.environ.get("INFINSIM_DIR", str(HOME / "Projects/InfiniSim")))
PATCH = Path(
    os.environ.get(
        "INFINITIME_PATCHES",
        str(HOME / "Projects/InfiniTime/tools/infinisim-patches"),
    )
)

# Prefer a Windows InfiniTime checkout mounted in WSL when present (any username).
users_root = Path("/mnt/c/Users")
if users_root.is_dir():
    for user_dir in users_root.iterdir():
        candidate = user_dir / "Projects/InfiniTime/tools/infinisim-patches"
        if candidate.is_dir() and (candidate / "apply-personal-sim.py").is_file():
            PATCH = candidate
            break

if not SIM.is_dir():
    sys.exit(f"missing InfiniSim at {SIM}")
if not PATCH.is_dir():
    sys.exit(f"missing patches at {PATCH}")

copies = [
    ("queue.cpp", "sim/queue.cpp"),
    ("queue.h", "sim/queue.h"),
    ("semphr.cpp", "sim/semphr.cpp"),
    ("semphr.h", "sim/semphr.h"),
    ("HeartRateTask.h", "sim/heartratetask/HeartRateTask.h"),
    ("HeartRateTask.cpp", "sim/heartratetask/HeartRateTask.cpp"),
    ("HeartRateController.h", "sim/components/heartrate/HeartRateController.h"),
    ("HeartRateController.cpp", "sim/components/heartrate/HeartRateController.cpp"),
]
for src_name, dst_rel in copies:
    src = PATCH / src_name
    dst = SIM / dst_rel
    if not src.is_file():
        sys.exit(f"missing patch file {src}")
    dst.parent.mkdir(parents=True, exist_ok=True)
    data = src.read_bytes().replace(b"\r\n", b"\n")
    dst.write_bytes(data)
    print(f"copied {src_name} -> {dst_rel}")

# CMakeLists.txt
cmake = SIM / "CMakeLists.txt"
text = cmake.read_text()
text = text.replace(
    'set(MONITOR_ZOOM 1 CACHE STRING "Scale simulator window by this factor")',
    'set(MONITOR_ZOOM 2 CACHE STRING "Scale simulator window by this factor")',
)
filt = (
    "# InfiniTime---Personal: apps/faces trimmed from the firmware build\n"
    'list(FILTER InfiniTime_SCREENS EXCLUDE REGEX '
    '".*/(InfiniPaint|Paddle|Twos|Metronome|Dice|WatchFaceAnalog|WatchFaceInfineat|'
    'WatchFacePineTimeStyle|WatchFacePrideFlag)\\\\.(cpp|h)$")\n'
)
if "list(FILTER InfiniTime_SCREENS EXCLUDE REGEX" not in text:
    marker = '  "${InfiniTime_DIR}/src/displayapp/screens/settings/*.cpp"\n)\n'
    if marker not in text:
        sys.exit("screens glob marker not found")
    text = text.replace(marker, marker + filt)
if "INFINITIME_SIMULATOR" not in text:
    text = text.replace(
        "add_executable(infinisim main.cpp)\n",
        "add_executable(infinisim main.cpp)\n"
        "target_compile_definitions(infinisim PUBLIC INFINITIME_SIMULATOR=1)\n",
    )
cmake.write_text(text)
print("CMakeLists updated")

# main.cpp
main = SIM / "main.cpp"
text = main.read_text()

old_globals = """Pinetime::Controllers::HeartRateController heartRateController;
Pinetime::Applications::HeartRateTask heartRateApp(heartRateSensor, heartRateController);

Pinetime::Controllers::FS fs {spiNorFlash};
Pinetime::Controllers::Settings settingsController {fs};
"""
new_globals = """Pinetime::Controllers::FS fs {spiNorFlash};
Pinetime::Controllers::Settings settingsController {fs};
Pinetime::Controllers::HeartRateController heartRateController;
Pinetime::Applications::HeartRateTask heartRateApp(heartRateSensor, heartRateController, settingsController, motionSensor);
"""
if old_globals in text:
    text = text.replace(old_globals, new_globals)
elif "settingsController, motionSensor);" in text:
    print("main.cpp HR globals already personal")
else:
    sys.exit("main.cpp globals block not found")

old_switch = """  void switch_to_screen(uint8_t screen_idx) {
    if (screen_idx == 1) {
      settingsController.SetWatchFace(Pinetime::Applications::WatchFace::Digital);
      displayApp.StartApp(Pinetime::Applications::Apps::Clock, Pinetime::Applications::DisplayApp::FullRefreshDirections::None);
    } else if (screen_idx == 2) {
      settingsController.SetWatchFace(Pinetime::Applications::WatchFace::Analog);
      displayApp.StartApp(Pinetime::Applications::Apps::Clock, Pinetime::Applications::DisplayApp::FullRefreshDirections::None);
    } else if (screen_idx == 3) {
      settingsController.SetWatchFace(Pinetime::Applications::WatchFace::PineTimeStyle);
      displayApp.StartApp(Pinetime::Applications::Apps::Clock, Pinetime::Applications::DisplayApp::FullRefreshDirections::None);
    } else if (screen_idx == 4) {
      displayApp.StartApp(Pinetime::Applications::Apps::Paddle, Pinetime::Applications::DisplayApp::FullRefreshDirections::None);
    } else if (screen_idx == 5) {
      displayApp.StartApp(Pinetime::Applications::Apps::Twos, Pinetime::Applications::DisplayApp::FullRefreshDirections::None);
    } else if (screen_idx == 6) {
      displayApp.StartApp(Pinetime::Applications::Apps::Metronome, Pinetime::Applications::DisplayApp::FullRefreshDirections::None);
    } else if (screen_idx == 7) {
"""
new_switch = """  void switch_to_screen(uint8_t screen_idx) {
    // Personal firmware faces: 1 Digital, 2 Terminal, 3 Casio Custom
    if (screen_idx == 1) {
      settingsController.SetWatchFace(Pinetime::Applications::WatchFace::Digital);
      displayApp.StartApp(Pinetime::Applications::Apps::Clock, Pinetime::Applications::DisplayApp::FullRefreshDirections::None);
    } else if (screen_idx == 2) {
      settingsController.SetWatchFace(Pinetime::Applications::WatchFace::Terminal);
      displayApp.StartApp(Pinetime::Applications::Apps::Clock, Pinetime::Applications::DisplayApp::FullRefreshDirections::None);
    } else if (screen_idx == 3) {
      settingsController.SetWatchFace(Pinetime::Applications::WatchFace::CasioStyleG7710);
      displayApp.StartApp(Pinetime::Applications::Apps::Clock, Pinetime::Applications::DisplayApp::FullRefreshDirections::None);
    } else if (screen_idx == 7) {
"""
if old_switch in text:
    text = text.replace(old_switch, new_switch)
elif "WatchFace::CasioStyleG7710" in text:
    print("main.cpp watchface keys already personal")
else:
    sys.exit("main.cpp switch_to_screen block not found")

text = text.replace(
    """    } else if (screen_idx == 12) {
      displayApp.StartApp(Pinetime::Applications::Apps::Paint, Pinetime::Applications::DisplayApp::FullRefreshDirections::None);
    } else if (screen_idx == 13) {""",
    """    } else if (screen_idx == 13) {""",
)

old_hr = """      if (heartRateController.State() == Pinetime::Controllers::HeartRateController::States::Stopped) {
        heartRateController.Enable();
      } else if (heartRateController.State() == Pinetime::Controllers::HeartRateController::States::NotEnoughData) {
        heartRateController.Update(Pinetime::Controllers::HeartRateController::States::Running, 10);
      } else {
        uint8_t heartrate = heartRateController.HeartRate();
        heartRateController.Update(Pinetime::Controllers::HeartRateController::States::Running, heartrate + 10);
      }"""
new_hr = """      if (heartRateController.State() == Pinetime::Controllers::HeartRateController::States::Disabled) {
        heartRateController.Enable();
      } else if (heartRateController.State() == Pinetime::Controllers::HeartRateController::States::NotEnoughData ||
                 heartRateController.State() == Pinetime::Controllers::HeartRateController::States::Searching ||
                 heartRateController.State() == Pinetime::Controllers::HeartRateController::States::Stopped) {
        heartRateController.UpdateState(Pinetime::Controllers::HeartRateController::States::Ready);
        heartRateController.UpdateHeartRate(10);
      } else {
        uint8_t heartrate = heartRateController.HeartRate();
        heartRateController.UpdateState(Pinetime::Controllers::HeartRateController::States::Ready);
        heartRateController.UpdateHeartRate(heartrate + 10);
      }"""
if old_hr in text:
    text = text.replace(old_hr, new_hr)
elif "UpdateHeartRate" in text:
    print("main.cpp HR hotkeys already personal")
else:
    sys.exit("main.cpp HR hotkey block not found")

old_start = """    systemTask.Start();

    // initialize the first LVGL screen"""
new_start = """    systemTask.Start();
    // Personal: keep the sim awake — dim/sleep fights SDL and looks like a black window
    systemTask.PushMessage(Pinetime::System::Messages::DisableSleeping);

    // initialize the first LVGL screen"""
if old_start in text:
    text = text.replace(old_start, new_start)
elif "DisableSleeping" in text:
    print("main.cpp DisableSleeping already present")
else:
    sys.exit("main.cpp systemTask.Start block not found")

# RebootPersist is defined in InfiniTime main (.noinit); sim needs a normal definition.
old_noinit = "std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds> NoInit_BackUpTime;"
new_noinit = (
    old_noinit
    + "\n// Firmware stores this in .noinit; sim needs a normal definition for the linker.\n"
    + "volatile Pinetime::Components::RebootPersist NoInit_Persistence;"
)
if "volatile Pinetime::Components::RebootPersist NoInit_Persistence" in text:
    print("main.cpp NoInit_Persistence already present")
elif old_noinit in text:
    text = text.replace(old_noinit, new_noinit, 1)
else:
    sys.exit("main.cpp NoInit_BackUpTime anchor not found")

main.write_text(text)
print("main.cpp updated")

# launcher
launcher = HOME / "Projects/run-infinisim.sh"
shutil.copyfile(PATCH / "run-infinisim.sh", launcher)
launcher.chmod(0o755)
# also keep a copy next to InfiniTime tools in the Linux tree when patches came from elsewhere
wsl_patch = HOME / "Projects/InfiniTime/tools/infinisim-patches"
wsl_patch.mkdir(parents=True, exist_ok=True)
for name in ("run-infinisim.sh", "HeartRateTask.cpp", "apply-personal-sim.py"):
    src = PATCH / name
    if src.is_file():
        data = src.read_bytes().replace(b"\r\n", b"\n")
        (wsl_patch / name).write_bytes(data)
        if name.endswith(".sh"):
            (wsl_patch / name).chmod(0o755)

PERSONAL_WATCHFACES = "WatchFace::Digital, WatchFace::Terminal, WatchFace::CasioStyleG7710"
PERSONAL_APPS = (
    "Apps::StopWatch, Apps::Alarm, Apps::Timer, Apps::Steps, Apps::HeartRate, "
    "Apps::Music, Apps::Navigation, Apps::Calculator, Apps::Weather"
)

cache = SIM / "build/CMakeCache.txt"
if cache.is_file():
    lines = []
    for line in cache.read_text().splitlines():
        if line.startswith("MONITOR_ZOOM:"):
            lines.append("MONITOR_ZOOM:STRING=2")
        elif line.startswith("WATCHFACE_TYPES:"):
            lines.append(f"WATCHFACE_TYPES:STRING={PERSONAL_WATCHFACES}")
        elif line.startswith("USERAPP_TYPES:"):
            lines.append(f"USERAPP_TYPES:STRING={PERSONAL_APPS}")
        else:
            lines.append(line)
    cache.write_text("\n".join(lines) + "\n")
    print("CMakeCache MONITOR_ZOOM/WATCHFACE_TYPES/USERAPP_TYPES updated")

print("ALL_OK")
print("Rebuild with personal faces:")
print(
    "  cmake -S ~/Projects/InfiniSim -B ~/Projects/InfiniSim/build "
    f'-DWATCHFACE_TYPES="{PERSONAL_WATCHFACES}" '
    f'-DUSERAPP_TYPES="{PERSONAL_APPS}"'
)
print("  cmake --build ~/Projects/InfiniSim/build --target infinisim")
