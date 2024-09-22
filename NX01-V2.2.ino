#include <Wire.h>
#include <RTClib.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <OneButton.h>

// Initialize the RTC
RTC_DS3231 rtc;

// Define Nixie tube control pins (assuming SN74141 or similar IC)
const int pinD = 9;
const int pinC = 10;
const int pinB = 11;
const int pinA = 12;
const int EnPin = 5;
// Interrupt pin for waking up and button input
const int buttonPin = 7;

// Initialize the OneButton library
OneButton button(buttonPin, true, true);

void wakeUp() {
  // This function is called when the interrupt is triggered but does nothing
  // It's required to bring the Arduino out of sleep mode
}

void enterSleep() {
  digitalWrite(EnPin, LOW); 
  Wire.end();
  set_sleep_mode(SLEEP_MODE_PWR_DOWN); // Set the sleep mode to power down
  ADCSRA = 0; // Disable ADC to save power
  sleep_enable(); // Enable sleep mode
  attachInterrupt(digitalPinToInterrupt(buttonPin), wakeUp, LOW); // Set wakeUp() to run when pin goes LOW
  
  sleep_mode(); // Enter sleep mode

  // Code resumes here after wake up
  detachInterrupt(digitalPinToInterrupt(buttonPin)); // Disable interrupt so it doesn't trigger again immediately
  sleep_disable(); // Disable sleep mode
  digitalWrite(EnPin, HIGH);
}

void setup() {
  // Start the I2C interface
  Wire.begin();
  // Start the serial interface
  Serial.begin(9600);
  
  // Initialize the RTC
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  
  // Setup pin modes
  pinMode(pinD, OUTPUT);
  pinMode(pinC, OUTPUT);
  pinMode(pinB, OUTPUT);
  pinMode(pinA, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP); // Set button pin as input with pull-up
  pinMode(LED_BUILTIN, OUTPUT); // Configure built-in LED for feedback

  // Initialize button actions
  button.attachClick(handleClick);
  button.attachDoubleClick(handleDoubleClick);
  button.attachLongPressStart(handleLongPressStart); // Attach the long press action

  // Blank the Nixie tubes to ensure they are turned off to conserve power
  blank_nixie();
  
  // Enter sleep mode immediately after setup
  enterSleep();
}

void loop() {
  button.tick(); // Check the status of the button
}

// Function to handle single click: Flash the current time
void handleClick() {
  flash_time();
  enterSleep(); // Re-enter sleep after displaying time
}

// Function to handle double click: Flash the date
void handleDoubleClick() {
  flash_date();
  enterSleep(); // Re-enter sleep after displaying date
}

// Function to handle long press: Cycle digits once
void handleLongPressStart() {
  cycle_digits_once(); // Cycle through digits once on long press
  enterSleep(); // Re-enter sleep after cycling digits
}

// Function to set the digit on the Nixie tube
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
  
  int index = digit - '0'; // Convert ASCII character to integer index
  digitalWrite(pinD, truth_table[index][0]);
  digitalWrite(pinC, truth_table[index][1]);
  digitalWrite(pinB, truth_table[index][2]);
  digitalWrite(pinA, truth_table[index][3]);
}

// Function to blank the Nixie tube
void blank_nixie() {
  digitalWrite(pinD, HIGH);
  digitalWrite(pinC, HIGH);
  digitalWrite(pinB, HIGH);
  digitalWrite(pinA, HIGH);
  delay(200); // Blank the tube for 0.2 seconds
}

// Function to cycle through digits 0-9 once
void cycle_digits_once() {
  for (int digit = 0; digit < 10; digit++) {
    display_digit('0' + digit); // Convert integer to ASCII character
    delay(500); // Slow cycle through digits
  }
  blank_nixie(); // Ensure the Nixie tube is blanked after cycling through digits
}

void flash_time() {
  DateTime now = rtc.now();

  int seconds = now.second();
  int minute = now.minute();
  int hour = now.hour();

  int hour12 = hour % 12; // Convert 24hr to 12hr format
  if (hour12 == 0) hour12 = 12; // Adjust 0 hour to 12 for 12-hour clock format

  char timeStr[5]; // Buffer to hold hour and minute
  sprintf(timeStr, "%02d%02d", hour12, minute);

  int startIndex = 0; // Start index to loop from
  if (timeStr[0] == '0') {
    startIndex = 1; // Skip the first digit if it is '0'
  }

  for (int part = startIndex; part < 4; part++) { // Loop through each part of the time starting from startIndex
    blank_nixie(); // Blank tube before showing next digit
    display_digit(timeStr[part]); // Display current part of the time
    delay(300); // Show each digit for 0.3 seconds
  }

  // Blank the Nixie tube after displaying the time to conserve power
  blank_nixie();
}

void flash_date() {
  DateTime now = rtc.now();

  int day = now.day();
  int month = now.month();

  char dateStr[5]; // Buffer to hold month and day
  sprintf(dateStr, "%02d%02d", month, day);

  for (int part = 0; part < 2; part++) { // Loop through month digits
    blank_nixie(); // Blank tube before showing next digit
    display_digit(dateStr[part]); // Display current part of the month
    delay(300); // Show each digit for 0.5 seconds
  }
  
  blank_nixie();
  delay(200); // Add a 1 second space between month and day

  for (int part = 2; part < 4; part++) { // Loop through day digits
    blank_nixie(); // Blank tube before showing next digit
    display_digit(dateStr[part]); // Display current part of the day
    delay(300); // Show each digit for 0.4 seconds
  }

  // Blank the Nixie tube after displaying the date to conserve power
  blank_nixie();
}