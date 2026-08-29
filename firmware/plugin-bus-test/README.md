# Showduino Plug-in Bus v1

Standalone commissioning firmware for the ESP32-P4 Showduino Plug-in Bus.

## Physical bus

- SDA: P4 GPIO7
- SCL: P4 GPIO8
- Speed: 100 kHz
- Logic: 3.3 V
- The Waveshare board already provides I2C pull-ups. Never add 5 V pull-ups.
- The bus is shared with onboard I2C hardware, including the ES8311 audio codec at 0x18.

## SX1509 layout

- 0x3E: 16-channel INPUT board
- 0x3F: 16-channel OUTPUT board

Input examples include buttons, PIR outputs, reed switches, limit switches, pressure mats and beam-break sensors. PIR modules are treated as digital sensors connected to an SX1509 input; they are not themselves I2C devices.

Output channels are logic/control outputs. Motors, solenoids, relay coils, high-power lamps and other loads must use suitable transistor/MOSFET/driver hardware rather than being powered directly by an SX1509 pin.

## Serial console

115200 baud commands:

- HELP
- SCAN
- STATUS
- ON:0 through ON:15
- OFF:0 through OFF:15
- ALL:OFF
- TEST:OUTPUTS

Input transitions are debounced and produce named events, for example:

    INPUT:BUTTON_1:ACTIVE
    INPUT:BUTTON_1:PRESSED
    INPUT:BUTTON_1:RELEASED
    INPUT:PIR_1:ACTIVE
    INPUT:DOOR_REED:INACTIVE

Outputs produce named state events such as:

    OUTPUT:WARNING_LAMP:ON

## Safety

The SX1509 output board is a control/logic layer, not a power driver. Use appropriate isolation, flyback protection, MOSFETs/transistors and separate load supplies where required.

## Planned expansion

The Plug-in Bus architecture is intended to expand to ADC, ToF distance, environmental, ambient-light, power/current and motion/IMU modules after the SX1509 foundation is commissioned.
