#include "pinnacle.h"


void setAdcAttenuation(uint8_t adcGain);
void tuneEdgeSensitivity();
void ERA_ReadBytes(uint16_t address, uint8_t * data, uint16_t count);
void ERA_WriteByte(uint16_t address, uint8_t data);
void RAP_Init();
void RAP_ReadBytes(byte address, byte * data, byte count);
void RAP_Write(byte address, byte data);
void ClipCoordinates(absData_t * coordinates);
void ScaleData(absData_t * coordinates, uint16_t xResolution, uint16_t yResolution);
void Assert_CS();
void DeAssert_CS();


/*  Pinnacle-based TM0XX0XX Functions  */
void Pinnacle_Init()
{
  RAP_Init();
  DeAssert_CS();
  pinMode(DR_PIN, INPUT);

  // Host clears SW_CC flag
  Pinnacle_ClearFlags();

  // Host configures bits of registers 0x03 and 0x05
  RAP_Write(0x03, SYSCONFIG_1);
  RAP_Write(0x05, FEEDCONFIG_2);

  // Host enables preferred output mode (absolute)
  RAP_Write(0x04, FEEDCONFIG_1);

  // Host sets z-idle packet count to 5 (default is 30)
  RAP_Write(0x0A, Z_IDLE_COUNT);

  // These functions are required for use with thick overlays (curved)
  setAdcAttenuation(ADC_ATTENUATE_1X);
  tuneEdgeSensitivity();

  Serial.println("Pinnacle Initialized...");
}

// Reads XYZ data from Pinnacle registers 0x14 through 0x17
// Stores result in absData_t struct with x_pos, y_pos, and z_pos members
void Pinnacle_GetAbsolute(absData_t * result)
{
  uint8_t data[6] = { 0,0,0,0,0,0 };
  RAP_ReadBytes(0x12, data, 6);

  Pinnacle_ClearFlags();

  result->buttonFlags = data[0] & 0x3F;
  result->x_pos = data[2] | ((data[4] & 0x0F) << 8);
  result->y_pos = data[3] | ((data[4] & 0xF0) << 4);
  result->z_pos = data[5] & 0x3F;

  result->touchDown = result->x_pos != 0;
}

// Checks touch data to see if it is a z-idle packet (all zeros)
bool Pinnacle_zIdlePacket(absData_t * data)
{
  return data->x_pos == 0 && data->y_pos == 0 && data->z_pos == 0;
}

// Clears Status1 register flags (SW_CC and SW_DR)
void Pinnacle_ClearFlags()
{
  RAP_Write(0x02, 0x00);
  delayMicroseconds(50);
}

// Enables/Disables the feed
void Pinnacle_EnableFeed(bool feedEnable)
{
  uint8_t temp;

  RAP_ReadBytes(0x04, &temp, 1);  // Store contents of FeedConfig1 register

  if(feedEnable)
  {
    temp |= 0x01;                 // Set Feed Enable bit
    RAP_Write(0x04, temp);
  }
  else
  {
    temp &= ~0x01;                // Clear Feed Enable bit
    RAP_Write(0x04, temp);
  }
}


/*  Curved Overlay Functions  */
// Adjusts the feedback in the ADC, effectively attenuating the finger signal
// By default, the the signal is maximally attenuated (ADC_ATTENUATE_4X for use with thin, flat overlays
void setAdcAttenuation(uint8_t adcGain)
{
  uint8_t temp = 0x00;

  Serial.println();
  Serial.println("Setting ADC gain...");
  ERA_ReadBytes(0x0187, &temp, 1);
  temp &= 0x3F; // clear top two bits
  temp |= adcGain;
  ERA_WriteByte(0x0187, temp);
  ERA_ReadBytes(0x0187, &temp, 1);
  Serial.print("ADC gain set to:\t");
  Serial.print(temp &= 0xC0, HEX);
  switch(temp)
  {
    case ADC_ATTENUATE_1X:
      Serial.println(" (X/1)");
      break;
    case ADC_ATTENUATE_2X:
      Serial.println(" (X/2)");
      break;
    case ADC_ATTENUATE_3X:
      Serial.println(" (X/3)");
      break;
    case ADC_ATTENUATE_4X:
      Serial.println(" (X/4)");
      break;
    default:
      break;
  }
}

