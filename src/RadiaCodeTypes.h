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

#ifndef RadiaCodeTypes_h
#define RadiaCodeTypes_h

#include <cstdint>
#include <string>
#include <vector>

// Forward declarations
class BytesBuffer;

// Enum for Virtual String (VS) identifiers
enum VS
{
    CONFIGURATION   = 2,
    //FW_DESCRIPTOR = 3,       not yet supported !!
    SERIAL_NUMBER   = 8,
    TEXT_MESSAGE    = 0xF,
    //MEM_SNAPSHOT  = 0xE0,    not yet supported !!
    DATA_BUF        = 0x100,
    SFR_FILE        = 0x101,
    SPECTRUM        = 0x200,
    ENERGY_CALIB    = 0x202,
    SPEC_ACCUM      = 0x205,
    //SPEC_DIFF     = 0x206,   not yet supported !!
    //SPEC_RESET    = 0x207    not yet supported !!
};

// Enum for Virtual Special Function Register (VSFR) identifiers
enum VSFR
{
    DEVICE_CTRL          = 0x0500,
    DEVICE_LANG          = 0x0502,
    DEVICE_ON            = 0x0503,
    DEVICE_TIME          = 0x0504,

    DISP_CTRL            = 0x0510,
    DISP_BRT             = 0x0511,
    DISP_CONTR           = 0x0512,
    DISP_OFF_TIME        = 0x0513,
    // DISP_ON           = 0x0514,   not supported anymore ?!
    DISP_DIR             = 0x0515,
    // DISP_BACKLT_ON    = 0x0516,   not supported anymore ?!

    SOUND_CTRL           = 0x0520,
    //SOUND_VOL          = 0x0521,   not supported anymore ?!
    SOUND_ON             = 0x0522,
    //SOUND_BUTTON       = 0x0523,   not supported anymore ?!

    VIBRO_CTRL           = 0x0530,
    VIBRO_ON             = 0x0531,

    //LEDS_CTRL          = 0x0540,   not supported anymore ?!
    //LED0_BRT           = 0x0541,   not supported anymore ?!
    //LED1_BRT           = 0x0542,   not supported anymore ?!
    //LED2_BRT           = 0x0543,   not supported anymore ?!
    //LED3_BRT           = 0x0544,   not supported anymore ?!
    //LEDS_ON            = 0x0545,   not supported anymore ?!

    ALARM_MODE           = 0x05E0,
    //PLAY_SIGNAL        = 0x05E1,   not supported anymore ?!

    //MS_CTRL            = 0x0600,   not supported anymore ?!
    //MS_MODE            = 0x0601,   not supported anymore ?!
    //MS_SUB_MODE        = 0x0602,   not supported anymore ?!
    //MS_RUN             = 0x0603,   not supported anymore ?!

    //BLE_TX_PWR         = 0x0700,   not supported anymore ?!

    DR_LEV1_uR_h         = 0x8000,
    DR_LEV2_uR_h         = 0x8001,
    //DS_LEV1_100uR      = 0x8002,   not supported anymore ?!
    //DS_LEV2_100uR      = 0x8003,   not supported anymore ?!
    DS_UNITS             = 0x8004,
    //CPS_FILTER         = 0x8005,   not supported anymore ?!
    //RAW_FILTER         = 0x8006,   not supported anymore ?!
    DOSE_RESET           = 0x8007,
    CR_LEV1_cp10s        = 0x8008,
    CR_LEV2_cp10s        = 0x8009,

    //USE_nSv_h          = 0x800C,   not supported anymore ?!

    CHN_TO_keV_A0        = 0x8010,
    CHN_TO_keV_A1        = 0x8011,
    CHN_TO_keV_A2        = 0x8012,
    CR_UNITS             = 0x8013,
    DS_LEV1_uR           = 0x8014,
    DS_LEV2_uR           = 0x8015,

    TEMP_UNITS           = 0x8016,

    //CPS                = 0x8020,   not supported anymore ?!
    //DR_uR_h            = 0x8021,   not supported anymore ?!
    //DS_uR              = 0x8022,   not supported anymore ?!

    TEMP_degC            = 0x8024,
    //ACC_X              = 0x8025,   not supported anymore ?!
    //ACC_Y              = 0x8026,   not supported anymore ?!
    //ACC_Z              = 0x8027,   not supported anymore ?!
    //OPT                = 0x8028,   not supported anymore ?!

    RAW_TEMP_degC        = 0x8033,
    TEMP_UP_degC         = 0x8034,
    TEMP_DN_degC         = 0x8035

