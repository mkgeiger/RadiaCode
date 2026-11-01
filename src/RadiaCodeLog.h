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

// Portable logging macros so the library compiles under both Arduino and ESP-IDF.
#ifndef RadiaCodeLog_h
#define RadiaCodeLog_h

#if defined(ESP_PLATFORM)

#include "esp_log.h"
static const char RC_LOG_TAG[] = "RadiaCode";
#define RC_LOGI(fmt, ...) ESP_LOGI(RC_LOG_TAG, fmt, ##__VA_ARGS__)
#define RC_LOGW(fmt, ...) ESP_LOGW(RC_LOG_TAG, fmt, ##__VA_ARGS__)
#define RC_LOGE(fmt, ...) ESP_LOGE(RC_LOG_TAG, fmt, ##__VA_ARGS__)

#elif defined(ARDUINO_ARCH_ESP32)

#include "Arduino.h"
#define RC_LOGI(fmt, ...) Serial.printf(fmt "\n", ##__VA_ARGS__)
#define RC_LOGW(fmt, ...) Serial.printf(fmt "\n", ##__VA_ARGS__)
#define RC_LOGE(fmt, ...) Serial.printf(fmt "\n", ##__VA_ARGS__)

#else

#define RC_LOGI(fmt, ...)
#define RC_LOGW(fmt, ...)
#define RC_LOGE(fmt, ...)

#endif

#endif  // RadiaCodeLog_h
