#include <stdio.h>

#include "XMC4700.h"
#include "xmc_common.h" // Defines human-readable macros for hexadecimal memory addresses for the XMC4700 memory map.
#include "xmc_gpio.h"   // Essential for pin routing.
#include "xmc_scu.h"    // Includes stuff for PLL (phase locked loop). Also needed for "ungating" (powering on) specific modules. SCU=System Control Unit
#include "xmc_vadc.h"   // This defines the VADC types and functions.

// The board is the BGA-196 package

// Define the PLL (phase-locked loop) sub-structure
const XMC_SCU_CLOCK_SYSPLL_CONFIG_t pulse_pll_config = {
   .p_div = 2,                                // (Pre-divider) Divide 12MHz crystal by 2 = 6MHz (PLL's internal phase detector requires a
                                              // reference frequency between 4 MHz and 16 MHz). The value 2 was chosen because lower
                                              // reference frequencies often allow for more stable "locking" of the high-speed multiplier.
   .n_div = 48,                               // (Feedback multiplier) Multiply 6MHz by 48 = 288MHz reference frequency (VCO)
                                              // note that 288 MHz is the "raw" internal speed. The chip never actually runs
                                              // at 288 MHz -- it would melt. This determines the speed of the VCO (voltage controlled oscillator).
                                              // The VCO is the "raw" high-speed heart of the chip, and it must run between 260 MHz and 520 MHz.
                                              // We take our 6 MHz (reference frequency -- 12 MHz/2) and multiply it by N (48) -- 6*48=288 MHz.
   .k_div = 2,                                //  (Post-divider) Divide 288MHz by 2 = 144MHz
                                              // tames the 288 MHz down to a usable system frequency 144MHz. VCO/K2=(288 MHz)/2 = 144 MHz.
   .mode = XMC_SCU_CLOCK_SYSPLL_MODE_NORMAL,  // There are two main modes: prescaler mode and normal mode. Prescaler mode bypasses the multiplier.
                                              // Normal mode engages the P, N, and K logic to synthesize the high frequency from the crystal.
                                              // Since we want 144 MHz, we must choose normal mode.
   .clksrc = XMC_SCU_CLOCK_SYSPLLCLKSRC_OSCHP // Use the High Precision Oscillator. This opens the electronic "gates" connected to K11 and K12.
                                              // K-11 (XTAL-1)/input from 12 MHz chrystal; K-12 (XTAL-2)/output from 12 MHz crystal
};

// Math is for a 12 MHz crystal
const XMC_SCU_CLOCK_CONFIG_t manual_144_config = {
   .syspll_config = pulse_pll_config,
   .enable_oschp = true,                       // Turn on the 12MHz crystal ("Powers on Y1; without, XMC ignores entirely)
   .enable_osculp = false,                     // We don't need the Ultra Low Power clock
   .calibration_mode = XMC_SCU_CLOCK_FOFI_CALIBRATION_MODE_FACTORY,
   .fstdby_clksrc = XMC_SCU_HIB_STDBYCLKSRC_OSI,
   .fsys_clksrc = XMC_SCU_CLOCK_SYSCLKSRC_PLL, // Tell the system to use the 144 MHz PLL instead of the 24 MHz emergency oscillator
   .fsys_clkdiv = 1,                           // fSYS = fPLL / 1 = 144MHz
   .fcpu_clkdiv = 1,                           // fCPU = fSYS / 1 = 144MHz
   .fccu_clkdiv = 1,                           // fCCU = fSYS / 1 = 144MHz
   .fperipheral_clkdiv = 1                     // fPERIPHERAL = fSYS / 1 = 144MHz This ensures the USIC (UART) and VADC (Analog-to-Digital
                                               // Converter) are running at the full 144 MHz
};


/* This stops the official startup from hanging on the clock setup */
void SystemInit(void)
{
   // Apply the 144MHz configuration
   XMC_SCU_CLOCK_Init(&manual_144_config);
}

int _write(int file, char *ptr, int len)
{
   for (int i = 0; i < len; i++)
   {
      // Wait for the ITM to be ready to accept data
      // 0xE0000F00 is the register for ITM Port 0
      while (ITM->PORT[0].u32 == 0);
      ITM_SendChar((uint32_t)(*ptr++));
   }
   return len;
}

