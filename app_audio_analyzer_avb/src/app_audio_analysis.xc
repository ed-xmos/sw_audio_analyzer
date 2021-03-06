#include <platform.h>
#include <xs1.h>
#include <xscope.h>
#include <timer.h>

#include "i2s_master.h"
#include "audio_analyzer.h"
#include "app_global.h"
#include "audiohw.h"
#include "timer.h"
#include "debug_print.h"
#include "xassert.h"
#include "signal_gen.h"
#include "xscope_handler.h"
#include "ethernet_tap.h"

#ifndef SIMULATOR_LOOPBACK
#define SIMULATOR_LOOPBACK 0
#endif

#define PORT_CLK_MAS            XS1_PORT_1E         /* Master clock */
#define PORT_CLK_BIT            XS1_PORT_1K         /* Bit clock */
#define PORT_CLK_LR             XS1_PORT_1I         /* LR clock */

#define PORT_DAC_0              XS1_PORT_1O
#define PORT_DAC_1              XS1_PORT_1H

#define PORT_ADC_0              XS1_PORT_1J
#define PORT_ADC_1              XS1_PORT_1L


on tile[0] : r_i2s i2s_resources =
{
  XS1_CLKBLK_3,
  XS1_CLKBLK_4,
  PORT_CLK_MAS,
  PORT_CLK_BIT,
  PORT_CLK_LR,
  { PORT_ADC_0, PORT_ADC_1 },
  { PORT_DAC_0, PORT_DAC_1 }
};

clock dummy_clk = on tile[0]: XS1_CLKBLK_2;
out port p_dummy_clk = on tile[0]: XS1_PORT_1A;


static void audio(streaming chanend c_i2s_data) {
  // First make sure the i2s client is ready
  for (int i = 0; i < I2S_MASTER_NUM_CHANS_ADC; i++)
    c_i2s_data <: 0;
  for (int i = 0; i < I2S_MASTER_NUM_CHANS_DAC; i++)
    c_i2s_data :> int _;

  // Initialize hardware
  if (SIMULATOR_LOOPBACK) {
    // approximate 24.576 with 25Mhz output (this will be loopbacked
    // by simulator into the MCLK input)
    configure_clock_rate(dummy_clk, 100, 4);
    configure_port_clock_output(p_dummy_clk, dummy_clk);
    start_clock(dummy_clk);
  }
  else {
    debug_printf("Initializing Hardware\n");
    AudioHwInit();
  }

  // Go into the I2S loop
  debug_printf("Starting I2S\n");
  i2s_master(i2s_resources, c_i2s_data, MCLK_FREQ / (SAMP_FREQ * 64));
}

chan_conf_t chan_conf[I2S_MASTER_NUM_CHANS_DAC] = CHAN_CONFIG;

int main(){
  interface audio_analysis_if i_analysis[4];
  interface audio_analysis_scheduler_if i_sched0[2], i_sched1[2];
  interface channel_config_if i_chan_config;
  interface analysis_control_if i_control[4];

  interface error_flow_control_if i_flow_control;
  interface error_reporting_if i_error_reporting[4];
  streaming chan c_i2s_data, c_dac_samples;
  chan c_host_data;
  par {
    // If the xscope handler is on tile[1] this needs to be commented (tools bug workaround)
//    xscope_host_data(c_host_data);

    on tile[0]: error_reporter(i_flow_control, i_error_reporting, 4);
    on tile[1]: {
      // Ensure the relay starts closed
      ethernet_tap_set_relay_close();
      delay_milliseconds(10);
      ethernet_tap_set_control_idle();
      xscope_handler(c_host_data, i_flow_control, i_chan_config, i_control, 4);
    }

    on tile[1].core[0]: audio_analyzer(i_analysis[0], i_sched0[0], SAMP_FREQ, 0,
        i_error_reporting[0], i_control[0]);
    on tile[1].core[0]: audio_analyzer(i_analysis[1], i_sched0[1], SAMP_FREQ, 1,
        i_error_reporting[1], i_control[1]);
    on tile[1].core[0]: analysis_scheduler(i_sched0, 2);

    on tile[1].core[1]: audio_analyzer(i_analysis[2], i_sched1[0], SAMP_FREQ, 2,
        i_error_reporting[2], i_control[2]);
    on tile[1].core[1]: audio_analyzer(i_analysis[3], i_sched1[1], SAMP_FREQ, 3,
        i_error_reporting[3], i_control[3]);
    on tile[1].core[1]: analysis_scheduler(i_sched1, 2);

    on tile[1]: {
      if (SIMULATOR_LOOPBACK)
        xscope_config_io(XSCOPE_IO_NONE);
      i2s_tap(c_i2s_data, c_dac_samples, i_analysis, I2S_MASTER_NUM_CHANS_DAC);
    }

    on tile[AUDIO_IO_TILE]: audio(c_i2s_data);
    on tile[AUDIO_IO_TILE]: genclock();
    on tile[AUDIO_IO_TILE]: {
      if (SIMULATOR_LOOPBACK)
        xscope_config_io(XSCOPE_IO_NONE);
      signal_gen(c_dac_samples, SAMP_FREQ, chan_conf, i_chan_config);
    }
  }
  return 0;
}

