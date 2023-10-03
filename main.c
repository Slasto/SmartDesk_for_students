// halconf.h => metti a true (HAL_USE_I2C)
// mcuconf.h => metti a true STM32_I2C_USE_I2C1
// importa la cartella ssd1306

#include "ch.h"
#include "hal.h"

#include "chprintf.h"
#include "ssd1306.h"

#include "Light.h"
#include "TempUmid.h"
#include "pomodoro.h"



/*          CONFIG DISPLAY         */

#define BUFF_SIZE1   20
char buff[BUFF_SIZE1];

static const I2CConfig i2ccfg2 = {
// I2C_TIMINGR address offset
    .timingr = 0x10, .cr1 = 0, .cr2 = 1, };

static const SSD1306Config ssd1306cfg = {&I2CD1, &i2ccfg2, SSD1306_SAD_0X78, };

static SSD1306Driver SSD1306D1;
/*    ---         */

const char *messages[] = {"DashBoard", "Temperatura %u C ",
                          "Lvl umidita' %u %%",
                          //"Lvl rumore Ottimale|Esagerato"
                          "Stato illuminaz %u"};
int messagesValue[] = {0, 0, 0, 0};
uint8_t yPos[] = {1, 20, 32, 44};

static THD_WORKING_AREA(waDisplayDashboard, 1024);
static THD_FUNCTION(ThdDisplayDashboard, arg) {
  (void)arg;

  chRegSetThreadName("OledDisplayDS");

  // Initialize SSD1306 Display Driver Object.

  ssd1306ObjectInit(&SSD1306D1);

  // Start the SSD1306 Display Driver Object with configuration.
  ssd1306Start(&SSD1306D1, &ssd1306cfg);
  ssd1306FillScreen(&SSD1306D1, 0x00);

  while (true) {
    messagesValue[1] = TEMP;
    messagesValue[2] = HR;
    messagesValue[3] = Light;
    for (uint8_t i = 0; i < 4; i++) {
      ssd1306GotoXy(&SSD1306D1, 0, yPos[i]);
      chsnprintf(buff, BUFF_SIZE1, messages[i], messagesValue[i]);
      if (i != 0) {
        ssd1306Puts(&SSD1306D1, buff, &ssd1306_font_7x10, SSD1306_COLOR_WHITE);
        //ontinue;
      }
      else
        ssd1306Puts(&SSD1306D1, buff, &ssd1306_font_11x18, SSD1306_COLOR_WHITE);
    }
    ssd1306UpdateScreen(&SSD1306D1);
    chThdSleepMilliseconds(1500);
  }

}

int main(void) {

  halInit();
  chSysInit();

  palSetPadMode(GPIOA, 2, PAL_MODE_ALTERNATE(7));
  palSetPadMode(GPIOA, 3, PAL_MODE_ALTERNATE(7));
  sdStart(&SD2, NULL);

  StartLight((BaseSequentialStream*)&SD2);
  StartTempUmid((BaseSequentialStream*)&SD2);
  StartPomodoro();

  // Configuring I2C related PINs
  palSetLineMode(
      PAL_LINE(GPIOB, 8U),
      PAL_MODE_ALTERNATE(4) | PAL_STM32_OTYPE_OPENDRAIN | PAL_STM32_OSPEED_HIGHEST | PAL_STM32_PUPDR_PULLUP); //D15
  palSetLineMode(
      PAL_LINE(GPIOB, 9U),
      PAL_MODE_ALTERNATE(4) | PAL_STM32_OTYPE_OPENDRAIN | PAL_STM32_OSPEED_HIGHEST | PAL_STM32_PUPDR_PULLUP); //D14

  chThdCreateStatic(waDisplayDashboard, sizeof(waDisplayDashboard),
                    NORMALPRIO + 1, ThdDisplayDashboard, NULL);
  while (1)
    chThdSleepSeconds(1000);
}