void ADC_Init(void) {
   // Global & Group Start
   XMC_VADC_GLOBAL_Init(VADC, &(XMC_VADC_GLOBAL_CONFIG_t){ .clock_config.analog_clock_divider = 5 });

   //XMC_VADC_GROUP_CONFIG_t group_config = { 0 }; // Initialize to zero first
    
   // Set the arbiter to "Continuous" mode (the equivalent of starting it)
   //group_config.arbiter_mode = XMC_VADC_GROUP_ARBMODE_ALWAYS;
    
   XMC_VADC_GROUP_Init(VADC_G0, &(XMC_VADC_GROUP_CONFIG_t){0});
   XMC_VADC_GROUP_SetPowerMode(VADC_G0, XMC_VADC_GROUP_POWERMODE_NORMAL);
   XMC_VADC_GLOBAL_StartupCalibration(VADC);

   // Initialize the Queue Struct to zeros first
   XMC_VADC_QUEUE_CONFIG_t q_config = { 0 };

   // Set the top-level members
   q_config.conv_start_mode = XMC_VADC_STARTMODE_WFS;
   q_config.req_src_priority = XMC_VADC_GROUP_RS_PRIORITY_0;

   // Assign the nested union member directly to bypass the "not a structure" error
   q_config.trigger_edge = XMC_VADC_TRIGGER_EDGE_NONE;
   q_config.trigger_signal = XMC_VADC_REQ_TR_A;
   q_config.gate_signal = XMC_VADC_REQ_GT_A;
   XMC_VADC_GROUP_QueueInit(VADC_G0, &q_config);

   XMC_VADC_CHANNEL_CONFIG_t ch_config = {0};
   ch_config.input_class = XMC_VADC_CHANNEL_CONV_GROUP_CLASS0; // Use Class 0 settings
   ch_config.result_reg_number = 0; // Store result in Result Register 0
   XMC_VADC_GROUP_ChannelInit(VADC_G0, 0, &ch_config);

   // ENABLE THE SLOT (The line you found!)
   XMC_VADC_GROUP_QueueEnableArbitrationSlot(VADC_G0);

   // INSERT THE CHANNEL (The "???")
   // We use refill_needed = true so it stays in the queue for our loop
   XMC_VADC_QUEUE_ENTRY_t q_entry =
   {
      .channel_num = 0,
      .refill_needed = true, 
      .external_trigger = false
   };
   XMC_VADC_GROUP_QueueInsertChannel(VADC_G0, q_entry);
}

// Define the likely candidates
#define LED_PORT XMC_GPIO_PORT3 
#define LED_PIN  9

/* Radar Control Pin Definitions for Sense2GoL Pulse (BGA-196) */
#define RADAR_VCC_EN    XMC_GPIO_PORT0, 15  // Active Low
#define RADAR_BB1_EN    XMC_GPIO_PORT0, 0   // Active High
//#define RADAR_BB2_EN    XMC_GPIO_PORT0, 1   // Active High
//#define RADAR_SH_EN     XMC_GPIO_PORT0, 2   // Active High

