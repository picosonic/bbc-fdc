#ifndef _PINS_H_
#define _PINS_H_

// Pin mode setting using BCM2835 library
#define GPIO_IN   BCM2835_GPIO_FSEL_INPT
#define GPIO_OUT  BCM2835_GPIO_FSEL_OUTP
#define PULL_UP   BCM2835_GPIO_PUD_UP

// GPIO and P1 numbers are for Raspberry Pi 2/3 with 40 pin header
// All used inputs and outputs of 34 pin connector go through 74LS06

// Output pins to 34pin cable
#define MOTOR_ON     RPI_V2_GPIO_P1_12  // GPIO 18 P1_12 to cable 16
#define DS0_OUT      RPI_V2_GPIO_P1_16  // GPIO 23 P1_16 to cable 10 or cable 12 via jumper J3
#define SIDE_SELECT  RPI_V2_GPIO_P1_22  // GPIO 25 P1_22 to cable 32 or cable  2 via jumper J4
#define WRITE_GATE   RPI_V2_GPIO_P1_03  // GPIO  2 P1_3  to cable 24
#define DIR_STEP     RPI_V2_GPIO_P1_07  // GPIO  4 P1_7  to cable 20
#define WRITE_DATA   RPI_V2_GPIO_P1_19  // GPIO 10 P1_19 to cable 22 (MOSI)
#define DIR_SEL      RPI_V2_GPIO_P1_10  // GPIO 15 P1_10 to cable 18

// Input pins from 34pin cable
#define TRACK_0        RPI_V2_GPIO_P1_13  // GPIO 27 P1_13 from cable 26
#define WRITE_PROTECT  RPI_V2_GPIO_P1_15  // GPIO 22 P1_15 from cable 28 (UART RX)
#define INDEX_PULSE    RPI_V2_GPIO_P1_18  // GPIO 24 P1_18 from cable  8 or cable  4 via jumper J2
#define READ_DATA      RPI_V2_GPIO_P1_21  // GPIO  9 P1_21 from cable 30 (MISO)

#endif
