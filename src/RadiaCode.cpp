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

/*
  RadiaCode.cpp - Library implementation for interfacing with RadiaCode radiation detection devices
*/

#include "RadiaCode.h"
#include "BluetoothTransport.h"
#include "Decoders.h"
#include "RadiaCodeLog.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <time.h>

// Debugging switches
#undef  RC_DEBUG_INFO
#define RC_DEBUG_WARNING
#define RC_DEBUG_ERROR

float spectrumChannelToEnergy(int channel_number, float a0, float a1, float a2)
{
    return a0 + a1 * channel_number + a2 * channel_number * channel_number;
}

const char* getDriverVersion(void)
{
    static char version[16];
    snprintf(version, sizeof(version), "%d.%d.%d", DRIVER_VERSION_MAJOR, DRIVER_VERSION_MINOR, DRIVER_VERSION_PATCH);
    return version;
}

RadiaCode::RadiaCode(const char* bluetooth_mac, bool ignore_firmware_compatibility_check)
{
    _seq = 0;
    _connection = nullptr;
    _base_time_sec = 0;
    _spectrum_format_version = 0;

    // Check if bluetooth is supported on this platform
#if defined(ARDUINO_ARCH_ESP32) || defined(ESP_PLATFORM)
    _bt_supported = true;
#else
    _bt_supported = false;
#endif

    // Connect via Bluetooth (no try/catch: ESP-IDF builds default to
    // -fno-exceptions, and nothing here deliberately throws)
    if (bluetooth_mac != nullptr && _bt_supported)
    {
#if defined(ARDUINO_ARCH_ESP32) || defined(ESP_PLATFORM)
        _connection = new BluetoothTransport(bluetooth_mac);
#endif
    }

    if (_connection == nullptr)
    {
#ifdef RC_DEBUG_ERROR
        RC_LOGE("Error: Failed to create transport connection");
#endif
        return;
    }

    // Initialize device with error handling
    uint8_t init_data[] = {0x01, 0xFF, 0x12, 0xFF};
    execute(COMMAND::SET_EXCHANGE, init_data, sizeof(init_data));

    // Set current time
    time_t now = time(nullptr);   // Get current time in UNIX format, years since 1900
    if (now > 0)
    {
        struct tm* timeinfo = localtime(&now);
        if (timeinfo != nullptr)
        {
            setLocalTime(timeinfo->tm_mday, timeinfo->tm_mon + 1, timeinfo->tm_year + 1900, 
                timeinfo->tm_sec, timeinfo->tm_min, timeinfo->tm_hour);
        }
        // Base time for timestamping DATA_BUF records. The device's ts_offset
        // grows over the session (it is relative to connect time), so the base
        // is fixed to host "now" at connect; decodeDataBufRecord() applies a
        // +128s correction to live RealTimeData records only.
        _base_time_sec = now;
    }

    deviceTime(0);

    // Check firmware version
    auto version = fwVersion();
    int vmaj = std::get<3>(version);
    int vmin = std::get<4>(version);

#ifdef RC_DEBUG_ERROR
    if (!ignore_firmware_compatibility_check && ((vmaj < 4) || ((vmaj == 4) && (vmin < 8))))
    {
        RC_LOGE("Error:Incompatible firmware version %d.%d, >=4.8 required. Upgrade device firmware", vmaj, vmin);
    }
#endif

    // Determine spectrum format version
    std::string config = configuration();
    size_t pos = config.find("SpecFormatVersion=");
    if (pos != std::string::npos)
    {
        std::string versionSubstr = config.substr(pos + 18);
        size_t newlinePos = versionSubstr.find('\n');
        if (newlinePos != std::string::npos)
        {
            versionSubstr = versionSubstr.substr(0, newlinePos);
        }
        _spectrum_format_version = static_cast<uint8_t>(atoi(versionSubstr.c_str()));
    }
}

RadiaCode::~RadiaCode(void)
{
    if (_connection != nullptr)
    {
        delete _connection;
        _connection = nullptr;
    }
}

