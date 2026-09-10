**Piezoelectric-Based Smart Helmet — Accident Detection System**
Final Year Project (BS Electrical Engineering) 
University of Engineering and Technology (UET), Lahore Session 2022–2026.

An IoT-based smart motorcycle helmet that detects accidents in real time and automatically alerts emergency contacts with the rider's GPS location, aiming to reduce response time during the critical "golden hour" after a crash.

**Authors
Raja Sana Ullah (2022-EE-620)
Farheen Fayyaz (2022-EE-604)
Waseem Ahmed (2022-EE-619)
Supervisor: Engr. Amna Javed**

**Overview**
The system fuses a piezoelectric impact sensor with an MPU6050 accelerometer/gyroscope for reliable dual-sensor crash detection, while a force-sensitive resistor (FSR) confirms the helmet is actually being worn (to reduce false triggers and save battery). On a detected impact, the system:

Sounds a 10-second warning (buzzer + vibration) — the rider can cancel via a push button if it's a false alarm.
If not cancelled, acquires GPS coordinates (NEO-6M module).
Sends an emergency SMS (via SIM800L GSM module) with a Google Maps link to predefined emergency contacts.
Results (from testing — see thesis for full data)
Detection accuracy: 95%
Impact force range validated: 4.9N – 64.8N
GPS location accuracy: within 2.5 meters
Reliable GSM SMS delivery across all test scenarios
Repository Structure
```
.
├── firmware/
│   └── smart_helmet.ino      # Arduino source code
├── hardware/
│   └── components.md         # BOM, specs, wiring/pin connections, power design
├── docs/
│   └── thesis.pdf            # Full BS thesis (78 pages) — design, testing, results

Hardware Used
Arduino Uno R3 • Piezoelectric sensor (PZT, 35mm) • MPU6050 • NEO-6M GPS • SIM800L GSM • FSR • push button • buzzer • 2x 18650 Li-ion cells • TP4056 • 7805 regulator • LM2596 buck converter
Full specs and wiring table: `hardware/components.md`

**Getting Started**
Wire the components as described in `hardware/components.md`.
Open `firmware/smart_helmet.ino` in the Arduino IDE.
Install the required libraries via Library Manager:
`TinyGPS++`
`SoftwareSerial` (bundled with Arduino IDE)
Set your emergency contact numbers — replace the placeholders near the top of the file:
```cpp
   const char num1[] PROGMEM = "+92XXXXXXXXXX"; // Emergency contact 1
   const char num2[] PROGMEM = "+92XXXXXXXXXX"; // Emergency contact 2
   ```
Insert a SIM card into the SIM800L module (SMS-capable, sufficient balance).
Flash to the Arduino and power the system from the battery pack.
**Documentation**
The full thesis (`docs/thesis.pdf`) covers:
Literature review of existing smart-helmet systems
System architecture & sensor-fusion methodology
Hardware design, circuit design, and component specifications
Prototype assembly and system integration
Testing methodology and results (calibration, impact tests, GPS/GSM tests)
Limitations & future work

**Future Work**
See thesis Section 7.5, including ideas such as improved false-positive filtering, cloud dashboard integration, and multi-language emergency alerts.
