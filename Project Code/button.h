#ifndef button_H  // include guard: skip this file if it was already included
#define button_H

#include <stdint.h>

// Sets pin 10 as an input with the internal pull-up. Call once in setup().
void buttonInit(void);

// Returns 1 exactly once per button release (debounced with the RTOS tick), otherwise 0.
// Waits with vTaskDelay() when no change is accepted, so call it only from a FreeRTOS task.
uint8_t checkbuttonReleased(void);

#endif