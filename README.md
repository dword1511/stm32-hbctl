# STM32-based half-bridge controller

This firmware code uses an `STM32F042` MCU to generate waveform for a STMicro `L6498` half-bridge driver,
which drives the MOSFETs in the inverter.

This particular inverter delivers 1 MHz AC current for our capacitive power transfer (CPT) project.
It has onboard buttons allowing us to cycle through 1 MHz +/- 100 kHz, with a few kHz of resolution.
It also has USB for ISP/DFU.

Unusual things this firmware does:

* Using HSI48 trimming in conjunction with timer prescaler to deliver a resolution of a few kHz across a large frequency range.
* Using a generic timer (rather than the advanced one) to generate complementary waveforms with dead time.

## General Build Instructions

You will need the following packages (Debian/Ubuntu):

* build-essential
* gcc-arm-none-eabi
* git
* dfu-util

The firmware can be loaded via USB DFU. However, you can also flash and debug via SWD. To do so, install the following package:

* stlink-tools

This project uses `libopencm3`. It will be automatically checked out during the building process.
To build the firmware, run `make`. To flash it, hold the `ISP/USR` button on the board, connect the board to USB, and then run `make flash-dfu`.

## Special Directions for CPT

Please be aware of the high voltage, especially at matching network's output.

Connect the inverter to the matching network first. Do not power on the inverter without any load.
Connect 12 V DC power to the 12 V port with a 4 mm banana test lead. Connect a variable DC power source to the HV port. Connect the ground together to GND.

Power the board on and observe the LEDs starting to flash. Use the `ISP/USR` button to enable the half-bridge. Then use the same button to cycle through frequencies.
The matching network reaches resonance when the output voltage peaks. Bridge current can be monitored by attaching an oscilloscope probe to the `CSP` pin.
