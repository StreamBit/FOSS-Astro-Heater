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
 #include <WiFi.h>
 #include <WebServer.h>
 

 // ---------------------------------------------------------------------
 // WIFI CREDENTIALS - EDIT THESE
 // ---------------------------------------------------------------------
 const char* ssid     = "YOUR_SSID";
 const char* password = "YOUR_PASSWORD";
 
 // Create the WebServer on port 80
 WebServer server(80);
 
 // ---------------------------------------------------------------------
 // OLED SETUP
 // ---------------------------------------------------------------------
 #define SCREEN_WIDTH 128
 #define SCREEN_HEIGHT 64
 #define OLED_RESET    -1 // Use -1 if no reset pin
 
 // First OLED at address 0x3C
 Adafruit_SSD1306 display1(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
 // Second OLED at address 0x3D
 Adafruit_SSD1306 display2(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
 
 // ---------------------------------------------------------------------
 // PINS
 // ---------------------------------------------------------------------
 #define POT_PIN      34  // ADC input for target pot
 #define ONE_WIRE_BUS 4   // DS18B20 data line
 #define HEATER_PIN   26  // MOSFET gate
 #define BUTTON_PIN   13  // Button for lock/unlock
 
 // ---------------------------------------------------------------------
 // DS18B20 SETUP
 // ---------------------------------------------------------------------
 OneWire oneWire(ONE_WIRE_BUS);
 DallasTemperature sensors(&oneWire);
 
 // ---------------------------------------------------------------------
 // 6-HOUR HISTORY (1080 points => 6hr, 1 every 20s)
 // We'll only show last 30 min = 90 points on OLED #2
 // ---------------------------------------------------------------------
 static const int HISTORY_LEN       = 1080; 
 static const int OLED_HISTORY_LEN  = 90;   // last 30min portion
 
 static float tempHistory[HISTORY_LEN];
 static bool  heaterHistory[HISTORY_LEN];
 static int   tempIndex = 0;
 
 static unsigned long lastSampleTime = 0;
 static const unsigned long SAMPLE_INTERVAL_MS = 20000; // 20s
 
 // track locked vs. pot
 static bool targetLocked = false;
 static int  lockedTarget = 0;
 
 // For button debouncing
 static bool lastButtonState = true;
 
 // ---------------------------------------------------------------------
 // Helper: draw text centered with black box behind
 // ---------------------------------------------------------------------
 void drawTextCenterWithBox(Adafruit_SSD1306 &disp, int16_t y, const char *text, uint8_t size)
 {
   disp.setTextSize(size);
   disp.setTextColor(WHITE);
   int16_t x1, y1;
   uint16_t w, h;
   disp.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
 
   int16_t x = (SCREEN_WIDTH - w) / 2;
   int padding = 2;
   disp.fillRect(x - padding, y - padding, w + 2*padding, h + 2*padding, BLACK);
   disp.setCursor(x, y);
   disp.print(text);
 }
 
 // ---------------------------------------------------------------------
 // STAR FIELD
 // ---------------------------------------------------------------------
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
 
 void drawStarsAndSplash(Adafruit_SSD1306 &disp, bool drawSplash, int offset)
 {
   disp.clearDisplay();
   // Draw star field
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
   // Optional splash
   if(drawSplash) {
     drawTextCenterWithBox(disp, 0 + offset, "FOSS", 1);
     drawTextCenterWithBox(disp, 16 + offset, "Astro", 2);
     drawTextCenterWithBox(disp, 40 + offset, "Heater", 2);
   }
 }
 
 void drawStarsAndSplash2(Adafruit_SSD1306 &disp, bool drawSplash, int offset)
 {
   disp.clearDisplay();
   // Draw star field
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
     drawTextCenterWithBox(disp, 0 + offset, "Version", 1);
     drawTextCenterWithBox(disp, 20 + offset, "0.2", 3);
   }
 }
 
 // ---------------------------------------------------------------------
 // GRAPH CONSTANTS (for OLED #2, we only plot last 30 min = 90 points)
 // ---------------------------------------------------------------------
 static const int GRAPH_LEFT   = 20; 
 static const int GRAPH_RIGHT  = 127; 
 static const int GRAPH_TOP    = 9;  
 static const int GRAPH_BOTTOM = 59; 
 
 float oledIndexToX(int i, int totalPoints)
 {
   float fraction = (float)i / (float)(totalPoints - 1);
   float usableWidth = (float)(GRAPH_RIGHT - (GRAPH_LEFT + 1));
   float x = (GRAPH_LEFT + 1) + fraction * usableWidth;
   return x;
 }
 
 int oledTempToY(float tempF)
 {
   // clamp 0..100
   if(tempF < 0)   tempF = 0;
   if(tempF > 100) tempF = 100;
   float ratio = (100.0f - tempF) / 100.0f;
   float yRange = (float)(GRAPH_BOTTOM - GRAPH_TOP);
   float y = GRAPH_TOP + ratio * yRange;
   return (int)(y + 0.5f);
 }
 
 // ---------------------------------------------------------------------
 // WEB HANDLERS
 // ---------------------------------------------------------------------
 
 // Handler to set target temperature from the browser
 void handleSetTarget() {
   // If not locked, ignore or show message
   if(!targetLocked) {
     server.send(200, "text/plain", "System is UNLOCKED! Can't set target now.");
     return;
   }
   // If locked, parse "temp"
   if(server.hasArg("temp")) {
     int newTemp = server.arg("temp").toInt();
     // clamp 0..100
     if(newTemp < 0)   newTemp = 0;
     if(newTemp > 100) newTemp = 100;
 
     lockedTarget = newTemp;
     server.sendHeader("Location", "/");
     server.send(303);
   } else {
     server.send(400, "text/plain", "Missing 'temp' parameter!");
   }
 }
 
 // Serve main page with auto-refresh every 10s
 void handleRoot() 
 {
   float currentTemp = sensors.getTempFByIndex(0);
   bool heaterOn = (currentTemp < lockedTarget);
 
   String page = "<!DOCTYPE html><html><head>";
   page += "<meta charset='UTF-8'>";
   // Refresh every 10s
   page += "<meta http-equiv='refresh' content='10'>";
   page += "<title>Dew Heater</title></head><body>";
   page += "<h2>Dew Heater Monitor</h2>";
 
   // Target
   page += "<p><strong>Target Temp (F): </strong>";
   page += lockedTarget;
   page += "</p>";
 
   // Current
   page += "<p><strong>Current Temp (F): </strong>";
   if(currentTemp > -100.0) page += String(currentTemp,1);
   else                     page += "ERR";
   page += "</p>";
 
   // Lock status
   page += "<p><strong>Lock Status:</strong> ";
   if(targetLocked) {
     page += "Locked";
     // form to set target
     page += "<form action='/setTarget' method='GET'>";
     page += "<label for='temp'>Set Target (0-100):</label>";
     page += "<input type='number' name='temp' min='0' max='100' value='" + String(lockedTarget) + "'>";
     page += "<input type='submit' value='Set Target'>";
     page += "</form>";
   } else {
     page += "Unlocked (reading from pot)";
   }
   page += "</p>";
 
   // Heater status
   page += "<p><strong>Heater:</strong> ";
   page += (heaterOn ? "ON" : "OFF");
   page += "</p>";
 
   // Link to graph
   page += "<p><a href='/graph'>View 6hr Graph</a></p>";
 
   page += "</body></html>";
 
   server.send(200, "text/html", page);
 }
 
 // Serve JSON array of entire 6hr logs + lockedTarget
 void handleGraphData() {
   // We add "lockedTarget" so the JS can draw a green line for target temp
   String json = "{\"tempHistory\":[";
   for (int i = 0; i < HISTORY_LEN; i++) {
     int idx = (tempIndex + i) % HISTORY_LEN;
     json += String(tempHistory[idx], 2);
     if (i < HISTORY_LEN - 1) json += ",";
   }
   json += "],\"heaterHistory\":[";
   for (int i = 0; i < HISTORY_LEN; i++) {
     int idx = (tempIndex + i) % HISTORY_LEN;
     json += (heaterHistory[idx] ? "1":"0");
     if (i < HISTORY_LEN - 1) json += ",";
   }
   json += "],\"lockedTarget\":";
   json += lockedTarget;  // add the numeric target
   json += "}";
 
   server.send(200, "application/json", json);
 }
 
 void handleGraphPage() {
   String page = "<!DOCTYPE html><html><head>";
   page += "<meta charset='UTF-8'>";
   page += "<title>6hr Temp Graph</title></head><body>";
   page += "<h2>6-Hour Temperature Graph</h2>";
 
   // The canvas first
   page += "<canvas id='tempCanvas' width='800' height='400' style='border:1px solid #ccc;'></canvas>";
 
   // Then our <script>
   page += "<script>";
   // Log that script is loaded
   page += "console.log('Graph script loading...');"
 
           // Start the fetch
           "fetch('/graphData')"
           ".then(r => {"
           "  console.log('Fetch status:', r.status);"
           "  return r.json();"
           "})"
           ".then(data => {"
           "  console.log('Data received:', data);"
           "  const temps = data.tempHistory;"
           "  const heater = data.heaterHistory;"
           "  const target = data.lockedTarget;"
           "  const canvas = document.getElementById('tempCanvas');"
           "  const ctx = canvas.getContext('2d');"
           "  ctx.clearRect(0, 0, canvas.width, canvas.height);"
 
           "  const marginLeft=40, marginRight=20, marginTop=20, marginBottom=40;"
           "  const graphW = canvas.width - marginLeft - marginRight;"
           "  const graphH = canvas.height - marginTop - marginBottom;"
 
           "  const minTemp = 0;"
           "  const maxTemp = 100;"
           "  const numPoints = temps.length;"
           "  const xStep = graphW/(numPoints-1);"
 
           "  function getX(i){"
           "    return marginLeft + i*xStep;"
           "  }"
           "  function getY(t){"
           "    let ratio=(t - minTemp)/(maxTemp-minTemp);"
           "    return marginTop + (1-ratio)*graphH;"
           "  }"
 
           "// Horizontal lines T=0..100 step=20\n"
           "ctx.font='12px sans-serif';"
           "ctx.textAlign='center';"
           "ctx.textBaseline='middle';"
           "for(let t=0; t<=100; t+=20){"
           "  let y=getY(t);"
           "  ctx.strokeStyle='#ccc';"
           "  ctx.beginPath();"
           "  ctx.moveTo(marginLeft, y);"
           "  ctx.lineTo(marginLeft+graphW, y);"
           "  ctx.stroke();"
           "  ctx.fillStyle='#000';"
           "  ctx.fillText(t, marginLeft-15, y);"
           "}"
 
           "// Vertical lines every 15 min => 45 samples\n"
           "for(let minutes=0; minutes<=360; minutes+=15){"
           "  let i = minutes*3;"
           "  if(i<0 || i>numPoints-1) continue;"
           "  let x = getX(i);"
           "  ctx.strokeStyle='#ccc';"
           "  ctx.beginPath();"
           "  ctx.moveTo(x, marginTop);"
           "  ctx.lineTo(x, marginTop+graphH);"
           "  ctx.stroke();"
           "  if(minutes%60===0){"
           "    let hour = 6-(minutes/60);"
           "    ctx.fillStyle='#000';"
           "    ctx.fillText(hour+'h', x, marginTop+graphH+15);"
           "  }"
           "}"
 
           "// Axes\n"
           "ctx.strokeStyle='#000';"
           "ctx.beginPath();"
           "ctx.moveTo(marginLeft, marginTop);"
           "ctx.lineTo(marginLeft, marginTop+graphH);"
           "ctx.lineTo(marginLeft+graphW, marginTop+graphH);"
           "ctx.stroke();"
 
           "// Plot temperature (blue)\n"
           "ctx.strokeStyle='blue';"
           "ctx.beginPath();"
           "ctx.moveTo(getX(0), getY(temps[0]));"
           "for(let i=1;i<numPoints;i++){"
           "  ctx.lineTo(getX(i), getY(temps[i]));"
           "}"
           "ctx.stroke();"
 
           "// Heater ticks in red\n"
           "for(let i=0; i<numPoints; i++){"
           "  if(heater[i]==='1'||heater[i]===1){"
           "    let x = getX(i);"
           "    ctx.strokeStyle='red';"
           "    ctx.beginPath();"
           "    ctx.moveTo(x, marginTop+graphH);"
           "    ctx.lineTo(x, marginTop+graphH-10);"
           "    ctx.stroke();"
           "  }"
           "}"
 
           "// Target line in green\n"
           "ctx.strokeStyle='green';"
           "let ty=getY(target);"
           "ctx.beginPath();"
           "ctx.moveTo(marginLeft, ty);"
           "ctx.lineTo(marginLeft+graphW, ty);"
           "ctx.stroke();"
 
           "console.log('Plotting done!');"
           "})" // end .then(data=>{...})
           ".catch(err => {"
           "  console.error('Fetch error:', err);"
           "});";
 
   page += "</script>";
 
   // Legend below the canvas
   page += "<p style='margin-top:10px;'>";
   page += "<strong style='color:blue;'>Blue</strong> = Sensor Temperature, ";
   page += "<strong style='color:green;'>Green</strong> = Current Target, ";
   page += "<strong style='color:red;'>Red ticks</strong> = Heater ON";
   page += "</p>";
 
   page += "</body></html>";
 
   // Optional debug print to check final HTML in Serial
   // Serial.println(page);
 
   server.send(200, "text/html", page);
 }
 
 
 // ---------------------------------------------------------------------
 // SETUP
 // ---------------------------------------------------------------------
 void setup()
 {
   Serial.begin(115200);
 
   // 1) Initialize OLEDs first so we can show "Starting WiFi..." early
   if(!display1.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
     Serial.println("SSD1306 allocation failed on 0x3C");
     while(true);
   }
   if(!display2.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
     Serial.println("SSD1306 allocation failed on 0x3D");
     while(true);
   }
 
   display1.clearDisplay();
   display1.setTextSize(1);
   display1.setTextColor(WHITE);
   display1.setCursor(0, 0);
   display1.println("Starting WiFi...");
   display1.display();
 
   display2.clearDisplay();
   display2.setTextSize(1);
   display2.setTextColor(WHITE);
   display2.setCursor(0, 0);
   display2.println("Starting WiFi...");
   display2.display();
 
   // 2) Begin WiFi
   WiFi.mode(WIFI_STA);
   WiFi.begin(ssid, password);
 
   // Show "Connecting to WiFi..."
   display1.clearDisplay();
   display1.setCursor(0,0);
   display1.println("Connecting to WiFi...");
   display1.display();
 
   display2.clearDisplay();
   display2.setCursor(0,0);
   display2.println("Connecting to WiFi...");
   display2.display();
 
   Serial.println("Connecting to WiFi...");
   while (WiFi.status() != WL_CONNECTED) {
     delay(500);
     Serial.print(".");
   }
   Serial.println("\nWiFi connected!");
   Serial.print("IP address: ");
   Serial.println(WiFi.localIP());
 
   // 3) Start web server
   server.on("/",          HTTP_GET, handleRoot);
   server.on("/setTarget", HTTP_GET, handleSetTarget);
   server.on("/graphData", HTTP_GET, handleGraphData);
   server.on("/graph",     HTTP_GET, handleGraphPage);
   server.begin();
   Serial.println("Web server started on port 80");
 
   // 4) Initialize pins, sensors
   pinMode(POT_PIN, INPUT);
   pinMode(BUTTON_PIN, INPUT_PULLUP);
   pinMode(HEATER_PIN, OUTPUT);
   digitalWrite(HEATER_PIN, LOW);
 
   sensors.begin();
   sensors.setResolution(9);
   Wire.begin();
 
   // 5) Show IP + "Press button to continue..." on both
   display1.clearDisplay();
   display1.setTextSize(1);
   display1.setTextColor(WHITE);
   display1.setCursor(0, 0);
   display1.println("WiFi Connected!");
   display1.print("IP: ");
   display1.println(WiFi.localIP().toString());
   display1.println();
   display1.println("Press button");
   display1.println("to continue...");
   display1.display();
 
   display2.clearDisplay();
   display2.setTextSize(1);
   display2.setTextColor(WHITE);
   display2.setCursor(0, 0);
   display2.println("WiFi Connected!");
   display2.print("IP: ");
   display2.println(WiFi.localIP().toString());
   display2.println();
   display2.println("Press button");
   display2.println("to continue...");
   display2.display();
 
   Serial.println("Waiting for button press to continue...");
   bool wasPressed = false;
   while(true) {
     bool btn = digitalRead(BUTTON_PIN);
     if(!btn && !wasPressed) {
       wasPressed = true;   // pressed
     }
     else if(btn && wasPressed) {
       break;               // released
     }
     delay(50);
   }
   Serial.println("Button pressed! Continuing...");
 
   // 6) Star field splash
   randomSeed(analogRead(0));
   generateStarField();
 
   // Twinkle ~3s
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
   // hold splash ~5s
   unsigned long splashStart = millis();
   while(millis() - splashStart < 5000) {
     drawStarsAndSplash(display1, true, 0);
     drawStarsAndSplash2(display2, true, 0);
     display1.display();
     display2.display();
     delay(150);
   }
 
   // Clear after splash
   display1.clearDisplay();
   display1.display();
   display2.clearDisplay();
   display2.display();
 
   // Init 6-hr histories
   for(int i = 0; i < HISTORY_LEN; i++){
     tempHistory[i] = 0.0;
     heaterHistory[i] = false;
   }
   tempIndex = 0;
   lastSampleTime = millis();
 
   Serial.println("ESP32 setup complete.");
 }
 
 // ---------------------------------------------------------------------
 // LOOP
 // ---------------------------------------------------------------------
 void loop()
 {
   // Handle web requests
   server.handleClient();
 
   // Check button for local toggling of lock
   bool currentButtonState = digitalRead(BUTTON_PIN);
   if(lastButtonState && !currentButtonState) {
     targetLocked = !targetLocked;
     Serial.printf("Button pressed -> targetLocked=%d\n", targetLocked);
     delay(50);
   }
   lastButtonState = currentButtonState;
 
   // If unlocked, read from pot
   if(!targetLocked) {
     int potValue = analogRead(POT_PIN);
     int liveTarget = map(potValue, 0, 4095, 0, 100);
     lockedTarget = liveTarget;
   }
 
   int targetTemp = lockedTarget;
 
   // DS18B20
   sensors.requestTemperatures();
   float currentTemp = sensors.getTempFByIndex(0);
 
   // Heater control
   bool heaterOn = (currentTemp < targetTemp);
   digitalWrite(HEATER_PIN, heaterOn ? HIGH : LOW);
 
   // Debug print
   Serial.print("Target: ");
   Serial.print(targetTemp);
   Serial.print(" | Current: ");
   Serial.print(currentTemp,1);
   Serial.println(heaterOn ? " -> MOSFET ON" : " -> MOSFET OFF");
 
   // Update 6-hr history
   unsigned long now = millis();
   if((now - lastSampleTime) >= SAMPLE_INTERVAL_MS) {
     tempHistory[tempIndex] = currentTemp;
     heaterHistory[tempIndex] = heaterOn;
     tempIndex = (tempIndex + 1) % HISTORY_LEN;
     lastSampleTime = now;
   }
 
   // -----------------------
   // OLED #1: Show target/current
   // -----------------------
   display1.clearDisplay();
   display1.drawLine(57, 0, 57, 64, WHITE);
 
   display1.setTextSize(1);
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
 
   display1.setTextSize(1);
   display1.setCursor(0,50);
   display1.print(targetLocked ? "Locked" : "Unlocked");
 
   display1.setCursor(67,50);
   display1.print(heaterOn ? "Heater ON" : "Heater OFF");
   display1.display();
 
   // -----------------------
   // OLED #2: last 30 min
   // -----------------------
   display2.clearDisplay();
   display2.setTextSize(1);
   display2.setCursor(20, 0);
   display2.print("Last 30 Min");
 
   // Y-axis
   display2.drawLine(GRAPH_LEFT, GRAPH_TOP, GRAPH_LEFT, GRAPH_BOTTOM, WHITE);
 
   // Label scale: 20,40,60,80,100
   int labels[5] = {20,40,60,80,100};
   for(int i=0; i<5; i++) {
     int val = labels[i];
     int y = oledTempToY(val);
     // short tick
     display2.drawLine(GRAPH_LEFT-3, y, GRAPH_LEFT, y, WHITE);
     // label
     display2.setCursor(0, y-3);
     display2.print(val);
   }
 
   // Plot last 90 points
   int idx0 = (tempIndex - OLED_HISTORY_LEN + HISTORY_LEN) % HISTORY_LEN;
   float t0 = tempHistory[idx0];
   float prevX = oledIndexToX(0, OLED_HISTORY_LEN);
   int   prevY = oledTempToY(t0);
 
   for(int i=1; i<OLED_HISTORY_LEN; i++){
     int idx = (tempIndex - OLED_HISTORY_LEN + i + HISTORY_LEN) % HISTORY_LEN;
     float t = tempHistory[idx];
     float x = oledIndexToX(i, OLED_HISTORY_LEN);
     int   y = oledTempToY(t);
     display2.drawLine((int)prevX, prevY, (int)x, y, WHITE);
     prevX = x;
     prevY = y;
   }
 
   // Heater bar at bottom
   for(int i=0; i<OLED_HISTORY_LEN; i++) {
     int idx = (tempIndex - OLED_HISTORY_LEN + i + HISTORY_LEN) % HISTORY_LEN;
     bool on = heaterHistory[idx];
     float x = oledIndexToX(i, OLED_HISTORY_LEN);
     if(on) {
       for(int dy=61; dy<64; dy++) {
         display2.drawPixel((int)x, dy, WHITE);
       }
     }
   }
   display2.display();
 
   delay(250); 
 }