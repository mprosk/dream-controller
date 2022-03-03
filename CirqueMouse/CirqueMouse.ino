#include "pinnacle.h"

static absData_t touchData;

static uint16_t xpos_last = 0;
static uint16_t ypos_last = 0;
static touchState_t state_last = LIFTOFF;



// setup() gets called once at power-up, sets up serial debug output and Cirque's Pinnacle ASIC.
void setup()
{
  Serial.begin(115200);
//  while(!Serial); // needed for USB

  pinMode(LED_0, OUTPUT);

  Pinnacle_Init();

  Serial.println();
  Serial.println("X\tY\tZ\tBtn\tData");
  Pinnacle_EnableFeed(true);
}

// loop() continuously checks to see if data-ready (DR) is high. If so, reads and reports touch data to terminal.
void loop()
{
    if(DR_Asserted())
    {
        Pinnacle_GetAbsolute(&touchData);
        Pinnacle_CheckValidTouch(&touchData);     // Checks for "hover" caused by curved overlays

        //    ScaleData(&touchData, 1024, 1024);      // Scale coordinates to arbitrary X, Y resolution

        Serial.print(touchData.x_pos);
        Serial.print('\t');
        Serial.print(touchData.y_pos);
        Serial.print('\t');
        Serial.print(touchData.z_pos);
        Serial.print('\t');
        Serial.print(touchData.buttonFlags);
        Serial.print('\t');
        if(Pinnacle_zIdlePacket(&touchData))
        {
            Serial.println("liftoff");
            state_last = LIFTOFF;
        }
        else if(touchData.hovering)
        {
            Serial.println("hovering");
            state_last = HOVERING;
        }
        else
        {
            Serial.println("valid");

            // Update the mouse
            if (state_last != VALID)
            {
                // If we just touched, down record the current position
                xpos_last = touchData.x_pos;
                ypos_last = touchData.y_pos;
            }
            else
            {
                // Last state was also valid, calculate deltas
                int16_t x_delta = (touchData.x_pos >> 2) - (xpos_last >> 2); 
                int16_t y_delta = (touchData.y_pos >> 2) - (ypos_last >> 2); 
                Mouse.move(-x_delta, -y_delta);
                xpos_last = touchData.x_pos;
                ypos_last = touchData.y_pos;
            }

            state_last = VALID;

        }
    }
    AssertSensorLED(touchData.touchDown);
}

