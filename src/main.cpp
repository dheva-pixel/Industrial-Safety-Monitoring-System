// Core Arduino functions, pin control, timing, and serial communication.
#include <Arduino.h>
// I2C communication used by the LCD.
#include <Wire.h>
// SPI communication used by the MFRC522 RFID reader.
#include <SPI.h>
// Connect the ESP32 to Wi-Fi.
#include <WiFi.h>
// Date/time formatting functions for event timestamps.
#include <time.h>
// Secure client used for HTTPS requests to Telegram.
#include <WiFiClientSecure.h>
// Telegram bot library for sending notifications.
#include <UniversalTelegramBot.h>
// Library for the 16x2 I2C LCD.
#include <LiquidCrystal_I2C.h>
// Library for reading temperature from the DHT22 sensor.
#include <DHT.h>
// ESP32-compatible servo motor control.
#include <ESP32Servo.h>
// Library for reading RFID card UIDs.
#include <MFRC522.h>

// =====================================================
// WIFI
// =====================================================

// Wi-Fi credentials for the Wokwi simulator (change for real hardware).
#define WIFI_SSID       "Wokwi-GUEST"
#define WIFI_PASSWORD   ""

// Telegram bot token. Keep this secret; rotate it if this file has been shared publicly.
#define BOT_TOKEN "Enter your bot token here"
// Telegram chat that receives system alerts.
#define CHAT_ID   "Enter your chat ID here"

// =====================================================
// TIME / NTP
// =====================================================

// India Standard Time = UTC +5:30
// India Standard Time offset: 5 hours 30 minutes ahead of UTC.
const long GMT_OFFSET_SEC = 19800;
// India does not use daylight-saving time.
const int DAYLIGHT_OFFSET_SEC = 0;

const char* NTP_SERVER_1 = "pool.ntp.org";
const char* NTP_SERVER_2 = "time.nist.gov";

// =====================================================
// PIN DEFINITIONS
// =====================================================

// Analog input from the gas sensor.
#define GAS_PIN              34
// Analog input from the LDR used as a simulated fire sensor.
#define LDR_PIN              35
// Digital data pin for the DHT22 temperature sensor.
#define DHT_PIN              4
#define DHT_TYPE             DHT22

// Active-low emergency-stop button (pressed = LOW).
#define EMERGENCY_BUTTON     27
// Active-low reset button; resets only while emergency mode is active.
#define RESET_BUTTON         26

// Buzzer output for audible emergency alerts.
#define BUZZER_PIN           25

// Blinks while an emergency is active.
#define RED_LED              32
// Indicates normal/ready status.
#define GREEN_LED            33
// Indicates that the machine is running.
#define BLUE_LED             16

// Servo that opens/closes the emergency exit.
#define SERVO_PIN            13
// Relay output used to switch the machine on/off.
#define RELAY_PIN            14

// RFID
// SPI chip-select pin for the RFID reader.
#define RFID_SS_PIN          5
// Reset pin for the RFID reader.
#define RFID_RST_PIN         17

// =====================================================
// THRESHOLDS
// =====================================================

// ESP32 ADC threshold; gas readings at/above this trigger emergency mode.
#define GAS_THRESHOLD              3700
// LDR readings at/below this trigger the simulated fire alarm.
#define LDR_THRESHOLD              500

// Temperature warning range begins at 40 C.
#define TEMP_WARNING_THRESHOLD     40.0
// Temperature at/above 50 C triggers emergency mode in this version.
#define TEMP_EMERGENCY_THRESHOLD   50.0

// =====================================================
// TIMING
// =====================================================

// Print sensor readings every 30,000 ms (30 seconds).
#define SENSOR_PRINT_INTERVAL      30000UL
// Toggle the emergency red LED every 150 ms.
#define RED_BLINK_INTERVAL         150UL

// =====================================================
// OBJECTS
// =====================================================

DHT dht(DHT_PIN, DHT_TYPE);

LiquidCrystal_I2C lcd(0x27, 16, 2);

Servo emergencyExitServo;

MFRC522 rfid(RFID_SS_PIN, RFID_RST_PIN);

// Secure Wi-Fi client used for Telegram HTTPS communication
WiFiClientSecure telegramClient;

// Telegram bot object used to send alerts to the configured chat
UniversalTelegramBot bot(
    BOT_TOKEN,
    telegramClient
);

// =====================================================
// SYSTEM VARIABLES
// =====================================================

