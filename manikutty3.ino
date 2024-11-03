#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <Wire.h>
#include <RTClib.h>

// Pin definitions for display
#define TFT_CS        5
#define TFT_RST       15
#define TFT_DC        32
#define TFT_MOSI      23
#define TFT_SCLK      18

// Pin definitions for buttons and buzzer
#define HOUR_BUTTON   13
#define MIN_BUTTON    12
#define MODE_BUTTON   14
#define ALARM_BUTTON  27
#define BUZZER_PIN    26

// Initialize display and RTC
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);
RTC_DS3231 rtc;

// Screen dimensions
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 280

// Text properties and positions
#define TIME_TEXT_SIZE 4
#define STATUS_TEXT_SIZE 2
#define TIME_Y_POS (SCREEN_HEIGHT/2 - 30)  // Time position
#define STATUS_Y_POS (SCREEN_HEIGHT/2 + 20) // Status position just below time

// Colors
#define TIME_COLOR ST77XX_GREEN
#define ALARM_COLOR ST77XX_YELLOW
#define STATUS_COLOR ST77XX_BLUE
#define BACKGROUND_COLOR ST77XX_BLACK

// Mode definitions
enum ClockMode {
    NORMAL,
    SET_HOUR,
    SET_MINUTE,
    SET_ALARM_HOUR,
    SET_ALARM_MINUTE
};

// Button structure for improved handling
struct Button {
    const uint8_t PIN;
    bool lastReading;
    bool state;
    unsigned long lastDebounceTime;
};

// Global variables
ClockMode currentMode = NORMAL;
bool alarmEnabled = false;
bool alarmTriggered = false;
int alarmHour = 7;
int alarmMinute = 0;
String prevTimeStr = "";
String prevStatusStr = "";
const int debounceDelay = 200;

// Initialize button objects
Button hourButton = {HOUR_BUTTON, HIGH, HIGH, 0};
Button minButton = {MIN_BUTTON, HIGH, HIGH, 0};
Button modeButton = {MODE_BUTTON, HIGH, HIGH, 0};
Button alarmButton = {ALARM_BUTTON, HIGH, HIGH, 0};

void setup() {
    Serial.begin(115200);
    Wire.begin();
    
    // Initialize display
    tft.init(SCREEN_HEIGHT, SCREEN_WIDTH);
    tft.setRotation(3);
    tft.fillScreen(BACKGROUND_COLOR);
    tft.setTextWrap(false);
    
    // Initialize RTC
    if (!rtc.begin()) {
        tft.setCursor(10, SCREEN_HEIGHT/2);
        tft.setTextColor(ST77XX_RED);
        tft.print("RTC ERROR!");
        Serial.println("RTC not found!");
        while (1);
    }
    
    // Initialize buttons and buzzer
    pinMode(HOUR_BUTTON, INPUT_PULLUP);
    pinMode(MIN_BUTTON, INPUT_PULLUP);
    pinMode(MODE_BUTTON, INPUT_PULLUP);
    pinMode(ALARM_BUTTON, INPUT_PULLUP);
    pinMode(BUZZER_PIN, OUTPUT);
    
    digitalWrite(BUZZER_PIN, LOW);
}

bool debounceButton(Button& button) {
    bool buttonPressed = false;
    bool currentReading = digitalRead(button.PIN);
    
    // If the switch changed, due to noise or pressing
    if (currentReading != button.lastReading) {
        // Reset the debouncing timer
        button.lastDebounceTime = millis();
    }
    
    // If enough time has passed since last change
    if ((millis() - button.lastDebounceTime) > debounceDelay) {
        // If the button state has changed
        if (currentReading != button.state) {
            button.state = currentReading;
            // Only register a press on the falling edge (button press, not release)
            if (button.state == LOW) {
                buttonPressed = true;
            }
        }
    }
    
    button.lastReading = currentReading;
    return buttonPressed;
}

void centerText(const char* text, int y, int textSize, uint16_t color) {
    int16_t x1, y1;
    uint16_t w, h;
    
    tft.setTextSize(textSize);
    tft.setTextColor(color);
    tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor((SCREEN_WIDTH - w) / 2, y);
    tft.print(text);
}

