#include <SoftwareSerial.h>
#include <TinyGPS++.h>

// Pin Definitions
#define WEAR_SENSOR_PIN 2      
#define VIBRATION_SENSOR_PIN A2  
#define MIC_DIGITAL_PIN 4       
#define MIC_ANALOG_PIN A1       
#define IR_SENSOR_PIN A0        
#define BUZZER_PIN 5            
#define GSM_RX 8                
#define GSM_TX 9               
#define CANCEL_BUTTON_PIN A3     // Push button to cancel alerts

// GSM Serial
SoftwareSerial sim(GSM_RX, GSM_TX);
TinyGPSPlus gps;

// Phone numbers in flash memory
const char num1[] PROGMEM = "+923076179930";
const char num2[] PROGMEM = "+923190426572";
char number1[15], number2[15];

// Status flags
bool helmetWorn = false;
bool gsmReady = false;
bool gpsReady = false;
bool accidentDetected = false;
bool emergencyDetected = false;
bool sleepDetected = false;
bool startupSent = false;
bool waitingForCancel = false;     // Flag to indicate we're waiting for cancel button
bool alertCancelled = false;       // Flag if alert was cancelled by user

// Timing
unsigned long lastAccident = 0;
unsigned long lastEmergency = 0;
unsigned long lastSleepAlert = 0;
unsigned long lastGSMCheck = 0;
unsigned long lastStatus = 0;
unsigned long helmetStart = 0;
unsigned long alertTriggerTime = 0;    // When alert was triggered
unsigned long lastButtonCheck = 0;      // For debouncing button

// Constants
const unsigned long DEBOUNCE = 3000;
const unsigned long SLEEP_INTERVAL = 5000;
const unsigned long GSM_INTERVAL = 30000;
const unsigned long GPS_TIMEOUT = 10000;
const unsigned long STATUS_INTERVAL = 2000;
const unsigned long STARTUP_DELAY = 5000;
const unsigned long CANCEL_WINDOW = 10000;     // 10 seconds to cancel
const unsigned long BUTTON_DEBOUNCE = 200;      // Button debounce time

// Thresholds
const int VIB_THRESHOLD = 1020; 
const int MIC_THRESHOLD = 1400;       
const int IR_THRESHOLD = 40;        

// Buffers
char buffer[160];
char latBuf[12], lngBuf[12];

void setup() {
  Serial.begin(9600);
  sim.begin(9600);
  
  // Load numbers from flash
  strcpy_P(number1, num1);
  strcpy_P(number2, num2);
  
  pinMode(WEAR_SENSOR_PIN, INPUT_PULLUP); 
  pinMode(VIBRATION_SENSOR_PIN, INPUT);   
  pinMode(MIC_DIGITAL_PIN, INPUT);        
  pinMode(MIC_ANALOG_PIN, INPUT);         
  pinMode(IR_SENSOR_PIN, INPUT);          
  pinMode(BUZZER_PIN, OUTPUT);            
  pinMode(CANCEL_BUTTON_PIN, INPUT_PULLUP);  // Button with internal pullup
  
  digitalWrite(BUZZER_PIN, LOW); 
  
  Serial.println(F("Smart Helmet v3.0 - With Cancel Button"));
  initGSM();
  Serial.println(F("Ready..."));
  Serial.println(F("Press button within 10 seconds to cancel alerts!"));
}

void loop() {
  checkHelmet();
  
  if (helmetWorn) {
    readGPS();
    checkGSM();
    
    if (!startupSent && gpsReady && gsmReady && (millis() - helmetStart > STARTUP_DELAY)) {
      sendStartup();
      startupSent = true;
    }
    
    checkAccident();
    checkEmergency();
    checkSleep();
    checkCancelButton();        // Check if button is pressed
    processPendingAlert();      // Process alert after cancel window
    showStatus();
  }
  
  delay(50);
}

void checkHelmet() {
  bool current = (digitalRead(WEAR_SENSOR_PIN) == LOW);
  
  if (current != helmetWorn) {
    helmetWorn = current;
    
    if (helmetWorn) {
      helmetStart = millis();
      startupSent = false;
      waitingForCancel = false;  // Reset cancel flag when helmet is put on
      alertCancelled = false;
      Serial.println(F("\nHelmet ON"));
      beep(2, 200);
    } else {
      Serial.println(F("\nHelmet OFF"));
      digitalWrite(BUZZER_PIN, LOW);
      accidentDetected = emergencyDetected = sleepDetected = false;
      waitingForCancel = false;  // Reset when helmet removed
      alertCancelled = false;
    }
  }
}