// Stores the current operating state of the safety system
bool emergencyState = false;

// Stores the current state of the emergency red LED
bool redLedState = false;

// Indicates whether the machine is currently running
bool machineRunning = false;

// Indicates whether the latest RFID card was authorized
bool workerAuthorized = false;

// Prevents repeated Telegram alerts for the same temperature warning
bool temperatureWarningLogged = false;

unsigned long lastSensorPrint = 0;

unsigned long lastRedBlink = 0;

// =====================================================
// LCD STATE
// =====================================================

enum DisplayState
{
    DISPLAY_NORMAL,
    DISPLAY_HIGH_TEMP,
    DISPLAY_EMERGENCY
};

DisplayState currentDisplay = DISPLAY_NORMAL;

// =====================================================
// WIFI / TIME STATE
// =====================================================

bool timeSynchronized = false;

// =====================================================
// TIMESTAMP FUNCTION
// =====================================================
// Returns the current date and time in YYYY-MM-DD HH:MM:SS format
// Used for timestamping system events and Telegram alerts
// Read the synchronized local clock and return a formatted timestamp.
String getTimestamp()
{
    struct tm timeinfo;

    if (!getLocalTime(&timeinfo))
    {
        return "TIME NOT SET";
    }

    char timestamp[25];

    strftime(
        timestamp,
        sizeof(timestamp),
        "%Y-%m-%d %H:%M:%S",
        &timeinfo
    );

    return String(timestamp);
}

// =====================================================
// EVENT LOGGER
// =====================================================

// Print an event with its timestamp to the Serial Monitor.
void logEvent(const String& event)
{
    Serial.print("[");
    Serial.print(getTimestamp());
    Serial.print("] ");
    Serial.println(event);
}

// Send a Telegram message when Wi-Fi is available.
void sendTelegramAlert(const String& message)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("Telegram: Wi-Fi not connected");
        return;
    }

    telegramClient.setInsecure();

    bool sent = bot.sendMessage(
        CHAT_ID,
        message,
        ""
    );

    if (sent)
    {
        Serial.println("Telegram: ALERT SENT");
    }
    else
    {
        Serial.println("Telegram: SEND FAILED");
    }
}

// =====================================================
// WIFI + NTP INITIALIZATION
// =====================================================

// Connect to Wi-Fi and synchronize the ESP32 clock using NTP.
void initializeTime()
{
    Serial.println();
    Serial.println("================================");
    Serial.println("       WIFI INITIALIZATION");
    Serial.println("================================");

    WiFi.mode(WIFI_STA);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.print("Connecting to Wi-Fi");

    unsigned long startAttempt = millis();

    while (WiFi.status() != WL_CONNECTED &&
           millis() - startAttempt < 15000)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println();

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("Wi-Fi: CONNECTED");

        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());

        configTime(
            GMT_OFFSET_SEC,
            DAYLIGHT_OFFSET_SEC,
            NTP_SERVER_1,
            NTP_SERVER_2
        );

        Serial.print("Synchronizing time");

        struct tm timeinfo;

        unsigned long timeStart = millis();

        while (!getLocalTime(&timeinfo) &&
               millis() - timeStart < 10000)
        {
            delay(500);
            Serial.print(".");
        }

        Serial.println();

        if (getLocalTime(&timeinfo))
        {
            timeSynchronized = true;

            Serial.print("Time synchronized: ");
            Serial.println(getTimestamp());
        }
        else
        {
            Serial.println("Time synchronization failed");
        }
    }
    else
    {
        Serial.println("Wi-Fi: CONNECTION FAILED");
        Serial.println("Event timestamps will show TIME NOT SET");
    }

    Serial.println("================================");
}

// Send a one-time Telegram test message; this function is not called in setup().
void testTelegram()
{
    telegramClient.setInsecure();

    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("Telegram test: Wi-Fi not connected");
        return;
    }

    Serial.println("Sending Telegram test...");

    bool result = bot.sendMessage(
        CHAT_ID,
        "🟢 Industrial Safety System\n\nTelegram connection test successful.",
        ""
    );

    if (result)
        Serial.println("Telegram: TEST MESSAGE SENT");
    else
        Serial.println("Telegram: TEST FAILED");
}
// =====================================================
// LCD FUNCTIONS
// =====================================================

