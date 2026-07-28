#include "displayapp/screens/WatchFaceCasioStyleG7710.h"

#include <lvgl/lvgl.h>
#include <cstdio>
#include <FreeRTOS.h>
#include <task.h>
#include "displayapp/screens/BatteryIcon.h"
#include "displayapp/screens/BleIcon.h"
#include "displayapp/screens/NotificationIcon.h"
#include "displayapp/screens/Symbols.h"
#include "displayapp/screens/WeatherSymbols.h"
#include "components/battery/BatteryController.h"
#include "components/ble/BleController.h"
#include "components/ble/NotificationManager.h"
#include "components/heartrate/HeartRateController.h"
#include "components/motion/MotionController.h"
#include "components/ble/SimpleWeatherService.h"
#include "components/brightness/BrightnessController.h"
#include "components/settings/Settings.h"
#include "components/alarm/AlarmController.h"

using namespace Pinetime::Applications::Screens;

extern lv_font_t jetbrains_mono_bold_20;
extern lv_font_t jetbrains_mono_extrabold_compressed;
LV_FONT_DECLARE(lv_font_sys_48);

namespace {
  // Warm Cockpit theme (mockup #9) — role colors on black.
  constexpr lv_color_t ColorTime = LV_COLOR_MAKE(0xFF, 0xC1, 0x07);      // amber
  constexpr lv_color_t ColorDate = LV_COLOR_MAKE(0xFF, 0x98, 0x00);      // orange
  constexpr lv_color_t ColorWeather = LV_COLOR_MAKE(0x80, 0xDE, 0xEA);   // soft cyan
  constexpr lv_color_t ColorHeart = LV_COLOR_MAKE(0xFF, 0x52, 0x52);     // red
  constexpr lv_color_t ColorSteps = LV_COLOR_MAKE(0xB2, 0xFF, 0x59);     // lime
  constexpr lv_color_t ColorLines = LV_COLOR_MAKE(0xC7, 0x91, 0x00);     // dim amber
  constexpr lv_color_t ColorChrome = LV_COLOR_MAKE(0xC7, 0x91, 0x00);    // status icons
  constexpr lv_color_t ColorHeartIdle = LV_COLOR_MAKE(0x1B, 0x1B, 0x1B);

  void event_handler(lv_obj_t* obj, lv_event_t event) {
    auto* screen = static_cast<WatchFaceCasioStyleG7710*>(obj->user_data);
    screen->OnOverlayButtonEvent(obj, event);
  }
}