    //VBIAS_mV           = 0xC000,   not supported anymore ?!
    //COMP_LEV           = 0xC001,   not supported anymore ?!
    //CALIB_MODE         = 0xC002,   not supported anymore ?!
    //DPOT_RDAC          = 0xC004,   not supported anymore ?!
    //DPOT_RDAC_EEPROM   = 0xC005,   not supported anymore ?!
    //DPOT_TOLER         = 0xC006,   not supported anymore ?!

    //SYS_MCU_ID0        = 0xFFFF0000,   not supported anymore ?!
    //SYS_MCU_ID1        = 0xFFFF0001,   not supported anymore ?!
    //SYS_MCU_ID2        = 0xFFFF0002,   not supported anymore ?!

    //SYS_DEVICE_ID      = 0xFFFF0005,   not supported anymore ?!
    //SYS_SIGNATURE      = 0xFFFF0006,   not supported anymore ?!
    //SYS_RX_SIZE        = 0xFFFF0007,   not supported anymore ?!
    //SYS_TX_SIZE        = 0xFFFF0008,   not supported anymore ?!
    //SYS_BOOT_VERSION   = 0xFFFF0009,   not supported anymore ?!
    //SYS_TARGET_VERSION = 0xFFFF000A,   not supported anymore ?!
    //SYS_STATUS         = 0xFFFF000B,   not supported anymore ?!
    //SYS_MCU_VREF       = 0xFFFF000C,   not supported anymore ?!
    //SYS_MCU_TEMP       = 0xFFFF000D,   not supported anymore ?!
    //SYS_FW_VER_BT      = 0xFFFFß010    not supported anymore ?!
};

// Enum for Command identifiers
enum COMMAND
{
    GET_STATUS          = 0x0005,
    SET_EXCHANGE        = 0x0007,
    GET_VERSION         = 0x000A,
    GET_SERIAL          = 0x000B,
    //FW_IMAGE_GET_INFO = 0x0012,   not yet supported !!
    FW_SIGNATURE        = 0x0101,
    //RD_HW_CONFIG      = 0x0807,   not yet supported !!
    //RD_FLASH          = 0x081C,   not yet supported !!
    //RD_VIRT_SFR       = 0x0824,   not yet supported !!
    WR_VIRT_SFR         = 0x0825,
    RD_VIRT_STRING      = 0x0826,
    WR_VIRT_STRING      = 0x0827,
    RD_VIRT_SFR_BATCH   = 0x082A,
    WR_VIRT_SFR_BATCH   = 0x082B,
    SET_TIME            = 0x0A04
};

// Enum for Device Control Flags
enum DEV_CTRL
{
    PWR   = 1 << 0,
    BIT_1 = 1 << 1,
    SOUND = 1 << 2,
    LIGHT = 1 << 3,
    VIBRO = 1 << 4,
    BIT_5 = 1 << 5
};

// Enum for Sound and Vibration Control Flags
enum CTRL
{
    BUTTONS                 = 1 << 0,
    CLICKS                  = 1 << 1,
    DOSE_RATE_ALARM_1       = 1 << 2,
    DOSE_RATE_ALARM_2       = 1 << 3,
    DOSE_RATE_OUT_OF_SCALE  = 1 << 4,
    DOSE_ALARM_1            = 1 << 5,
    DOSE_ALARM_2            = 1 << 6,
    DOSE_OUT_OF_SCALE       = 1 << 7,
    CONNECTION              = 1 << 8,
    POWER                   = 1 << 9,
    COUNT_RATE_ALARM_1      = 1 << 10,
    COUNT_RATE_ALARM_2      = 1 << 11,
    COUNT_RATE_OUT_OF_SCALE = 1 << 12
};

// Enum for Event IDs
enum EventId
{
    POWER_OFF          = 0,
    POWER_ON           = 1,
    TOGGLE_SIGNAL      = 3,
    EVENT_DOSE_RESET   = 4,
    BATTERY_FULL       = 7,
    CHARGE_STOP        = 8,
    DOSE_RATE_ALARM1   = 9,
    DOSE_RATE_ALARM2   = 10,
    DOSE_ALARM1        = 12,
    DOSE_ALARM2        = 13,
    EVENT_TEXT_MESSAGE = 17,
    SPECTRUM_RESET     = 19,
    COUNT_RATE_ALARM1  = 20,
    COUNT_RATE_ALARM2  = 21
};

