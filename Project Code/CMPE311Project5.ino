#include "fan.h"                // fan module: fanInit() (Timer1 PWM on pin 9) and lightUpdate() (fan state machine)
#include "button.h"             // button module: buttonInit() (pin 10 input with pull-up) and checkbuttonReleased()
#include <Arduino_FreeRTOS.h>   // FreeRTOS: xTaskCreate(), vTaskDelay(), pdMS_TO_TICKS()

// ===== Serial input buffer =====
String inputBuffer = "";  // builds up one line of typed input, character by character

// ===== Configuration captured from the user =====
int getLED1Num   = 0;   // set to 1 once the user picks LED 1
int getLED2Num   = 0;   // set to 2 once the user picks LED 2
unsigned long getInterval1 = 0;   // blink interval for LED 1, in ms
unsigned long getInterval2 = 0;   // blink interval for LED 2, in ms

// ===== Question-flow state machine flags =====
bool askedLED      = false;  // have we asked "What LED?" for the current round?
bool askedInterval = false;  // have we asked "What interval?" for the current round?

bool gotLine = true;       // true exactly when a full line of input just finished
int value;                 // holds the number parsed from that completed line
int currentSelection = 0;  // which LED this round's interval answer belongs to (1 or 2)


void askQuestions(void *pvParameters);
void serviceLED1(void *pvParameters);
void serviceLED2(void *pvParameters);
void checkButton(void *pvParameters);   // fan task: steps the fan speed on each debounced button release (calls lightUpdate())
void changeFrequency1(int LEDNum, unsigned long LEDInterval);
void changeFrequency2(int LEDNum, unsigned long LEDInterval);
void blinkLED1(unsigned long LEDInterval);
void blinkLED2(unsigned long LEDInterval);
bool serialData(int &result);


void setup() {
  pinMode(2, OUTPUT);
  pinMode(3, OUTPUT);
  Serial.begin(9600);
  buttonInit();    // button: pin 10 as input with internal pull-up
  fanInit();       // fan: Timer1 Fast PWM on pin 9 (OC1A) for the MOSFET gate

  // Create the four tasks (name, stack size in bytes, priority; higher number = higher priority).
  // The scheduler starts when setup() returns.
  xTaskCreate(askQuestions, "askQuestions", 200, NULL, 1, NULL);
  xTaskCreate(serviceLED1,  "serviceLED1",   64, NULL, 3, NULL);
  xTaskCreate(serviceLED2,  "serviceLED2",   64, NULL, 3, NULL);
  xTaskCreate(checkButton,  "checkButton",  200, NULL, 2, NULL);
}

void loop() {
  // empty: all work is done by the FreeRTOS tasks
}


// Dialog task: asks for an LED number, then its blink interval, over and over.
void askQuestions(void *pvParameters) {
  for (;;) {
    // Ask which LED, once per round.
    if (!askedLED) {
      Serial.println("What LED? (1 or 2)");
      askedLED = true;
    }

    // Try to read ONE completed line this pass.
    value = 0;
    gotLine = serialData(value);

    if (gotLine) {
      if (!askedInterval) {
        // We were waiting for the LED number.
        if (value == 1) {
          getLED1Num = value;
          Serial.println("What interval (in msec)?");
          askedInterval = true;
          currentSelection = 1;
        } else if (value == 2) {
          getLED2Num = value;
          Serial.println("What interval (in msec)?");
          askedInterval = true;
          currentSelection = 2;
        }
      } else {
        // We were waiting for the interval: route it to whichever LED was picked.
        if (currentSelection == 1) {
          getInterval1 = value;
        } else if (currentSelection == 2) {
          getInterval2 = value;
        }

        // Reset the flow so the next completed line starts a fresh round
        // (new LED number) instead of being misread as another interval.
        askedLED      = false;
        askedInterval = false;
      }
    }
  }
}

// LED 1 task (priority 3): blinks LED 1 at its interval once it has been set.
void serviceLED1(void *pvParameters) {
  (void) pvParameters;
  for (;;) {
    changeFrequency1(getLED1Num, getInterval1);
  }
}

// LED 2 task (priority 3): blinks LED 2 at its interval once it has been set.
void serviceLED2(void *pvParameters) {
  (void) pvParameters;
  for (;;) {
    changeFrequency2(getLED2Num, getInterval2);
  }
}

// Fan task (priority 2): lightUpdate() reads the debounced button (RTOS tick) and, on each
// release, advances the fan OFF -> LOW -> MED -> HIGH -> MED -> LOW -> OFF.
// The button module waits with vTaskDelay(), so the other tasks keep running.
void checkButton(void *pvParameters) {
  for (;;) {
    lightUpdate();
  }
}

// Blinks LED 1 only if LED 1 was selected and its interval is set; otherwise waits 20 ms.
void changeFrequency1(int LEDNum, unsigned long LEDInterval) {
  if ((LEDNum == 1) && (LEDInterval > 0)) {
    blinkLED1(LEDInterval);
  } else {
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// Blinks LED 2 only if LED 2 was selected and its interval is set; otherwise waits 20 ms.
void changeFrequency2(int LEDNum, unsigned long LEDInterval) {
  if ((LEDNum == 2) && (LEDInterval > 0)) {
    blinkLED2(LEDInterval);
  } else {
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// One blink cycle for LED 1 (pin 2): on for one interval, off for one interval.
// vTaskDelay() lets the other tasks run during each wait.
void blinkLED1(unsigned long LEDInterval) {
  digitalWrite(2, HIGH);
  vTaskDelay(pdMS_TO_TICKS(LEDInterval));
  digitalWrite(2, LOW);
  vTaskDelay(pdMS_TO_TICKS(LEDInterval));
}

// One blink cycle for LED 2 (pin 3): on for one interval, off for one interval.
void blinkLED2(unsigned long LEDInterval) {
  digitalWrite(3, HIGH);
  vTaskDelay(pdMS_TO_TICKS(LEDInterval));
  digitalWrite(3, LOW);
  vTaskDelay(pdMS_TO_TICKS(LEDInterval));
}

// Reads at most ONE character per call. Returns true only on the call where it reads '\n':
// the full line is then complete, so it converts the buffered text to an int and clears the buffer.
// If no character is waiting, it waits 30 ms with vTaskDelay() so other tasks can run.
bool serialData(int &result) {
  if (Serial.available() > 0) {
    char incomingChar = Serial.read();

    if (incomingChar == '\n') {
      result = inputBuffer.toInt();
      inputBuffer = "";
      return true;
    } else {
      inputBuffer += incomingChar;  // keep building the line
    }
  } else {
    vTaskDelay(pdMS_TO_TICKS(30));
  }
  return false;
}