void checkCancelButton() {
  // Check if we're waiting for cancel and button is pressed
  if (waitingForCancel && !alertCancelled) {
    // Read button with debouncing
    if (millis() - lastButtonCheck > BUTTON_DEBOUNCE) {
      if (digitalRead(CANCEL_BUTTON_PIN) == LOW) {  // Button pressed (LOW because of pullup)
        alertCancelled = true;
        waitingForCancel = false;
        
        Serial.println(F("\n*** ALERT CANCELLED BY USER ***"));
        Serial.println(F("You are OK. No SMS will be sent."));
        
        // Give feedback with buzzer (short beep to acknowledge cancellation)
        beep(1, 100);
        
        // Reset detection flags
        accidentDetected = false;
        emergencyDetected = false;
        alertTriggerTime = 0;
      }
      lastButtonCheck = millis();
    }
  }
}

void processPendingAlert() {
  // Check if we have a pending alert that wasn't cancelled
  if (waitingForCancel && !alertCancelled) {
    unsigned long elapsed = millis() - alertTriggerTime;
    
    // Show countdown (every second)
    static unsigned long lastCountdownShow = 0;
    if (elapsed < CANCEL_WINDOW && millis() - lastCountdownShow > 1000) {
      int secondsLeft = (CANCEL_WINDOW - elapsed) / 1000;
      Serial.print(F("\n[ALERT] Press button to cancel - "));
      Serial.print(secondsLeft);
      Serial.println(F(" seconds left"));
      
      // Beep every second as reminder
      beep(1, 50);
      lastCountdownShow = millis();
    }
    
    // If cancel window expired, send the alert
    if (elapsed >= CANCEL_WINDOW) {
      waitingForCancel = false;
      Serial.println(F("\n*** NO CANCELLATION - SENDING ALERT ***"));
      sendAlertNow();  // Send the actual alert
    }
  }
}

void checkAccident() {
  int val = analogRead(VIBRATION_SENSOR_PIN);
  
  if (val > VIB_THRESHOLD && !accidentDetected && !waitingForCancel && (millis() - lastAccident > DEBOUNCE)) {
    accidentDetected = true;
    lastAccident = millis();
    Serial.println(F("ACCIDENT DETECTED!"));
    
    // Start the cancellation window instead of sending immediately
    startAlertProcess("ACCIDENT", val);
  } else if (val <= VIB_THRESHOLD && !waitingForCancel) {
    accidentDetected = false;
  }
}

void checkEmergency() {
  bool dig = digitalRead(MIC_DIGITAL_PIN) == HIGH;
  int val = analogRead(MIC_ANALOG_PIN);
  
  if ((dig || val > MIC_THRESHOLD) && !emergencyDetected && !waitingForCancel && (millis() - lastEmergency > DEBOUNCE)) {
    emergencyDetected = true;
    lastEmergency = millis();
    Serial.println(F("EMERGENCY DETECTED!"));
    
    // Start the cancellation window
    startAlertProcess("VOICE", val);
  } else if (!dig && val <= MIC_THRESHOLD && !waitingForCancel) {
    emergencyDetected = false;
  }
}

void startAlertProcess(const char* type, int val) {
  if (!helmetWorn) {
    Serial.println(F("Helmet not worn - alert ignored"));
    return;
  }
  
  alertTriggerTime = millis();
  waitingForCancel = true;
  alertCancelled = false;
  
  Serial.println(F("========================================"));
  Serial.print(F("ALERT: "));
  Serial.println(type);
  Serial.print(F("Value: "));
  Serial.println(val);
  Serial.print(F("You have "));
  Serial.print(CANCEL_WINDOW / 1000);
  Serial.println(F(" seconds to press the cancel button if you're OK"));
  Serial.println(F("========================================"));
  
  // Beep pattern to indicate alert (3 short beeps)
  for(int i = 0; i < 3; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(100);
    digitalWrite(BUZZER_PIN, LOW);
    delay(100);
  }
  
  // Store alert details for later
  strcpy(buffer, type);  // Store type temporarily
  // We'll need to store val in a global variable for later use
  // For simplicity, we'll recreate it when sending
}

