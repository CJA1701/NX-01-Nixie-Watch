#include <Wire.h>
#include <RTClib.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include <OneButton.h>

RTC_DS3231 rtc;

const int pinD = 9;
const int pinC = 10;
const int pinB = 11;
const int pinA = 12;
const int EnPin = 5;
const int buttonPin = 7;
const int battPin = A5;

const int timeDisplayBuffer = 55;

const float battFullV = 4.2;
const float battEmptyV = 3.0;
const float dividerRatio = 100.0 / (470.0 + 100.0);

OneButton button(buttonPin, true, true);

volatile bool justWokeUp = false;

int lastDigit = -1;

unsigned long longPressStartTime = 0;
bool resetTriggered = false;
int lastCountdown = 0;

void wakeUp() {
  justWokeUp = true;
}

void enterSleep() {
  digitalWrite(EnPin, LOW);
  Wire.end();
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  ADCSRA = 0;
  sleep_enable();
  attachInterrupt(digitalPinToInterrupt(buttonPin), wakeUp, LOW);

  sleep_mode();

  detachInterrupt(digitalPinToInterrupt(buttonPin));
  sleep_disable();
  ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
  digitalWrite(EnPin, HIGH);

  justWokeUp = true;
}

void setup() {
  MCUSR = 0;
  wdt_disable();

  Wire.begin();

  if (!rtc.begin()) {
    while (1);
  }

  if (rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  pinMode(pinD, OUTPUT);
  pinMode(pinC, OUTPUT);
  pinMode(pinB, OUTPUT);
  pinMode(pinA, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(battPin, INPUT);

  analogReference(INTERNAL);

  button.attachClick(handleClick);
  button.attachDoubleClick(handleDoubleClick);
  button.attachLongPressStart(handleLongPressStart);
  button.attachDuringLongPress(handleDuringLongPress);
  button.attachLongPressStop(handleLongPressStop);
  button.attachMultiClick(handleMultiClick);

  blank_nixie();

  enterSleep();
}

unsigned long lastWakeTime = 0;

void loop() {
  if (justWokeUp) {
    lastWakeTime = millis();
    button.tick();
    justWokeUp = false;
  }

  button.tick();

  if (millis() - lastWakeTime > 5000) {
    enterSleep();
  }
}

void handleClick() {
  if (rtc.lostPower()) {
    flash_error();
  } else {
    flash_time();
  }
  enterSleep();
}

void handleDoubleClick() {
  if (rtc.lostPower()) {
    flash_error();
  } else {
    flash_date();
  }
  enterSleep();
}

void handleLongPressStart() {
  resetTriggered = false;
  lastCountdown = 0;
  display_battery();
  longPressStartTime = millis();
}

void handleDuringLongPress() {
  if (resetTriggered) return;

  unsigned long elapsed = millis() - longPressStartTime;

  if (elapsed >= 4000) {
    resetTriggered = true;
    for (int i = 0; i < 3; i++) {
      display_digit('9');
      delay(60);
      display_digit('6');
      delay(60);
    }
    blank_nixie();
  } else if (elapsed >= 3000 && lastCountdown != 1) {
    lastCountdown = 1;
    display_digit('1');
  } else if (elapsed >= 2000 && lastCountdown != 2) {
    lastCountdown = 2;
    display_digit('2');
  } else if (elapsed >= 1000 && lastCountdown != 3) {
    lastCountdown = 3;
    display_digit('3');
  }
}

void handleLongPressStop() {
  if (resetTriggered) {
    waitForSerialSync();
  }
  blank_nixie();
  enterSleep();
}

void handleMultiClick() {
  int clicks = button.getNumberClicks();

  if (clicks == 3) {
    if (rtc.lostPower()) {
      flash_error();
    } else {
      display_temperature();
    }
  }

  enterSleep();
}

void waitForSerialSync() {
  USBDevice.detach();
  delay(250);
  USBDevice.attach();
  Serial.begin(9600);

  // Flash 9-6 while USB enumerates with the host (~3 seconds)
  for (int i = 0; i < 3; i++) {
    display_digit('9');
    delay(250);
    blank_nixie();
    delay(50);
    display_digit('6');
    delay(250);
    blank_nixie();
    delay(50);
  }

  while (Serial.available()) Serial.read();

  while (true) {
    display_digit('9');
    delay(200);
    blank_nixie();
    delay(50);
    display_digit('6');
    delay(200);
    blank_nixie();

    unsigned long waitStart = millis();
    while (millis() - waitStart < 1500) {
      if (Serial.available()) {
        long timestamp = Serial.parseInt();
        if (timestamp > 1000000000L) {
          Wire.begin();
          rtc.adjust(DateTime((uint32_t)timestamp));

          for (int i = 9; i >= 0; i--) {
            display_digit('0' + i);
            delay(100);
          }
          blank_nixie();

          Serial.end();
          return;
        }
      }
      if (digitalRead(buttonPin) == LOW) {
        delay(50);
        if (digitalRead(buttonPin) == LOW) {
          while (digitalRead(buttonPin) == LOW);
          Serial.end();
          return;
        }
      }
    }
  }
}

void flash_error() {
  unsigned long startTime = millis();
  while (millis() - startTime < 3000) {
    display_digit('9');
    delay(100);
  }
  blank_nixie();
}

void display_digit(char digit) {
  int truth_table[10][4] = {
    {0, 0, 0, 0}, // 0
    {0, 0, 0, 1}, // 1
    {0, 0, 1, 0}, // 2
    {0, 0, 1, 1}, // 3
    {0, 1, 0, 0}, // 4
    {0, 1, 0, 1}, // 5
    {0, 1, 1, 0}, // 6
    {0, 1, 1, 1}, // 7
    {1, 0, 0, 0}, // 8
    {1, 0, 0, 1}  // 9
  };

  int index = digit - '0';
  digitalWrite(pinD, truth_table[index][0]);
  digitalWrite(pinC, truth_table[index][1]);
  digitalWrite(pinB, truth_table[index][2]);
  digitalWrite(pinA, truth_table[index][3]);
}

void blank_nixie() {
  digitalWrite(pinD, HIGH);
  digitalWrite(pinC, HIGH);
  digitalWrite(pinB, HIGH);
  digitalWrite(pinA, HIGH);
  delay(200);
}

void smooth_scroll(int fromDigit, int toDigit) {
  if (fromDigit == -1) {
    display_digit('0' + toDigit);
    return;
  }

  int current = fromDigit;

  while (current != toDigit) {
    display_digit('0' + current);
    delay(30);
    current = (current + 1) % 10;
  }

  display_digit('0' + toDigit);
}

void scramble_to_digit(int targetDigit, int scrambleCount) {
  Wire.begin();

  DateTime now = rtc.now();
  randomSeed(now.second() * 1000 + now.minute());

  for (int i = 0; i < scrambleCount; i++) {
    int randomDigit = random(0, 10);
    display_digit('0' + randomDigit);
    delay(40);
  }

  display_digit('0' + targetDigit);
}

void flash_time() {
  Wire.begin();

  DateTime now = rtc.now();

  int seconds = now.second();
  int minute = now.minute();
  int hour = now.hour();

  if (seconds >= timeDisplayBuffer) {
    minute++;
    if (minute == 60) {
      minute = 0;
      hour++;
      if (hour == 24) {
        hour = 0;
      }
    }
  }

  int hour12 = hour % 12;
  if (hour12 == 0) hour12 = 12;

  char timeStr[5];
  sprintf(timeStr, "%02d%02d", hour12, minute);

  int startIndex = 0;
  if (timeStr[0] == '0') {
    startIndex = 1;
  }

  lastDigit = -1;

  for (int part = startIndex; part < 4; part++) {
    int targetDigit = timeStr[part] - '0';

    if (part == startIndex) {
      scramble_to_digit(targetDigit, 8);
    } else {
      smooth_scroll(lastDigit, targetDigit);
    }

    lastDigit = targetDigit;
    delay(250);
    blank_nixie();
    delay(50);
  }

  blank_nixie();
  lastDigit = -1;
}

void flash_date() {
  Wire.begin();

  DateTime now = rtc.now();

  int day = now.day();
  int month = now.month();

  char dateStr[5];
  sprintf(dateStr, "%02d%02d", month, day);

  lastDigit = -1;

  for (int part = 0; part < 2; part++) {
    int targetDigit = dateStr[part] - '0';

    if (part == 0) {
      scramble_to_digit(targetDigit, 6);
    } else {
      smooth_scroll(lastDigit, targetDigit);
    }

    lastDigit = targetDigit;
    delay(200);
    blank_nixie();
    delay(50);
  }

  blank_nixie();
  delay(250);
  lastDigit = -1;

  for (int part = 2; part < 4; part++) {
    int targetDigit = dateStr[part] - '0';

    if (part == 2) {
      scramble_to_digit(targetDigit, 6);
    } else {
      smooth_scroll(lastDigit, targetDigit);
    }

    lastDigit = targetDigit;
    delay(200);
    blank_nixie();
    delay(50);
  }

  blank_nixie();
  lastDigit = -1;
}

void display_battery() {
  analogRead(battPin);
  delay(10);
  int raw = analogRead(battPin);

  float voltage = (raw * 2.56) / 1024.0 / dividerRatio;

  int percent = (int)((voltage - battEmptyV) / (battFullV - battEmptyV) * 99.0 + 0.5);
  if (percent > 99) percent = 99;
  if (percent < 0) percent = 0;

  char battStr[3];
  sprintf(battStr, "%02d", percent);

  lastDigit = -1;

  for (int i = 0; i < 2; i++) {
    int targetDigit = battStr[i] - '0';

    if (i == 0) {
      scramble_to_digit(targetDigit, 6);
    } else {
      smooth_scroll(lastDigit, targetDigit);
    }

    lastDigit = targetDigit;
    delay(300);
    blank_nixie();
    delay(50);
  }

  blank_nixie();
  lastDigit = -1;
}

void display_temperature() {
  Wire.begin();

  float temp = rtc.getTemperature();
  int tempF = (int)(temp * 9.0 / 5.0 + 32.0);

  char tempStr[4];
  sprintf(tempStr, "%3d", tempF);

  lastDigit = -1;

  for (int i = 0; i < 3; i++) {
    if (tempStr[i] == ' ') continue;

    int targetDigit = tempStr[i] - '0';

    if (lastDigit == -1) {
      scramble_to_digit(targetDigit, 8);
    } else {
      smooth_scroll(lastDigit, targetDigit);
    }

    lastDigit = targetDigit;
    delay(300);
    blank_nixie();
    delay(50);
  }

  blank_nixie();
  lastDigit = -1;
}
