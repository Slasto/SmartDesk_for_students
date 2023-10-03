#include "ch.h"
#include "hal.h"
#include "chprintf.h"

//ADC1

static BaseSequentialStream *chp = NULL;
#define ADCUsed &ADCD1 //A4
#define LINE_LIGHT PAL_LINE(GPIOB, 3) //D3


/* CONFIGURAZIONE ADC */
#define ADC_GRP_NUM_CHANNELS 1
#define ADC_GRP_BUF_DEPTH 10

static adcsample_t samples[ADC_GRP_NUM_CHANNELS * ADC_GRP_BUF_DEPTH];
static const ADCConversionGroup linearcfg = {
    .circular = false,
    .num_channels = ADC_GRP_NUM_CHANNELS,
    .end_cb = NULL,
    .error_cb = NULL,
    .cfgr = ADC_CFGR_CONT,
    .cfgr2 = 0U,
    .tr1 = ADC_TR_DISABLED,
    .tr2 = ADC_TR_DISABLED,
    .tr3 = ADC_TR_DISABLED,
    .awd2cr = 0U,
    .awd3cr = 0U,
    .smpr = {
        ADC_SMPR1_SMP_AN7(ADC_SMPR_SMP_47P5), //
        0U
    },
    .sqr = {
        ADC_SQR1_SQ1_N(ADC_CHANNEL_IN7),
        0U,
        0U,
        0U
    }
};

static enum LEDinfo{
  OFF=0,
  ON=1
}Light;

// thread
static THD_WORKING_AREA(waADC_Light,512);
static THD_FUNCTION(ADC_Light, arg) {
    (void) arg;
    int intensity = 0;
    while (1) {
        intensity=0;
        adcConvert(ADCUsed, &linearcfg, samples, ADC_GRP_BUF_DEPTH);
        for (int i = 0; i < ADC_GRP_BUF_DEPTH; i++)
          intensity+=samples[i];

        intensity = intensity/ADC_GRP_BUF_DEPTH;

        // Verifica se l'intensità è inferiore alla soglia
        if (intensity < 500) {
          palSetLine(LINE_LIGHT);
          Light=ON;

        } else {
            // Spegni il LED
            palClearLine(LINE_LIGHT);
            Light=OFF;
        }
        //chprintf(chp, "FotoresistorVoltage: %d\t\r", intensity);
        chThdSleepMilliseconds(100);
    }
}

void StartLight(BaseSequentialStream *Tempchp){

  chp=Tempchp;
  /* Initializes a ADC driver. */
  adcStart(ADCUsed, NULL);
  adcSTM32EnableVREF(ADCUsed);


  palSetLineMode(LINE_LIGHT, PAL_MODE_OUTPUT_PUSHPULL);
  chThdCreateStatic(waADC_Light, sizeof(waADC_Light), NORMALPRIO, ADC_Light, NULL);
}
