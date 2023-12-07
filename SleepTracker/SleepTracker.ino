#include <Adafruit_SleepyDog.h>

#include <seeed_graphics_base.h>
#include <seeed_graphics_define.h>
#include <seeed_line_chart.h>

#include "TFT_eSPI.h"
TFT_eSPI tft;


#define TIME_STRING_MAX 9  // Should be big enough to hold the time (so "00:00:00 AM\0")
#define SLEEP_TIME 1000 // how long to sleep for

void setup() {
  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(4);
  digitalWrite(LCD_BACKLIGHT, HIGH);

  pinMode(LED_BUILTIN, OUTPUT);  // For blinking when we do stuff

}

bool am = true;          // true if it is AM
int hours = 0;   // 0-24 (should mod, 0-11 is AM, 12-24 is PM)
int minutes = 0; 
int seconds = 0;
int msSinceStart = 10500000;
void loop() {
  unsigned long start = millis(); // We need to keep track of how long drawTime takes so that our time is accurate (drawing to the screen is slow)
  seconds = (msSinceStart / 1000);
  minutes = (seconds / 60) % 60;
  hours = (seconds / 3600) % 12;
  hours = hours == 0 ? 12 : hours;
  am = ((minutes / 60) % 24) < 12;
  drawTime();
  toggleLed();
  unsigned long end = millis();
  int slept = Watchdog.sleep(SLEEP_TIME);
  msSinceStart += slept + (end - start);
  
}
int minutesBefore = -1;
void drawTime() {
  if (minutes == minutesBefore) {
    return; // no need to update screen (reduce flickering, sprites do weird things to sleepydog)
  }
  // draw the time
  tft.fillScreen(TFT_BLACK);
  char str[TIME_STRING_MAX];
  int error = snprintf(str, TIME_STRING_MAX, "%.2d:%.2d %s", hours, minutes, am ? "AM" : "PM");
  if (error < 0) {
    sos();
  }
  tft.drawString(str, 0, 0);

  
  minutesBefore = minutes;
}

void sos() {  // For when things go irrecoverably wrong
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

void blink(unsigned long t) {
  digitalWrite(LED_BUILTIN, HIGH);
  delay(t);
  digitalWrite(LED_BUILTIN, LOW);
  delay(t);
}

uint32_t ledState = HIGH;
void toggleLed() {
  ledState = ledState == LOW ? HIGH : LOW;
  digitalWrite(LED_BUILTIN, ledState);
}