int main(void)
{
   // Enable DWT and ITM
   CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

   // Force the Debug Block out of reset by clearing the relevant bit in the SCU
   // On XMC4700, PRCLR0 register controls peripheral resets. 
   // Bit 16 (DBGRS) is the Debug Reset bit.
   SCU_RESET->PRCLR0 = (1UL << 16); 

   // Enable Trace in the CoreDebug block
   CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

   // Unlock the ITM
   ITM->LAR = 0xC5ACCE55;

   // Enable ITM
   ITM->TCR = ITM_TCR_ITMENA_Msk;
   ITM->TER = 1UL;
   // Route the SWO (Trace) signal to P0.1
   // Mode 0 = Alternate Function 0 (Trace Output)
   XMC_GPIO_SetMode(XMC_GPIO_PORT0, 1, XMC_GPIO_MODE_OUTPUT_ALT2);
   
   // Setup the "Switches"
   XMC_GPIO_SetMode(RADAR_VCC_EN, XMC_GPIO_MODE_OUTPUT_PUSH_PULL);
   XMC_GPIO_SetMode(RADAR_BB1_EN, XMC_GPIO_MODE_OUTPUT_PUSH_PULL);

   // Flip the Switches
   XMC_GPIO_SetOutputLow(RADAR_VCC_EN);  // Turn on Radar Power
   XMC_GPIO_SetOutputHigh(RADAR_BB1_EN); // Turn on the Amp

   ADC_Init();

   /* Initialize Port 1, Pin 14 as a Push-Pull Output */
   XMC_GPIO_CONFIG_t led_config =
   {
      .mode = XMC_GPIO_MODE_OUTPUT_PUSH_PULL,
      .output_level = XMC_GPIO_OUTPUT_LEVEL_LOW,
      .output_strength = XMC_GPIO_OUTPUT_STRENGTH_MEDIUM
   };

   // P1.13 (Blue): Often used for "System Live" or "Data Transfer" (the "B" in RGB).
   // P1.14 (Green): The "All Clear" or "User OK" signal.
   // P1.15 (Red): The "Error" or "Halt" signal.

   XMC_GPIO_Init(XMC_GPIO_PORT1, 13, &led_config);
   XMC_GPIO_Init(XMC_GPIO_PORT1, 14, &led_config);
   XMC_GPIO_Init(XMC_GPIO_PORT1, 15, &led_config);
   // Inside your main function, after initialization:
   XMC_GPIO_SetMode(XMC_GPIO_PORT1, 13, XMC_GPIO_MODE_OUTPUT_PUSH_PULL);
   XMC_GPIO_SetMode(XMC_GPIO_PORT1, 14, XMC_GPIO_MODE_OUTPUT_PUSH_PULL);
   XMC_GPIO_SetMode(XMC_GPIO_PORT1, 15, XMC_GPIO_MODE_OUTPUT_PUSH_PULL);

   while(1)
   {
      // Trigger a conversion on Group 0, Channel 0
      XMC_VADC_GROUP_QueueTriggerConversion(VADC_G0);

      printf("Starting Conversion...\n");
      XMC_VADC_GROUP_QueueTriggerConversion(VADC_G0);

      uint32_t result;
      uint32_t timeout = 100000;
      do
      {
         result = XMC_VADC_GROUP_GetResult(VADC_G0, 0);
         // Bit 31 of the result register is the 'Valid' flag.
         // We loop here until that bit becomes 1.
         timeout--;
      } while (((result & 0x80000000) == 0) && (timeout > 0));

      // Toggle on LEDs
      XMC_GPIO_ToggleOutput(XMC_GPIO_PORT1, 13);
      XMC_GPIO_ToggleOutput(XMC_GPIO_PORT1, 14);
      XMC_GPIO_ToggleOutput(XMC_GPIO_PORT1, 15);
      // Immediately dim LEDs
      XMC_GPIO_SetOutputHigh(XMC_GPIO_PORT1, 13);
      XMC_GPIO_SetOutputHigh(XMC_GPIO_PORT1, 14);
      XMC_GPIO_SetOutputHigh(XMC_GPIO_PORT1, 15);

      if ((result & 0x80000000) == 0)
      {
         // VADC failed; Turn red LED ON
         XMC_GPIO_SetOutputLow(XMC_GPIO_PORT1, 15);
         XMC_GPIO_SetOutputHigh(XMC_GPIO_PORT1, 13);
         XMC_GPIO_SetOutputHigh(XMC_GPIO_PORT1, 14);
      }
      else
      {
         // VADC succeeded; Turn green LED ON
         XMC_GPIO_SetOutputLow(XMC_GPIO_PORT1, 14);
         XMC_GPIO_SetOutputHigh(XMC_GPIO_PORT1, 13);
         XMC_GPIO_SetOutputHigh(XMC_GPIO_PORT1, 15);
         // Send it to Manjaro!
         printf("Radar Value: %" PRIu32 "\n", result);
      }

      /*
      // This code toggles R,G,B LEDS in a pattern
      XMC_GPIO_ToggleOutput(XMC_GPIO_PORT1, 13);
      for(uint32_t i = 0; i < 1000000; i++); // Simple delay
      XMC_GPIO_ToggleOutput(XMC_GPIO_PORT1, 14);
      for(uint32_t i = 0; i < 1000000; i++); // Simple delay
      XMC_GPIO_ToggleOutput(XMC_GPIO_PORT1, 15);
      for(uint32_t i = 0; i < 1000000; i++); // Simple delay
      */
   }
}