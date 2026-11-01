/**********************************************************************************/
/* Copyright (c) 2025 Markus Geiger                                               */
/*                                                                                */
/* Permission is hereby granted, free of charge, to any person obtaining a copy   */
/* of this software and associated documentation files (the "Software"), to deal  */
/* in the Software without restriction, including without limitation the rights   */
/* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell      */
/* copies of the Software, and to permit persons to whom the Software is          */
/* furnished to do so, subject to the following conditions:                       */
/*                                                                                */
/* The above copyright notice and this permission notice shall be included in all */
/* copies or substantial portions of the Software.                                */
/*                                                                                */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR     */
/* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,       */
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE    */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER         */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,  */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE  */
/* SOFTWARE.                                                                      */
/**********************************************************************************/

// Portable millis()/delay() so BluetoothTransport compiles under both Arduino and ESP-IDF.
#ifndef RadiaCodeCompat_h
#define RadiaCodeCompat_h

#if defined(ESP_PLATFORM)

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

inline uint32_t rc_millis(void) {
    return (uint32_t)(esp_timer_get_time() / 1000);
}

inline void rc_delay_ms(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

#elif defined(ARDUINO_ARCH_ESP32)

#include "Arduino.h"

inline uint32_t rc_millis(void) {
    return millis();
}

inline void rc_delay_ms(uint32_t ms) {
    delay(ms);
}

#else

// Host/native build (eg. unit tests) - no Arduino or ESP-IDF available.
#include <chrono>
#include <cstdint>
#include <thread>

inline uint32_t rc_millis(void) {
    using namespace std::chrono;
    return (uint32_t)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

inline void rc_delay_ms(uint32_t ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

#endif

#endif  // RadiaCodeCompat_h
