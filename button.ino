#ifdef ESP_USE_BUTTON

// Кнопка лампы: жест (серия кликов или удержание после неё) выбирает действие из таблицы
// на странице настроек (Настройки > Кнопка), а выполняет его LampControl.

static bool brightDirection;
static uint8_t holdAction = BTN_NONE;                       // регулировка, которая идёт, пока кнопку держат
static uint8_t buttonType = 1U;                             // 0 - нет кнопки, 1 - сенсорная, 2 - механическая

// тип кнопки со страницы настроек: подтяжка пина и антидребезг
void buttonApply()
{
  buttonType = (uint8_t)db[kk::hw_button];
  touch.setType(buttonType == 2U ? HIGH_PULL : LOW_PULL);   // механическая замыкает пин на GND и подтянута к питанию, сенсорный модуль сам выдаёт уровень
  touch.setDebounce(buttonType == 2U ? 55U : 20U);          // мс; контакты механической кнопки дребезжат дольше
}

void buttonTick()
{
  if (!buttonEnabled || buttonType == 0U)                   // кнопка заблокирована или не подключена: без неё пин ловит наводки
  {
    return;
  }

  touch.tick();

  #ifdef BUTTON_PRESS_FEEDBACK
  if (touch.isPress())                                      // отклик на само касание - мгновенно, ещё до распознавания серии
  {
    buttonFeedbackAt = millis();
  }
  #endif //BUTTON_PRESS_FEEDBACK

  uint8_t clicks = touch.hasClicks() ? touch.getClicks() : 0U;
  if (clicks)
  {
    if (serviceTextActive())                                // клик обрывает служебную строку и дальше срабатывает как обычно
    {
      serviceTextStop();
    }

    // пока на лампе кубик, один клик бросает его ещё раз (если включено на странице), два возвращают к эффекту
    if (diceActive() && !dawnFlag && (clicks == 2U || (clicks == 1U && (bool)db[kk::dice_click])))
    {
      if (clicks == 2U)
      {
        diceExit();
      }
      else
      {
        diceRoll((uint8_t)db[kk::dice_last]);
      }
    }
    else if (clicks <= 7U)
    {
      uint8_t action = (uint8_t)db[buttonClickKeys[ONflag ? 0U : 1U][clicks - 1U]];
      lampDoAction(action < BTN_HOLD_ONLY ? action : BTN_NONE);
    }
  }

  if (touch.isHolded())                                     // кнопку только начали удерживать
  {
    brightDirection = !brightDirection;
    uint8_t holdClicks = touch.getHoldClicks();
    uint8_t action = holdClicks < 8U ? (uint8_t)db[buttonHoldKeys[holdClicks]] : BTN_NONE;

    if (action == BTN_BRIGHTNESS || action == BTN_SPEED || action == BTN_SCALE)
    {
      holdAction = action;
      if (!ONflag)                                          // на выключенной лампе регулировка включает белый свет
      {
        lampDoAction(BTN_WHITE);
        #ifdef BUTTON_PAUSE_AFTER_TURN_ON
        holdAction = BTN_NONE;                              // регулировать - следующим удержанием
        #endif
      }
    }
    else
    {
      lampDoAction(action);
    }
  }

  if (holdAction != BTN_NONE)
  {
    if (touch.isStep())
    {
      lampNudge(holdAction, brightDirection);
    }
    if (!touch.isHold())                                    // кнопку отпустили - новое значение уходит в MQTT
    {
      holdAction = BTN_NONE;
      mqttRequestPublish();
    }
  }
}
#endif