BytesBuffer RadiaCode::execute(COMMAND reqtype, const uint8_t* args, size_t args_len)
{
    // Check if connection is valid
    if (_connection == nullptr)
    {
#ifdef RC_DEBUG_ERROR
        RC_LOGE("Error: Connection is null in execute()");
#endif
        return BytesBuffer(); // Return empty buffer
    }

    uint8_t req_seq_no = 0x80 + _seq;
    _seq = (_seq + 1) % 32;

    // Create request header
    uint8_t req_header[4];
    req_header[0] = (uint8_t)((uint16_t)reqtype & 0xFF);
    req_header[1] = (uint8_t)(((uint16_t)reqtype >> 8) & 0xFF);
    req_header[2] = 0;
    req_header[3] = req_seq_no;

#ifdef RC_DEBUG_INFO
    // Debug: Print command info
    RC_LOGI("CMD 0x%04X (%u) seq=0x%02X args_len=%zu", (unsigned)reqtype, (unsigned)reqtype, req_seq_no, args_len);
#endif

    // Calculate total request size
    size_t request_size = sizeof(req_header) + (args_len > 0 ? args_len : 0);
    uint8_t* request = new uint8_t[request_size];

    // Copy header and args to request buffer
    memcpy(request, req_header, sizeof(req_header));
    if (args != nullptr && args_len > 0)
    {
        memcpy(request + sizeof(req_header), args, args_len);
    }

#ifdef RC_DEBUG_INFO
    // Debug: Print request bytes
    RC_LOGI(" TX[%zu]:", request_size + 4);
#endif

    // Create full request with length prefix
    uint32_t request_len = request_size;
    uint8_t* full_request = new uint8_t[request_size + 4];
    memcpy(full_request, &request_len, 4);
    memcpy(full_request + 4, request, request_size);

#ifdef RC_DEBUG_INFO
    // Print length prefix and request (kept as a no-op placeholder - hex dump removed
    // since it added little value beyond the CMD/TX/RX summary lines already logged).
#endif

    // Execute request
    BytesBuffer response = _connection->execute(full_request, request_size + 4);

#ifdef RC_DEBUG_INFO
    // Debug: Print response bytes
    RC_LOGI(" RX[%zu]:", response.getSize());
#endif

#ifdef RC_DEBUG_INFO
    // Print first 128 bytes of response for debugging
    size_t debug_len = std::min((size_t)128, response.getSize());
    uint8_t temp_buf[128];
    size_t original_pos = response.getPosition();
    response.setPosition(0);
    response.readBytes(temp_buf, debug_len);
    response.setPosition(original_pos);
#endif

    // Clean up
    delete[] request;
    delete[] full_request;

    // Check response header
    uint8_t resp_header[4];
    response.readBytes(resp_header, 4);

#ifdef RC_DEBUG_INFO
    for (int i = 0; i < 4; i++)
    {
        if (req_header[i] != resp_header[i])
        {
            RC_LOGI("Header mismatch: req=%02X%02X%02X%02X, resp=%02X%02X%02X%02X",
                req_header[0], req_header[1], req_header[2], req_header[3],
                resp_header[0], resp_header[1], resp_header[2], resp_header[3]);
            break;
        }
    }
#endif

    return response;
}

BytesBuffer RadiaCode::readRequest(uint32_t command_id)
{
#ifdef RC_DEBUG_INFO
    RC_LOGI("READ_REQ 0x%08X (%u)", (unsigned)command_id, (unsigned)command_id);
#endif

    uint8_t cmd_bytes[4];
    memcpy(cmd_bytes, &command_id, 4);

    BytesBuffer r = execute(COMMAND::RD_VIRT_STRING, cmd_bytes, 4);

    // Check if we got a valid response with enough data for header (8 bytes minimum)
    if (r.getSize() < 8)
    {
#ifdef RC_DEBUG_ERROR
        RC_LOGE("Error: Invalid response size for command %u", (unsigned)command_id);
#endif
        return BytesBuffer(); // Return empty buffer
    }

    uint32_t retcode = 0;
    uint32_t flen = 0;
    bool headerValid = r.readUint32(&retcode) && r.readUint32(&flen);

    if (!headerValid)
    {
#ifdef RC_DEBUG_ERROR
        RC_LOGE("Error: Failed to read response header");
#endif
        return BytesBuffer(); // Return empty buffer
    }

#ifdef RC_DEBUG_INFO
    RC_LOGI(" -> retcode=%u len=%u", (unsigned)retcode, (unsigned)flen);
#endif

#ifdef RC_DEBUG_ERROR
    if (retcode != 1)
    {
        RC_LOGE("Error: Unexpected return code for command 0x%08X: %u", (unsigned)command_id, (unsigned)retcode);
        // Continue processing anyway
    }
#endif

#ifdef RC_DEBUG_WARNING
    // Add a safety check for the expected data size
    if (flen > (BytesBuffer::MAX_BUFFER_SIZE - 8))
    {
        RC_LOGW("Warning: Data length too large in readRequest: %u", (unsigned)flen);
    }
#endif

    // Workaround for firmware bug (if present)
    size_t remaining_size = r.available();
    if (remaining_size == (flen + 1))
    {
        uint8_t last_byte;
        r.peekBytes(&last_byte, remaining_size - 1, 1);
        if (last_byte == 0x00)
        {
            // Truncate last byte
            r.setSize(r.getPosition() + remaining_size - 1);
        }
    }

#ifdef RC_DEBUG_ERROR
    if (r.available() != flen)
    {
        RC_LOGE("Error: Unexpected data size for command 0x%08X: expected %u, got %zu",
            (unsigned)command_id, (unsigned)flen, r.available());
    }
#endif

  return r;
}

