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
- Next I set up the serial output. For debugging/observation purposes, this is a necessity. `printf()` needs somewhere to go. The XMC4700 has a J-Link Debugger built in, so there is a virtual com (VCOM) port which sends data over the debugger micro-USB connection. I wanted to do it this way rather than use the kit's DAVE application for VCOM, which abstracted away the communication setup. So instead I used JLink. I ran `JLinkExe` with the following options:
    `J-Link>connect`
    `Please specify device / core. <Default>: XMC4700-2048`
      `J) JTAG (Default)`
      `S) SWD`
      `T) cJTAG`
    `TIF>S`
    `Specify the target interface speed [kHz]. <Default>: 4000 kHz`
    `Speed>` (Enter - use default)
    `Device "XMC4700-2048 selected.`
    `Connecting to target via SWD`
    `Performing XMC4500 connection sequence.`
    `DPIDR: 0x2BA01477`
    `CoreSight SoC-400 or earlier`
    `Scanning AP map to find all available APs`
    `AP[1]: Stopped AP scan as end of AP map has been reached`
    `AP[0]: AHB-AP (IDR: 0x24770011, ADDR: 0x00000000)`
    `Iterating through AP map to find AHB-AP to use`
    `AP[0]: Core found`
    `AP[0]: AHB-AP ROM base: 0xE00FF000`
    `CPUID register: 0x410FC241. Implementer code: 0x41 (ARM)`
    `Found Cortex-M4 r0p1, Little endian.`
    `FPUnit: 6 code (BP) slots and 2 literal slots`
    `CoreSight components:`
    `ROMTbl[0] @ E00FF000`
    `[0][0]: E000E000 CID B105E00D PID 000BB00C SCS-M7`
    `[0][1]: E0001000 CID B105E00D PID 003BB002 DWT`
    `[0][2]: E0002000 CID B105E00D PID 002BB003 FPB`
    `[0][3]: E0000000 CID B105E00D PID 003BB001 ITM`
    `[0][4]: E0040000 CID B105900D PID 000BB9A1 TPIU`
    `[0][5]: E0041000 CID B105900D PID 000BB925 ETM`
    `Memory zones:`
    `Zone: "Default" Description: Default access mode`
    `Cortex-M4 identified.`
  At this point the connection to J-Link has been established. For one reason or another, this interrupts the execution on the XMC4700, so an instruction has to be sent through J-Link to resume execution/
    `J-Link>g`
    `Memory map 'after startup completion point' is active`
  To exit J-Link, I do a \<Ctrl\>+C.
  Alternatively, you can run `JLinkExe -device XMC4700-2048 -if SWD -speed 4000`.
  - Alternatively alternatively, you can do `JLinkSWOViewerExe -device XMC4700-2048 -cpufreq 144000000 -swofreq 1000000` which will open JLinkExe w/ go in a separate window.


## Helpful Resources
- [Google Gemini](https://gemini.google.com/app)
- [Infineon XMC4700/4800 Data Sheet](https://www.infineon.com/assets/row/public/documents/30/49/infineon-xmc4700-xmc4800-datasheet-en.pdf?fileId=5546d462518ffd850151908ea8db00b3)
- [Infineon XMC4700/4800 Reference Manual](https://www.infineon.com/assets/row/public/documents/30/44/infineon-referencemanual-xmc4700-xmc4800-um-en.pdf)
- [Infineon Sense2GoLPulse Software Guide](https://www.infineon.com/assets/row/public/documents/24/44/infineon-um-demo-sense2gol-pulse-software-guide-usermanual-en.pdf)
- [ModusToolbox AUR](https://aur.archlinux.org/packages/modustoolbox)
