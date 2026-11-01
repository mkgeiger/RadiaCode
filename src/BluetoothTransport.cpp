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

#include "BluetoothTransport.h"
#include "Decoders.h"
#include "RadiaCodeLog.h"
#include "RadiaCodeCompat.h"

#include <cstring>

#ifdef BT_SUPPORT_ENABLED

// Include platform-specific BLE libraries
#if defined(ARDUINO_ARCH_ESP32)
#include <BLEDevice.h>
#include <BLEClient.h>
#elif defined(ESP_PLATFORM)
#include "NimBLEDevice.h"
#endif

// Debugging switches
#undef  BT_DEBUG_INFO
#define BT_DEBUG_WARNING

// Define the static buffer for BluetoothTransport
uint8_t BluetoothTransport::_resp_buffer[BluetoothTransport::MAX_RESP_SIZE];
#endif

BluetoothTransport::BluetoothTransport(const char* mac)
{
#ifdef BT_SUPPORT_ENABLED
    _buf_len = 0;
    _resp_received = 0;
    _resp_size = 0;
    _response_ready = false;
    _parse_data_buf = false;
    _base_time_sec = 0;
    _header_skipped = false;
    // Initialize the buffer to zeros
    memset(_resp_buffer, 0, MAX_RESP_SIZE);

    // Initialize BLE client - platform-specific code
#if defined(ARDUINO_ARCH_ESP32)
    // ESP32 BLE initialization
    BLEDevice::init("RadiaCode Client");
    BLEClient* pClient = BLEDevice::createClient();

    // Connect to the RadiaCode device
    BLEAddress bleAddress(mac);
    if (pClient->connect(bleAddress))
    {
        _peripheral = (void*)pClient;

        // Get service and characteristics
        BLEUUID serviceUUID("e63215e5-7003-49d8-96b0-b024798fb901");
        BLERemoteService* pService = pClient->getService(serviceUUID);

        if (pService != nullptr)
        {
            // Get write characteristic
            BLEUUID writeUUID("e63215e6-7003-49d8-96b0-b024798fb901");
            BLERemoteCharacteristic* pWriteChar = pService->getCharacteristic(writeUUID);
            _write_handle = 0; // In ESP32, we use the characteristic directly

            // Get notify characteristic
            BLEUUID notifyUUID("e63215e7-7003-49d8-96b0-b024798fb901");
            BLERemoteCharacteristic* pNotifyChar = pService->getCharacteristic(notifyUUID);
            _notify_handle = 0; // In ESP32, we use the characteristic directly

            // Set up notification callback
            if (pNotifyChar != nullptr)
            {
                // Register for notifications
                pNotifyChar->registerForNotify([this](BLERemoteCharacteristic* pChar, uint8_t* data, size_t length, bool isNotify)
                {
                    onNotifyData(data, length);
                });

                // Enable notifications
                pNotifyChar->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)"\x01\x00", 2);
            }
        }
    }
    else
    {
        RC_LOGE("Failed to connect to BLE device");
        _peripheral = nullptr;
    }
#elif defined(ESP_PLATFORM)
    // NimBLE-cpp initialization (ESP-IDF, no Arduino) - relies on the caller
    // (e.g. task_ble.cpp, which also scans) having already called NimBLEDevice::init()
    if (!NimBLEDevice::isInitialized())
    {
        NimBLEDevice::init("RadiaCode Client");
    }
    NimBLEClient* pClient = NimBLEDevice::createClient();

    if (pClient->connect(NimBLEAddress(std::string(mac), BLE_ADDR_PUBLIC)))
    {
        _peripheral = (void*)pClient;

        NimBLEUUID serviceUUID("e63215e5-7003-49d8-96b0-b024798fb901");
        NimBLERemoteService* pService = pClient->getService(serviceUUID);

        if (pService != nullptr)
        {
            NimBLEUUID writeUUID("e63215e6-7003-49d8-96b0-b024798fb901");
            NimBLERemoteCharacteristic* pWriteChar = pService->getCharacteristic(writeUUID);
            _write_handle = 0;

            NimBLEUUID notifyUUID("e63215e7-7003-49d8-96b0-b024798fb901");
            NimBLERemoteCharacteristic* pNotifyChar = pService->getCharacteristic(notifyUUID);
            _notify_handle = 0;

            if (pNotifyChar != nullptr)
            {
                pNotifyChar->subscribe(true, [this](NimBLERemoteCharacteristic* pChar, uint8_t* data, size_t length, bool isNotify)
                {
                    onNotifyData(data, length);
                });
                // NimBLE-cpp's subscribe() writes the CCCD (0x2902) internally.
            }
        }
    }
    else
    {
        RC_LOGE("Failed to connect to BLE device");
        _peripheral = nullptr;
    }
#else
    _peripheral = nullptr; // Would be initialized with actual BLE connection
#endif
#else
    RC_LOGE("Bluetooth not supported on this platform");
#endif
}