void RadiaCode::writeRequest(uint32_t command_id, const uint8_t* data, size_t data_len)
{
#ifdef RC_DEBUG_INFO
    RC_LOGI("WRITE_REQ 0x%08X (%u) len=%zu", (unsigned)command_id, (unsigned)command_id, data_len);
#endif

    // Create command buffer
    size_t cmd_size = 4 + (data != nullptr ? data_len : 0);
    uint8_t* cmd_data = new uint8_t[cmd_size];

    // Add command ID
    memcpy(cmd_data, &command_id, 4);

    // Add data if present
    if (data != nullptr && data_len > 0)
    {
        memcpy(cmd_data + 4, data, data_len);
    }

    // Execute command
    BytesBuffer r = execute(COMMAND::WR_VIRT_SFR, cmd_data, cmd_size);

    // Clean up
    delete[] cmd_data;

    // Check response
    uint32_t retcode;
    r.readUint32(&retcode);

#ifdef RC_DEBUG_INFO
    RC_LOGI(" -> retcode=%u", (unsigned)retcode);
#endif

#ifdef RC_DEBUG_ERROR
    if (retcode != 1)
    {
        RC_LOGE("Write request failed, retcode=%u", (unsigned)retcode);
    }
#endif
}

std::vector<float> RadiaCode::batchReadVSFRs(const std::vector<uint32_t>& vsfr_ids)
{
    std::vector<float> ret;

    size_t nvsfr = vsfr_ids.size();
    if (nvsfr == 0)
    {
#ifdef RC_DEBUG_ERROR
        RC_LOGE("Error: No VSFRs specified");
#endif
        return ret;
    }

    // Prepare message buffer
    size_t msg_size = 4 + nvsfr * 4; // 4 bytes for count, 4 bytes per VSFR ID
    uint8_t* msg = new uint8_t[msg_size];

    // Add count
    uint32_t count = nvsfr;
    memcpy(msg, &count, 4);

    // Add VSFR IDs
    for (size_t i = 0; i < nvsfr; i++)
    {
        uint32_t vsfr_id = vsfr_ids[i];
        memcpy(msg + 4 + i * 4, &vsfr_id, 4);
    }

    // Send request
    BytesBuffer r = execute(COMMAND::RD_VIRT_SFR_BATCH, msg, msg_size);

    // Clean up
    delete[] msg;

    // Check validity flags
    uint32_t valid_flags;
    r.readUint32(&valid_flags);

    uint32_t expected_flags = (1 << nvsfr) - 1;
    if (valid_flags != expected_flags)
    {
#ifdef RC_DEBUG_ERROR
        RC_LOGE("Error: Unexpected validity flags, bad vsfr_id? 0x%X != 0x%X", (unsigned)valid_flags, (unsigned)expected_flags);
#endif
        return ret;
    }

    // Read values
    for (size_t i = 0; i < nvsfr; i++)
    {
        uint32_t raw_value;
        r.readUint32(&raw_value);

        // Convert to appropriate type based on VSFR format
        // This is a simplified version as Arduino doesn't easily support complex type handling
        // In a full implementation, we'd need to map each VSFR to its format

        uint32_t vsfr_id = vsfr_ids[i];
        if ((vsfr_id == VSFR::CHN_TO_keV_A0) || (vsfr_id == VSFR::CHN_TO_keV_A1) || (vsfr_id == VSFR::CHN_TO_keV_A2))
        {
            // These are float values
            float float_val;
            memcpy(&float_val, &raw_value, 4);
            ret.push_back(float_val);
        }
        else if ((vsfr_id == VSFR::DS_UNITS) || (vsfr_id == VSFR::CR_UNITS))
        {
            // These are boolean flags in the LSB
            ret.push_back(raw_value & 0x01);
        }
        else if ((vsfr_id == VSFR::TEMP_degC) || (vsfr_id == VSFR::RAW_TEMP_degC) || 
                 (vsfr_id == VSFR::TEMP_UP_degC) || (vsfr_id == VSFR::TEMP_DN_degC))
        {
            // Temperature values - these are IEEE-754 float values
            float float_val;
            memcpy(&float_val, &raw_value, 4);
            ret.push_back(float_val);
        }
        else
        {
            // Most values are integers
            ret.push_back((float)raw_value);
        }
    }

    return ret;
}

