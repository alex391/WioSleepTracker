#include <Adafruit_SleepyDog.h>

// #include <seeed_graphics_base.h>
// #include <seeed_graphics_define.h>
// #include <seeed_line_chart.h>

#include "TFT_eSPI.h"
TFT_eSPI tft;


#define TIME_STRING_MAX 50  // Should be big enough to hold the time (so "00:00:00 AM\0") TODO: this is the wrong value
#define SET_TIME_STRING_MAX 19 // Big enough to fit "Set the minute: 59\0"
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

  attachInterrupt(digitalPinToInterrupt(WIO_KEY_C), showTimeInterrupt, FALLING);
  attachInterrupt(digitalPinToInterrupt(WIO_KEY_A), flashModeInterrupt, FALLING);

  digitalWrite(LCD_BACKLIGHT, LOW);

}


unsigned long msSinceStart = 0;
unsigned int backlightTimeout = 0;
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
    // makes it easier to uplad things without hard reset
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
  // TODO: the rest of the function
  tft.drawString("Set the hour: ", 0, 0);
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
  int error = snprintf(str, TIME_STRING_MAX, "h: %d m: %d s: %d ms: %d", hours, minutes, seconds, msSinceStart);
  if (error < 0) {
    sos();
  }
  tft.drawString(str, 0, 0);

  minutesBefore = minutes;
}

// Interupt to show the time
void showTimeInterrupt() {
  backlightTimeout = SCREEN_ON_TIMEOUT;
}

void flashModeInterrupt() { // for dev purposes, TODO: delete
  flashMode = true;
}

// For when things go irrecoverably wrong
void sos() {
  const unsigned long shortDelay = 100;
  const unsigned long longDelay = shortDelay * 2;
  while (true) {
    for (int i = 0; i < 3; i++) {
      blink(shortDelay);
    }
    for (int i = 0; i < 3; i++) {
      blink(longDelay);
    }
    for (int i = 0; i < 3; i++) {
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
