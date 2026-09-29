#pragma once


struct AlarmType
{
  bool State = false;
  uint16_t Time = 0U;
};

struct ModeType
{
  uint8_t Brightness = 50U;
  uint8_t Speed = 225U;
  uint8_t Scale = 40U;
};

typedef void (*ShowWarningDelegate)(CRGB color, uint32_t duration, uint16_t blinkHalfPeriod);

// картинка поверх эффекта (overlay.ino)
enum OverlayKind : uint8_t
{
  OVERLAY_NONE,
  OVERLAY_COUNTDOWN,
  OVERLAY_DICE
};

// действие жеста кнопки: номер хранится в настройках, поэтому новые действия добавляются только в конец
// своей части. Действия до BTN_HOLD_ONLY назначаются и кликам, и удержанию, остальные - только удержанию:
// регулировки идут, пока кнопка удерживается, а служебные действия удержание подтверждает
enum ButtonAction : uint8_t
{
  BTN_NONE,
  BTN_POWER,
  BTN_NEXT,
  BTN_PREV,
  BTN_WHITE,
  BTN_SLEEP,
  BTN_CYCLE,
  BTN_IP,
  BTN_TIME,
  BTN_DICE,
  BTN_COUNTDOWN,
  BTN_CLICK_END,                                            // конец действий для кликов; до BTN_HOLD_ONLY - запас под новые
  BTN_HOLD_ONLY = 16,
  BTN_BRIGHTNESS = BTN_HOLD_ONLY,
  BTN_SPEED,
  BTN_SCALE,
  BTN_OTA,
  BTN_WIFI
};