uint32_t RadiaCode::readVSFR(uint32_t vsfr_id)
{
    // Prepare message buffer for single VSFR
    size_t msg_size = 4 + 4; // 4 bytes for count, 4 bytes for VSFR ID
    uint8_t* msg = new uint8_t[msg_size];

    // Add count (1 VSFR)
    uint32_t count = 1;
    memcpy(msg, &count, 4);

    // Add VSFR ID
    memcpy(msg + 4, &vsfr_id, 4);

    // Send request
    BytesBuffer r = execute(COMMAND::RD_VIRT_SFR_BATCH, msg, msg_size);

    // Clean up
    delete[] msg;

    // Check validity flags
    uint32_t valid_flags;
    r.readUint32(&valid_flags);

    if (valid_flags != 1)
    {
#ifdef RC_DEBUG_ERROR
        RC_LOGE("Error: Invalid VSFR ID 0x%08X, validity flags: 0x%X", (unsigned)vsfr_id, (unsigned)valid_flags);
#endif
        return 0;
    }

    // Read the single value
    uint32_t raw_value;
    r.readUint32(&raw_value);

    return raw_value;
}

float RadiaCode::getTemperature(void)
{
    std::vector<uint32_t> vsfr_ids = {VSFR::TEMP_degC};
    std::vector<float> values = batchReadVSFRs(vsfr_ids);
    if (values.size() > 0)
    {
        return values[0];
    }
    return 0.0f;
}

uint32_t RadiaCode::deviceStatus(void)
{
    uint32_t flags;
    BytesBuffer r;

    r = execute(COMMAND::GET_STATUS);
    r.readUint32(&flags);
    return flags;
}

void RadiaCode::setLocalTime(uint8_t day, uint8_t month, uint16_t year, uint8_t second, uint8_t minute, uint8_t hour)
{
    uint8_t d[8];
    d[0] = day;
    d[1] = month;
    if (year >= 2000)
    {
        d[2] = year - 2000; // 2-digit year
    }
    else
    {
        d[2] = year - 1900; // 2-digit year
    }
    d[3] = 0;
    d[4] = second;
    d[5] = minute;
    d[6] = hour;
    d[7] = 0;

    execute(COMMAND::SET_TIME, d, sizeof(d));
}

std::string RadiaCode::fwSignature(void)
{
    uint32_t signature;
    char buf[256];
    BytesBuffer r = execute(COMMAND::FW_SIGNATURE);

    r.readUint32(&signature);
    std::string filename = r.readString();
    std::string idstring = r.readString();
    sprintf(buf, "Signature: %08lX, FileName=\"%s\", IdString=\"%s\"", (unsigned long)signature, filename.c_str(), idstring.c_str());
    return std::string(buf);
}

std::tuple<int, int, std::string, int, int, std::string> RadiaCode::fwVersion(void)
{
    BytesBuffer r;
    uint16_t boot_minor, boot_major, target_minor, target_major;
    std::string boot_date, target_date;

    r = execute(COMMAND::GET_VERSION);
    r.readUint16(&boot_minor);
    r.readUint16(&boot_major);
    boot_date = r.readString();

    r.readUint16(&target_minor);
    r.readUint16(&target_major);

    target_date = r.readString();
    if (!target_date.empty())
    {
        target_date.resize(target_date.length() - 1);  // Remove trailing null byte
    }

    return std::make_tuple(boot_major, boot_minor, boot_date, target_major, target_minor, target_date);
}

std::string RadiaCode::hwSerialNumber(void)
{
    uint32_t serial_len;
    BytesBuffer r = execute(COMMAND::GET_SERIAL);

    r.readUint32(&serial_len);

    if ((serial_len % 4) != 0)
    {
#ifdef RC_DEBUG_ERROR
        RC_LOGE("Error: Serial number length is not a multiple of 4");
#endif
        return "";
    }

    std::string serial = "";
    for (uint32_t i = 0; i < (serial_len / 4); i++)
    {
        uint32_t group;
        r.readUint32(&group);
        char buf[10];
        sprintf(buf, "%08lX", (unsigned long)group);
        if (i > 0) serial += "-";
        serial += buf;
    }

    return serial;
}

