# Radar Project with Infineon XMC4700 Microcontroller Sense2GoLPulse Radar Kit

## System Specification
- This project was developed on a Manjaro Linux system. Required development packages were downloaded from the AUR (Arch User Repository).
  - `sudo pacman -Sy modustoolbox` (something like that...)
- The IDE (Modus Dashboard) was not used in the making of this project.

## Building
- Build the application: `make build CY_COMPILER_GCC_ARM_DIR=/usr` -- the Makefile needs to be told where to look for the gcc compiler. I did not use the gcc compiler which came in the modustoolbox package. I used the one native to my system.
- Flash the target: `make program`

## Development
- First I validated that my environment was set up (i.e., I could build and flash the board properly). I accomplished this by setting out the task of toggling the LEDs -- there are one of each red, green, and blue LEDs built into the radar circuit board. I was able to determine which physical pin numbers on the GPIO port (XMC_GPIO_PORT1) correspond to which LEDs (see Infineon Sense2GoLPulse's software guide page 23):
  - Red is pin #15
  - Green is pin #14
  - Blue is pin #13
- Next I set up the serial output. For debugging/observation purposes, this is a necessity. `printf()` needs somewhere to go. The XMC4700 has a J-Link Debugger built in, so there is a virtual com (VCOM) port which sends data over the debugger micro-USB connection. I wanted to do it this way rather than use the kit's DAVE application for VCOM, which abstracted away the communication setup. So instead I used JLinkSWOViewerExe, the dedicated sniffer. This program, while capable of more than the way I used it here, is used for ITM (Instrumentation Trace Macrocell), which handles `printf` data. *Ultimately though, due to restrictions on the XMC4700, I switched to using SWO/ITM rather than stick with VCOM/UART.*
  - I ran: `JLinkSWOViewerExe -device XMC4700-2048 -cpufreq 144000000 -swofreq 1000000`, which opens JLinkExe w/ go in a separate window (to put it more accurately, JLinkSWOViewerExe is a specialized frontend that connects to the J-Link probe to visualize the trace stream).
  - Most critically, I wrote my own custom `_write()` function which retargets `stdout` to the ITM channel of communication, sending data to the SWO pin on the XMC4700. 
  - ITM was chosen over UART, because UART wasn't actually a selectable option in `JLinkExe` with XMC4700-2048. UART, being general-purpose, is likely to be used in other ways for communication across the board, while ITM is a dedicated debugging tool.


## Helpful Resources
- [Google Gemini](https://gemini.google.com/app)
- [Infineon XMC4700/4800 Data Sheet](https://www.infineon.com/assets/row/public/documents/30/49/infineon-xmc4700-xmc4800-datasheet-en.pdf?fileId=5546d462518ffd850151908ea8db00b3)
- [Infineon XMC4700/4800 Reference Manual](https://www.infineon.com/assets/row/public/documents/30/44/infineon-referencemanual-xmc4700-xmc4800-um-en.pdf)
- [Infineon Sense2GoLPulse Software Guide](https://www.infineon.com/assets/row/public/documents/24/44/infineon-um-demo-sense2gol-pulse-software-guide-usermanual-en.pdf)
- [ModusToolbox AUR](https://aur.archlinux.org/packages/modustoolbox)
