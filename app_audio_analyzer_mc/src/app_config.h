#if STEREO_BOARD_TESTER
#define I2S_MASTER_NUM_CHANS_ADC 2
#define I2S_MASTER_NUM_CHANS_DAC 2
#endif

#ifndef I2S_MASTER_NUM_CHANS_ADC
#define I2S_MASTER_NUM_CHANS_ADC 4
#endif
#ifndef I2S_MASTER_NUM_CHANS_DAC
#define I2S_MASTER_NUM_CHANS_DAC 4
#endif

#if I2S_MASTER_NUM_CHANS_DAC == 4
#define CHAN_CONFIG \
 {{ 1, SINE, 1000, 0, 0, 0 }, \
  { 1, SINE, 2000, 0, 0, 0 }, \
  { 1, SINE, 3000, 0, 0, 0 }, \
  { 1, SINE, 4000, 0, 0, 0 } \
 }
 #elif I2S_MASTER_NUM_CHANS_DAC == 2
 #define CHAN_CONFIG \
 {{ 1, SINE, 1000, 0, 0, 0 }, \
  { 1, SINE, 2000, 0, 0, 0 } \
 }
 #else
 #error Unsupported I2S_MASTER_NUM_CHANS_DAC value
 #endif

#if SPDIF_IN_TESTER == 1
 #ifndef DIGITAL_MASTER_NUM_CHANS
  #define DIGITAL_MASTER_NUM_CHANS 2
 #endif
 #define SPDIF_CONFIG \
 {{ 1, RAMP, 5, 0, 0, 0 }, \
  { 1, RAMP, -7, 0, 0, 0 } \
 }
#else
 #define DIGITAL_MASTER_NUM_CHANS 0
#endif



#define BASE_CHAN_ID       0
#define BASE_DIG_CHAN_ID   8