std::string RadiaCode::configuration(void)
{
    std::string result;
    BytesBuffer r = readRequest(VS::CONFIGURATION);

    size_t len = r.available();
    char* buffer = new char[len + 1];
    r.readBytes((uint8_t*)buffer, len);
    buffer[len] = '\0';
    result = std::string(buffer);
    delete[] buffer;
    return result;
}

std::string RadiaCode::textMessage(void)
{
    BytesBuffer r = readRequest(VS::TEXT_MESSAGE);

    size_t len = r.available();
    char* buffer = new char[len + 1];
    r.readBytes((uint8_t*)buffer, len);
    buffer[len] = '\0';

    std::string result = std::string(buffer);
    delete[] buffer;

    return result;
}

std::string RadiaCode::serialNumber(void)
{
    std::string result;
    size_t len;
    BytesBuffer r;
    char* buffer;

    r = readRequest(VS::SERIAL_NUMBER);
    len = r.available();
    buffer = new char[len + 1];
    r.readBytes((uint8_t*)buffer, len);
    buffer[len] = '\0';
    result = std::string(buffer);
    delete[] buffer;
    return result;
}

std::string RadiaCode::commands(void)
{
    BytesBuffer r = readRequest(VS::SFR_FILE);

    size_t len = r.available();
    char* buffer = new char[len + 1];
    r.readBytes((uint8_t*)buffer, len);
    buffer[len] = '\0';

    std::string result = std::string(buffer);
    delete[] buffer;

    return result;
}

void RadiaCode::deviceTime(uint32_t v)
{
    uint8_t data[4];
    memcpy(data, &v, 4);

    writeRequest(VSFR::DEVICE_TIME, data, 4);
}

std::vector<DataItem*> RadiaCode::dataBuf(void)
{
    if (_connection == nullptr)
    {
        return std::vector<DataItem*>();
    }

    // Enable streaming record parsing on the transport so a large DATA_BUF
    // response is decoded incrementally as BLE notifications arrive, instead
    // of buffering the whole (potentially huge) response in RAM.
    _connection->setDataBufStream(_base_time_sec);

    // Send the DATA_BUF read request: RD_VIRT_STRING with the DATA_BUF VS id.
    uint32_t command_id = VS::DATA_BUF;
    uint8_t cmd_bytes[4];
    memcpy(cmd_bytes, &command_id, 4);
    BytesBuffer r = execute(COMMAND::RD_VIRT_STRING, cmd_bytes, 4);

    // Drain the records the transport parsed incrementally.
    return _connection->drainDataBufRecords();
}

bool RadiaCode::isConnected(void) const
{
    return (_connection != nullptr) && _connection->isConnected();
}

Spectrum RadiaCode::spectrum(void)
{
    // Use a static Spectrum object
    static Spectrum result;

    // Clear the object for reuse
    result.clear();

    // Get the raw spectrum data
    BytesBuffer r = readRequest(VS::SPECTRUM);

    // Check if we got valid data back
    if (r.getSize() < 16)
    { // Minimum size for header (duration + a0,a1,a2)
#ifdef RC_DEBUG_ERROR
        RC_LOGE("Error: Invalid or empty spectrum data received");
#endif
        return result; // Return empty spectrum
    }

    // Process the buffer directly into our static object
    decodeSpectrum(r, _spectrum_format_version, result);

#ifdef RC_DEBUG_WARNING
    // Safety check - make sure we got some data
    if (result.count_size == 0)
    {
        RC_LOGW("Warning: No spectrum data points decoded");
    }
#endif

    // Return a copy
    return result;
}

Spectrum RadiaCode::spectrumAccum(void)
{
    // Use a static Spectrum object
    static Spectrum result;

    // Clear the object for reuse
    result.clear();
    
    // Get the raw spectrum data
    BytesBuffer r = readRequest(VS::SPEC_ACCUM);
    
    // Check if we got valid data back
    if (r.getSize() < 16)
    { // Minimum size for header (duration + a0,a1,a2)
#ifdef RC_DEBUG_ERROR
        RC_LOGE("Error: Invalid or empty accumulated spectrum data received");
#endif
        return result; // Return empty spectrum
    }

    // Process the buffer directly into our static object
    decodeSpectrum(r, _spectrum_format_version, result);

#ifdef RC_DEBUG_WARNING
    // Safety check - make sure we got some data
    if (result.count_size == 0)
    {
        RC_LOGW("Warning: No accumulated spectrum data points decoded");
    }
#endif

    // Return a copy
    return result;
}

