#include "ch.h"
#include "hal.h"
#include "chprintf.h"
#include "ssd1306.h"
#include "stdio.h"

#define LINE_EXT_BUTTON        PAL_LINE(GPIOA,10) // EXTERNAL BUTTON D2
#define LINE_EXT_BUTTON_PAUSE       PAL_LINE(GPIOA,5) // EXTERNAL BUTTON D13
#define BUZZER PAL_LINE(GPIOB,5) //D4
//D7=>SDA
//D8=>SCL
//D5 => servo motore
#define LINE_TRIGGER                PAL_LINE(GPIOB, 10U) //D6
#define LINE_ECHO                   PAL_LINE(GPIOB, 14U) //6 M a dx dal basso

/* ICU */

#define ANSI_ESCAPE_CODE_ALLOWED    TRUE
//BaseSequentialStream * chp = (BaseSequentialStream*) &SD2;
uint8_t measure_done = 0;

#define ICU_TIM_FREQ                1000000
#define M_TO_CM                     100.0f
#define SPEED_OF_SOUND              343.2f

static float lastdistance = 0.0;

static void icuwidthcb2(ICUDriver *icup) {

  icucnt_t width = icuGetWidthX(icup);
  lastdistance = (SPEED_OF_SOUND * width * M_TO_CM) / (ICU_TIM_FREQ * 2);
  icuStopCapture(&ICUD15);
  icuStop(&ICUD15);
}

static ICUConfig icucfg3 = {ICU_INPUT_ACTIVE_HIGH,
ICU_TIM_FREQ, /* 1MHz ICU clock frequency.   */
                            icuwidthcb2, NULL, NULL, ICU_CHANNEL_1, 0,
                            0xFFFFFFFFU};

static THD_WORKING_AREA( waProx, 128 );
static THD_FUNCTION( thdProx, arg ) {
  (void)arg;
  while (true) {
    /* Triggering */
    icuStart(&ICUD15, &icucfg3);
    icuStartCapture(&ICUD15);
    icuEnableNotifications(&ICUD15);
    palWriteLine(LINE_TRIGGER, PAL_HIGH);
    chThdSleepMicroseconds(10);
    palWriteLine(LINE_TRIGGER, PAL_LOW);
    chprintf(chp, "%.2f\n\r", lastdistance);

    chThdSleepMilliseconds(500);
  }
}

/* PWM */

#define PWM_TIMER_FREQUENCY     1000000                               /*  Ticks per second  */
#define PWM_PERIOD              (PWM_TIMER_FREQUENCY * 20 / 1000)     /*  The SERVO SG90 runs on a 20ms period  */

static PWMConfig pwmcfg = {.frequency = PWM_TIMER_FREQUENCY, /*  Timer clock in Hz   */
                           .period = PWM_PERIOD, /*  PWM Period in ticks */
                           .callback = NULL, /*  Callback            */
                           .channels = { /*  PWM3 has 4 channels */
                           {PWM_OUTPUT_ACTIVE_HIGH, NULL}, {PWM_OUTPUT_DISABLED,
                                                            NULL},
                           {PWM_OUTPUT_DISABLED, NULL}, {PWM_OUTPUT_DISABLED,
                                                         NULL}}};

#define BUFF_SIZE   50
char buff2[BUFF_SIZE];
static const I2CConfig i2ccfg = {.timingr = 0x10, .cr1 = 0, .cr2 = 1, };
static const SSD1306Config ssd1306cfg2 = {&I2CD3, &i2ccfg, SSD1306_SAD_0X78, };
static SSD1306Driver SSD1306D2;

