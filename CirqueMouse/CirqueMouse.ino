#include "pinnacle.h"

#define DAC_MAX (4095)
#define DAC_HALF  (DAC_MAX >> 1)

#define PIN_DAC_LATCH (2)
#define PIN_DAC_CS    (14)
#define PIN_LED_ASSIGN (4)
#define PIN_LED_TURBO  (3)
#define PIN_MODE_BTN   (21)

static absData_t touchData;

static uint16_t xpos_last = 0;
static uint16_t ypos_last = 0;
static touchState_t state_last = LIFTOFF;

static bool mouse_only = false;
static uint8_t mode_last = 0;


void setup()
{
    Serial.begin(115200);

    // Init GPIO
    pinMode(PIN_MODE_BTN, INPUT_PULLUP);
    pinMode(PIN_LED_ASSIGN, OUTPUT);
    pinMode(PIN_LED_TURBO, OUTPUT);
    mode_last = digitalRead(PIN_MODE_BTN);

    // Init DAC
    pinMode(PIN_DAC_LATCH, OUTPUT);
    pinMode(PIN_DAC_CS, OUTPUT);
    digitalWrite(PIN_DAC_LATCH, HIGH);
    digitalWrite(PIN_DAC_CS, HIGH);
    SPI1.begin();
    SPI1.beginTransaction(SPISettings(1e6, MSBFIRST, SPI_MODE0));

    // Init Touch Sensor
    Pinnacle_Init();
  
    Serial.println();
    Serial.println("X\tY\tData");
    Pinnacle_EnableFeed(true);
}

// loop() continuously checks to see if data-ready (DR) is high. If so, reads and reports touch data to terminal.
void loop()
{
    // Check mode
    uint8_t mode = digitalRead(PIN_MODE_BTN);
    if ((mode != mode_last) && (mode == LOW))
    {
        mouse_only = !mouse_only;
        digitalWrite(PIN_LED_TURBO, mouse_only);
        if (mouse_only)
        {
            // Put the DAC in the middle
            dac_set_xy(DAC_HALF, DAC_HALF);
        }
    }
    mode_last = mode;

    // Process touch data
    if(DR_Asserted())
    {
        Pinnacle_GetAbsolute(&touchData);
        Pinnacle_CheckValidTouch(&touchData);     // Checks for "hover" caused by curved overlays

        //    ScaleData(&touchData, 1024, 1024);      // Scale coordinates to arbitrary X, Y resolution

        Serial.print(touchData.x_pos);
        Serial.print('\t');
        Serial.print(touchData.y_pos);
        Serial.print('\t');
        if(Pinnacle_zIdlePacket(&touchData) || touchData.hovering)
        {
            if (touchData.hovering)
            {
                Serial.println("hovering");
                state_last = HOVERING;
            }
            else
            {
                Serial.println("liftoff");
                state_last = LIFTOFF;
            }
            
            // Put the DAC in the middle
            dac_set_xy(DAC_HALF, DAC_HALF);
        }
        else
        {
            Serial.println("valid");

            // Update the DAC
            uint16_t dac_x = map(touchData.x_pos, PINNACLE_X_LOWER, PINNACLE_X_UPPER, DAC_MAX, 0);
            uint16_t dac_y = map(touchData.y_pos, PINNACLE_Y_LOWER, PINNACLE_Y_UPPER, DAC_MAX, 0);
            Serial.print("DAC: "); Serial.print(dac_x); Serial.print(" "); Serial.println(dac_y);
            if (!mouse_only)
            {
                dac_set_xy(dac_y, dac_x);
            }

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
                Mouse.move((y_delta << 1), -(x_delta << 1));
                xpos_last = touchData.x_pos;
                ypos_last = touchData.y_pos;
            }

            state_last = VALID;

        }
        
    }
    digitalWrite(PIN_LED_ASSIGN, touchData.touchDown);
}

void dac_set_xy(uint16_t xval, uint16_t yval)
{
    // Write X value to A channel
    digitalWrite(PIN_DAC_CS, LOW);
    SPI1.transfer(0b01110000 | (xval >> 8));
    SPI1.transfer(xval & 0xFF);
    digitalWrite(PIN_DAC_CS, HIGH);
    digitalWrite(PIN_DAC_LATCH, LOW);
    digitalWrite(PIN_DAC_LATCH, HIGH);

    // Write Y value to B channel
    digitalWrite(PIN_DAC_CS, LOW);
    SPI1.transfer(0b11110000 | (yval >> 8));
    SPI1.transfer(yval & 0xFF);
    digitalWrite(PIN_DAC_CS, HIGH);
    digitalWrite(PIN_DAC_LATCH, LOW);
    digitalWrite(PIN_DAC_LATCH, HIGH);
}