void RadiaCode::doseReset(void)
{
    writeRequest(VSFR::DOSE_RESET);
}

void RadiaCode::spectrumReset(void)
{
    uint32_t cmd = VS::SPECTRUM;
    uint32_t size = 0;

    uint8_t data[8];
    memcpy(data, &cmd, 4);
    memcpy(data + 4, &size, 4);

    BytesBuffer r = execute(COMMAND::WR_VIRT_STRING, data, 8);

    uint32_t retcode;
    r.readUint32(&retcode);

#ifdef RC_DEBUG_ERROR
    if (retcode != 1)
    {
        RC_LOGE("Spectrum reset failed, retcode=%u", (unsigned)retcode);
    }
#endif
}

std::vector<float> RadiaCode::energyCalib(void)
{
    BytesBuffer r;
    std::vector<float> coefficients;

    r = readRequest(VS::ENERGY_CALIB);
    for (int i = 0; i < 3; i++)
    {
        float coef;
        r.readFloat(&coef);
        coefficients.push_back(coef);
    }

    return coefficients;
}

void RadiaCode::setEnergyCalib(float a0, float a1, float a2)
{
    uint32_t retcode;
    BytesBuffer r;
    uint8_t  pc[12]; // 3 floats * 4 bytes each
    uint32_t cmd = VS::ENERGY_CALIB;
    uint32_t size = sizeof(pc);
    uint8_t  data[8 + sizeof(pc)];

    memcpy(pc, &a0, 4);
    memcpy(pc + 4, &a1, 4);
    memcpy(pc + 8, &a2, 4);
    memcpy(data, &cmd, 4);
    memcpy(data + 4, &size, 4);
    memcpy(data + 8, pc, sizeof(pc));
    r = execute(COMMAND::WR_VIRT_STRING, data, 8 + sizeof(pc));
    r.readUint32(&retcode);

#ifdef RC_DEBUG_ERROR
    if (retcode != 1)
    {
        RC_LOGE("Set energy calibration failed, retcode=%u", (unsigned)retcode);
    }
#endif
}

uint8_t RadiaCode::getSpectrumFormatVersion(void)
{
    return _spectrum_format_version;
}

void RadiaCode::setLanguage(const char* lang)
{
    if ((strcmp(lang, "ru") != 0) && (strcmp(lang, "en") != 0))
    {
#ifdef RC_DEBUG_ERROR
        RC_LOGE("Error: Unsupported language. Use 'ru' or 'en'");
#endif
        return;
    }

    uint32_t value = (strcmp(lang, "en") == 0) ? 1 : 0;
    uint8_t data[4];
    memcpy(data, &value, 4);

    writeRequest(VSFR::DEVICE_LANG, data, 4);
}

void RadiaCode::setDeviceOn(bool on)
{
    uint32_t value = on ? 1 : 0;
    uint8_t data[4];
    memcpy(data, &value, 4);

    writeRequest(VSFR::DEVICE_ON, data, 4);
}

void RadiaCode::setSoundOn(bool on)
{
    uint32_t value = on ? 1 : 0;
    uint8_t data[4];
    memcpy(data, &value, 4);

    writeRequest(VSFR::SOUND_ON, data, 4);
}

void RadiaCode::setVibroOn(bool on)
{
    uint32_t value = on ? 1 : 0;
    uint8_t data[4];
    memcpy(data, &value, 4);

    writeRequest(VSFR::VIBRO_ON, data, 4);
}

void RadiaCode::setLightOn(bool on)
{
    uint8_t data[4];
    uint32_t deviceCtrl = readVSFR(VSFR::DEVICE_CTRL);
    if (on)
    {
        deviceCtrl |= DEV_CTRL::LIGHT;
    }
    else
    {
        deviceCtrl &= ~DEV_CTRL::LIGHT;
    }
    memcpy(data, &deviceCtrl, 4);
    writeRequest(VSFR::DEVICE_CTRL, data, 4);
}

void RadiaCode::setDeviceCtrl(DEV_CTRL ctrl_flags)
{
    uint8_t data[4];
    uint32_t value = (ctrl_flags & ~DEV_CTRL::BIT_1) | DEV_CTRL::BIT_5; // Ensure BIT1 is always 0 and BIT5 is always 1

    memcpy(data, &value, 4);
    writeRequest(VSFR::DEVICE_CTRL, data, 4);
}

