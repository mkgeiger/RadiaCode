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

#ifndef BluetoothTransport_h
#define BluetoothTransport_h

#include "RadiaCodeTransport.h"
#include "BytesBuffer.h"
#include "RadiaCodeTypes.h"
#include <vector>

#if defined(ARDUINO_ARCH_ESP32) || defined(ESP_PLATFORM)
#define BT_SUPPORT_ENABLED
#endif

class BluetoothTransport : public RadiaCodeTransport
{
    public:
        BluetoothTransport(const char* mac);
        virtual ~BluetoothTransport(void);
        virtual BytesBuffer execute(const uint8_t* request, size_t length) override;
        virtual bool isConnected(void) const override;

        // Enable streaming parse of the DATA_BUF response: complete records are
        // decoded incrementally as BLE notifications arrive (bounded memory even
        // for a very large DATA_BUF), and are collected for drainDataBufRecords().
        void setDataBufStream(uint32_t base_time_sec);

        // Return the records parsed so far by the streaming path (transferring
        // ownership of the heap-allocated DataItems to the caller).
        std::vector<DataItem*> drainDataBufRecords(void);

    private:
#ifdef BT_SUPPORT_ENABLED
        // Platform-specific Bluetooth variables
        void* _peripheral; // Pointer to Bluetooth peripheral object
        int _write_handle;
        int _notify_handle;

        // Use fixed buffer instead of dynamic allocation for response.
        // Sized to hold a full raw spectrum (~4KB + header) as well as the
        // bounded streaming DATA_BUF window.
        static const size_t MAX_RESP_SIZE = 8192;
        static uint8_t _resp_buffer[MAX_RESP_SIZE];
        size_t _buf_len;       // valid bytes currently buffered at _resp_buffer[0]
        size_t _resp_received; // total bytes received for the current response
        size_t _resp_size;     // expected total response size (prefix + payload)
        bool _response_ready;

        // Streaming record-parse state
        bool _parse_data_buf;
        uint32_t _base_time_sec;
        bool _header_skipped;               // 12-byte protocol header consumed
        std::vector<DataItem*> _parsed_records;

        // Append incoming notification bytes to the buffer, then try to parse
        // complete DATA_BUF records out of it (shifting the buffer as it goes).
        void onNotifyData(const uint8_t* data, size_t length);
        void appendNotifyData(const uint8_t* data, size_t length);
        void parseAvailableRecords(void);
        void resetResponse(void);
#endif
};

#endif
