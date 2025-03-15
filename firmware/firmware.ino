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
 #include <Adafruit_SHT31.h>
 #include <WiFi.h>
 #include <WebServer.h>
 

 // WIFI CREDENTIALS - EDIT THESE 
 const char* ssid     = "YOUR_SSID";
 const char* password = "YOUR_PASSWORD";
 

 // Create the WebServer on port 80
 WebServer server(80);
 
 // OLED SETUP
 #define SCREEN_WIDTH 128
 #define SCREEN_HEIGHT 64
 #define OLED_RESET    -1 // Use -1 if no reset pin
 
 // First OLED at address 0x3C (shows main status)
 Adafruit_SSD1306 display1(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
 // Second OLED at address 0x3D (shows the graph/history)
 Adafruit_SSD1306 display2(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
 
 // PINS
 #define POT_PIN      34  // ADC input from potentiometer (for live target temperature)
 #define ONE_WIRE_BUS 4   // DS18B20 data line (temperature sensor)
 #define HEATER_PIN   26  // Output to control MOSFET that switches the heater
 #define BUTTON_1   13  // Button used to toggle lock/unlock mode
 #define BUTTON_2 12 // Used to toggle auto mode
 
 // DS18B20 SETUP
 OneWire oneWire(ONE_WIRE_BUS);            // Initialize OneWire on the defined pin
 DallasTemperature sensors(&oneWire);      // Pass OneWire to the DallasTemperature library
 
 // 6-HOUR HISTORY (1080 points => 6hr, 1 every 20s)
 static const int HISTORY_LEN       = 1080; 
 static const int OLED_HISTORY_LEN  = 90;   // Last 30 minutes
 
 static float tempHistory[HISTORY_LEN];    // Array to store temperature readings
 static bool  heaterHistory[HISTORY_LEN];  // Array to store heater state (on/off)
 static int   tempIndex = 0;               // Index pointer for the history arrays
 
 static unsigned long lastSampleTime = 0;  // Timestamp of the last sample taken
 static const unsigned long SAMPLE_INTERVAL_MS = 20000; // 20 seconds sampling interval
 
 // Track if the target temperature is locked or is taken live from the potentiometer
 static bool targetLocked = false;
 static int  lockedTarget = 0; // Current target temperature (when locked)
 
 // For button debouncing (to detect state changes)
 static bool lastButtonState = true;
 
 // Object for the SHT30 sensor that reads ambient temperature and humidity.
 Adafruit_SHT31 sht3x = Adafruit_SHT31();
 
 // Variables to store ambient temperature and humidity for the webpage.
 static float ambientTempF = -999.0;
 static float ambientHumidity = -999.0;
 static float dewPointF = -999.0;
 
 // --- Auto Mode globals ---
 static bool autoMode = false;         // Flag to track auto mode
 static bool autoButtonLastState = HIGH; // Previous state of the auto button
 
 // For enabling/disabling WiFi at start
 bool enableWifi = false;
 
 

 // Draw text center with a box on the OLED display
 void drawTextCenterWithBox(Adafruit_SSD1306 &disp, int16_t y, const char *text, uint8_t size)
 {
   disp.setTextSize(size);
   disp.setTextColor(WHITE);
   int16_t x1, y1;
   uint16_t w, h;
   // Get dimensions of the text to center it
   disp.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
 
   int16_t x = (SCREEN_WIDTH - w) / 2;
   int padding = 2;
   // Draw a background box behind the text
   disp.fillRect(x - padding, y - padding, w + 2*padding, h + 2*padding, BLACK);
   disp.setCursor(x, y);
   disp.print(text);
 }
 


 // STAR FIELD (for splash animation)
 #define NUM_STARS 30
 static int starX[NUM_STARS]; // X coordinates for stars
 static int starY[NUM_STARS]; // Y coordinates for stars
 
 // Generate random positions for stars on the display
 void generateStarField() {
   // Use the ESP32's hardware random function for a better seed
   randomSeed(esp_random());
   for (int i = 0; i < NUM_STARS; i++) {
     starX[i] = random(0, SCREEN_WIDTH);
     starY[i] = random(0, SCREEN_HEIGHT);
   }
 }
 
 

 // Draw star field and splash text on a display
 void drawStarsAndSplash(Adafruit_SSD1306 &disp, bool drawSplash, int offset)
 {
   disp.clearDisplay();
   // Draw each star as a pixel
   for(int i = 0; i < NUM_STARS; i++) {
     disp.drawPixel(starX[i], starY[i], WHITE);
   }
   // Randomly reposition some stars to create a twinkle effect
   for(int i = 0; i < NUM_STARS; i++) {
     if(random(0, 8) == 0) {
       starX[i] = random(0, SCREEN_WIDTH);
       starY[i] = random(0, SCREEN_HEIGHT);
     }
   }
   // If drawSplash is true, print the splash text with a vertical offset
   if(drawSplash) {
     drawTextCenterWithBox(disp, offset+0, "FOSS", 1);
     drawTextCenterWithBox(disp, offset+16, "Astro", 2);
     drawTextCenterWithBox(disp, offset+40, "Heater", 2);
   }
 }
 


 // Second OLED splash function
 void drawStarsAndSplash2(Adafruit_SSD1306 &disp, bool drawSplash, int offset)
 {
   disp.clearDisplay();
   for(int i = 0; i < NUM_STARS; i++) {
     disp.drawPixel(starX[i], starY[i], WHITE);
   }
   for(int i = 0; i < NUM_STARS; i++) {
     if(random(0, 8) == 0) {
       starX[i] = random(0, SCREEN_WIDTH);
       starY[i] = random(0, SCREEN_HEIGHT);
     }
   }
   if(drawSplash) {
     drawTextCenterWithBox(disp, offset+0, "Version", 1);
     drawTextCenterWithBox(disp, offset+20, "0.4", 3);
   }
 }
 


 // GRAPH CONSTANTS (for OLED #2: graphing the last 30 min data)
 static const int GRAPH_LEFT   = 20; 
 static const int GRAPH_RIGHT  = 127; 
 static const int GRAPH_TOP    = 9;  
 static const int GRAPH_BOTTOM = 59; 
 
 // Map a history index to an X coordinate on the OLED graph
 float oledIndexToX(int i, int totalPoints)
 {
   float fraction = (float)i / (float)(totalPoints - 1);
   float usableWidth = (float)(GRAPH_RIGHT - (GRAPH_LEFT + 1));
   float x = (GRAPH_LEFT + 1) + fraction * usableWidth;
   return x;
 }
 
 // Convert a temperature value (in °F) to a Y coordinate on the graph
 int oledTempToY(float tempF)
 {
   if(tempF < 0)   tempF = 0;
   if(tempF > 100) tempF = 100;
   float ratio = (100.0f - tempF) / 100.0f;
   float yRange = (float)(GRAPH_BOTTOM - GRAPH_TOP);
   float y = GRAPH_TOP + ratio * yRange;
   return (int)(y + 0.5f);
 }
 


 // WEB HANDLERS
 
 // Handler to set the target temperature from the browser
 void handleSetTarget() {
   // Only allow changes if the target is locked (fixed)
   if(!targetLocked) {
     server.send(200, "text/plain", "System is UNLOCKED! Can't set target now.");
     return;
   }
   if(server.hasArg("temp")) {
     int newTemp = server.arg("temp").toInt();
     if(newTemp < 0)   newTemp = 0;
     if(newTemp > 100) newTemp = 100;
 
     lockedTarget = newTemp; // Update the locked target temperature
     server.sendHeader("Location", "/"); // Redirect to the main page
     server.send(303);
   } else {
     server.send(400, "text/plain", "Missing 'temp' parameter!");
   }
 }

 // Main webpage handler: Displays system status and sensor readings.
 void handleRoot() 
 {
   float currentTempF = sensors.getTempFByIndex(0);  // Read DS18B20 temperature
   bool heaterOn = (currentTempF < lockedTarget);      // Heater on if current temp is below target
 
   // Build HTML content as a string
   String page = "<!DOCTYPE html><html><head>";
   page += "<meta charset='UTF-8'>";
   page += "<meta http-equiv='refresh' content='10'>"; // Auto-refresh every 10 seconds
   page += "<title>Dew Heater</title></head><body>";
   page += "<h2>Dew Heater Monitor</h2>";
 
   // Display the target temperature
   page += "<p><strong>Heater Target (F): </strong>";
   page += lockedTarget;
   page += "</p>";
 
   // Display the DS18B20 temperature reading
   page += "<p><strong>DS18B20 Temp (F): </strong>";
   if(currentTempF > -100.0) page += String(currentTempF,1);
   else                      page += "ERR";
   page += "</p>";
 
   // Display lock status and, if locked, a form to change the target temperature and a button to engage auto mode
   page += "<p><strong>Lock Status:</strong> ";
   if(targetLocked) {
     page += "Locked";
     // Only display the set temperature form if auto mode is NOT active
     if(!autoMode) {
       page += "<form action='/setTarget' method='GET'>";
       page += "<label for='temp'>Set Target (0-100):</label>";
       page += "<input type='number' name='temp' min='0' max='100' value='" + String(lockedTarget) + "'>";
       page += "<input type='submit' value='Set Target'>";
       page += "</form>";
     }
     
     // Auto mode toggle button is always available when locked
     page += "<form action='/toggleAuto' method='GET' style='margin-top:10px;'>";
     page += "<input type='submit' value='Toggle Auto Mode'>";
     page += "</form>";
   } else {
     page += "Unlocked (reading from pot)";
   }
   page += "</p>";
 
 
   // Display heater status
   page += "<p><strong>Heater:</strong> ";
   page += (heaterOn ? "ON" : "OFF");
   page += "</p>";
 
   // --- Show SHT30 readings (ambient temperature, humidity, and dew point)
   page += "<p><strong>Ambient Temp (F):</strong> ";
   if(ambientTempF > -200.0) {
     page += String(ambientTempF, 1);
   } else {
     page += "ERR";
   }
   page += "<br><strong>Humidity (%):</strong> ";
   if(ambientHumidity >= 0.0) {
     page += String(ambientHumidity, 1);
   } else {
     page += "ERR";
   }
   page += "<br><strong>Dew Point (F):</strong> ";
   if(dewPointF > -200.0) {
     page += String(dewPointF, 1);
   } else {
     page += "ERR";
   }
   page += "</p>";
 
 
   // Link to the detailed 6-hour graph page
   page += "<p><a href='/graph'>View 6hr Graph</a></p>";
 
   page += "</body></html>";
 
   // Send the HTML page to the browser
   server.send(200, "text/html", page);
 }
 
 // Handler to serve JSON data containing the 6-hour temperature and heater logs
 void handleGraphData() {
   String json = "{\"tempHistory\":[";
   // Build JSON array for temperature history
   for (int i = 0; i < HISTORY_LEN; i++) {
     int idx = (tempIndex + i) % HISTORY_LEN;
     json += String(tempHistory[idx], 2);
     if (i < HISTORY_LEN - 1) json += ",";
   }
   json += "],\"heaterHistory\":[";
   // Build JSON array for heater on/off history (1 = on, 0 = off)
   for (int i = 0; i < HISTORY_LEN; i++) {
     int idx = (tempIndex + i) % HISTORY_LEN;
     json += (heaterHistory[idx] ? "1":"0");
     if (i < HISTORY_LEN - 1) json += ",";
   }
   json += "],\"lockedTarget\":";
   json += lockedTarget;  
   json += "}";
 
   // Send the JSON data with the appropriate MIME type
   server.send(200, "application/json", json);
 }
 
 // Handler to serve an HTML page with a canvas that displays the 6-hour graph
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
 
 // Handler to toggle auto mode from the webpage
 void handleToggleAuto() {
   // Only allow toggling auto mode if the system is locked
   if(!targetLocked) {
     server.send(200, "text/plain", "System is UNLOCKED! Cannot toggle auto mode.");
     return;
   }
   autoMode = !autoMode;  // Toggle auto mode on/off
   Serial.printf("Web toggled auto mode -> %d\n", autoMode);
   server.sendHeader("Location", "/"); // Redirect back to the main page
   server.send(303);
 }
 
 

 // SETUP
 void setup()
 {
   Serial.begin(115200);  // Start serial communication for debugging
 
   // 1) Initialize both OLED displays
   if(!display1.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
     Serial.println("SSD1306 allocation failed on 0x3C");
     while(true); // Stop execution if display initialization fails
   }
   if(!display2.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
     Serial.println("SSD1306 allocation failed on 0x3D");
     while(true);
   }
 
   // WiFi Selection
   pinMode(BUTTON_1, INPUT_PULLUP);
   pinMode(BUTTON_2, INPUT_PULLUP);
 
   // Display the WiFi toggle prompt on the OLED
   display1.clearDisplay();
   display2.clearDisplay();
   display1.setTextSize(1);
   display1.setTextColor(WHITE);
   display1.setCursor(0, 0);
   display1.println("Enable WiFi?");
   display1.println("\nYES: Button 1");
   display1.println("\nNO : Button 2");
   display1.display();
   display2.display();
 
   // Wait for user input
   while (true) {
     if (digitalRead(BUTTON_1) == LOW) {
       enableWifi = true;
       break;
     }
     if (digitalRead(BUTTON_2) == LOW) {
       enableWifi = false;
       break;
     }
     delay(50);
   }
 
   // Show message based on the selection
   display1.clearDisplay();
   display2.clearDisplay();
   display1.setCursor(0, 0);
   if (enableWifi) {
     display1.println("WiFi Enabled");
   } else {
     display1.println("WiFi Disabled");
   }
   display1.display();
   delay(1000);
 
 
   if (enableWifi) {
     // Display an initial message on both OLEDs
     display1.clearDisplay();
     display1.setTextSize(1);
     display1.setTextColor(WHITE);
     display1.setCursor(0, 0);
     display1.println("Starting WiFi...");
     display1.display();
 
     // 2) Begin WiFi connection in station mode
     WiFi.mode(WIFI_STA);
     WiFi.begin(ssid, password);
 
     // Update OLEDs to indicate WiFi connection in progress
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
 
     // Display WiFi connection info and instructions on both OLEDs
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
     // Wait for a button press (press and release) before continuing
     bool wasPressed = false;
     while(true) {
       bool btn = digitalRead(BUTTON_1);
       if(!btn && !wasPressed) {
         wasPressed = true;   // Button pressed
       }
       else if(btn && wasPressed) {
         break;               // Button released -> proceed
       }
       delay(50);
     }
     Serial.println("Button pressed! Continuing...");
 
     // 3) Start the web server and register handlers for each route
     server.on("/",          HTTP_GET, handleRoot);
     server.on("/setTarget", HTTP_GET, handleSetTarget);
     server.on("/toggleAuto", HTTP_GET, handleToggleAuto);
     server.on("/graphData", HTTP_GET, handleGraphData);
     server.on("/graph",     HTTP_GET, handleGraphPage);
     server.begin();
     Serial.println("Web server started on port 80");
   }
 
   // 4) Initialize pins and sensors
   pinMode(POT_PIN, INPUT);            // Potentiometer for live target temp
   pinMode(BUTTON_1, INPUT_PULLUP);    // Button for toggling lock (using internal pull-up)
   pinMode(HEATER_PIN, OUTPUT);          // Output pin to control the heater
   digitalWrite(HEATER_PIN, LOW);        // Initialize heater OFF
   pinMode(BUTTON_2, INPUT_PULLUP);  // Use the internal pull-up resistor
 
   sensors.begin();                    // Initialize DS18B20 sensor
   sensors.setResolution(9);           // Set DS18B20 resolution to 9-bit
 
   Wire.begin(); // Start I2C for OLEDs and SHT30
 
   // Try to initialize the SHT30 sensor at its default address (0x44); if it fails, try 0x45.
   if(!sht3x.begin(0x44)) { 
     Serial.println("SHT30 not found at 0x44? Trying 0x45...");
     if(!sht3x.begin(0x45)) {
       Serial.println("SHT30 not found at 0x45 either. Check wiring!");
     }
   }
 
   // 6) Splash screen: Star field animation and swipe-up effect
   randomSeed(analogRead(0)); // Seed random number generator
   generateStarField();
 
   // Display a twinkling star field for ~3 seconds
   unsigned long twinkleStart = millis();
   while(millis() - twinkleStart < 3000) {
     drawStarsAndSplash(display1, false, 0);
     drawStarsAndSplash2(display2, false, 0);
     display1.display();
     display2.display();
     delay(150);
   }
   // Swipe-up animation: gradually slide the splash text into view
   for(int offset = 64; offset >= 0; offset -= 4) {
     drawStarsAndSplash(display1, true, offset);
     drawStarsAndSplash2(display2, true, offset);
     display1.display();
     display2.display();
     delay(80);
   }
   // Hold the splash screen for ~5 seconds
   unsigned long splashStart = millis();
   while(millis() - splashStart < 5000) {
     drawStarsAndSplash(display1, true, 0);
     drawStarsAndSplash2(display2, true, 0);
     display1.display();
     display2.display();
     delay(150);
   }
 
   // Clear displays after splash animation
   display1.clearDisplay();
   display1.display();
   display2.clearDisplay();
   display2.display();
 
   // Initialize the 6-hour history arrays to zero/out-of-range values
   for(int i = 0; i < HISTORY_LEN; i++){
     tempHistory[i] = 0.0;
     heaterHistory[i] = false;
   }
   tempIndex = 0;
   lastSampleTime = millis();
 
   Serial.println("ESP32 setup complete.");
 }
 


 // LOOP
 void loop()
 {
   // Process any incoming web requests
   server.handleClient();
 
   // Check the button state to toggle the lock status for the target temperature
   bool currentButtonState = digitalRead(BUTTON_1);
   if(lastButtonState && !currentButtonState) { // Button press detected (transition HIGH to LOW)
     targetLocked = !targetLocked; // Toggle between locked and unlocked modes
     Serial.printf("Button pressed -> targetLocked=%d\n", targetLocked);
     delay(50); // Debounce delay
   }
   lastButtonState = currentButtonState;
 
   // When unlocked, update the target temperature from the potentiometer reading
   if(!targetLocked) {
     int potValue = analogRead(POT_PIN);
     int liveTarget = map(potValue, 0, 4095, 0, 100);  // Map ADC value to a range of 0-100°F
     lockedTarget = liveTarget;
   }
 
   // Request and read the DS18B20 temperature sensor
   sensors.requestTemperatures();
   float currentTempF = sensors.getTempFByIndex(0);
 
   // Control the heater: turn it ON if the current temperature is below the target temperature
   bool heaterOn = (currentTempF < lockedTarget);
   digitalWrite(HEATER_PIN, heaterOn ? HIGH : LOW);
 
   // --- SHT30 read and Dew Point Calculation ---
   float tC = sht3x.readTemperature();
   float h  = sht3x.readHumidity();
   if(!isnan(tC) && !isnan(h)) {
     ambientTempF = tC * 1.8f + 32.0f;  // Convert Celsius to Fahrenheit
     ambientHumidity = h;
     
     // Calculate dew point in Celsius using the approximation formula:
     //   α = ln(RH/100) + (a*T)/(b+T)
     //   Td = (b * α) / (a - α)
     // where a = 17.27 and b = 237.7
     float a = 17.27;
     float b = 237.7;
     float alpha = ((a * tC) / (b + tC)) + log(h / 100.0);
     float dewPointC = (b * alpha) / (a - alpha);
     
     // Convert dew point from Celsius to Fahrenheit
     dewPointF = dewPointC * 1.8f + 32.0f;
   } else {
     ambientTempF = -999.0;
     ambientHumidity = -1.0;
     dewPointF = -999.0;
   }
 

   // --- AUTO MODE BUTTON HANDLING ---
   // Read the auto button state
   bool currentAutoButtonState = digitalRead(BUTTON_2);
   if (autoButtonLastState == HIGH && currentAutoButtonState == LOW) {
     // Button press detected (transition HIGH -> LOW)
     // Only toggle auto mode if the system is locked
     if (targetLocked) {
       autoMode = !autoMode;  // Toggle auto mode on/off
       Serial.printf("Auto mode toggled -> %d\n", autoMode);
       delay(50); // Simple debounce delay
     }
   }
   autoButtonLastState = currentAutoButtonState;
 
   // Turn auto mode off if the system is unlocked (pot active)
   if (!targetLocked) {
     autoMode = false;
   }
 
   // If auto mode is active and the dew point is valid, update the target temperature
   if (autoMode && dewPointF > -200.0) {
     lockedTarget = (int)ceil(dewPointF + 3);
   }
 
   // Print debug info to the Serial Monitor
   Serial.print("Target Temp: ");
   Serial.print(lockedTarget);
   Serial.print(" | Scope Temp: ");
   Serial.print(currentTempF, 1);
   if(heaterOn) Serial.print(" -> Heater ON");
   else         Serial.print(" -> Heater OFF");
   Serial.print(" | Ambient Temp (F): ");
   Serial.print(ambientTempF,1);
   Serial.print(" | Ambient Humidity: ");
   Serial.println(ambientHumidity,1);
   Serial.print(" | Dew Point (F): ");
   Serial.println(dewPointF, 1);
 
   // Update the 6-hour history arrays every SAMPLE_INTERVAL_MS (20 seconds)
   unsigned long now = millis();
   if((now - lastSampleTime) >= SAMPLE_INTERVAL_MS) {
     tempHistory[tempIndex] = currentTempF;
     heaterHistory[tempIndex] = heaterOn;
     tempIndex = (tempIndex + 1) % HISTORY_LEN;  // Use a circular buffer
     lastSampleTime = now;
   }
 

   // OLED #1: Display the target and current DS18B20 temperature,
   // plus the lock status, auto mode indicator, and heater state.
   display1.clearDisplay();
   // Draw a vertical divider between the left (target) and right (current) sections
   display1.drawLine(59, 0, 59, 33, WHITE);
   display1.drawLine(0, 34, 128, 34, WHITE);
 
   display1.setTextSize(1);
   display1.setCursor(2, 0);
   display1.print("Target F");
   display1.setTextSize(2);
   display1.setCursor(2, 16);
   display1.print(lockedTarget);
 
   // Dew Point (above lock status)
   display1.setTextSize(1);
   display1.setCursor(0, 39);
   if(dewPointF > -200.0) {
     display1.print("Dew: ");
     display1.print(dewPointF, 1);
   } else {
     display1.print("Dew: ERR");
   }
 
   // Lock status (bottom left)
   display1.setTextSize(1);
   display1.setCursor(0, 51);
   display1.print(targetLocked ? "Locked" : "Unlocked");
 
   display1.setTextSize(1);
   display1.setCursor(67, 0);
   display1.print("Scope F");
   display1.setTextSize(2);
   display1.setCursor(67, 16);
   if (currentTempF > -100.0)
     display1.print(currentTempF, 1);
   else
     display1.print("ERR");
 
 
   // Auto Mode indicator (above heater state)
   display1.setTextSize(1);
   display1.setCursor(67, 39);
   display1.print(autoMode ? "Auto ON" : "Auto OFF");
 
   // Heater state (bottom right)
   display1.setTextSize(1);
   display1.setCursor(67, 51);
   display1.print(heaterOn ? "Heater ON" : "Heater OFF");
 
   display1.display();
 
 
   
   // OLED #2: Display the last 30 minutes of temperature history as a graph.
   display2.clearDisplay();
   display2.setTextSize(1);
   display2.setCursor(20, 0);
   display2.print("Last 30 Min");
 
   // Draw the left vertical axis of the graph
   display2.drawLine(GRAPH_LEFT, GRAPH_TOP, GRAPH_LEFT, GRAPH_BOTTOM, WHITE);
 
   // Draw horizontal scale labels (20, 40, 60, 80, 100°F) along the left axis
   int labels[5] = {20,40,60,80,100};
   for(int i=0; i<5; i++) {
     int val = labels[i];
     int y = oledTempToY(val);
     display2.drawLine(GRAPH_LEFT-3, y, GRAPH_LEFT, y, WHITE);
     display2.setCursor(0, y-3);
     display2.print(val);
   }
 
   // Plot the temperature history as a line graph
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
 
   // Mark moments when the heater was ON with red ticks at the bottom of the graph
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
 
   delay(250); // Small delay to control the loop speed
 }