void RadiaCode::setSoundCtrl(CTRL ctrl_flags)
{
    uint32_t value = ctrl_flags;
    uint8_t data[4];
    memcpy(data, &value, 4);

    writeRequest(VSFR::SOUND_CTRL, data, 4);
}

void RadiaCode::setVibroCtrl(CTRL ctrl_flags)
{
    // Check if CLICKS flag is set, which is not supported for vibro
    if (ctrl_flags & CTRL::CLICKS)
    {
#ifdef RC_DEBUG_ERROR
        RC_LOGE("Error: CTRL::CLICKS not supported for vibro");
#endif
        return;
    }
    // Check if CONNECTION flag is set, which is not supported for vibro
    if (ctrl_flags & CTRL::CONNECTION)
    {
#ifdef RC_DEBUG_ERROR
        RC_LOGE("Error: CTRL::CONNECTION not supported for vibro");
#endif
        return;
    }
    // Check if POWER flag is set, which is not supported for vibro
    if (ctrl_flags & CTRL::POWER)
    {
#ifdef RC_DEBUG_ERROR
        RC_LOGE("Error: CTRL::POWER not supported for vibro");
#endif
        return;
    }

    uint32_t value = ctrl_flags;
    uint8_t data[4];
    memcpy(data, &value, 4);

    writeRequest(VSFR::VIBRO_CTRL, data, 4);
}

void RadiaCode::setDisplayOffTime(uint8_t seconds)
{
    if ((seconds != 5) && (seconds != 10) && (seconds != 15) && (seconds != 30))
    {
#ifdef RC_DEBUG_ERROR
        RC_LOGE("Error: Display off time must be 5, 10, 15, or 30 seconds");
#endif
        return;
    }

    uint32_t value = (seconds == 30) ? 3 : ((seconds / 5) - 1);
    uint8_t data[4];
    memcpy(data, &value, 4);

    writeRequest(VSFR::DISP_OFF_TIME, data, 4);
}

void RadiaCode::setDisplayBrightness(uint8_t brightness)
{
    if (brightness > 9)
    {
#ifdef RC_DEBUG_ERROR
        RC_LOGE("Error: Brightness must be between 0 and 9");
#endif
        return;
    }

    uint32_t value = brightness;
    uint8_t data[4];
    memcpy(data, &value, 4);

    writeRequest(VSFR::DISP_BRT, data, 4);
}

void RadiaCode::setDisplayDirection(DisplayDirection direction)
{
    uint32_t value = direction;
    uint8_t data[4];
    memcpy(data, &value, 4);

    writeRequest(VSFR::DISP_DIR, data, 4);
}

void RadiaCode::setDisplayCtrl(DISPLAY_CTRL ctrl_flags)
{
    uint32_t value = ctrl_flags;
    uint8_t data[4];
    memcpy(data, &value, 4);

    writeRequest(VSFR::DISP_CTRL, data, 4);
}

void RadiaCode::setMeasurementUnit(MeasurementUnits unit)
{
    uint32_t value = unit;
    uint8_t data[4];
    memcpy(data, &value, 4);

    writeRequest(VSFR::DS_UNITS, data, 4);
}

void RadiaCode::setCountRateUnit(CountRateUnits unit)
{
    uint32_t value = unit;
    uint8_t data[4];
    memcpy(data, &value, 4);

    writeRequest(VSFR::CR_UNITS, data, 4);
}

void RadiaCode::setTemperatureUnit(TemperatureUnits unit)
{
    uint32_t value = unit;
    uint8_t data[4];
    memcpy(data, &value, 4);

    writeRequest(VSFR::TEMP_UNITS, data, 4);
}

void RadiaCode::setAlarmSignalMode(AlarmSignalMode mode)
{
    uint32_t value = mode;
    uint8_t data[4];
    memcpy(data, &value, 4);

    writeRequest(VSFR::ALARM_MODE, data, 4);
}

