/*
  Alex Leute
  Wio Sleep Tracker
*/



#include <Adafruit_SleepyDog.h>

// #include <seeed_graphics_base.h>
// #include <seeed_graphics_define.h>
// #include <seeed_line_chart.h>

#include "TFT_eSPI.h"
TFT_eSPI tft;


#define TIME_STRING_MAX 12  // Should be big enough to hold the time ("00:00:00 AM\0") 
#define SET_TIME_STRING_MAX 20 // Big enough to fit "Set the minute: 59\0"

#define SLEEP_TIME_MS 10000 // How long sleepydog should sleep for in ms
#define SCREEN_ON_TIMEOUT 1 // After pressing WIO_KEY_A, how long the screen should be on for (will be on for SELEEP_TIME_MS * SCREEN_ON_TIMEOUT ms)

#define DEBUG_MODE false // Disable sleepydog and a bunch of other annoyances while debugging when true


void setup() {
  if (DEBUG_MODE) {
    Serial.begin(115200);
    while(!Serial);

  }
  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);

  pinMode(LED_BUILTIN, OUTPUT);  // For blinking when we do stuff

  pinMode(WIO_5S_UP, INPUT_PULLUP);
  pinMode(WIO_5S_DOWN, INPUT_PULLUP);
  pinMode(WIO_5S_LEFT, INPUT_PULLUP);
  pinMode(WIO_5S_RIGHT, INPUT_PULLUP);
  pinMode(WIO_5S_PRESS, INPUT_PULLUP);
  pinMode(WIO_KEY_A, INPUT_PULLUP);
  pinMode(WIO_KEY_B, INPUT_PULLUP);
  pinMode(WIO_KEY_C, INPUT_PULLUP);

  setTime(); // Need to do this before we set the interupts

  attachInterrupt(digitalPinToInterrupt(WIO_KEY_C), showTimeInterrupt, FALLING);
  attachInterrupt(digitalPinToInterrupt(WIO_KEY_A), flashModeInterrupt, FALLING);

  digitalWrite(LCD_BACKLIGHT, LOW);

  tft.setTextSize(6); // The text size for the clock face
}


uint64_t usSinceStart = 0; // Microseconds since the start. Actuall initial value will be set by setTime().
unsigned int backlightTimeout = SCREEN_ON_TIMEOUT;
bool flashMode = false;
void loop() {
  unsigned long start = micros(); // We need to keep track of how long these things take

  toggleLed();  
  drawTime(usSinceStart);

  if (backlightTimeout > 0) {
    digitalWrite(LCD_BACKLIGHT, HIGH);
    backlightTimeout--;
  } else {
    digitalWrite(LCD_BACKLIGHT, LOW);
  }

  if (flashMode) {
    // for development purposes (will remove later)
    // makes it easier to upload things without hard reset
    while(true) {
      #if defined(USBCON) && !defined(USE_TINYUSB)
        USBDevice.attach();
      #endif
      delay(100);
      toggleLed();
    }
  }
  unsigned long end = micros();
  int slept = 0;
  if (!DEBUG_MODE) {
    slept = Watchdog.sleep(SLEEP_TIME_MS);
  }
  if (end < start) {
    // Integer overflow of micros() detected (happens every 70 minutes)
    end = start;
  }
  usSinceStart += slept * 1000 + (end - start);
}

void setTime() {
  if (DEBUG_MODE) {
    Serial.println((long) usSinceStart);
  }
  tft.setTextSize(2);
  digitalWrite(LCD_BACKLIGHT, HIGH);

  
  bool am = true;

  int hour = setTimeNumber(12, "Set the hour: %d", true);
  int minute = setTimeNumber(60, "Set the minute: %d", false);

  if (DEBUG_MODE) {
    Serial.printf("set hours %d\n", hour);
    Serial.printf("set minute %d\n", minute);
  }

  bool redraw = true;
  // set AM or PM
  while(digitalRead(WIO_KEY_C) != LOW) {
    if (digitalRead(WIO_5S_UP) == LOW || digitalRead(WIO_5S_DOWN) == LOW) {
      am = !am;
      redraw = true;
    }
    if (redraw) {
      tft.fillScreen(TFT_BLACK);
      char setAM[SET_TIME_STRING_MAX];
      sosIfNegative(snprintf(setAM, SET_TIME_STRING_MAX, "Set AM or PM: %s", am ? "AM": "PM"));
      tft.drawString(setAM, 0, 0);
      redraw = false;
    }
    delay(100);
  }

  if (!am) {
    hour += 12;
  }
  uint64_t seconds = (hour * 3600 + minute * 60);
  usSinceStart = seconds * 1000000;

  if (DEBUG_MODE) {
    Serial.println((long)seconds);
    Serial.println((long)usSinceStart);
  }
}

