#include <stdio.h>

#include "xmc_common.h" // Defines human-readable macros for hexadecimal memory addresses for the XMC4700 memory map.
#include "xmc_gpio.h"   // Essential for pin routing.
#include "xmc_scu.h"    // Includes stuff for PLL (phase locked loop). Also needed for "ungating" (powering on) specific modules. SCU=System Control Unit
#include "xmc_uart.h"   // UART is a "mode" of the USIC (Universal Serial Interface Channel). USIC can be SPI, I2C, or UART. UART=Universal Asynchronous Receiver-Transmitter
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
    .k_div = 2,                               //  (Post-divider) Divide 288MHz by 2 = 144MHz
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

    // Now, any peripheral you start (like UART) 
    // will automatically use this 144MHz base.
}

// This replaces the need for cy_retarget_io
int _write(int file, char *ptr, int len)
{
    for (volatile int i = 0; i < len; i++) {
        XMC_UART_CH_Transmit(XMC_UART0_CH0, (uint8_t)ptr[i]);
        // Wait for the transmit buffer to empty
        while(!(XMC_UART_CH_GetStatusFlag(XMC_UART0_CH0) & XMC_UART_CH_STATUS_FLAG_TRANSMISSION_IDLE));
    }
    return len;
}

void ADC_Init(void) {
    // 1. Global & Group Start
    XMC_VADC_GLOBAL_Init(VADC, &(XMC_VADC_GLOBAL_CONFIG_t){ .clock_config.analog_clock_divider = 5 });
    
    // 2. Group Setup - THIS IS THE FIX
    XMC_VADC_GROUP_CONFIG_t group_config = { 0 }; // Initialize to zero first
    
    // Set the arbiter to "Continuous" mode (the equivalent of starting it)
    group_config.arbiter_mode = XMC_VADC_GROUP_ARBMODE_ALWAYS;
    
    XMC_VADC_GROUP_Init(VADC_G0, &(XMC_VADC_GROUP_CONFIG_t){0});
    XMC_VADC_GLOBAL_StartupCalibration(VADC);

    // 2. Initialize the Queue Struct to zeros first
    XMC_VADC_QUEUE_CONFIG_t q_config = { 0 };
    
    // Set the top-level members
    q_config.conv_start_mode = XMC_VADC_STARTMODE_WFS;
    q_config.req_src_priority = XMC_VADC_GROUP_RS_PRIORITY_0;
    
    // Assign the nested union member directly to bypass the "not a structure" error
    q_config.trigger_edge = XMC_VADC_TRIGGER_EDGE_NONE;
    q_config.trigger_signal = XMC_VADC_REQ_TR_A;
    q_config.gate_signal = XMC_VADC_REQ_GT_A;
    XMC_VADC_GROUP_QueueInit(VADC_G0, &q_config);

    // 3. ENABLE THE SLOT (The line you found!)
    XMC_VADC_GROUP_QueueEnableArbitrationSlot(VADC_G0);

    // 4. INSERT THE CHANNEL (The "???")
    // We use refill_needed = true so it stays in the queue for our loop
    XMC_VADC_QUEUE_ENTRY_t q_entry = {
        .channel_num = 0,
        .refill_needed = true, 
        .external_trigger = false
    };
    XMC_VADC_GROUP_QueueInsertChannel(VADC_G0, q_entry);
}

// Define the likely candidates
#define LED_PORT XMC_GPIO_PORT3 
#define LED_PIN  9

// On the Sense2Go/Relax Kit, the J-Link VCOM is usually USIC0 Channel 0
#define UART_CH XMC_UART0_CH0

/* Radar Control Pin Definitions for Sense2GoL Pulse (BGA-196) */
#define RADAR_VCC_EN    XMC_GPIO_PORT0, 15  // Active Low
#define RADAR_BB1_EN    XMC_GPIO_PORT0, 0   // Active High
#define RADAR_BB2_EN    XMC_GPIO_PORT0, 1   // Active High
#define RADAR_SH_EN     XMC_GPIO_PORT0, 2   // Active High

int main(void)
{
    /* 1. Hardware Configuration */
    XMC_UART_CH_CONFIG_t uart_config = {
        .baudrate = 115200,
        .data_bits = 8,
        .stop_bits = 1,
        .parity_mode = XMC_USIC_CH_PARITY_MODE_NONE
    };

    /* 2. Initialize the USIC Channel */
    XMC_UART_CH_Init(UART_CH, &uart_config);
    XMC_UART_CH_Start(UART_CH); // CRITICAL: USIC needs a start command!

    /* 3. Set P1.5 to UART Transmit (Output) */
    // We set it to "ALT2" which tells the pin: "Connect your internal wire to the UART"
    // (Port, Pin, Mode)
    XMC_GPIO_SetMode(XMC_GPIO_PORT1, 5, XMC_GPIO_MODE_OUTPUT_PUSH_PULL_ALT2);

    /* 4. Set P1.4 to UART Receive (Input) */
    XMC_GPIO_SetMode(XMC_GPIO_PORT1, 4, XMC_GPIO_MODE_INPUT_PULL_UP);  // Use Pull-Up instead of Tristate because Tristate is 
                                                                       // much more sensitive to radar transmissions, which could
                                                                       // cause electrical interference and loss of signal
                                                                       // integrity on the line

    /*. Tell the USIC hardware to "Listen" to the correct DX (Data Input) line
        P1.4 maps to DX0B for USIC0 Channel 0 */
    XMC_UART_CH_SetInputSource(XMC_UART0_CH0, XMC_UART_CH_INPUT_RXD, USIC0_C0_DX0_P1_4);

    // Setup the "Switches"
XMC_GPIO_SetMode(RADAR_VCC_EN, XMC_GPIO_MODE_OUTPUT_PUSH_PULL);
XMC_GPIO_SetMode(RADAR_BB1_EN, XMC_GPIO_MODE_OUTPUT_PUSH_PULL);

// Flip the Switches
XMC_GPIO_SetOutputLow(RADAR_VCC_EN);  // Turn on Radar Power
XMC_GPIO_SetOutputHigh(RADAR_BB1_EN); // Turn on the Amp
    
    ADC_Init();

    /* Initialize Port 1, Pin 14 as a Push-Pull Output */
    XMC_GPIO_CONFIG_t led_config = {
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

        // Wait for the "Snapshot" to finish (Busy-wait for now)
        uint32_t result;
        do {
            result = XMC_VADC_GROUP_GetResult(VADC_G0, 0);
            // Bit 31 of the result register is the 'Valid' flag.
            // We loop here until that bit becomes 1.
           } while ((result & 0x80000000) == 0);

        // Send it to Manjaro!
        printf("Radar Value: %d\r\n", result);

        XMC_GPIO_ToggleOutput(XMC_GPIO_PORT1, 13);
        for(uint32_t i = 0; i < 1000000; i++); // Simple delay
        XMC_GPIO_ToggleOutput(XMC_GPIO_PORT1, 14);
        for(uint32_t i = 0; i < 1000000; i++); // Simple delay
        XMC_GPIO_ToggleOutput(XMC_GPIO_PORT1, 15);
        for(uint32_t i = 0; i < 1000000; i++); // Simple delay
    }
}