// Display normal system status and whether the machine is ON or OFF.
void showNormalLCD()
{
    if (currentDisplay == DISPLAY_NORMAL)
        return;

    currentDisplay = DISPLAY_NORMAL;

    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("SYSTEM: NORMAL");

    lcd.setCursor(0, 1);
    lcd.print("MACHINE: ");

    if (machineRunning)
        lcd.print("ON ");
    else
        lcd.print("OFF");
}


// Display the high-temperature warning and current temperature.
void showHighTempLCD(float temperature)
{
    if (currentDisplay != DISPLAY_HIGH_TEMP)
    {
        currentDisplay = DISPLAY_HIGH_TEMP;

        lcd.clear();

        lcd.setCursor(0, 0);
        lcd.print("HIGH TEMPERATURE");
    }

    lcd.setCursor(0, 1);

    lcd.print("Temp: ");
    lcd.print(temperature, 1);
    lcd.print(" C   ");
}


// Display an emergency banner and its reason on the LCD.
void showEmergencyLCD(const char* reason)
{
    if (currentDisplay == DISPLAY_EMERGENCY)
        return;

    currentDisplay = DISPLAY_EMERGENCY;

    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("!!! EMERGENCY !!!");

    lcd.setCursor(0, 1);
    lcd.print(reason);
}

// =====================================================
// RFID AUTHORIZATION
// =====================================================

// Return true only for the three RFID UIDs listed below.
bool isAuthorizedCard()
{
    // 01:02:03:04
    if (rfid.uid.size == 4 &&
        rfid.uid.uidByte[0] == 0x01 &&
        rfid.uid.uidByte[1] == 0x02 &&
        rfid.uid.uidByte[2] == 0x03 &&
        rfid.uid.uidByte[3] == 0x04)
    {
        return true;
    }

    // 11:22:33:44
    if (rfid.uid.size == 4 &&
        rfid.uid.uidByte[0] == 0x11 &&
        rfid.uid.uidByte[1] == 0x22 &&
        rfid.uid.uidByte[2] == 0x33 &&
        rfid.uid.uidByte[3] == 0x44)
    {
        return true;
    }

    // 55:66:77:88
    if (rfid.uid.size == 4 &&
        rfid.uid.uidByte[0] == 0x55 &&
        rfid.uid.uidByte[1] == 0x66 &&
        rfid.uid.uidByte[2] == 0x77 &&
        rfid.uid.uidByte[3] == 0x88)
    {
        return true;
    }

    return false;
}

// =====================================================
// RFID CHECK
// =====================================================

// Check for a new RFID card and grant or deny machine access.
void checkRFID()
{
    if (!rfid.PICC_IsNewCardPresent())
        return;

    if (!rfid.PICC_ReadCardSerial())
        return;

    // Build UID string
    // Build properly formatted UID string
String uidString = "";

for (byte i = 0; i < rfid.uid.size; i++)
{
    if (i > 0)
        uidString += ":";

    if (rfid.uid.uidByte[i] < 0x10)
        uidString += "0";

    uidString += String(rfid.uid.uidByte[i], HEX);
}

uidString.toUpperCase();

    Serial.println();
    Serial.println("================================");
    Serial.println("       RFID CARD DETECTED");
    Serial.println("================================");

    Serial.print("UID: ");
    Serial.println(uidString);

    // =================================================
    // AUTHORIZED
    // =================================================

    if (isAuthorizedCard())
    {
        workerAuthorized = true;

        Serial.println("STATUS: AUTHORIZED");
        Serial.println("ACCESS: GRANTED");

        logEvent(
            "AUTHORIZED RFID - UID: " + uidString
        );

        // Emergency overrides RFID
        if (emergencyState)
        {
            Serial.println("EMERGENCY ACTIVE");
            Serial.println("MACHINE START BLOCKED");

            logEvent(
                "MACHINE START BLOCKED - EMERGENCY ACTIVE"
            );

            workerAuthorized = false;
        }
        else
        {
            // Toggle machine
            machineRunning = !machineRunning;

            if (machineRunning)
            {
                digitalWrite(RELAY_PIN, HIGH);
                digitalWrite(BLUE_LED, HIGH);

                Serial.println("MACHINE: ON");

                logEvent(
                    "MACHINE START - AUTHORIZED RFID"
                );

                sendTelegramAlert(
                    "🟢 MACHINE STARTED\n\n"
                    "Authorization: RFID\n"
                    "UID: " + uidString + "\n"
                    "Machine: ON\n"
                    "Time: " + getTimestamp()
                );
            }
            else
            {
                digitalWrite(RELAY_PIN, LOW);
                digitalWrite(BLUE_LED, LOW);

                Serial.println("MACHINE: OFF");

                logEvent(
                    "MACHINE STOP - AUTHORIZED RFID"
                );
                sendTelegramAlert(
    "🔵 MACHINE STOPPED\n\n"
    "Authorization: RFID\n"
    "UID: " + uidString + "\n"
    "Machine: OFF\n"
    "Time: " + getTimestamp()
);
            }

            // Update LCD
            currentDisplay = DISPLAY_EMERGENCY;
            showNormalLCD();
        }
    }

    // =================================================
    // UNAUTHORIZED
    // =================================================

    else
    {
        workerAuthorized = false;

        machineRunning = false;

        digitalWrite(RELAY_PIN, LOW);
        digitalWrite(BLUE_LED, LOW);

        Serial.println("STATUS: UNAUTHORIZED");
        Serial.println("ACCESS: DENIED");

        logEvent(
            "UNAUTHORIZED RFID - UID: " + uidString
        );
        sendTelegramAlert(
            "🔐 UNAUTHORIZED RFID ACCESS\n\n"
            "UID: " + uidString + "\n"
            "Access: DENIED\n"
            "Machine: OFF\n"
            "Time: " + getTimestamp()
        );
        logEvent(
            "MACHINE ACCESS DENIED"
        );
    }

    Serial.println("================================");

    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
}

