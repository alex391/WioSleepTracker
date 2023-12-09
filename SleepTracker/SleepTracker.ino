#include <Adafruit_SleepyDog.h>

// #include <seeed_graphics_base.h>
// #include <seeed_graphics_define.h>
// #include <seeed_line_chart.h>

#include "TFT_eSPI.h"
TFT_eSPI tft;


#define TIME_STRING_MAX 50  // Should be big enough to hold the time (so "00:00:00 AM\0") TODO: this is the wrong value
#define SET_TIME_STRING_MAX 25 // Big enough to fit "Set the minute: 59\0" TODO: this is the wrong value

#define SLEEP_TIME_MS 1000 // How long sleepydog should sleep for
#define SCREEN_ON_TIMEOUT 5 // After pressing WIO_KEY_A, how long the screen should be on for (will be on for SELEEP_TIME_MS * SCREEN_ON_TIMEOUT ms)

void setup() {
  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);

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

  // TODO: set the font size to something bigger for the clock display here

  attachInterrupt(digitalPinToInterrupt(WIO_KEY_C), showTimeInterrupt, FALLING);
  attachInterrupt(digitalPinToInterrupt(WIO_KEY_A), flashModeInterrupt, FALLING);

  digitalWrite(LCD_BACKLIGHT, LOW);


}


unsigned long msSinceStart = 0; // actuall value will be set by setTime()
unsigned int backlightTimeout = SCREEN_ON_TIMEOUT;
bool flashMode = false;
void loop() {
  unsigned long start = millis(); // We need to keep track of how long these things take

  toggleLed();
  drawTime();

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
  unsigned long end = millis();
  int slept = Watchdog.sleep(SLEEP_TIME_MS);
  msSinceStart += slept + (end - start); // Time might get messed up if it runs for more than 50 days somehow... that's OK!
}

void setTime() {
  digitalWrite(LCD_BACKLIGHT, HIGH);

  int hour = 12;
  int minute = 0;

  // set hours
  bool redraw = true;
  while(digitalRead(WIO_KEY_C) != LOW) {
    if (digitalRead(WIO_5S_UP) == LOW) {
      hour += 1;
      hour = hour % 12;
      redraw = true;
    }
    else if (digitalRead(WIO_5S_DOWN) == LOW) {
      hour -= 1;
      hour = positiveModulo(hour, 12);
      redraw = true;
    }
    hour = (hour == 0) ? 12 : hour;
    if (redraw) {
      tft.fillScreen(TFT_BLACK);
      char setHours[SET_TIME_STRING_MAX];
      sosIfNegative(snprintf(setHours, SET_TIME_STRING_MAX, "Set the hour: %d", hour));
      tft.drawString(setHours, 0, 0);
      redraw = false;
    }
    delay(100);

  }
  while(digitalRead(WIO_KEY_C) == LOW); // Wait for you to release the C key
  // set minutes
  redraw = true;
  while(digitalRead(WIO_KEY_C) != LOW) {
    if (digitalRead(WIO_5S_UP) == LOW) {
      minute += 1;
      minute = minute % 60;
      redraw = true;
    }
    else if (digitalRead(WIO_5S_DOWN) == LOW) {
      minute -= 1;
      minute = positiveModulo(minute, 60);
      redraw = true;
    }
    if (redraw) {
      tft.fillScreen(TFT_BLACK);
      char setMinutes[SET_TIME_STRING_MAX];
      sosIfNegative(snprintf(setMinutes, SET_TIME_STRING_MAX, "Set the minute: %d", minute));
      tft.drawString(setMinutes, 0, 0);
      redraw = false;
    }
    delay(100);
  }
  msSinceStart = hour * 3600000 + minute * 60000;
}

// Draw the screen
int minutesBefore = -1;
void drawTime() {
  unsigned long seconds = msSinceStart / 1000;
  unsigned long minutes = (seconds / 60) % 60;
  unsigned long hours = (seconds / 3600) % 12; 
  hours = hours == 0 ? 12 : hours;
  bool am = ((minutes / 60) % 24) < 12;
  if (minutes == minutesBefore) {
    return; // no need to update screen (reduce flickering, sprites do weird things to sleepydog)
  }
  // draw the time
  tft.fillScreen(TFT_BLACK);
  char str[TIME_STRING_MAX];
  sosIfNegative(snprintf(str, TIME_STRING_MAX, "h: %d m: %d s: %d ms: %d", hours, minutes, seconds, msSinceStart));
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

void flashModeInterrupt() { // for dev purposes, TODO: delete
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
