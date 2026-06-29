# Radar Project with Infineon XMC4700 Sense2GoLPulse Radar Kit

## System Specification
- This project was developed on a Manjaro Linux system. Required development packages were downloaded from the AUR (Arch User Repository).
  - `sudo pacman -Sy modustoolbox` (something like that...)
- The IDE (Modus Dashboard) was not used in the making of this project.

## Building
- Build the application: `make build CY_COMPILER_GCC_ARM_DIR=/usr` -- the Makefile needs to be told where to look for the gcc compiler. I did not use the gcc compiler which came in the modustoolbox package. I used the one native to my system.
- Flash the target: `make program`

## Helpful Resources
- [Google Gemini](https://gemini.google.com/app)
- [Infineon XMC4700/4800 Reference Manual](https://www.infineon.com/assets/row/public/documents/30/49/infineon-xmc4700-xmc4800-datasheet-en.pdf?fileId=5546d462518ffd850151908ea8db00b3)
- [Infineon Sense2GoLPulse Software Guide](https://www.infineon.com/assets/row/public/documents/24/44/infineon-um-demo-sense2gol-pulse-software-guide-usermanual-en.pdf)
- [ModusToolbox AUR](https://aur.archlinux.org/packages/modustoolbox)