void displayTime(DateTime now) {
    char timeStr[9];
    char statusStr[32];
    uint16_t timeColor = TIME_COLOR;
    
    // Format time string based on mode
    switch(currentMode) {
        case NORMAL:
            snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
            snprintf(statusStr, sizeof(statusStr), "Alarm: %02d:%02d %s", 
                    alarmHour, alarmMinute, alarmEnabled ? "ON" : "OFF");
            break;
        case SET_HOUR:
            snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
            snprintf(statusStr, sizeof(statusStr), "Set Hour");
            timeColor = ST77XX_YELLOW;
            break;
        case SET_MINUTE:
            snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
            snprintf(statusStr, sizeof(statusStr), "Set Minute");
            timeColor = ST77XX_YELLOW;
            break;
        case SET_ALARM_HOUR:
            snprintf(timeStr, sizeof(timeStr), "%02d:%02d", alarmHour, alarmMinute);
            snprintf(statusStr, sizeof(statusStr), "Set Alarm Hour");
            timeColor = ALARM_COLOR;
            break;
        case SET_ALARM_MINUTE:
            snprintf(timeStr, sizeof(timeStr), "%02d:%02d", alarmHour, alarmMinute);
            snprintf(statusStr, sizeof(statusStr), "Set Alarm Minute");
            timeColor = ALARM_COLOR;
            break;
    }
    
    // Update time display if changed
    if (String(timeStr) != prevTimeStr) {
        tft.fillRect(0, TIME_Y_POS - 5, SCREEN_WIDTH, 45, BACKGROUND_COLOR);
        centerText(timeStr, TIME_Y_POS, TIME_TEXT_SIZE, timeColor);
        prevTimeStr = timeStr;
    }
    
    // Update status display if changed
    if (String(statusStr) != prevStatusStr) {
        tft.fillRect(0, STATUS_Y_POS - 5, SCREEN_WIDTH, 25, BACKGROUND_COLOR);
        centerText(statusStr, STATUS_Y_POS, STATUS_TEXT_SIZE, STATUS_COLOR);
        prevStatusStr = statusStr;
    }
}

void handleButtons(DateTime& now) {
    // Check each button with proper debouncing
    if (debounceButton(modeButton)) {
        switch(currentMode) {
            case NORMAL: currentMode = SET_HOUR; break;
            case SET_HOUR: currentMode = SET_MINUTE; break;
            case SET_MINUTE: currentMode = SET_ALARM_HOUR; break;
            case SET_ALARM_HOUR: currentMode = SET_ALARM_MINUTE; break;
            case SET_ALARM_MINUTE: currentMode = NORMAL; break;
        }
    }
    
    if (debounceButton(hourButton)) {
        if (currentMode == SET_HOUR) {
            DateTime newTime = DateTime(now.year(), now.month(), now.day(), 
                                    (now.hour() + 1) % 24, now.minute(), now.second());
            rtc.adjust(newTime);
        }
        else if (currentMode == SET_ALARM_HOUR) {
            alarmHour = (alarmHour + 1) % 24;
        }
    }
    
    if (debounceButton(minButton)) {
        if (currentMode == SET_MINUTE) {
            DateTime newTime = DateTime(now.year(), now.month(), now.day(),
                                    now.hour(), (now.minute() + 1) % 60, now.second());
            rtc.adjust(newTime);
        }
        else if (currentMode == SET_ALARM_MINUTE) {
            alarmMinute = (alarmMinute + 1) % 60;
        }
    }
    
    if (debounceButton(alarmButton)) {
        if (alarmTriggered) {
            alarmTriggered = false;
            digitalWrite(BUZZER_PIN, LOW);
        } else {
            alarmEnabled = !alarmEnabled;
        }
    }
}

void checkAlarm(DateTime now) {
    if (alarmEnabled && !alarmTriggered &&
        now.hour() == alarmHour && 
        now.minute() == alarmMinute && 
        now.second() == 0) {
        alarmTriggered = true;
    }
    
    if (alarmTriggered) {
        static unsigned long lastBeep = 0;
        static bool beepState = false;
        if (millis() - lastBeep > 500) {
            beepState = !beepState;
            digitalWrite(BUZZER_PIN, beepState);
            lastBeep = millis();
        }
    }
}

void loop() {
    DateTime now = rtc.now();
    
    handleButtons(now);
    displayTime(now);
    checkAlarm(now);
    
    delay(50);
}