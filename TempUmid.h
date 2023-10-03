#include "ch.h"
#include "hal.h"
#include "chprintf.h"

BaseSequentialStream *chpTU = NULL;

#define ICUUsed PAL_LINE(GPIOA, 0)      //A0
#define LINE_VENTOLA PAL_LINE(GPIOA, 6) //D12

/* CONFIGURAZIONE ICU */

#define    MCU_REQUEST_WIDTH                     18000
#define    DHT_ERROR_WIDTH                         200
#define    DHT_START_BIT_WIDTH                      80
#define    DHT_LOW_BIT_WIDTH                        28
#define    DHT_HIGH_BIT_WIDTH                       70
#define ICU_FREQUENCY           1000000

static uint8_t TEMP, HR, tmp = 0, bit_counter = 0;
static icucnt_t widths[40];

//Callback
static void icuwidthcb(ICUDriver *icup) {
  icucnt_t width = icuGetWidthX(icup);
  if (width >= DHT_START_BIT_WIDTH) {
    /* starting bit resetting the bit counter */
    bit_counter = 0;
  }
  else {
    widths[bit_counter] = width;
    if (width > DHT_LOW_BIT_WIDTH) {
      tmp |= (1 << (7 - (bit_counter % 8)));
    }
    else {
      tmp &= ~(1 << (7 - (bit_counter % 8)));
    }
    if (bit_counter == 7)
      HR = tmp;
    if (bit_counter == 23)
      TEMP = tmp;
    bit_counter++;
  }
  //chSysUnlock();
}

static ICUConfig icucfg = {ICU_INPUT_ACTIVE_HIGH,
ICU_FREQUENCY,
                           NULL, icuwidthcb, NULL, ICU_CHANNEL_1, 0U,
                           0xFFFFFFFFU};

/* CONFIGURAZIONE componente 2 */

/*           THD main          */

//seleziona waADC_Light, poi tasto sx>refactor>Rename ; stessa cosa con ADC_Light per cambiare il nome
static THD_WORKING_AREA(waICU_TempUmid,512);
static THD_FUNCTION(ICU_TempUmid, arg) {
  (void)arg;
  while (1) {
    /*
     * Making a request
     */
    palSetPadMode(GPIOA, GPIOC_PIN0, PAL_MODE_OUTPUT_PUSHPULL);
    palWritePad(GPIOA, GPIOC_PIN0, PAL_LOW);
    chThdSleepMicroseconds(MCU_REQUEST_WIDTH);
    palWritePad(GPIOA, GPIOC_PIN0, PAL_HIGH);
    /*
     * Initializes the ICU driver 1.
     * GPIOA8 is the ICU input.
     */
    palSetPadMode(GPIOA, 0, PAL_MODE_ALTERNATE(2));
    icuStart(&ICUD5, &icucfg);
    icuStartCapture(&ICUD5);
    icuEnableNotifications(&ICUD5);
    chThdSleepMilliseconds(700);
    icuStopCapture(&ICUD5);
    icuStop(&ICUD5);
    //chprintf(chpTU, "Temperature: %d C, Humidity Rate: %d %% \n\r", TEMP, HR);
    if (TEMP >= 29)
      palSetLine(LINE_VENTOLA);
    else
      palClearLine(LINE_VENTOLA);
    chThdSleepMilliseconds(1000);

  }
}

void StartTempUmid(BaseSequentialStream *Tempchp) {
  chpTU = Tempchp;
  palSetLineMode(LINE_VENTOLA, PAL_MODE_OUTPUT_PUSHPULL);
  chThdCreateStatic(waICU_TempUmid, sizeof(waICU_TempUmid), NORMALPRIO + 1,
                    ICU_TempUmid, NULL);
}
