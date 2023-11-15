#ifndef _PINS_H_
#define _PINS_H_

// Pin mode setting using BCM2835 library
#define GPIO_IN   BCM2835_GPIO_FSEL_INPT
#define GPIO_OUT  BCM2835_GPIO_FSEL_OUTP
#define PULL_UP   BCM2835_GPIO_PUD_UP

// All used inputs and outputs of 34 pin connector go through 74LS06

// Hardware detection uses info from the following websites
//
//   https://elinux.org/RPi_HardwareHistory
//   https://www.raspberrypi.org/documentation/hardware/raspberrypi/revision-codes/README.md
//   https://www.raspberrypi.org/documentation/configuration/config-txt/overclocking.md
//   https://www.raspberrypi.com/documentation/computers/raspberry-pi.html

// Try to detect RPi Zero / Zero W
#if defined(REV_900092) || defined(REV_920092) || defined(REV_900093) || defined(REV_9000c1) || defined(REV_920093)
  #define RPI0 1
  #define HAS_BCM2835 1
  #define CLOCK_400 1
#endif

// Try to detect RPi Zero 2 W
#if defined(REV_902120)
   #define RPI0 1
   #define HAS_BCM2837 1
   #define CLOCK_400 1
#endif

// Try to detect RPi 1, rev 1
#if defined (REV_0002) || defined(REV_0003)
  #define RPI1_1 1
  #define HAS_BCM2835 1
  #define CLOCK_250 1
#endif

// Try to detect RPi 1, rev 2
#if defined(REV_0004) || defined(REV_0005) || defined(REV_0006) || defined(REV_0007) || defined(REV_0008) || defined(REV_0009) || defined(REV_000d) || defined(REV_000e) || defined(REV_000f) || defined(REV_0010) || defined(REV_0011) || defined(REV_0012) || defined(REV_0013) || defined(REV_0014) || defined(REV_0015) || defined(REV_900021) || defined(REV_900032) || defined(REV_900061)
  #define RPI1_2 1
  #define HAS_BCM2835 1
  #define CLOCK_250 1
#endif

// Try to detect RPi 2
#if defined (REV_a01040) || defined(REV_a01041) || defined(REV_a21041)
  #define RPI2 1
  #define HAS_BCM2836 1
  #define CLOCK_250 1
#endif

#if defined (REV_a02042) || defined(REV_a22042)
  #define RPI2 1
  #define HAS_BCM2837 1
  #define CLOCK_250 1
#endif

// Try to detect RPi 3 / 3A+ / 3B+
#if defined(REV_9020e0) || defined (REV_a02082) || defined(REV_a020a0) || defined(REV_a020d3) || defined(REV_a22082) || defined(REV_a220a0) || defined(REV_a32082) || defined(REV_a52082) || defined(REV_a22083) || defined(REV_a02100)
  #define RPI3 1
  #define HAS_BCM2837 1
  #define CLOCK_400 1
#endif

// Try to detect Rpi 4
#if defined (REV_a03111) || defined(REV_b03111) || defined(REV_b03112) || defined(REV_c03111) || defined(REV_c03112) || defined(REV_d03114) || defined(REV_a03140) || defined(REV_b03140) || defined(REV_c03140) || defined(REV_d03140) || defined(REV_c03130)
  #define RPI4 1
  #define HAS_BCM2711 1
  #define CLOCK_500 1
#endif

// Try to detect RPi 5
#if defined (REV_c04170) || defined(REV_d04170)
  #define RPI5 1
  #define HAS_BCM2712 1
  #define CLOCK_500 1
#endif

#ifdef RPI1_1
  // GPIO and P1 numbers are for Raspberry Pi 1 rev 1, with 26 pin header

  // Output pins to 34pin cable
  #define MOTOR_ON     RPI_GPIO_P1_12  // GPIO 18 P1_12 to cable 16
  #define DS0_OUT      RPI_GPIO_P1_16  // GPIO 23 P1_16 to cable 10 or cable 12 via jumper J3
  #define SIDE_SELECT  RPI_GPIO_P1_22  // GPIO 25 P1_22 to cable 32 or cable  2 via jumper J4
  #define WRITE_GATE   RPI_GPIO_P1_03  // GPIO  0 P1_3  to cable 24
  #define DIR_STEP     RPI_GPIO_P1_07  // GPIO  4 P1_7  to cable 20
  #define WRITE_DATA   RPI_GPIO_P1_19  // GPIO 10 P1_19 to cable 22 (MOSI)
  #define DIR_SEL      RPI_GPIO_P1_10  // GPIO 15 P1_10 to cable 18

  // Input pins from 34pin cable
  #define TRACK_0        RPI_GPIO_P1_13  // GPIO 21 P1_13 from cable 26
  #define WRITE_PROTECT  RPI_GPIO_P1_15  // GPIO 22 P1_15 from cable 28 (UART RX)
  #define INDEX_PULSE    RPI_GPIO_P1_18  // GPIO 24 P1_18 from cable  8 or cable  4 via jumper J2
  #define READ_DATA      RPI_GPIO_P1_21  // GPIO  9 P1_21 from cable 30 (MISO)
#else
  #ifdef RPI1_2
    // GPIO and P1 numbers are for Raspberry Pi 1 rev 2, with 26 pin header

    // Output pins to 34pin cable
    #define MOTOR_ON     RPI_GPIO_P1_12  // GPIO 18 P1_12 to cable 16
    #define DS0_OUT      RPI_GPIO_P1_16  // GPIO 23 P1_16 to cable 10 or cable 12 via jumper J3
    #define SIDE_SELECT  RPI_GPIO_P1_22  // GPIO 25 P1_22 to cable 32 or cable  2 via jumper J4
    #define WRITE_GATE   RPI_GPIO_P1_03  // GPIO  2 P1_3  to cable 24
    #define DIR_STEP     RPI_GPIO_P1_07  // GPIO  4 P1_7  to cable 20
    #define WRITE_DATA   RPI_GPIO_P1_19  // GPIO 10 P1_19 to cable 22 (MOSI)
    #define DIR_SEL      RPI_GPIO_P1_10  // GPIO 15 P1_10 to cable 18

    // Input pins from 34pin cable
    #define TRACK_0        RPI_GPIO_P1_13  // GPIO 27 P1_13 from cable 26
    #define WRITE_PROTECT  RPI_GPIO_P1_15  // GPIO 22 P1_15 from cable 28 (UART RX)
    #define INDEX_PULSE    RPI_GPIO_P1_18  // GPIO 24 P1_18 from cable  8 or cable  4 via jumper J2
    #define READ_DATA      RPI_GPIO_P1_21  // GPIO  9 P1_21 from cable 30 (MISO)
  #else
    // GPIO and P1 numbers are for Raspberry Pi 2/3 with 40 pin header

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
#endif

#endif
