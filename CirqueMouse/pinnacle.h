#ifndef __PINNACLE_H__
#define __PINNACLE_H__

// Copyright (c) 2018 Cirque Corp. Restrictions apply. See: www.cirque.com/sw-license

#include <Arduino.h>
#include <SPI.h>


// ___ Using a Cirque TM0XX0XX w/ Curved Overlay and Arduino ___
// This demonstration application is built to work with a Teensy 3.1/3.2 but it can easily be adapted to
// work with Arduino-based systems.
// When using with DK000013 development kit, connect sensor to the FFC connector
// labeled 'Sensor0'.
// This application connects to a TM0XX0XX circular touch pad via SPI. To verify that your touch pad is configured
// for SPI-mode, make sure that R1 is populated with a 470k resistor (or whichever resistor connects pins 24 & 25 of the 1CA027 IC).
// The pad is configured for Absolute mode tracking.  Touch data is sent in text format over USB CDC to
// the host PC.  You can open a terminal window on the PC to the USB CDC port and see X, Y, and Z data
// fill the window when you touch the sensor. Tools->Serial Monitor can be used to view touch data.
// NOTE: all config values applied in this sample are meant for a module using REXT = 976kOhm

//  Pinnacle TM0XX0XX with Arduino
//  Hardware Interface
//  GND
//  +3.3V
//  SCK = Pin 13
//  MISO = Pin 12
//  MOSI = Pin 11
//  SS = Pin 8
//  DR = Pin 7

// Hardware pin-number labels
#define SCK_PIN   13
#define DIN_PIN   12
#define DOUT_PIN  11
#define CS_PIN    10
#define DR_PIN    9

#define SDA_PIN   18
#define SCL_PIN   19

#define LED_0     21

// Masks for Cirque Register Access Protocol (RAP)
#define WRITE_MASK  0x80
#define READ_MASK   0xA0

// Register config values for this demo
#define SYSCONFIG_1   0x00
#define FEEDCONFIG_1  0x03
#define FEEDCONFIG_2  0x1F
#define Z_IDLE_COUNT  0x05

// Coordinate scaling values
#define PINNACLE_XMAX     2047    // max value Pinnacle can report for X
#define PINNACLE_YMAX     1535    // max value Pinnacle can report for Y
#define PINNACLE_X_LOWER  127     // min "reachable" X value
#define PINNACLE_X_UPPER  1919    // max "reachable" X value
#define PINNACLE_Y_LOWER  63      // min "reachable" Y value
#define PINNACLE_Y_UPPER  1471    // max "reachable" Y value
#define PINNACLE_X_RANGE  (PINNACLE_X_UPPER-PINNACLE_X_LOWER)
#define PINNACLE_Y_RANGE  (PINNACLE_Y_UPPER-PINNACLE_Y_LOWER)
#define ZONESCALE 256   // divisor for reducing x,y values to an array index for the LUT
#define ROWS_Y ((PINNACLE_YMAX + 1) / ZONESCALE)
#define COLS_X ((PINNACLE_XMAX + 1) / ZONESCALE)

// ADC-attenuation settings (held in BIT_7 and BIT_6)
// 1X = most sensitive, 4X = least sensitive
#define ADC_ATTENUATE_1X   0x00
#define ADC_ATTENUATE_2X   0x40
#define ADC_ATTENUATE_3X   0x80
#define ADC_ATTENUATE_4X   0xC0

// Convenient way to store and access measurements
typedef struct _absData
{
  uint16_t x_pos;
  uint16_t y_pos;
  uint16_t z_pos;
  uint8_t buttonFlags;
  bool touchDown;
  bool hovering;
} absData_t;

typedef enum _touchState
{
    VALID,
    HOVERING,
    LIFTOFF,
} touchState_t;

//const uint16_t ZONESCALE = 256;
//const uint16_t ROWS_Y = 6;
//const uint16_t COLS_X = 8;

// These values require tuning for optimal touch-response
// Each element represents the Z-value below which is considered "hovering" in that XY region of the sensor.
// The values present are not guaranteed to work for all HW configurations.
static const uint8_t ZVALUE_MAP[ROWS_Y][COLS_X] =
{
  {0, 0,  0,  0,  0,  0, 0, 0},
  {0, 2,  3,  5,  5,  3, 2, 0},
  {0, 3,  5, 15, 15,  5, 2, 0},
  {0, 3,  5, 15, 15,  5, 3, 0},
  {0, 2,  3,  5,  5,  3, 2, 0},
  {0, 0,  0,  0,  0,  0, 0, 0},
};

void Pinnacle_Init();
void Pinnacle_GetAbsolute(absData_t * result);
bool Pinnacle_zIdlePacket(absData_t * data);
void Pinnacle_EnableFeed(bool feedEnable);
void Pinnacle_CheckValidTouch(absData_t * touchData);
void Pinnacle_ClearFlags();

void AssertSensorLED(bool state);
bool DR_Asserted();

#endif
