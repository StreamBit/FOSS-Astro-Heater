/*
 * FOSS-Astro-Heater
 * 
 * Copyright (C) 2024 StreamBit <70787940+StreamBit@users.noreply.github.com>
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 * 
 * Description:
 * This is firmware for an ESP32-based heater system to monitor and control
 * a dew heater band for Astronomy.
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <OneWire.h>
#include <DallasTemperature.h>

////////////////////
// OLED SETUP
////////////////////
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1 // Use -1 when no reset pin is present

// First OLED at address 0x3C
Adafruit_SSD1306 display1(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Second OLED at address 0x3D
Adafruit_SSD1306 display2(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

////////////////////
// PINS
////////////////////
#define POT_PIN 34        // ADC input for target pot
#define ONE_WIRE_BUS 4    // DS18B20 data line
#define HEATER_PIN 26     // MOSFET gate pin
#define BUTTON_PIN 13     // Button pin to lock/unlock target

////////////////////
// DS18B20 SETUP
////////////////////
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

/////////////////////////////////////
// 30-MINUTE HISTORY
// (90 data points, one every 20s)
/////////////////////////////////////
// 90 points * 20s = 1800s = 30 minutes
static const int HISTORY_LEN = 90;
static float tempHistory[HISTORY_LEN];
static bool heaterHistory[HISTORY_LEN]; // track ON/OFF states
static int tempIndex = 0;

// We'll track time to store a new sample every 20 seconds
static unsigned long lastSampleTime = 0;
static const unsigned long SAMPLE_INTERVAL_MS = 20000; // 20s => 30 minutes total

// We need to track if the target temp is locked
static bool targetLocked = false;
// We'll store the "locked" target separately
static int lockedTarget = 0;

// For button debouncing
static bool lastButtonState = true;

////////////////////////////
// Draw centered text
// with black box behind
////////////////////////////
void drawTextCenterWithBox(Adafruit_SSD1306 &disp, int16_t y, const char *text, uint8_t size)
{
    disp.setTextSize(size);
    disp.setTextColor(WHITE);

    // Measure text bounds
    int16_t x1, y1;
    uint16_t w, h;
    disp.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

    // Compute centered x
    int16_t x = (SCREEN_WIDTH - w) / 2;

    // Draw a black rectangle behind text
    int padding = 2;
    disp.fillRect(x - padding, y - padding, w + 2*padding, h + 2*padding, BLACK);

    // Print text in white on top
    disp.setCursor(x, y);
    disp.print(text);
}

////////////////////
// STAR FIELD LOGIC
////////////////////
#define NUM_STARS 30
static int starX[NUM_STARS];
static int starY[NUM_STARS];

void generateStarField()
{
    for(int i = 0; i < NUM_STARS; i++) {
        starX[i] = random(0, SCREEN_WIDTH);
        starY[i] = random(0, SCREEN_HEIGHT);
    }
}

// Draw star field, then optionally draw the splash text at a vertical offset
void drawStarsAndSplash(Adafruit_SSD1306 &disp, bool drawSplash, int offset)
{
    disp.clearDisplay();

    // Draw the star field first
    for(int i = 0; i < NUM_STARS; i++) {
        disp.drawPixel(starX[i], starY[i], WHITE);
    }

    // Twinkle a bit (move some stars randomly)
    for(int i = 0; i < NUM_STARS; i++) {
        if(random(0, 8) == 0) {
            starX[i] = random(0, SCREEN_WIDTH);
            starY[i] = random(0, SCREEN_HEIGHT);
        }
    }

    // If we should draw the splash text
    if(drawSplash) {
        drawTextCenterWithBox(disp, 0 + offset, "FOSS", 1);
        drawTextCenterWithBox(disp, 16 + offset, "Astro", 2);
        drawTextCenterWithBox(disp, 40 + offset, "Heater", 2);
    }
}

void drawStarsAndSplash2(Adafruit_SSD1306 &disp, bool drawSplash, int offset)
{
    disp.clearDisplay();

    // Draw star field first
    for(int i = 0; i < NUM_STARS; i++) {
        disp.drawPixel(starX[i], starY[i], WHITE);
    }

    // Twinkle
    for(int i = 0; i < NUM_STARS; i++) {
        if(random(0, 8) == 0) {
            starX[i] = random(0, SCREEN_WIDTH);
            starY[i] = random(0, SCREEN_HEIGHT);
        }
    }

    if(drawSplash) {
        // For display2: top line "Version", bigger line "0.1"
        drawTextCenterWithBox(disp, 0 + offset, "Version", 1);
        drawTextCenterWithBox(disp, 20 + offset, "0.1", 3);
    }
}

////////////////////////////////////
// GRAPH CONSTANTS for Display #2
////////////////////////////////////

static const int GRAPH_LEFT = 20;     // left side for the y-axis
static const int GRAPH_RIGHT = 127;   // rightmost pixel
static const int GRAPH_TOP = 9;       // 1 px below the title
static const int GRAPH_BOTTOM = 59;   // 4 px above the bottom

float indexToX(int i)
{
    // i in [0..HISTORY_LEN-1]
    float fraction = (float)i / (float)(HISTORY_LEN - 1); // 0..1

    float usableWidth = (float)((GRAPH_RIGHT - (GRAPH_LEFT + 1)));
    float x = (GRAPH_LEFT + 1) + fraction * usableWidth;
    return x; // from ~21..127
}

// We'll define temp->y
int tempToY(float tempF)
{
    // clamp 0..100
    if(tempF < 0) tempF = 0;
    if(tempF > 100) tempF = 100;
    // map 0 => 59, 100 => 9
    float ratio = (100.0f - tempF) / 100.0f;
    float yRange = (float)(GRAPH_BOTTOM - GRAPH_TOP);
    float y = GRAPH_TOP + ratio * yRange;
    return (int)(y + 0.5f);
}

void setup()
{
    Serial.begin(115200);
    Serial.println("ESP32 setup complete.");

    pinMode(POT_PIN, INPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    sensors.begin();
    sensors.setResolution(9);

    Wire.begin();

    if(!display1.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("SSD1306 allocation failed on 0x3C");
        for(;;);
    }
    if(!display2.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
        Serial.println("SSD1306 allocation failed on 0x3D");
        for(;;);
    }

    randomSeed(analogRead(0));
    generateStarField();

    // ~3 seconds twinkling
    unsigned long twinkleStart = millis();
    while(millis() - twinkleStart < 3000) {
        drawStarsAndSplash(display1, false, 0);
        drawStarsAndSplash2(display2, false, 0);
        display1.display();
        display2.display();
        delay(150);
    }

    // Swipe up
    for(int offset = 64; offset >= 0; offset -= 4) {
        drawStarsAndSplash(display1, true, offset);
        drawStarsAndSplash2(display2, true, offset);
        display1.display();
        display2.display();
        delay(80);
    }

    // hold splash
    unsigned long splashStart = millis();
    while(millis() - splashStart < 5000) {
        drawStarsAndSplash(display1, true, 0);
        drawStarsAndSplash2(display2, true, 0);
        display1.display();
        display2.display();
        delay(150);
    }

    pinMode(HEATER_PIN, OUTPUT);
    digitalWrite(HEATER_PIN, LOW);

    // Clear after splash
    display1.clearDisplay();
    display1.display();
    display2.clearDisplay();
    display2.display();

    // init histories
    for(int i=0; i<HISTORY_LEN; i++) {
        tempHistory[i] = 0.0;
        heaterHistory[i] = false;
    }
    lastSampleTime = millis();
}

void loop()
{
    // Check button
    bool currentButtonState = digitalRead(BUTTON_PIN);
    if(lastButtonState == true && currentButtonState == false) {
        targetLocked = !targetLocked;
        Serial.print("Button pressed. targetLocked = ");
        Serial.println(targetLocked);
        delay(50);
    }
    lastButtonState = currentButtonState;

    // pot reading
    int potValue = analogRead(POT_PIN);
    int liveTarget = map(potValue, 0, 4095, 0, 100);
    if(!targetLocked) {
        lockedTarget = liveTarget;
    }
    int targetTemp = lockedTarget;

    // DS18B20
    sensors.requestTemperatures();
    float currentTemp = sensors.getTempFByIndex(0);

    // heater control
    bool heaterOn;
    if(currentTemp < targetTemp) {
        digitalWrite(HEATER_PIN, HIGH);
        heaterOn = true;
    } else {
        digitalWrite(HEATER_PIN, LOW);
        heaterOn = false;
    }

    // Serial
    Serial.print("Target: ");
    Serial.print(targetTemp);
    Serial.print(" | Current: ");
    Serial.print(currentTemp,1);
    if(heaterOn) {
        Serial.println(" -> MOSFET ON");
    } else {
        Serial.println(" -> MOSFET OFF");
    }

    // update history every 20s
    unsigned long now = millis();
    if((now - lastSampleTime) >= SAMPLE_INTERVAL_MS) {
        tempHistory[tempIndex] = currentTemp;
        heaterHistory[tempIndex] = heaterOn; // record ON/OFF state
        tempIndex = (tempIndex + 1) % HISTORY_LEN;
        lastSampleTime = now;
    }

    //////////////////////////////////
    // Display #1: target/current
    //////////////////////////////////
    display1.clearDisplay();
    // We'll keep your existing layout

    display1.drawLine(57, 0, 57, 64, WHITE);

    display1.setTextSize(1);
    display1.setTextColor(WHITE);
    display1.setCursor(0,0);
    display1.print("Target F");
    display1.setTextSize(2);
    display1.setCursor(0,16);
    display1.print(targetTemp);

    display1.setTextSize(1);
    display1.setCursor(67,0);
    display1.print("Current F");
    display1.setTextSize(2);
    display1.setCursor(67,16);
    if(currentTemp > -100.0) {
        display1.print(currentTemp,1);
    } else {
        display1.print("ERR");
    }

    // bottom row: lock/unlock on left
    display1.setTextSize(1);
    display1.setCursor(0,50);
    if(targetLocked) {
        display1.print("Locked");
    } else {
        display1.print("Unlocked");
    }

    display1.setCursor(67,50);
    if(heaterOn) {
        display1.print("Heater ON");
    } else {
        display1.print("Heater OFF");
    }

    display1.display();

    //////////////////////////////////
    // Display #2: 30min history
    //////////////////////////////////
    display2.clearDisplay();

    // 1) Draw updated Title
    display2.setTextSize(1);
    display2.setCursor(32, 0); // shift so it's centered-ish
    display2.print("Last 30 Min");

    // 2) Draw Y-axis from (GRAPH_LEFT, GRAPH_TOP)..(GRAPH_LEFT,GRAPH_BOTTOM)
    display2.drawLine(GRAPH_LEFT, GRAPH_TOP, GRAPH_LEFT, GRAPH_BOTTOM, WHITE);

    // 3) Label the scale: 20,40,60,80,100
    int labels[5] = {20,40,60,80,100};
    for(int i=0; i<5; i++) {
        int val = labels[i];
        int y = tempToY(val);
        // short horizontal tick from x=(GRAPH_LEFT-3)..(GRAPH_LEFT)
        display2.drawLine(GRAPH_LEFT-3, y, GRAPH_LEFT, y, WHITE);
        // print label at x=0
        display2.setCursor(0, y-3);
        display2.print(val);
    }

    // 4) Plot the temperature data (lines)
    // Now we have HISTORY_LEN=90, i=0..89

    // get the first data point as prevX, prevY
    int idx0 = (tempIndex + 0) % HISTORY_LEN;
    float t0 = tempHistory[idx0];
    int prevX = (int)indexToX(0);
    int prevY = tempToY(t0);

    for(int i=1; i<HISTORY_LEN; i++) {
        int idx = (tempIndex + i) % HISTORY_LEN;
        float t = tempHistory[idx];
        int x = (int)indexToX(i);
        int y = tempToY(t);
        display2.drawLine(prevX, prevY, x, y, WHITE);
        prevX = x;
        prevY = y;
    }

    // 5) Draw the heater bar at bottom for each data sample
    for(int i=0; i<HISTORY_LEN; i++) {
        int idx = (tempIndex + i) % HISTORY_LEN;
        bool on = heaterHistory[idx];
        int x = (int)indexToX(i);
        if(on) {
            for(int dy=61; dy<64; dy++) {
                display2.drawPixel(x, dy, WHITE);
            }
        }
    }

    display2.display();

    delay(250);
}