// Enum for Display Control Flags
enum DISPLAY_CTRL
{
    BACKLT_OFF          = 0,
    BACKLT_ON_BY_BUTTON = 1 << 2,
    BACKLT_ON_AUTO      = 1 << 3
};

// Enum for Display Direction
enum DisplayDirection
{
    AUTO  = 0,
    RIGHT = 1,
    LEFT  = 2
};

// Enum for Alarm Signal Modes
enum AlarmSignalMode
{
    CONTINUOUSLY = 0,
    ONCE         = 1
};

// Enum for Measurement Units
enum MeasurementUnits
{
    ROENTGEN = 0,
    SIEVERT  = 1
};

// Enum for Count Rate Units
enum CountRateUnits
{
    CPS = 0,
    CPM = 1
};

// Enum for Temperature Units
enum TemperatureUnits
{
    CELSIUS    = 0,
    FAHRENHEIT = 1
};

// Define item types for identification without RTTI
enum DataItemType
{
    TYPE_UNKNOWN        = 0,
    TYPE_REAL_TIME_DATA = 1,
    TYPE_RAW_DATA       = 2,
    TYPE_DOSE_RATE_DB   = 3,
    TYPE_RARE_DATA      = 4,
    TYPE_EVENT          = 5,
    TYPE_DOSE_COUNTER   = 6
};

class DataItem
{
    public:
        virtual ~DataItem() {}
        uint32_t timestamp;               // Unix timestamp
        DataItemType type = TYPE_UNKNOWN; // Item type for identification
};

class RealTimeData : public DataItem
{
    public:
        RealTimeData() { type = TYPE_REAL_TIME_DATA; }
        float count_rate;
        float count_rate_err;
        float dose_rate;  // Changed from int to float for proper data handling
        float dose_rate_err;
        uint16_t flags;
        uint8_t real_time_flags;
};

class RawData : public DataItem
{
    public:
        RawData() { type = TYPE_RAW_DATA; }
        float count_rate;
        float dose_rate;
};

class DoseRateDB : public DataItem
{
    public:
        DoseRateDB() { type = TYPE_DOSE_RATE_DB; }
        uint32_t count;
        float count_rate;
        float dose_rate;
        float dose_rate_err;
        uint16_t flags;
};

class RareData : public DataItem
{
    public:
        RareData() { type = TYPE_RARE_DATA; }
        uint32_t duration;
        float dose;
        float temperature;
        float charge_level;
        uint16_t flags;
};

class DoseCounter : public DataItem
{
    public:
        DoseCounter() { type = TYPE_DOSE_COUNTER; }
        uint32_t dose_counter;
        uint16_t flags;
};

class Event : public DataItem
{
    public:
        Event() { type = TYPE_EVENT; }
        EventId event;
        uint8_t event_param1;
        uint16_t flags;
        // Alarm / off-scale events carry the rate that triggered them. Only one
        // of the (count_rate, dose_rate) pairs is populated, matching the
        // python reference decoder.
        float count_rate = 0.0f;
        float count_rate_err = 0.0f;
        bool has_count_rate = false;
        float dose_rate = 0.0f;
        float dose_rate_err = 0.0f;
        bool has_dose_rate = false;
};

struct Spectrum
{
    uint32_t duration_sec; // Duration in seconds
    float a0;
    float a1;
    float a2;

    // Define a reasonable number of channels that won't overflow the ESP32 stack
    static const size_t MAX_CHANNELS = 1024;

    // Use a static array shared by all instances to avoid stack overflow
    // This means only one spectrum can be processed at a time, but that's usually okay
    static uint32_t shared_counts[MAX_CHANNELS];

    // Fixed array to avoid any memory allocations/deallocations
    // This is a pointer to the static array
    uint32_t* counts;

    size_t count_size; // Number of valid elements in the counts array

    // Constructor
    Spectrum(void);

    // Destructor - nothing to do with fixed arrays
    ~Spectrum(void);

    // Copy constructor
    Spectrum(const Spectrum& other);

    // Assignment operator
    Spectrum& operator=(const Spectrum& other);

    // Clear method - just reset the size
    void clear(void);

    // Vector-like interface methods for compatibility
    size_t size(void) const;
    bool empty(void) const;
    void push_back(uint32_t value);
    uint32_t at(size_t index) const;
};

struct AlarmLimits
{
    float l1_count_rate;
    float l2_count_rate;
    std::string count_unit;
    float l1_dose_rate;
    float l2_dose_rate;
    float l1_dose;
    float l2_dose;
    std::string dose_unit;
};

#endif