// Move the exit servo to 90 degrees to represent an open door.
void openEmergencyDoor()
{
    Serial.println("Opening emergency door...");

    emergencyExitServo.attach(
        SERVO_PIN,
        500,
        2400
    );

    emergencyExitServo.write(90);

    delay(500);

    Serial.println("Door -> OPEN");
    Serial.println("Servo -> 90 degrees");
}

// Move the exit servo to 0 degrees to represent a closed door.
void closeEmergencyDoor()
{
    Serial.println("Closing emergency door...");

    emergencyExitServo.attach(
        SERVO_PIN,
        500,
        2400
    );

    emergencyExitServo.write(0);

    delay(500);

    Serial.println("Door -> CLOSED");
    Serial.println("Servo -> 0 degrees");
}
// =====================================================
// ENTER EMERGENCY
// =====================================================

// Enter emergency mode once, stop the machine, activate alarms, and notify Telegram.
void enterEmergency(const char* reason)
{
    if (emergencyState)
        return;

    emergencyState = true;

    // Machine must immediately stop
    machineRunning = false;
    workerAuthorized = false;

    digitalWrite(RELAY_PIN, LOW);
    digitalWrite(BLUE_LED, LOW);

    // Green OFF
    digitalWrite(GREEN_LED, LOW);

    // Red LED starts blinking
    redLedState = true;
    digitalWrite(RED_LED, HIGH);

    lastRedBlink = millis();

    // Open emergency exit
    openEmergencyDoor();

    // Continuous buzzer
    tone(BUZZER_PIN, 2000);

    // LCD
    showEmergencyLCD(reason);

    // Serial
    Serial.println();
    Serial.println("================================");
    Serial.println("       !!! EMERGENCY !!!");
    Serial.print("Reason: ");
    Serial.println(reason);
    Serial.println("================================");

    Serial.println("Machine: OFF");
    Serial.println("Emergency exit: OPEN");
    Serial.println("Servo position: 90 degrees");

    // Timestamped event
    logEvent(
        String("EMERGENCY - ") + reason
    );

    logEvent(
        "MACHINE STOPPED"
    );
    sendTelegramAlert(
    "🚨 INDUSTRIAL SAFETY EMERGENCY 🚨\n\n"
    "Reason: " + String(reason) + "\n"
    "Machine: OFF\n"
    "Emergency Exit: OPEN\n"
    "Time: " + getTimestamp()
);
}

// =====================================================
// RESET SYSTEM
// =====================================================