AlarmLimits RadiaCode::getAlarmLimits(void)
{
    std::vector<uint32_t> regs =
    {
        VSFR::CR_LEV1_cp10s,
        VSFR::CR_LEV2_cp10s,
        VSFR::DR_LEV1_uR_h,
        VSFR::DR_LEV2_uR_h,
        VSFR::DS_LEV1_uR,
        VSFR::DS_LEV2_uR,
        VSFR::DS_UNITS,
        VSFR::CR_UNITS
    };

    std::vector<float> resp = batchReadVSFRs(regs);

    AlarmLimits limits;

    if (resp.size() >= 8)
    {
        float dose_multiplier = resp[6] ? 100.0f : 1.0f;
        float count_multiplier = resp[7] ? 60.0f : 1.0f;

        limits.l1_count_rate = resp[0] / 10.0f * count_multiplier;
        limits.l2_count_rate = resp[1] / 10.0f * count_multiplier;
        limits.l1_dose_rate = resp[2] / dose_multiplier;
        limits.l2_dose_rate = resp[3] / dose_multiplier;
        limits.l1_dose = resp[4] / 1.0e6f / dose_multiplier;
        limits.l2_dose = resp[5] / 1.0e6f / dose_multiplier;
        limits.dose_unit = resp[6] ? "Sv" : "R";
        limits.count_unit = resp[7] ? "cpm" : "cps";
    }

    return limits;
}

bool RadiaCode::setAlarmLimits(
    float l1_count_rate, 
    float l2_count_rate,
    float l1_dose_rate, 
    float l2_dose_rate,
    float l1_dose, 
    float l2_dose,
    bool dose_unit_sv, 
    bool count_unit_cpm)
{
    std::vector<uint32_t> which_limits;
    std::vector<uint32_t> limit_values;

    float dose_multiplier = dose_unit_sv ? 100.0f : 1.0f;
    float count_multiplier = count_unit_cpm ? 1.0f / 6.0f : 10.0f;

    if (l1_count_rate >= 0.0f)
    {
        which_limits.push_back(VSFR::CR_LEV1_cp10s);
        limit_values.push_back((uint32_t)std::round(l1_count_rate * count_multiplier));
    }

    if (l2_count_rate >= 0.0f)
    {
        which_limits.push_back(VSFR::CR_LEV2_cp10s);
        limit_values.push_back((uint32_t)std::round(l2_count_rate * count_multiplier));
    }

    if (l1_dose_rate >= 0.0f)
    {
        which_limits.push_back(VSFR::DR_LEV1_uR_h);
        limit_values.push_back((uint32_t)std::round(l1_dose_rate * dose_multiplier));
    }

    if (l2_dose_rate >= 0.0f)
    {
        which_limits.push_back(VSFR::DR_LEV2_uR_h);
        limit_values.push_back((uint32_t)std::round(l2_dose_rate * dose_multiplier));
    }

    if (l1_dose >= 0.0f)
    {
        which_limits.push_back(VSFR::DS_LEV1_uR);
        limit_values.push_back((uint32_t)std::round(l1_dose * 1.0e6f * dose_multiplier));
    }

    if (l2_dose >= 0.0f)
    {
        which_limits.push_back(VSFR::DS_LEV2_uR);
        limit_values.push_back((uint32_t)std::round(l2_dose * 1.0e6f * dose_multiplier));
    }

    size_t num_to_set = which_limits.size();
    if (num_to_set == 0)
    {
#ifdef RC_DEBUG_ERROR
        RC_LOGE("Error: No limits specified");
#endif
        return false;
    }

    which_limits.push_back(VSFR::DS_UNITS);
    limit_values.push_back((uint32_t)(dose_unit_sv ? 1 : 0));

    which_limits.push_back(VSFR::CR_UNITS);
    limit_values.push_back((uint32_t)(count_unit_cpm ? 1 : 0));

    num_to_set = which_limits.size();

    // Create message buffer
    size_t msg_size = 4 + num_to_set * 4 * 2; // 4 bytes for count, 4 bytes per ID and value
    uint8_t* msg = new uint8_t[msg_size];

    // Add count
    uint32_t count = num_to_set;
    memcpy(msg, &count, 4);

    // Add IDs
    for (size_t i = 0; i < num_to_set; i++)
    {
        uint32_t id = which_limits[i];
        memcpy(msg + 4 + i * 4, &id, 4);
    }

    // Add values
    for (size_t i = 0; i < num_to_set; i++)
    {
        uint32_t value = limit_values[i];
        memcpy(msg + 4 + num_to_set * 4 + i * 4, &value, 4);
    }

    // Send request
    BytesBuffer resp = execute(COMMAND::WR_VIRT_SFR_BATCH, msg, msg_size);

    // Clean up
    delete[] msg;

    // Check response
    uint32_t result;
    resp.readUint32(&result);

    uint32_t expected_valid = (1 << num_to_set) - 1;
    return expected_valid == result;
}