// Changes thresholds to improve detection of fingers
void tuneEdgeSensitivity()
{
  uint8_t temp = 0x00;

  Serial.println();
  Serial.println("Setting xAxis.WideZMin...");
  ERA_ReadBytes(0x0149, &temp, 1);
  Serial.print("Current value:\t");
  Serial.println(temp, HEX);
  ERA_WriteByte(0x0149,  0x04);
  ERA_ReadBytes(0x0149, &temp, 1);
  Serial.print("New value:\t");
  Serial.println(temp, HEX);

  Serial.println();
  Serial.println("Setting yAxis.WideZMin...");
  ERA_ReadBytes(0x0168, &temp, 1);
  Serial.print("Current value:\t");
  Serial.println(temp, HEX);
  ERA_WriteByte(0x0168,  0x03);
  ERA_ReadBytes(0x0168, &temp, 1);
  Serial.print("New value:\t");
  Serial.println(temp, HEX);
}

// This function identifies when a finger is "hovering" so your system can choose to ignore them.
// Explanation: Consider the response of the sensor to be flat across it's area. The Z-sensitivity of the sensor projects this area
// a short distance upwards above the surface of the sensor. Imagine it is a solid cylinder (wider than it is tall)
// in which a finger can be detected and tracked. Adding a curved overlay will cause a user's finger to dip deeper in the middle, and higher
// on the perimeter. If the sensitivity is tuned such that the sensing area projects to the highest part of the overlay, the lowest
// point will likely have excessive sensitivity. This means the sensor can detect a finger that isn't actually contacting the overlay in the shallower area.
// ZVALUE_MAP[][] stores a lookup table in which you can define the Z-value and XY position that is considered "hovering". Experimentation/tuning is required.
// NOTE: Z-value output decreases to 0 as you move your finger away from the sensor, and it's maximum value is 0x63 (6-bits).
void Pinnacle_CheckValidTouch(absData_t * touchData)
{
  uint32_t zone_x, zone_y;
  //eliminate hovering
  zone_x = touchData->x_pos / ZONESCALE;
  zone_y = touchData->y_pos / ZONESCALE;
  touchData->hovering = !(touchData->z_pos > ZVALUE_MAP[zone_y][zone_x]);
}


/*  ERA (Extended Register Access) Functions  */
// Reads <count> bytes from an extended register at <address> (16-bit address),
// stores values in <*data>
void ERA_ReadBytes(uint16_t address, uint8_t * data, uint16_t count)
{
  uint8_t ERAControlValue = 0xFF;

  Pinnacle_EnableFeed(false); // Disable feed

  RAP_Write(0x1C, (uint8_t)(address >> 8));     // Send upper byte of ERA address
  RAP_Write(0x1D, (uint8_t)(address & 0x00FF)); // Send lower byte of ERA address

  for(uint16_t i = 0; i < count; i++)
  {
    RAP_Write(0x1E, 0x05);  // Signal ERA-read (auto-increment) to Pinnacle

    // Wait for status register 0x1E to clear
    do
    {
      RAP_ReadBytes(0x1E, &ERAControlValue, 1);
    } while(ERAControlValue != 0x00);

    RAP_ReadBytes(0x1B, data + i, 1);

    Pinnacle_ClearFlags();
  }
}