void sendAlertNow() {
  // Determine what type of alert we're sending
  const char* type = buffer;  // Type stored in buffer
  
  if (!helmetWorn || !gsmReady) {
    Serial.println(F("Cannot send - helmet off or GSM not ready"));
    return;
  }
  
  // Get the value from the appropriate sensor
  int val = 0;
  if (strcmp(type, "ACCIDENT") == 0) {
    val = analogRead(VIBRATION_SENSOR_PIN);
  } else {
    val = analogRead(MIC_ANALOG_PIN);
  }
  
  // Get coordinates as strings
  getCoordinates();
  
  // Build message with clear latitude and longitude
  char msgBuffer[200];
  sprintf(msgBuffer, 
          "🚨EMERGENCY:%s\n"
          "Value:%d\n"
          "Latitude:%s\n"
          "Longitude:%s\n"
          "Map:http://maps.google.com/?q=%s,%s\n"
          "Time:%s\n"
          "HELP NEEDED!",
          type, val, latBuf, lngBuf, latBuf, lngBuf, getTimeStr());
  
  Serial.println(F("\nSending Alert:"));
  Serial.println(msgBuffer);
  
  sendSMS(number1, msgBuffer);
  delay(2000);
  sendSMS(number2, msgBuffer);
  
  // Long beep pattern for emergency
  beep(3, 500);
  
  // Reset detection flags
  accidentDetected = false;
  emergencyDetected = false;
}

void sendAlert(const char* type, int val) {
  // This function is kept for compatibility but won't be used directly
  // Alerts now go through the cancellation process
  if (!waitingForCancel) {
    startAlertProcess(type, val);
  }
}

void sendStartup() {
  getCoordinates();
  
  sprintf(buffer, 
          "🪖Helmet Activated\n"
          "Latitude:%s\n"
          "Longitude:%s\n"
          "Map:http://maps.google.com/?q=%s,%s\n"
          "Time:%s",
          latBuf, lngBuf, latBuf, lngBuf, getTimeStr());
  
  Serial.println(F("\nSending Startup:"));
  Serial.println(buffer);
  
  sendSMS(number1, buffer);
  delay(2000);
  sendSMS(number2, buffer);
  beep(2, 100);
}

void readGPS() {
  while (Serial.available()) {
    gps.encode(Serial.read());
  }
  
  static unsigned long lastFix = 0;
  if (gps.location.isValid()) {
    if (!gpsReady) {
      Serial.println(F("GPS Fix OK"));
      printCoordinates();
    }
    gpsReady = true;
    lastFix = millis();
  } else if (millis() - lastFix > GPS_TIMEOUT) {
    gpsReady = false;
  }
}

void printCoordinates() {
  Serial.print(F("Lat: "));
  Serial.print(gps.location.lat(), 6);
  Serial.print(F(" Lon: "));
  Serial.println(gps.location.lng(), 6);
}

void checkGSM() {
  if (millis() - lastGSMCheck < GSM_INTERVAL) return;
  lastGSMCheck = millis();
  
  sim.println(F("AT"));
  delay(300);
  
  if (sim.available()) {
    String resp = sim.readString();
    gsmReady = (resp.indexOf("OK") >= 0);
  } else {
    gsmReady = false;
  }
  
  if (!gsmReady) initGSM();
}

void initGSM() {
  Serial.println(F("Init GSM..."));
  delay(3000);                      // Wait for SIM800L to fully boot

  for (int i = 0; i < 5; i++) {    // 5 attempts instead of 3
    while (sim.available()) sim.read(); // Flush garbage first
    
    sim.println(F("AT"));
    delay(1000);                    // Was 500 — SIM800L needs more time
    
    if (sim.available()) {
      String resp = sim.readString();
      Serial.print(F("GSM resp: "));
      Serial.println(resp);         // Print raw response so you can see it
      
      if (resp.indexOf("OK") >= 0) {
        sim.println(F("AT+CMGF=1"));
        delay(500);
        gsmReady = true;
        Serial.println(F("GSM OK"));
        return;
      }
    } else {
      Serial.print(F("No response, attempt "));
      Serial.println(i + 1);
    }
  }
  Serial.println(F("GSM Failed"));
}