// Clear emergency mode and restore the system to its safe idle state.
void resetSystem()
{
    emergencyState = false;

    machineRunning = false;
    workerAuthorized = false;

    // Stop buzzer
    noTone(BUZZER_PIN);

    digitalWrite(BUZZER_PIN, LOW);

    // Stop red LED
    redLedState = false;

    digitalWrite(RED_LED, LOW);

    // Green ON
    digitalWrite(GREEN_LED, HIGH);

    // Machine OFF
    digitalWrite(RELAY_PIN, LOW);

    digitalWrite(BLUE_LED, LOW);

    // Close door
    closeEmergencyDoor();

    Serial.println();
    Serial.println("================================");
    Serial.println("          SYSTEM RESET");
    Serial.println("================================");

    
    Serial.println("System -> NORMAL");

    logEvent("SYSTEM RESET");
    logEvent("MACHINE OFF AFTER RESET");

    sendTelegramAlert(
    "🔄 SYSTEM RESET\n\n"
    "System: NORMAL\n"
    "Machine: OFF\n"
    "Emergency Exit: CLOSED\n"
    "Time: " + getTimestamp()
);
    // Force LCD refresh
    currentDisplay = DISPLAY_EMERGENCY;

    showNormalLCD();

    Serial.println("================================");
}

// =====================================================
// EMERGENCY RED LED BLINK
// =====================================================

// Blink the red LED without blocking the main loop with a long delay.
void updateEmergencyLED()
{
    if (!emergencyState)
        return;

    if (millis() - lastRedBlink >= RED_BLINK_INTERVAL)
    {
        lastRedBlink = millis();

        redLedState = !redLedState;

        digitalWrite(
            RED_LED,
            redLedState
        );
    }
}

// =====================================================
// SETUP
// =====================================================

// Run once at startup: configure pins, initialize devices, and connect to Wi-Fi/time.
void setup()
{
    Serial.begin(115200);

    // =================================================
    // INPUTS
    // =================================================

    pinMode(
        EMERGENCY_BUTTON,
        INPUT_PULLUP
    );

    pinMode(
        RESET_BUTTON,
        INPUT_PULLUP
    );

    // =================================================
    // OUTPUTS
    // =================================================

    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(RED_LED, OUTPUT);
    pinMode(GREEN_LED, OUTPUT);
    pinMode(BLUE_LED, OUTPUT);
    pinMode(RELAY_PIN, OUTPUT);

    // Initial outputs
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(RED_LED, LOW);
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(BLUE_LED, LOW);
    digitalWrite(RELAY_PIN, LOW);

    // =================================================
    // DHT
    // =================================================

    dht.begin();

    // =================================================
    // LCD
    // =================================================

    lcd.init();
    lcd.backlight();

    // =================================================
    // SERVO
    // =================================================

    emergencyExitServo.setPeriodHertz(50);

    emergencyExitServo.attach(
        SERVO_PIN,
        500,
        2400
    );

    emergencyExitServo.write(0);

    // =================================================
    // RFID
    // =================================================

    SPI.begin(
        18,
        19,
        23,
        RFID_SS_PIN
    );

    rfid.PCD_Init();

    Serial.println("RFID reader initialized");

    // =================================================
    // LCD INITIAL STATE
    // =================================================

    currentDisplay = DISPLAY_EMERGENCY;

    showNormalLCD();

    // =================================================
    // WIFI + TIME
    // =================================================

    initializeTime();
    

    // =================================================
    // SYSTEM START
    // =================================================

    Serial.println();
    Serial.println("================================");
    Serial.println(" INDUSTRIAL SAFETY SYSTEM");
    Serial.println("        ESP32 STARTED");
    Serial.println("================================");

    Serial.println("System status: NORMAL");

    if (timeSynchronized)
    {
        logEvent("SYSTEM START");
    }
    else
    {
        Serial.println("Warning: Time not synchronized");
    }

    lastSensorPrint = millis();
    
}

// =====================================================
// MAIN LOOP
// =====================================================

