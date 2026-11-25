#include "button.h"
#include <stdint.h>             // standard integer types only; no avr/io.h or avr/interrupt.h needed
#include <Arduino_FreeRTOS.h>   // FreeRTOS: xTaskGetTickCount(), vTaskDelay(), pdMS_TO_TICKS()


// ===== Own register constants (ATmega328P data-memory addresses) =====
#define myPINB  (*(volatile uint8_t *)0x23)     // Port B Input Pins (read-only use): button level
#define myDDRB  (*(volatile uint8_t *)0x24)     // Port B Data Direction Register: button = input
#define myPORTB (*(volatile uint8_t *)0x25)     // Port B Data Register: enables the pull-up for the button

// ===== Pin assignment =====
#define BUTTON_BIT  2    // PB2 = Arduino pin 10, push button to ground, internal pull-up (released = 1, pressed = 0)

static uint8_t prevButton = 1;        // last accepted button level: 1 = released, 0 = pressed (pull-up)
static TickType_t lastChange = 0;     // RTOS tick count at the last accepted button change, for debounce

// Sets pin 10 as an input with the internal pull-up.
void buttonInit() {
  myPORTB |= (1 << BUTTON_BIT);     // pull-up on the button pin: reads 1 released, 0 pressed
  myDDRB &= ~(1 << BUTTON_BIT);     // button pin as input
}

// Returns 1 exactly once per button release (pressed -> released), otherwise 0.
// Changes within 35 ms of the last accepted change are ignored (debounce).
// When no change is accepted it waits 30 ms with vTaskDelay(), so call it only from a FreeRTOS task.
uint8_t checkbuttonReleased() {
  TickType_t now = xTaskGetTickCount();   // current RTOS tick count

  uint8_t buttonReleased = 0;
  if ((myPINB & (1 << BUTTON_BIT)) && (prevButton == 0) && (TickType_t)(now - lastChange) >= pdMS_TO_TICKS(35)) {          // pin reads 1 and was 0: button released
    buttonReleased = 1;        // report the release event
    prevButton = 1;            // remember the button is now released
    lastChange = now;          // restart the debounce window
  } else if (!(myPINB & (1 << BUTTON_BIT)) && (prevButton == 1) && (TickType_t)(now - lastChange) >= pdMS_TO_TICKS(35)) { // pin reads 0 and was 1: button pressed
    buttonReleased = 0;        // a press is not reported, only tracked
    prevButton = 0;            // remember the button is now pressed
    lastChange = now;          // restart the debounce window
  } else {
    vTaskDelay(pdMS_TO_TICKS(30));   // no accepted change: let the other tasks run
  }
  return buttonReleased;
}