bool BluetoothTransport::isConnected(void) const
{
#ifdef BT_SUPPORT_ENABLED
    if (_peripheral == nullptr)
    {
        return false;
    }
#if defined(ARDUINO_ARCH_ESP32)
    BLEClient* pClient = (BLEClient*)_peripheral;
    return pClient->isConnected();
#elif defined(ESP_PLATFORM)
    NimBLEClient* pClient = (NimBLEClient*)_peripheral;
    return pClient->isConnected();
#else
    return true;
#endif
#else
    return false;
#endif
}

BluetoothTransport::~BluetoothTransport(void)
{
#ifdef BT_SUPPORT_ENABLED
    // Clean up BLE resources - platform-specific code
#if defined(ARDUINO_ARCH_ESP32)
    if (_peripheral != nullptr)
    {
        BLEClient* pClient = (BLEClient*)_peripheral;
        pClient->disconnect();
    }
#elif defined(ESP_PLATFORM)
    if (_peripheral != nullptr)
    {
        NimBLEClient* pClient = (NimBLEClient*)_peripheral;
        pClient->disconnect();
    }
#endif

    // Reset state variables
    _resp_size = 0;
    _resp_received = 0;
    _buf_len = 0;
    _response_ready = false;
    _header_skipped = false;

    // Free any records that were parsed but never drained
    for (size_t i = 0; i < _parsed_records.size(); i++)
    {
        delete _parsed_records[i];
    }
    _parsed_records.clear();
#endif
}

BytesBuffer BluetoothTransport::execute(const uint8_t* request, size_t length)
{
    BytesBuffer response;

#ifdef BT_SUPPORT_ENABLED
    if (_peripheral == nullptr)
    {
        RC_LOGE("Bluetooth not connected");
        return response;
    }

    // Reset response tracking
    resetResponse();

    // Send request in chunks - platform-specific code
#if defined(ARDUINO_ARCH_ESP32)
    BLEClient* pClient = (BLEClient*)_peripheral;
    BLEUUID writeUUID("e63215e6-7003-49d8-96b0-b024798fb901");
    BLERemoteService* pService = pClient->getService(BLEUUID("e63215e5-7003-49d8-96b0-b024798fb901"));
    BLERemoteCharacteristic* pWriteChar = pService->getCharacteristic(writeUUID);

    // Send in chunks of 18 bytes (BLE MTU limitation)
    const size_t chunkSize = 18;
    for (size_t pos = 0; pos < length; pos += chunkSize)
    {
        size_t remaining = length - pos;
        size_t toSend = (remaining > chunkSize) ? chunkSize : remaining;

        uint8_t chunk[chunkSize];
        memcpy(chunk, request + pos, toSend);
        
        pWriteChar->writeValue(chunk, toSend);
        delay(5); // Small delay to avoid overwhelming the BLE stack
    }
#elif defined(ESP_PLATFORM)
    NimBLEClient* pClient = (NimBLEClient*)_peripheral;
    NimBLEUUID writeUUID("e63215e6-7003-49d8-96b0-b024798fb901");
    NimBLERemoteService* pService = pClient->getService(NimBLEUUID("e63215e5-7003-49d8-96b0-b024798fb901"));
    NimBLERemoteCharacteristic* pWriteChar = pService->getCharacteristic(writeUUID);

    // Send in chunks of 18 bytes (BLE MTU limitation)
    const size_t chunkSize = 18;
    for (size_t pos = 0; pos < length; pos += chunkSize)
    {
        size_t remaining = length - pos;
        size_t toSend = (remaining > chunkSize) ? chunkSize : remaining;

        uint8_t chunk[chunkSize];
        memcpy(chunk, request + pos, toSend);

        pWriteChar->writeValue(chunk, toSend, false); // write without response
        rc_delay_ms(5); // Small delay to avoid overwhelming the BLE stack
    }
#endif

    // Wait for response with timeout - extended to 30 seconds
    uint32_t startTime = rc_millis();
    const uint32_t TIMEOUT_MS = 30000; // Increased timeout to 30 seconds
    uint32_t elapsedTime = 0;

    while ((!_response_ready) && ((elapsedTime = (rc_millis() - startTime)) < TIMEOUT_MS))
    {
#ifdef BT_DEBUG_INFO
        // Print progress every 2 seconds
        if ((elapsedTime % 2000) < 20)
        {
            RC_LOGI("Waiting for BLE response: %zu/%zu bytes (%us)", _resp_received, _resp_size, (unsigned)(elapsedTime / 1000));
        }
#endif
        rc_delay_ms(50); // Less frequent polling
#if defined(ARDUINO_ARCH_ESP32)
        yield(); // Allow ESP32 background tasks to run
#endif
    }

    // Streaming is a per-read mode: disable it once the response has finished so
    // subsequent (non-DATA_BUF) commands aren't parsed as records.
    _parse_data_buf = false;

    if (!_response_ready)
    {
#ifdef BT_DEBUG_WARNING
        RC_LOGW("Warning: Bluetooth response timeout after %us. Received %zu of %zu bytes",
            (unsigned)(TIMEOUT_MS / 1000), _resp_received, _resp_size);
#endif
        return response;
    }

    // Any remaining un-parsed bytes still hold a (partial) protocol header /
    // record tail. Build a BytesBuffer from the valid buffered data.
    if (_buf_len > 0)
    {
        size_t dataSize = _buf_len;
        // The response buffer starts at the 4-byte size prefix on older
        // transports; the first 4 bytes are that prefix, which callers skip.
        response = BytesBuffer(_resp_buffer, dataSize);
        response.setPosition(0);
    }
#else
    RC_LOGE("Bluetooth not supported on this platform");
#endif

    return response;
}

