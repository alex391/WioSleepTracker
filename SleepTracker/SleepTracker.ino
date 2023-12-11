/*
  Alex Leute
  Wio Sleep Tracker
*/

#include <LIS3DHTR.h>
#include <Adafruit_SleepyDog.h>

#include "TFT_eSPI.h"
TFT_eSPI tft;

LIS3DHTR<TwoWire> lis;

#define TIME_STRING_MAX 12  // Should be big enough to hold the time ("00:00:00 AM\0") 
#define SET_TIME_STRING_MAX 20 // Big enough to fit "Set the minute: 59\0"
#define HOURS_STRING_MAX 25 // Big enough for "Slept for %2.1f hours\0"

#define SLEEP_TIME_MS 10000 // How long sleepydog should sleep for in ms.
#define MAX_DATA_POINTS 8640 // number of SLEEP_TIME_MS intervals in a day

#define SCREEN_ON_TIMEOUT 1 // After pressing WIO_KEY_A, how long the screen should be on for (will be on for SELEEP_TIME_MS * SCREEN_ON_TIMEOUT ms)

// HACK: Can't use Serial and SleepyDog at the same time.
#define DEBUG_MODE false // Disable sleepydog and a bunch of other annoyances while debugging when true


void setup() {
  
  if (DEBUG_MODE) {
    Serial.begin(115200);
    while(!Serial);
  }

  lis.begin(Wire1);
  if (!lis) {
    sos();
  }
  lis.setOutputDataRate(LIS3DHTR_DATARATE_25HZ); //Data output rate
  lis.setFullScaleRange(LIS3DHTR_RANGE_2G); //Scale range set to 2g

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
  pinMode(WIO_MIC, INPUT);

  setTime(); // Need to do this before we set the interupts

  attachInterrupt(digitalPinToInterrupt(WIO_KEY_C), showTimeInterrupt, FALLING);
  attachInterrupt(digitalPinToInterrupt(WIO_KEY_B), endSleepInterrupt, FALLING);
  attachInterrupt(digitalPinToInterrupt(WIO_KEY_A), flashModeInterrupt, FALLING);

  digitalWrite(LCD_BACKLIGHT, LOW);

  tft.setTextSize(6); // The text size for the clock face
}


unsigned long msSinceStart = 0; // Milliseconds since the start. Actuall initial value will be set by setTime().
unsigned int backlightTimeout = SCREEN_ON_TIMEOUT;
bool flashMode = false;
bool endSleep = false;
float accelrometerData[MAX_DATA_POINTS];
uint32_t volumeData[MAX_DATA_POINTS];
size_t dataPoint = 0; // index into data
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

  // Log acclerometer data and volume data:
  if (dataPoint < MAX_DATA_POINTS) {
    accelrometerData[dataPoint] = measureAcceleration();
    volumeData[dataPoint] = measureVolume();
    dataPoint++;
  }

  if (DEBUG_MODE) {
    for (size_t i = 0; i < dataPoint; i++) {
      Serial.print(accelrometerData[i]);
      Serial.print(", ");
    }
    Serial.println();

    for (size_t i = 0; i < dataPoint; i++) {
      Serial.print(volumeData[i]);
      Serial.print(", ");
    }
    Serial.println();
  }

  if (endSleep) {
    end();
  }
  
  unsigned long end = millis();
  int slept = 0;
  if (!DEBUG_MODE) {
    slept = Watchdog.sleep(SLEEP_TIME_MS);
  }
  else {
    slept = SLEEP_TIME_MS;
    delay(SLEEP_TIME_MS);
  }
  msSinceStart += slept + (end - start);
}

// Set the time
void setTime() {
  if (DEBUG_MODE) {
    Serial.println(msSinceStart);
  }
  tft.setTextSize(2);
  digitalWrite(LCD_BACKLIGHT, HIGH);

  
  bool am = true;

  unsigned int hour = setTimeNumber(12, "Set the hour: %d", true);
  unsigned int minute = setTimeNumber(60, "Set the minute: %d", false);

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
      sosIfNegative(snprintf(setAM, SET_TIME_STRING_MAX, "Set AM or PM: %s", am? "AM": "PM"));
      tft.drawString(setAM, 0, 0);
      redraw = false;
    }
    delay(100);
  }

  if (!am) {
    hour += 12;
  }
  unsigned int seconds = hour * 3600 + minute * 60;
  msSinceStart = seconds * 1000;

  if (DEBUG_MODE) {
    Serial.println(seconds);
    Serial.println(msSinceStart);
  }
}