// Writes a byte, <data>, to an extended register at <address> (16-bit address)
void ERA_WriteByte(uint16_t address, uint8_t data)
{
  uint8_t ERAControlValue = 0xFF;

  Pinnacle_EnableFeed(false); // Disable feed

  RAP_Write(0x1B, data);      // Send data byte to be written

  RAP_Write(0x1C, (uint8_t)(address >> 8));     // Upper byte of ERA address
  RAP_Write(0x1D, (uint8_t)(address & 0x00FF)); // Lower byte of ERA address

  RAP_Write(0x1E, 0x02);  // Signal an ERA-write to Pinnacle

  // Wait for status register 0x1E to clear
  do
  {
    RAP_ReadBytes(0x1E, &ERAControlValue, 1);
  } while(ERAControlValue != 0x00);

  Pinnacle_ClearFlags();
}

/*  RAP Functions */

void RAP_Init()
{
  pinMode(CS_PIN, OUTPUT);
  SPI.begin();
}

// Reads <count> Pinnacle registers starting at <address>
void RAP_ReadBytes(byte address, byte * data, byte count)
{
  byte cmdByte = READ_MASK | address;   // Form the READ command byte

  SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE1));

  Assert_CS();
  SPI.transfer(cmdByte);  // Signal a RAP-read operation starting at <address>
  SPI.transfer(0xFC);     // Filler byte
  SPI.transfer(0xFC);     // Filler byte
  for(byte i = 0; i < count; i++)
  {
    data[i] =  SPI.transfer(0xFC);  // Each subsequent SPI transfer gets another register's contents
  }
  DeAssert_CS();

  SPI.endTransaction();
}

// Writes single-byte <data> to <address>
void RAP_Write(byte address, byte data)
{
  byte cmdByte = WRITE_MASK | address;  // Form the WRITE command byte

  SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE1));

  Assert_CS();
  SPI.transfer(cmdByte);  // Signal a write to register at <address>
  SPI.transfer(data);    // Send <value> to be written to register
  DeAssert_CS();

  SPI.endTransaction();
}

/*  Logical Scaling Functions */
// Clips raw coordinates to "reachable" window of sensor
// NOTE: values outside this window can only appear as a result of noise
void ClipCoordinates(absData_t * coordinates)
{
  if(coordinates->x_pos < PINNACLE_X_LOWER)
  {
    coordinates->x_pos = PINNACLE_X_LOWER;
  }
  else if(coordinates->x_pos > PINNACLE_X_UPPER)
  {
    coordinates->x_pos = PINNACLE_X_UPPER;
  }
  if(coordinates->y_pos < PINNACLE_Y_LOWER)
  {
    coordinates->y_pos = PINNACLE_Y_LOWER;
  }
  else if(coordinates->y_pos > PINNACLE_Y_UPPER)
  {
    coordinates->y_pos = PINNACLE_Y_UPPER;
  }
}

// Scales data to desired X & Y resolution
void ScaleData(absData_t * coordinates, uint16_t xResolution, uint16_t yResolution)
{
  uint32_t xTemp = 0;
  uint32_t yTemp = 0;

  ClipCoordinates(coordinates);

  xTemp = coordinates->x_pos;
  yTemp = coordinates->y_pos;

  // translate coordinates to (0, 0) reference by subtracting edge-offset
  xTemp -= PINNACLE_X_LOWER;
  yTemp -= PINNACLE_Y_LOWER;

  // scale coordinates to (xResolution, yResolution) range
  coordinates->x_pos = (uint16_t)(xTemp * xResolution / PINNACLE_X_RANGE);
  coordinates->y_pos = (uint16_t)(yTemp * yResolution / PINNACLE_Y_RANGE);
}

/*  I/O Functions */
void Assert_CS()
{
  digitalWrite(CS_PIN, LOW);
}

void DeAssert_CS()
{
  digitalWrite(CS_PIN, HIGH);
}

void AssertSensorLED(bool state)
{
  digitalWrite(LED_0, !state);
}

bool DR_Asserted()
{
  return digitalRead(DR_PIN);
}