// Repeats continuously: handle controls, check sensors, and respond to hazards.
void loop()
{
    // Safety priority: emergency mode is handled before normal sensor processing.
    // =================================================
    // EMERGENCY RED LED
    // =================================================

    updateEmergencyLED();

    // =================================================
    // RFID
    // =================================================

    checkRFID();

    // =================================================
    // RESET BUTTON
    // =================================================

    // Reset is accepted only during an emergency, preventing an ordinary press from
    // changing the machine state during normal operation.
    if (digitalRead(RESET_BUTTON) == LOW)
    {
        if (emergencyState)
        {
            resetSystem();

            // Wait for button release
            // Wait for the reset button to be released so one press is not processed repeatedly.
            while (digitalRead(RESET_BUTTON) == LOW)
            {
                delay(10);
            }

            delay(200);
        }
    }

    // =================================================
    // EMERGENCY STATE
    // =================================================

    if (emergencyState)
    {
        // Machine OFF
        digitalWrite(RELAY_PIN, LOW);
        digitalWrite(BLUE_LED, LOW);

        // Green OFF
        digitalWrite(GREEN_LED, LOW);

        return;
    }

    // =================================================
    // MANUAL EMERGENCY STOP
    // =================================================

    // INPUT_PULLUP means a pressed button reads LOW.
    if (digitalRead(EMERGENCY_BUTTON) == LOW)
    {
        enterEmergency("EMERGENCY STOP");

        delay(300);

        return;
    }

    // =================================================
    // READ SENSORS
    // =================================================

    // Read the gas sensor ADC value (ESP32 ADC readings are typically 0–4095).
int gasValue = analogRead(GAS_PIN);

    // Read the LDR value; threshold direction depends on the circuit wiring.
int ldrValue = analogRead(LDR_PIN);

    // Read temperature in Celsius; DHT failures can return NaN.
float temperature = dht.readTemperature();

    // =================================================
    // GAS CHECK
    // =================================================

    // A gas reading at or above the configured ADC threshold is treated as a leak.
    if (gasValue >= GAS_THRESHOLD)
    {
        logEvent(
            "GAS THRESHOLD EXCEEDED - ADC: " +
            String(gasValue)
        );

        enterEmergency("GAS LEAK");

        return;
    }

    // =================================================
    // FIRE CHECK
    // =================================================

    // This assumes the LDR reading falls when the simulated flame/light condition occurs.
    if (ldrValue <= LDR_THRESHOLD)
    {
        logEvent(
            "FIRE SENSOR TRIGGERED - LDR: " +
            String(ldrValue)
        );

        enterEmergency("FIRE DETECTED");

        return;
    }

    // =================================================
    // TEMPERATURE
    // =================================================

    if (!isnan(temperature))
    {
        // -------------------------------------------------
        // HIGH TEMPERATURE WARNING
        // -------------------------------------------------

        // Warning range: show the temperature and send one alert until the warning clears.
        if (temperature >= TEMP_WARNING_THRESHOLD &&
    temperature < TEMP_EMERGENCY_THRESHOLD)
{
    showHighTempLCD(temperature);

    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(RED_LED, LOW);
    digitalWrite(BUZZER_PIN, LOW);

    if (!temperatureWarningLogged)
    {
        temperatureWarningLogged = true;

        logEvent(
            "HIGH TEMPERATURE WARNING - " +
            String(temperature, 1) +
            " C"
        );

        sendTelegramAlert(
            "⚠️ HIGH TEMPERATURE WARNING ⚠️\n\n"
            "Temperature: " + String(temperature, 1) + " C\n"
            "Machine: " + String(machineRunning ? "ON" : "OFF") + "\n"
            "System: WARNING\n"
            "Time: " + getTimestamp()
        );
    }
}

        // -------------------------------------------------
        // NORMAL TEMPERATURE
        // -------------------------------------------------

        else if (temperature < TEMP_WARNING_THRESHOLD)
        {
            static bool temperatureWarningLogged = false;

            // This local static is separate from the one above,
            // so warning reset is handled below using a dedicated
            // mechanism.
            
            showNormalLCD();

            digitalWrite(GREEN_LED, HIGH);
            digitalWrite(RED_LED, LOW);
            digitalWrite(BUZZER_PIN, LOW);
        }

        // -------------------------------------------------
        // CRITICAL TEMPERATURE
        // -------------------------------------------------

        // Critical range: switch to emergency mode and stop further loop processing.
        else if (temperature >= TEMP_EMERGENCY_THRESHOLD)
        {
            logEvent(
                "CRITICAL TEMPERATURE - " +
                String(temperature, 1) +
                " C"
            );

            enterEmergency("HIGH TEMP");

            return;
        }
    }

    // =================================================
    // SERIAL SENSOR STATUS
    // EVERY 30 SECONDS
    // =================================================

    if (millis() - lastSensorPrint >= SENSOR_PRINT_INTERVAL)
    {
        lastSensorPrint = millis();

        Serial.println();
        Serial.println("------ SENSOR STATUS ------");

        Serial.print("Gas: ");
        Serial.println(gasValue);

        Serial.print("LDR: ");
        Serial.println(ldrValue);

        Serial.print("Temperature: ");

        if (isnan(temperature))
        {
            Serial.println("ERROR");
        }
        else
        {
            Serial.print(temperature, 2);
            Serial.println(" C");
        }

        Serial.println("---------------------------");
    }

    delay(100);
}