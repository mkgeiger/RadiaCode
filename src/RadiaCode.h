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
  RadiaCode.h - Library for interfacing with RadiaCode radiation detection devices
*/

#ifndef RadiaCode_h
#define RadiaCode_h

#include <string>
#include "BytesBuffer.h"
#include "RadiaCodeTypes.h"
#include "RadiaCodeTransport.h"

// Driver version
#define DRIVER_VERSION_MAJOR 1
#define DRIVER_VERSION_MINOR 0
#define DRIVER_VERSION_PATCH 0

// Forward declarations
class BluetoothTransport;

float spectrumChannelToEnergy(int channel_number, float a0, float a1, float a2);
const char* getDriverVersion(void);

class RadiaCode
{
    public:
        // Constructor and Destructor
        RadiaCode(const char* bluetooth_mac = nullptr, bool ignore_firmware_compatibility_check = false);
        ~RadiaCode(void);

        // Device information methods
        uint32_t deviceStatus(void);
        std::string fwSignature(void);
        std::tuple<int, int, std::string, int, int, std::string> fwVersion(void);
        std::string hwSerialNumber(void);
        std::string serialNumber(void);
        std::string configuration(void);
        std::string textMessage(void);
        std::string commands(void);

        // Time and configuration methods
        void setLocalTime(uint8_t day, uint8_t month, uint16_t year, uint8_t second, uint8_t minute, uint8_t hour);
        void deviceTime(uint32_t v);

        // Data acquisition methods
        std::vector<DataItem*> dataBuf(void);
        Spectrum spectrum(void);
        Spectrum spectrumAccum(void);

        // Connection status
        bool isConnected(void) const;

        // Reset methods
        void doseReset(void);
        void spectrumReset(void);

        // Calibration methods
        std::vector<float> energyCalib(void);
        void setEnergyCalib(float a0, float a1, float a2);

        // Debug methods
        uint8_t getSpectrumFormatVersion(void);

        // Device settings methods
        void setLanguage(const char* lang);
        void setDeviceOn(bool on);
        void setSoundOn(bool on);
        void setVibroOn(bool on);
        void setLightOn(bool on);
        void setDeviceCtrl(DEV_CTRL ctrl_flags);
        void setSoundCtrl(CTRL ctrl_flags);
        void setVibroCtrl(CTRL ctrl_flags);
        void setDisplayCtrl(DISPLAY_CTRL ctrl_flags);
        void setDisplayOffTime(uint8_t seconds);
        void setDisplayBrightness(uint8_t brightness);
        void setDisplayDirection(DisplayDirection direction);
        void setMeasurementUnit(MeasurementUnits unit);
        void setCountRateUnit(CountRateUnits unit);
        void setTemperatureUnit(TemperatureUnits unit);

        // Alarm methods
        void setAlarmSignalMode(AlarmSignalMode mode);
        AlarmLimits getAlarmLimits(void);
        bool setAlarmLimits(
            float l1_count_rate = -1, 
            float l2_count_rate = -1,
            float l1_dose_rate = -1, 
            float l2_dose_rate = -1,
            float l1_dose = -1, 
            float l2_dose = -1,
            bool dose_unit_sv = false, 
            bool count_unit_cpm = false);

        // Direct sensor reading methods
        float getTemperature(void);

    private:
        // Low-level communication methods
        BytesBuffer execute(COMMAND reqtype, const uint8_t* args = nullptr, size_t args_len = 0);
        BytesBuffer readRequest(uint32_t command_id);
        void writeRequest(uint32_t command_id, const uint8_t* data = nullptr, size_t data_len = 0);
        std::vector<float> batchReadVSFRs(const std::vector<uint32_t>& vsfr_ids);
        uint32_t readVSFR(uint32_t vsfr_id);

        // Variables
        RadiaCodeTransport* _connection;
        uint8_t _seq;
        bool _bt_supported;
        uint32_t _base_time_sec;
        uint8_t _spectrum_format_version;
};

#endif