static THD_WORKING_AREA(waOledDisplay, 1024);
static THD_FUNCTION(OledDisplay, arg) {
  (void)arg;

  chRegSetThreadName("OledDisplayTIMER");
  ssd1306ObjectInit(&SSD1306D2);
  ssd1306Start(&SSD1306D2, &ssd1306cfg2);
  ssd1306FillScreen(&SSD1306D2, 0x00);
  ssd1306UpdateScreen(&SSD1306D2);

  uint8_t flag_while = 0;
  uint8_t conteggio_break = 1;
  int pausa_work = 1;
  int pausa_break = 1;

  while (palReadLine(LINE_EXT_BUTTON) == PAL_HIGH) {
    chThdSleepMilliseconds(20);
  }
  int cont_timer_work = 25;  // work for 25 minutes
  int cont_timer_pause = 5;   //   for 5 minutes

  while (true) {
    ssd1306FillScreen(&SSD1306D2, 0x00);
    flag_while = 0;
    cont_timer_work = 25;  // work for 25 minutes
    cont_timer_pause = 5;   //   for 5 minutes
    ssd1306GotoXy(&SSD1306D2, 40, 1);
    chsnprintf(buff2, BUFF_SIZE, "TIMER");
    ssd1306Puts(&SSD1306D2, buff2, &ssd1306_font_11x18, SSD1306_COLOR_WHITE);

    // WORK TIMER
    while (cont_timer_work >= 0) {
      ssd1306GotoXy(&SSD1306D2, 40, 20);
      chsnprintf(buff2, BUFF_SIZE, "%02d:00", cont_timer_work, 0);
      ssd1306Puts(&SSD1306D2, buff2, &ssd1306_font_11x18, SSD1306_COLOR_BLACK);
      ssd1306GotoXy(&SSD1306D2, 15, 40);
      chsnprintf(buff2, BUFF_SIZE, "WORK  MODE");
      ssd1306Puts(&SSD1306D2, buff2, &ssd1306_font_11x18, SSD1306_COLOR_BLACK);
      ssd1306UpdateScreen(&SSD1306D2);

      //BUTTON RESTART
      if (palReadLine(LINE_EXT_BUTTON) == PAL_LOW) {
        while (palReadLine(LINE_EXT_BUTTON) == PAL_LOW) {
          chThdSleepMilliseconds(20);
        }
        flag_while = 1;
        break;
      }

      //BUTTON PAUSE
      if (lastdistance > 50) {
        for (int x = 0; x < 2; x++) {
          palSetLine(BUZZER);
          chThdSleepMilliseconds(500);
          palClearLine(BUZZER);
          chThdSleepMilliseconds(500);

        }
        while (lastdistance > 50) {

          chThdSleepMilliseconds(20);
        }
      }

      if (palReadLine(LINE_EXT_BUTTON_PAUSE) == PAL_LOW) {
        while (palReadLine(LINE_EXT_BUTTON_PAUSE) == PAL_LOW) {
          chThdSleepMilliseconds(20);
        }
        pausa_work = pausa_work * (-1);
      }

      if (pausa_work > 0) {
        pwmEnableChannel(&PWMD3, 0, PWM_PERCENTAGE_TO_WIDTH(&PWMD3, 750));
        cont_timer_work--;
      }
      else
        pwmEnableChannel(&PWMD3, 0, PWM_PERCENTAGE_TO_WIDTH(&PWMD3, 250));

      chThdSleepMilliseconds(1000);

    }
    if (flag_while == 1)
      continue;

    pwmEnableChannel(&PWMD3, 0, PWM_PERCENTAGE_TO_WIDTH(&PWMD3, 250));
    for (int x = 0; x < 4; x++) {
      palSetLine(BUZZER);
      chThdSleepMilliseconds(500);
      palClearLine(BUZZER);
      chThdSleepMilliseconds(500);
    }

    // PAUSE TIMER
    conteggio_break++;
    if (conteggio_break == 4)
      cont_timer_pause = 20;
    while (cont_timer_pause >= 0) {
      ssd1306GotoXy(&SSD1306D2, 40, 20);
      chsnprintf(buff2, BUFF_SIZE, "%02d:00", cont_timer_pause, 0);
      ssd1306Puts(&SSD1306D2, buff2, &ssd1306_font_11x18, SSD1306_COLOR_BLACK);

      ssd1306GotoXy(&SSD1306D2, 15, 40);
      if (conteggio_break != 4)
        chsnprintf(buff2, BUFF_SIZE, "BREAK MODE");
      else
        chsnprintf(buff2, BUFF_SIZE, "LONG BREAK");
      ssd1306Puts(&SSD1306D2, buff2, &ssd1306_font_11x18, SSD1306_COLOR_BLACK);
      ssd1306UpdateScreen(&SSD1306D2);

      //BUTTON RESTART
      if (palReadLine(LINE_EXT_BUTTON) == PAL_LOW) {
        while (palReadLine(LINE_EXT_BUTTON) == PAL_LOW) {
          chThdSleepMilliseconds(20);
        }
        flag_while = 1;
        break;
      }

      //BUTTON PAUSE
      if (palReadLine(LINE_EXT_BUTTON_PAUSE) == PAL_LOW) {
        while (palReadLine(LINE_EXT_BUTTON_PAUSE) == PAL_LOW) {
          chThdSleepMilliseconds(20);
        }
        pausa_break = pausa_break * (-1);
      }

      if (pausa_break > 0)
        cont_timer_pause--;

      chThdSleepMilliseconds(1000);
    }

    if (conteggio_break == 4)
      conteggio_break = 1;
    if (flag_while == 1)
      continue;

    for (uint8_t x = 0; x < 8; x++) {
      palSetLine(BUZZER);
      chThdSleepMilliseconds(250);
      palClearLine(BUZZER);
      chThdSleepMilliseconds(250);
    }
  }
}

void StartPomodoro(void) {
  palSetLineMode(
      PAL_LINE(GPIOC, 8U),
      PAL_MODE_ALTERNATE(8) | PAL_STM32_OTYPE_OPENDRAIN | PAL_STM32_OSPEED_HIGHEST | PAL_STM32_PUPDR_PULLUP); //PC8
  palSetLineMode(
      PAL_LINE(GPIOC, 9U),
      PAL_MODE_ALTERNATE(8) | PAL_STM32_OTYPE_OPENDRAIN | PAL_STM32_OSPEED_HIGHEST | PAL_STM32_PUPDR_PULLUP); //PC9

  palSetLineMode(LINE_EXT_BUTTON, PAL_MODE_INPUT_PULLUP); // EXTERNAL BUTTON
  palSetLineMode(LINE_EXT_BUTTON_PAUSE, PAL_MODE_INPUT_PULLUP); // EXTERNAL BUTTON 2
  palSetPadMode(GPIOB, 4, PAL_MODE_ALTERNATE(2)); //PWM
  pwmStart(&PWMD3, &pwmcfg);
  palSetLineMode(BUZZER, PAL_MODE_OUTPUT_PUSHPULL); //BUZZER

  /*    ICU     */
  palSetLineMode(LINE_ECHO, PAL_MODE_ALTERNATE(1));
  palSetLineMode(LINE_TRIGGER, PAL_MODE_OUTPUT_PUSHPULL);

  chThdCreateStatic(waOledDisplay, sizeof(waOledDisplay), NORMALPRIO,
                    OledDisplay, NULL);
  chThdCreateStatic(waProx, sizeof(waProx), NORMALPRIO, thdProx, NULL);
}
