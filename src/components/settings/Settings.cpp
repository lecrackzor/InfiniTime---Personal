#include "components/settings/Settings.h"
#include <cstdlib>
#include <cstring>

using namespace Pinetime::Controllers;

Settings::Settings(Pinetime::Controllers::FS& fs) : fs {fs} {
}

void Settings::Init() {

  // Load default settings from Flash
  LoadSettingsFromFile();
}

void Settings::SaveSettings() {

  // verify if is necessary to save
  if (settingsChanged) {
    if (SaveSettingsToFile()) {
      settingsChanged = false;
    }
  }
}

void Settings::LoadSettingsFromFile() {
  SettingsData bufferSettings;
  lfs_file_t settingsFile;

  if (fs.FileOpen(&settingsFile, "/settings.dat", LFS_O_RDONLY) != LFS_ERR_OK) {
    return;
  }
  const auto read = fs.FileRead(&settingsFile, reinterpret_cast<uint8_t*>(&bufferSettings), sizeof(settings));
  fs.FileClose(&settingsFile);
  // Reject short / truncated files — partial reads leave default-constructed fields that can
  // look "valid" when version happens to match (power-loss during a previous save).
  if (read != static_cast<int>(sizeof(settings))) {
    return;
  }
  if (bufferSettings.version == settingsVersion) {
    settings = bufferSettings;
  }
}

bool Settings::SaveSettingsToFile() {
  lfs_file_t settingsFile;

  // Truncate so a shorter SettingsData never leaves a stale tail from an older layout.
  if (fs.FileOpen(&settingsFile, "/settings.dat", LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC) != LFS_ERR_OK) {
    return false;
  }
  const auto written = fs.FileWrite(&settingsFile, reinterpret_cast<uint8_t*>(&settings), sizeof(settings));
  fs.FileClose(&settingsFile);
  return written == static_cast<int>(sizeof(settings));
}