// Let the user pick a number (0 <= number < upperBounds) and display that number in %d of formatString
// If noZero is true, then it will display upperBounds rather than 0
unsigned int setTimeNumber(unsigned int upperBounds, const char * formatString, bool noZero) {
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
void drawTime() {
  unsigned int seconds = msSinceStart / 1000;
  unsigned int minutes = (seconds / 60) % 60;
  unsigned int hours = (seconds / 3600) % 12; 
  hours = hours == 0 ? 12 : hours;
  bool am = ((seconds / 3600) % 24) < 12;
  if (DEBUG_MODE) {
    char buff[200];
    sosIfNegative(snprintf(buff, sizeof buff, "seconds: %d, hours: %d, minutes: %d, am: %d\n", seconds, hours, minutes, am));
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

// Measure the acceleration
float measureAcceleration() {
  float z = lis.getAccelerationZ();
  if (DEBUG_MODE) {
    Serial.print("Z: ");
    Serial.println(z);
  }
  return z;
}

// Measure the volume
uint32_t measureVolume() {
  uint32_t volume = analogRead(WIO_MIC);
  if (DEBUG_MODE) {
    Serial.print("Volume: ");
    Serial.println(volume);
  }
  return volume;
}


// The function that runs at the end of the program
// Display the data collected throughout the night
void end() {
  unsigned long endTime = msSinceStart;
  digitalWrite(LCD_BACKLIGHT, HIGH);
  tft.fillScreen(TFT_BLACK);

  // BUG: There's an off by one error here that makes the graph not take up the whole screen
  // FIXME: DRY
  size_t i = 0;
  const long acclerometerMax = 204;
  const long acclerometerMin = -204;
  // due to screen orientation, height and width are flipped for TFT_WIDTH and TFT_HEIGHT
  int32_t previousPointY = map((long)(accelrometerData[i] * 100.0f), acclerometerMin, acclerometerMax, 0, TFT_WIDTH); // accelerometer data will be from roughly -2G to 2G, and map it to the height of the screen
  int32_t previousPointX = map((long)i, 0, dataPoint, 0, TFT_HEIGHT);
  i++;
  for (; i < dataPoint; i++) {
    int32_t pointY = map((long)(accelrometerData[i] * 100.0f), acclerometerMin, acclerometerMax, 0, TFT_WIDTH);
    int32_t pointX = map((long)i, 0, dataPoint, 0, TFT_HEIGHT);
    if (DEBUG_MODE) {
      Serial.printf("plotting from (%d, %d) to (%d, %d)\n", previousPointX, previousPointY, pointX, pointY);
    }
    tft.drawLine(previousPointX, previousPointY, pointX, pointY, TFT_GREEN);
    previousPointY = pointY;
    previousPointX = pointX;
  }

  i = 0;
  const long micMin = 0;
  const long micMax = 1023;
  previousPointY = map((long)volumeData[i], micMin, micMax, 0, TFT_WIDTH);
  previousPointX = map((long)i, 0, dataPoint, 0, TFT_HEIGHT);
  i++;
  for (; i < dataPoint; i++) {
    int32_t pointY = map((long)volumeData[i], micMin, micMax, 0, TFT_WIDTH);
    int32_t pointX = map((long)i, 0, dataPoint, 0, TFT_HEIGHT);
    if (DEBUG_MODE) {
      Serial.printf("plotting from (%d, %d) to (%d, %d)\n", previousPointX, previousPointY, pointX, pointY);
    }
    tft.drawLine(previousPointX, previousPointY, pointX, pointY, TFT_BLUE);
    previousPointY = pointY;
    previousPointX = pointX;
  }

  // Draw the number of hours on the scren
  float hours = (float)endTime / 3600000.0f;
  tft.setTextSize(2);
  char str[HOURS_STRING_MAX];
  sosIfNegative(snprintf(str, HOURS_STRING_MAX, "Slept for %2.1f hours", hours));
  tft.drawString(str,0 ,0 );

  while(true); // Loop forever at the end of the program
}

// Thanks https://stackoverflow.com/a/14997413 by Martin B https://stackoverflow.com/users/134877/martin-b
// Strictly positive modulo, like '%' in Python
unsigned int positiveModulo(int i, int n) {
    return (i % n + n) % n;
}

// Interupt to show the time
void showTimeInterrupt() {
  backlightTimeout = SCREEN_ON_TIMEOUT;
}

void flashModeInterrupt() { // For dev purposes
  flashMode = true;
}

// Interupt to end sleep and display the data
void endSleepInterrupt() {
  endSleep = true;
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