WatchFaceCasioStyleG7710::WatchFaceCasioStyleG7710(Controllers::DateTime& dateTimeController,
                                                   const Controllers::Battery& batteryController,
                                                   const Controllers::Ble& bleController,
                                                   Controllers::NotificationManager& notificatioManager,
                                                   Controllers::Settings& settingsController,
                                                   Controllers::HeartRateController& heartRateController,
                                                   Controllers::MotionController& motionController,
                                                   Controllers::SimpleWeatherService& weatherService,
                                                   Controllers::BrightnessController& brightnessController,
                                                   Controllers::FS& filesystem,
                                                   const Controllers::AlarmController& alarmController)
  : currentDateTime {{}},
    batteryIcon(true),
    dateTimeController {dateTimeController},
    batteryController {batteryController},
    bleController {bleController},
    notificatioManager {notificatioManager},
    settingsController {settingsController},
    heartRateController {heartRateController},
    motionController {motionController},
    weatherService {weatherService},
    brightnessController {brightnessController},
    alarmController {alarmController} {

  // Two flash fonts for PPGv2 RAM headroom: big 7-segment time + Casio dots for
  // date/day/temp. Dropped 7segments_40 (digits-only, ~760B) — dots covers those glyphs.
  // Fall back to built-in JetBrains if load fails (do not free those pointers).
  lfs_file f = {};
  if (filesystem.FileOpen(&f, "/fonts/lv_font_dots_40.bin", LFS_O_RDONLY) >= 0) {
    filesystem.FileClose(&f);
    font_dot40 = lv_font_load("F:/fonts/lv_font_dots_40.bin");
  }
  if (font_dot40 == nullptr) {
    font_dot40 = &jetbrains_mono_bold_20;
  } else {
    font_dot40_loaded = true;
  }

  if (filesystem.FileOpen(&f, "/fonts/7segments_115.bin", LFS_O_RDONLY) >= 0) {
    filesystem.FileClose(&f);
    font_segment115 = lv_font_load("F:/fonts/7segments_115.bin");
  }
  if (font_segment115 == nullptr) {
    font_segment115 = &jetbrains_mono_extrabold_compressed;
  } else {
    font_segment115_loaded = true;
  }

  label_battery_value = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_align(label_battery_value, lv_scr_act(), LV_ALIGN_IN_TOP_RIGHT, 0, 0);
  lv_label_set_text_static(label_battery_value, "00%");

  batteryIcon.Create(lv_scr_act());
  lv_obj_align(batteryIcon.GetObject(), label_battery_value, LV_ALIGN_OUT_LEFT_MID, -5, 0);
  {
    const auto batteryPercent = batteryController.PercentRemaining();
    batteryIcon.SetBatteryPercentage(batteryPercent);
    lv_obj_set_style_local_text_color(label_battery_value,
                                      LV_LABEL_PART_MAIN,
                                      LV_STATE_DEFAULT,
                                      BatteryIcon::ColorFromPercentage(batteryPercent));
    lv_label_set_text_fmt(label_battery_value, "%d%%", batteryPercent);
  }

  batteryPlug = lv_label_create(lv_scr_act(), nullptr);
  lv_label_set_text_static(batteryPlug, Symbols::plug);
  lv_obj_align(batteryPlug, batteryIcon.GetObject(), LV_ALIGN_OUT_LEFT_MID, -5, 0);

  bleIcon = lv_label_create(lv_scr_act(), nullptr);
  lv_label_set_text_static(bleIcon, Symbols::bluetooth);
  lv_obj_align(bleIcon, batteryPlug, LV_ALIGN_OUT_LEFT_MID, -5, 0);

  alarmIcon = lv_label_create(lv_scr_act(), nullptr);
  lv_label_set_text_static(alarmIcon, Symbols::bell);
  lv_obj_align(alarmIcon, bleIcon, LV_ALIGN_OUT_LEFT_MID, -5, 0);
  lv_obj_set_hidden(alarmIcon, !alarmController.IsEnabled());

  notificationIcon = lv_label_create(lv_scr_act(), nullptr);
  lv_label_set_text_static(notificationIcon, NotificationIcon::GetIcon(false));
  lv_obj_align(notificationIcon, alarmIcon, LV_ALIGN_OUT_LEFT_MID, -5, 0);

  label_date = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_align(label_date, lv_scr_act(), LV_ALIGN_IN_TOP_LEFT, 5, 22);
  lv_obj_set_style_local_text_font(label_date, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, font_dot40);
  lv_label_set_text_static(label_date, "6-30");

  label_day_of_week = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_align(label_day_of_week, lv_scr_act(), LV_ALIGN_IN_TOP_LEFT, 10, 64);
  lv_obj_set_style_local_text_font(label_day_of_week, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, font_dot40);
  lv_label_set_text_static(label_day_of_week, "SUN");

  label_temperature_unit = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_font(label_temperature_unit, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, font_dot40);
  lv_label_set_text_static(label_temperature_unit, "");

  label_temperature = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_font(label_temperature, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, font_dot40);
  lv_label_set_text_static(label_temperature, "--");

  label_weather_icon = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_font(label_weather_icon, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &fontawesome_weathericons);
  lv_label_set_text_static(label_weather_icon, "");

  label_temperature_low = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_font(label_temperature_low, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &jetbrains_mono_bold_20);
  lv_label_set_text_static(label_temperature_low, "L--");

  label_temperature_high = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_font(label_temperature_high, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &jetbrains_mono_bold_20);
  lv_label_set_text_static(label_temperature_high, "H--");

  lv_obj_align(label_temperature_unit, lv_scr_act(), LV_ALIGN_IN_TOP_RIGHT, -8, 28);
  lv_obj_align(label_temperature, label_temperature_unit, LV_ALIGN_OUT_LEFT_MID, -4, 2);
  lv_obj_align(label_weather_icon, label_temperature, LV_ALIGN_OUT_LEFT_MID, -10, 0);
  lv_obj_align(label_temperature_low, lv_scr_act(), LV_ALIGN_IN_TOP_LEFT, 105, 78);
  lv_obj_align(label_temperature_high, lv_scr_act(), LV_ALIGN_IN_TOP_RIGHT, -8, 78);

  lv_style_init(&style_line);
  lv_style_set_line_width(&style_line, LV_STATE_DEFAULT, 2);
  lv_style_set_line_color(&style_line, LV_STATE_DEFAULT, ColorLines);
  lv_style_set_line_rounded(&style_line, LV_STATE_DEFAULT, true);

  lv_style_init(&style_border);
  lv_style_set_line_width(&style_border, LV_STATE_DEFAULT, 6);
  lv_style_set_line_color(&style_border, LV_STATE_DEFAULT, ColorLines);
  lv_style_set_line_rounded(&style_border, LV_STATE_DEFAULT, true);

  line_icons = lv_line_create(lv_scr_act(), nullptr);
  lv_line_set_points(line_icons, line_icons_points, 3);
  lv_obj_add_style(line_icons, LV_LINE_PART_MAIN, &style_line);
  lv_obj_align(line_icons, nullptr, LV_ALIGN_IN_TOP_RIGHT, -10, 18);

  line_day_of_week_number = lv_line_create(lv_scr_act(), nullptr);
  lv_line_set_points(line_day_of_week_number, line_day_of_week_number_points, 4);
  lv_obj_add_style(line_day_of_week_number, LV_LINE_PART_MAIN, &style_border);
  lv_obj_align(line_day_of_week_number, nullptr, LV_ALIGN_IN_TOP_LEFT, 0, 8);

  line_temperature = lv_line_create(lv_scr_act(), nullptr);
  lv_line_set_points(line_temperature, line_day_of_year_points, 3);
  lv_obj_add_style(line_temperature, LV_LINE_PART_MAIN, &style_line);
  lv_obj_align(line_temperature, nullptr, LV_ALIGN_IN_TOP_RIGHT, 0, 60);

  line_date = lv_line_create(lv_scr_act(), nullptr);
  lv_line_set_points(line_date, line_date_points, 3);
  lv_obj_add_style(line_date, LV_LINE_PART_MAIN, &style_line);
  lv_obj_align(line_date, nullptr, LV_ALIGN_IN_TOP_RIGHT, 0, 100);

  label_time = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_font(label_time, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, font_segment115);
  lv_obj_align(label_time, lv_scr_act(), LV_ALIGN_CENTER, 0, 40);

  line_time = lv_line_create(lv_scr_act(), nullptr);
  lv_line_set_points(line_time, line_time_points, 3);
  lv_obj_add_style(line_time, LV_LINE_PART_MAIN, &style_line);
  lv_obj_align(line_time, nullptr, LV_ALIGN_IN_BOTTOM_RIGHT, 0, -25);

  backgroundLabel = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_click(backgroundLabel, true);
  lv_label_set_long_mode(backgroundLabel, LV_LABEL_LONG_CROP);
  lv_obj_set_size(backgroundLabel, 240, 240);
  lv_obj_set_pos(backgroundLabel, 0, 0);
  lv_label_set_text_static(backgroundLabel, "");

  heartbeatIcon = lv_label_create(lv_scr_act(), nullptr);
  lv_label_set_text_static(heartbeatIcon, Symbols::heartBeat);
  lv_obj_align(heartbeatIcon, lv_scr_act(), LV_ALIGN_IN_BOTTOM_LEFT, 5, -2);

  heartbeatValue = lv_label_create(lv_scr_act(), nullptr);
  lv_label_set_text_static(heartbeatValue, "");
  lv_obj_align(heartbeatValue, heartbeatIcon, LV_ALIGN_OUT_RIGHT_MID, 5, 0);

  stepValue = lv_label_create(lv_scr_act(), nullptr);
  lv_label_set_text_static(stepValue, "0");
  lv_obj_align(stepValue, lv_scr_act(), LV_ALIGN_IN_BOTTOM_RIGHT, -5, -2);

  stepIcon = lv_label_create(lv_scr_act(), nullptr);
  lv_label_set_text_static(stepIcon, Symbols::shoe);
  lv_obj_align(stepIcon, stepValue, LV_ALIGN_OUT_LEFT_MID, -5, 0);

  btnBrightness = lv_btn_create(lv_scr_act(), nullptr);
  btnBrightness->user_data = this;
  lv_obj_set_size(btnBrightness, 150, 60);
  lv_obj_align(btnBrightness, lv_scr_act(), LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_local_radius(btnBrightness, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, 20);
  lv_obj_set_style_local_bg_opa(btnBrightness, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_50);
  lv_obj_set_event_cb(btnBrightness, event_handler);
  lblBrightness = lv_label_create(btnBrightness, nullptr);
  lv_obj_set_style_local_text_font(lblBrightness, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &lv_font_sys_48);
  lv_label_set_text_static(lblBrightness, brightnessController.GetIcon());
  lv_obj_set_hidden(btnBrightness, true);

  ApplyTheme();
  taskRefresh = lv_task_create(RefreshTaskCallback, LV_DISP_DEF_REFR_PERIOD, LV_TASK_PRIO_MID, this);
  Refresh();
}

WatchFaceCasioStyleG7710::~WatchFaceCasioStyleG7710() {
  lv_task_del(taskRefresh);
  settingsController.SaveSettings();

  lv_style_reset(&style_line);
  lv_style_reset(&style_border);

  if (font_dot40_loaded) {
    lv_font_free(font_dot40);
  }

  if (font_segment115_loaded) {
    lv_font_free(font_segment115);
  }

  lv_obj_clean(lv_scr_act());
}

void WatchFaceCasioStyleG7710::ApplyTheme() {
  // Battery % / icon stay on the charge-level HSV curve (not theme chrome).
  const auto batteryPercent = batteryController.PercentRemaining();
  batteryIcon.SetBatteryPercentage(batteryPercent);
  lv_obj_set_style_local_text_color(label_battery_value,
                                    LV_LABEL_PART_MAIN,
                                    LV_STATE_DEFAULT,
                                    BatteryIcon::ColorFromPercentage(batteryPercent));

  lv_obj_set_style_local_text_color(batteryPlug, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, ColorChrome);
  lv_obj_set_style_local_text_color(bleIcon, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, ColorChrome);
  lv_obj_set_style_local_text_color(alarmIcon, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, ColorChrome);
  lv_obj_set_style_local_text_color(notificationIcon, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, ColorChrome);

  lv_obj_set_style_local_text_color(label_date, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, ColorDate);
  lv_obj_set_style_local_text_color(label_day_of_week, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, ColorDate);

  lv_obj_set_style_local_text_color(label_temperature_unit, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, ColorWeather);
  lv_obj_set_style_local_text_color(label_temperature, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, ColorWeather);
  lv_obj_set_style_local_text_color(label_weather_icon, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, ColorWeather);
  lv_obj_set_style_local_text_color(label_temperature_low, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, ColorWeather);
  lv_obj_set_style_local_text_color(label_temperature_high, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, ColorWeather);

  lv_obj_set_style_local_text_color(label_time, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, ColorTime);

  lv_obj_set_style_local_text_color(stepValue, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, ColorSteps);
  lv_obj_set_style_local_text_color(stepIcon, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, ColorSteps);

  lv_obj_set_style_local_text_color(heartbeatValue, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, ColorHeart);
  if (heartbeatRunning.Get()) {
    lv_obj_set_style_local_text_color(heartbeatIcon, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, ColorHeart);
  } else {
    lv_obj_set_style_local_text_color(heartbeatIcon, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, ColorHeartIdle);
  }

  lv_style_set_line_color(&style_line, LV_STATE_DEFAULT, ColorLines);
  lv_style_set_line_color(&style_border, LV_STATE_DEFAULT, ColorLines);
  lv_obj_add_style(line_icons, LV_LINE_PART_MAIN, &style_line);
  lv_obj_add_style(line_day_of_week_number, LV_LINE_PART_MAIN, &style_border);
  lv_obj_add_style(line_temperature, LV_LINE_PART_MAIN, &style_line);
  lv_obj_add_style(line_date, LV_LINE_PART_MAIN, &style_line);
  lv_obj_add_style(line_time, LV_LINE_PART_MAIN, &style_line);
}

bool WatchFaceCasioStyleG7710::IsOverlayVisible() const {
  return !lv_obj_get_hidden(btnBrightness);
}

void WatchFaceCasioStyleG7710::CloseOverlay() {
  settingsController.SaveSettings();
  lv_obj_set_hidden(btnBrightness, true);
  colorMenuTick = 0;
}

bool WatchFaceCasioStyleG7710::OnTouchEvent(Pinetime::Applications::TouchEvents event) {
  if (event == Pinetime::Applications::TouchEvents::LongTap && !IsOverlayVisible()) {
    lv_label_set_text_static(lblBrightness, brightnessController.GetIcon());
    lv_obj_set_hidden(btnBrightness, false);
    colorMenuTick = xTaskGetTickCount();
    return true;
  }
  if (event == Pinetime::Applications::TouchEvents::DoubleTap && IsOverlayVisible()) {
    return true;
  }
  return false;
}

bool WatchFaceCasioStyleG7710::OnButtonPushed() {
  if (IsOverlayVisible()) {
    CloseOverlay();
    return true;
  }
  return false;
}

void WatchFaceCasioStyleG7710::OnOverlayButtonEvent(lv_obj_t* object, lv_event_t event) {
  if (event != LV_EVENT_CLICKED) {
    return;
  }
  if (object == btnBrightness) {
    brightnessController.Step();
    lv_label_set_text_static(lblBrightness, brightnessController.GetIcon());
    settingsController.SetBrightness(brightnessController.Level());
    colorMenuTick = xTaskGetTickCount();
  }
}

void WatchFaceCasioStyleG7710::Refresh() {
  if (IsOverlayVisible() && colorMenuTick > 0 &&
      (xTaskGetTickCount() - colorMenuTick > pdMS_TO_TICKS(3000))) {
    CloseOverlay();
  }

  bool statusBarDirty = false;

  powerPresent = batteryController.IsPowerPresent();
  if (powerPresent.IsUpdated()) {
    lv_label_set_text_static(batteryPlug, BatteryIcon::GetPlugIcon(powerPresent.Get()));
    statusBarDirty = true;
  }

  batteryPercentRemaining = batteryController.PercentRemaining();
  if (batteryPercentRemaining.IsUpdated()) {
    auto batteryPercent = batteryPercentRemaining.Get();
    batteryIcon.SetBatteryPercentage(batteryPercent);
    lv_obj_set_style_local_text_color(label_battery_value,
                                      LV_LABEL_PART_MAIN,
                                      LV_STATE_DEFAULT,
                                      BatteryIcon::ColorFromPercentage(batteryPercent));
    lv_label_set_text_fmt(label_battery_value, "%d%%", batteryPercent);
    statusBarDirty = true;
  }

  bleState = bleController.IsConnected();
  bleRadioEnabled = bleController.IsRadioEnabled();
  if (bleState.IsUpdated() || bleRadioEnabled.IsUpdated()) {
    lv_label_set_text_static(bleIcon, BleIcon::GetIcon(bleState.Get()));
    statusBarDirty = true;
  }

  alarmEnabled = alarmController.IsEnabled();
  if (alarmEnabled.IsUpdated()) {
    lv_obj_set_hidden(alarmIcon, !alarmEnabled.Get());
    statusBarDirty = true;
  }

  notificationState = notificatioManager.AreNewNotificationsAvailable();
  if (notificationState.IsUpdated()) {
    lv_label_set_text_static(notificationIcon, NotificationIcon::GetIcon(notificationState.Get()));
    statusBarDirty = true;
  }

  if (statusBarDirty) {
    lv_obj_realign(label_battery_value);
    lv_obj_realign(batteryIcon.GetObject());
    lv_obj_realign(batteryPlug);
    lv_obj_realign(bleIcon);
    lv_obj_realign(alarmIcon);
    lv_obj_realign(notificationIcon);
  }

  currentDateTime = std::chrono::time_point_cast<std::chrono::minutes>(dateTimeController.CurrentDateTime());
  if (currentDateTime.IsUpdated()) {
    uint8_t hour = dateTimeController.Hours();
    uint8_t minute = dateTimeController.Minutes();

    if (settingsController.GetClockType() == Controllers::Settings::ClockType::H12) {
      // No A/P marker — 12h digits only.
      if (hour == 0) {
        hour = 12;
      } else if (hour > 12) {
        hour = hour - 12;
      }
      lv_label_set_text_fmt(label_time, "%d:%02d", hour, minute);
    } else {
      lv_label_set_text_fmt(label_time, "%02d:%02d", hour, minute);
    }
    lv_obj_realign(label_time);

    currentDate = std::chrono::time_point_cast<std::chrono::days>(currentDateTime.Get());
    if (currentDate.IsUpdated()) {
      Controllers::DateTime::Months month = dateTimeController.Month();
      uint8_t day = dateTimeController.Day();

      lv_label_set_text_fmt(label_date, "%d-%d", static_cast<uint8_t>(month), day);

      lv_label_set_text_fmt(label_day_of_week, "%s", dateTimeController.DayOfWeekShortToString());

      lv_obj_realign(label_day_of_week);
      lv_obj_realign(label_date);
    }
  }

  currentWeather = weatherService.Current();
  if (currentWeather.IsUpdated()) {
    auto optCurrentWeather = currentWeather.Get();
    if (optCurrentWeather) {
      const bool imperial = settingsController.GetWeatherFormat() == Controllers::Settings::WeatherFormat::Imperial;
      int16_t temp = imperial ? optCurrentWeather->temperature.Fahrenheit() : optCurrentWeather->temperature.Celsius();
      int16_t tempMin =
        imperial ? optCurrentWeather->minTemperature.Fahrenheit() : optCurrentWeather->minTemperature.Celsius();
      int16_t tempMax =
        imperial ? optCurrentWeather->maxTemperature.Fahrenheit() : optCurrentWeather->maxTemperature.Celsius();

      lv_label_set_text_fmt(label_temperature, "%d", temp);
      lv_label_set_text_fmt(label_temperature_unit, "%c", imperial ? 'F' : 'C');
      lv_label_set_text(label_weather_icon, Symbols::GetSymbol(optCurrentWeather->iconId, weatherService.IsNight()));
      lv_label_set_text_fmt(label_temperature_low, "L%d", tempMin);
      lv_label_set_text_fmt(label_temperature_high, "H%d", tempMax);
    } else {
      lv_label_set_text_static(label_temperature, "--");
      lv_label_set_text_static(label_temperature_unit, "");
      lv_label_set_text_static(label_weather_icon, "");
      lv_label_set_text_static(label_temperature_low, "L--");
      lv_label_set_text_static(label_temperature_high, "H--");
    }

    lv_obj_align(label_temperature_unit, lv_scr_act(), LV_ALIGN_IN_TOP_RIGHT, -8, 28);
    lv_obj_align(label_temperature, label_temperature_unit, LV_ALIGN_OUT_LEFT_MID, -4, 2);
    lv_obj_align(label_weather_icon, label_temperature, LV_ALIGN_OUT_LEFT_MID, -10, 0);

    lv_obj_align(label_temperature_low, lv_scr_act(), LV_ALIGN_IN_TOP_LEFT, 105, 78);
    lv_obj_align(label_temperature_high, lv_scr_act(), LV_ALIGN_IN_TOP_RIGHT, -8, 78);
  }

  heartbeat = heartRateController.HeartRate();
  heartbeatRunning = heartRateController.State() != Controllers::HeartRateController::States::Disabled;
  if (heartbeat.IsUpdated() || heartbeatRunning.IsUpdated()) {
    if (heartbeatRunning.Get()) {
      lv_obj_set_style_local_text_color(heartbeatIcon, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, ColorHeart);
      // Hold last known BPM between samples; stay blank until the first trusted reading.
      if (heartbeat.Get() > 0) {
        lv_label_set_text_fmt(heartbeatValue, "%d", heartbeat.Get());
      } else {
        lv_label_set_text_static(heartbeatValue, "");
      }
    } else {
      lv_obj_set_style_local_text_color(heartbeatIcon, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, ColorHeartIdle);
      lv_label_set_text_static(heartbeatValue, "");
    }

    lv_obj_realign(heartbeatIcon);
    lv_obj_realign(heartbeatValue);
  }

  stepCount = motionController.NbSteps();
  if (stepCount.IsUpdated()) {
    lv_label_set_text_fmt(stepValue, "%lu", stepCount.Get());
    lv_obj_realign(stepValue);
    lv_obj_realign(stepIcon);
  }
}

bool WatchFaceCasioStyleG7710::IsAvailable(Pinetime::Controllers::FS& filesystem) {
  lfs_file file = {};

  if (filesystem.FileOpen(&file, "/fonts/lv_font_dots_40.bin", LFS_O_RDONLY) < 0) {
    return false;
  }

  filesystem.FileClose(&file);
  if (filesystem.FileOpen(&file, "/fonts/7segments_115.bin", LFS_O_RDONLY) < 0) {
    return false;
  }

  filesystem.FileClose(&file);
  return true;
}