void checkSleep() {
  int val = analogRead(IR_SENSOR_PIN);
  
  if (val < IR_THRESHOLD) {
    if (!sleepDetected) {
      sleepDetected = true;
      Serial.println(F("SLEEP!"));
    }
    
    if (millis() - lastSleepAlert > SLEEP_INTERVAL) {
      digitalWrite(BUZZER_PIN, HIGH);
      delay(500);
      digitalWrite(BUZZER_PIN, LOW);
      lastSleepAlert = millis();
    }
  } else {
    sleepDetected = false;
    digitalWrite(BUZZER_PIN, LOW);
  }
}

void getCoordinates() {
  if (gps.location.isValid()) {
    dtostrf(gps.location.lat(), 2, 6, latBuf);
    dtostrf(gps.location.lng(), 2, 6, lngBuf);
  } else {
    strcpy(latBuf, "Waiting...");
    strcpy(lngBuf, "Waiting...");
  }
}

char* getTimeStr() {
  static char timeBuf[20];
  if (gps.time.isValid()) {
    sprintf(timeBuf, "%02d:%02d:%02d", 
            gps.time.hour(), gps.time.minute(), gps.time.second());
  } else {
    unsigned long up = millis() / 1000;
    sprintf(timeBuf, "Up:%luh%lum", up/3600, (up%3600)/60);
  }
  return timeBuf;
}

void sendSMS(const char* num, const char* msg) {
  sim.println(F("AT+CMGF=1"));
  delay(300);
  
  sim.print(F("AT+CMGS=\""));
  sim.print(num);
  sim.println(F("\""));
  delay(500);
  
  sim.println(msg);
  delay(200);
  sim.println((char)26);
  delay(3000);
  
  Serial.print(F("Sent to: "));
  Serial.println(num);
}

void showStatus() {
  if (millis() - lastStatus < STATUS_INTERVAL) return;
  lastStatus = millis();
  
  Serial.print(F("\n=== STATUS ===\nH:"));
  Serial.print(helmetWorn ? "ON" : "OFF");
  Serial.print(F(" GPS:"));
  Serial.print(gpsReady ? "OK" : "NO");
  Serial.print(F(" Sats:"));
  Serial.print(gps.satellites.value());
  
  if (gps.location.isValid()) {
    Serial.print(F("\nLat:"));
    Serial.print(gps.location.lat(), 6);
    Serial.print(F(" Lon:"));
    Serial.print(gps.location.lng(), 6);
  }
  
  Serial.print(F("\nGSM:"));
  Serial.print(gsmReady ? "OK" : "NO");
  Serial.print(F(" V:"));
  Serial.print(analogRead(VIBRATION_SENSOR_PIN));
  Serial.print(F(" M:"));
  Serial.print(analogRead(MIC_ANALOG_PIN));
  Serial.print(F(" I:"));
  Serial.print(analogRead(IR_SENSOR_PIN));
  
  // Show cancel button status
  Serial.print(F(" Cancel:"));
  Serial.print(waitingForCancel ? "Waiting" : (alertCancelled ? "Cancelled" : "Ready"));
  
  if (waitingForCancel && !alertCancelled) {
    unsigned long remaining = (CANCEL_WINDOW - (millis() - alertTriggerTime)) / 1000;
    Serial.print(F(" TimeLeft:"));
    Serial.print(remaining);
    Serial.print(F("s"));
  }
  Serial.println();
}

void beep(int count, int duration) {
  for (int i = 0; i < count; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(duration);
    digitalWrite(BUZZER_PIN, LOW);
    delay(100);
  }
}

// Test functions
void testSMS() {
  if (helmetWorn && gsmReady) {
    getCoordinates();
    sprintf(buffer, 
            "Test Message\n"
            "Lat:%s\n"
            "Lon:%s\n"
            "Map:http://maps.google.com/?q=%s,%s",
            latBuf, lngBuf, latBuf, lngBuf);
    sendSMS(number1, buffer);
  }
}

void testAlert() {
  if (helmetWorn) {
    Serial.println(F("Testing alert system with cancel window..."));
    startAlertProcess("TEST", 999);
  }
}

void printRawGPS() {
  if (gps.location.isValid()) {
    Serial.print(F("Raw - Lat: "));
    Serial.print(gps.location.lat(), 6);
    Serial.print(F(" Lon: "));
    Serial.println(gps.location.lng(), 6);
  }
}
