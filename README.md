# STM32-based half-bridge controller

This firmware code uses an `STM32F042` MCU to generate waveform for a STMicro `L6498` half-bridge driver,
which drives the MOSFETs in the inverter.

This particular inverter delivers 1 MHz AC current for our capacitive power transfer (CPT) project.
It has onboard buttons allowing us to cycle through 1 MHz +/- 100 kHz, with a few kHz of resolution.
It also has USB for ISP/DFU.

Unusual things this firmware does:

* Using HSI48 trimming in conjunction with timer prescaler to deliver a resolution of a few kHz across a large frequency range.
* Using a generic timer (rather than the advanced one) to generate complementary waveforms with dead time.