// Let the user pick a number (0 <= number < upperBounds) and display that number in %d of formatString
// If noZero is true, then it will display upperBounds rather than 0
int setTimeNumber(int upperBounds, const char* formatString, bool noZero) {
  bool redraw = true;
  int number = 0;
  while(digitalRead(WIO_KEY_C) != LOW) {
    if (digitalRead(WIO_5S_UP) == LOW) {
      number += 1;
      number = number % upperBounds;
      redraw = true;
    }
    else if (digitalRead(WIO_5S_DOWN) == LOW) {
      number -= 1;
      number = positiveModulo(number, upperBounds);
      redraw = true;
    }
    if (redraw) {
      tft.fillScreen(TFT_BLACK);
      char str[SET_TIME_STRING_MAX];
      sosIfNegative(snprintf(str, SET_TIME_STRING_MAX, formatString, (number == 0 && noZero)? upperBounds : number));
      tft.drawString(str, 0, 0);
      redraw = false;
    }
    delay(100);
  }

  while(digitalRead(WIO_KEY_C) == LOW); // Wait for you to release the C key
  return number;

}


// Draw the screen
unsigned int minutesBefore = 1;
void drawTime(uint64_t microseconds) {
  uint64_t seconds = microseconds / 1000000;
  unsigned int minutes = ((int)seconds / 60) % 60;
  unsigned int hours = ((int)seconds / 3600) % 12; 
  hours = hours == 0 ? 12 : hours;
  bool am = (((int)seconds / 3600) % 24) < 12;
  if (DEBUG_MODE) {
    char buff[200];
    sosIfNegative(snprintf(buff, sizeof buff, "seconds: %d, hours: %d, minutes: %d, am: %d\n", (int)seconds, hours, minutes, am));
    Serial.print(buff);
  }
  if (minutes == minutesBefore && !DEBUG_MODE) {
    return; // no need to update screen (reduce flickering, sprites do weird things to sleepydog)
  }
  // draw the time
  tft.fillScreen(TFT_BLACK);
  char str[TIME_STRING_MAX];
  sosIfNegative(snprintf(str, TIME_STRING_MAX, "%.2d:%.2d %s", hours, minutes, am ? "AM" : "PM"));
  tft.drawString(str, 0, 0);

  minutesBefore = minutes;
}

// Thanks https://stackoverflow.com/a/14997413 by Martin B https://stackoverflow.com/users/134877/martin-b
// Strictly positive modulo, like '%' in Python
int positiveModulo(int i, int n) {
    return (i % n + n) % n;
}

// Interupt to show the time
void showTimeInterrupt() {
  backlightTimeout = SCREEN_ON_TIMEOUT;
}

void flashModeInterrupt() { // For dev purposes
  flashMode = true;
}

// For error checking (mostly snprintf)
void sosIfNegative(int check) {
  if (check < 0) {
    sos();
  }
}

// For when things go irrecoverably wrong
void sos() {
  const unsigned long shortDelay = 100;
  const unsigned long longDelay = shortDelay * 2;
  while (true) {
    for (char i = 0; i < 3; i++) {
      blink(shortDelay);
    }
    for (char i = 0; i < 3; i++) {
      blink(longDelay);
    }
    for (char i = 0; i < 3; i++) {
      blink(shortDelay);
    }
    delay(shortDelay);
  }
}

// Blink the LED
void blink(unsigned long t) {
  digitalWrite(LED_BUILTIN, HIGH);
  delay(t);
  digitalWrite(LED_BUILTIN, LOW);
  delay(t);
}

// Toggle the LED
uint32_t ledState = HIGH;
void toggleLed() {
  ledState = ledState == LOW ? HIGH : LOW;
  digitalWrite(LED_BUILTIN, ledState);
}


/*
“I’m going to a commune in Vermont and will deal with no unit of time shorter than a season.”
— Tracy Kidder
*/