#ifdef BT_SUPPORT_ENABLED

void BluetoothTransport::setDataBufStream(uint32_t base_time_sec)
{
    _parse_data_buf = true;
    _base_time_sec = base_time_sec;
}

std::vector<DataItem*> BluetoothTransport::drainDataBufRecords(void)
{
    std::vector<DataItem*> ret;
    ret.swap(_parsed_records); // transfer ownership to the caller
    _parsed_records.clear();
    return ret;
}

void BluetoothTransport::resetResponse(void)
{
    _buf_len = 0;
    _resp_received = 0;
    _resp_size = 0;
    _response_ready = false;
    _header_skipped = false;
}

void BluetoothTransport::appendNotifyData(const uint8_t* data, size_t length)
{
    if (data == nullptr || length == 0) return;

    size_t space = MAX_RESP_SIZE - _buf_len;
    size_t copy = (length < space) ? length : space;
    if (copy < length)
    {
#ifdef BT_DEBUG_WARNING
        RC_LOGW("Warning: Truncating BLE packet to fit buffer");
#endif
    }
    if (copy > 0)
    {
        memcpy(_resp_buffer + _buf_len, data, copy);
        _buf_len += copy;
    }
}

void BluetoothTransport::onNotifyData(const uint8_t* data, size_t length)
{
    if (data == nullptr || length == 0) return;

    // First packet carries a 4-byte little-endian response size prefix.
    if (_resp_size == 0)
    {
        if (length < 4) return; // not enough for the prefix yet

        uint32_t size_value = (uint32_t)data[0] |
                              ((uint32_t)data[1] << 8) |
                              ((uint32_t)data[2] << 16) |
                              ((uint32_t)data[3] << 24);
        // Total response size = prefix(4) + payload.
        _resp_size = size_value + 4;

        // Non-streaming commands still need a bounded buffer; the streaming
        // DATA_BUF path never needs the whole (possibly huge) response in RAM.
        if (!_parse_data_buf && _resp_size > MAX_RESP_SIZE)
        {
#ifdef BT_DEBUG_WARNING
            RC_LOGW("Warning: Response size too large (%zu bytes), limiting to %zu", _resp_size, (size_t)MAX_RESP_SIZE);
#endif
            _resp_size = MAX_RESP_SIZE;
        }

        _buf_len = 0;
        _resp_received = 0;
        _header_skipped = false;

        // Skip the size prefix and buffer the rest of this packet.
        appendNotifyData(data + 4, length - 4);
        _resp_received += length;
    }
    else
    {
        appendNotifyData(data, length);
        _resp_received += length;
    }

    // Stream-parse complete records out of the buffer as bytes arrive.
    if (_parse_data_buf)
    {
        parseAvailableRecords();
    }

    if (_resp_received >= _resp_size)
    {
        // Final parse of any complete records still buffered.
        if (_parse_data_buf)
        {
            parseAvailableRecords();
        }
        _response_ready = true;
#ifdef BT_DEBUG_INFO
        RC_LOGI("Response complete: %zu of %zu bytes received", _resp_received, _resp_size);
#endif
    }
}

void BluetoothTransport::parseAvailableRecords(void)
{
    // Strip the 12-byte protocol header (cmd echo 4 + retcode 4 + flen 4)
    // that precedes the DATA_BUF records, once.
    if (!_header_skipped)
    {
        if (_buf_len < 12) return; // need more data
        memmove(_resp_buffer, _resp_buffer + 12, _buf_len - 12);
        _buf_len -= 12;
        _header_skipped = true;
    }

    while (_buf_len > 0)
    {
        BytesBuffer br(_resp_buffer, _buf_len);
        std::vector<DataItem*> items;
        size_t consumed_bytes = decodeDataBufRecord(br, _base_time_sec, items);
        if (consumed_bytes == 0)
        {
            break; // need more data, or hit an unknown record type
        }

        for (DataItem* item : items)
        {
            if (item != nullptr)
            {
                _parsed_records.push_back(item);
            }
        }

        // Discard the consumed record(s) from the front of the buffer.
        memmove(_resp_buffer, _resp_buffer + consumed_bytes, _buf_len - consumed_bytes);
        _buf_len -= consumed_bytes;
    }
}

#endif
