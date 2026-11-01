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

#include "Decoders.h"
#include "BytesBuffer.h"
#include "RadiaCodeTypes.h"
#include "RadiaCodeLog.h"
#include "RadiaCodeCompat.h"

// Debugging switches
#undef  DEC_DEBUG_INFO
#define DEC_DEBUG_WARNING
#define DEC_DEBUG_ERROR

// Helper function for spectrum decoding (version 0)
void decodeCountsV0(BytesBuffer& br, Spectrum& spectrum)
{
    // Clear the counts before starting
    spectrum.count_size = 0;

    // Safety check - make sure we have data available
    if (br.available() < sizeof(uint32_t))
    {
#ifdef DEC_DEBUG_WARNING
        RC_LOGW("Warning: Not enough data for spectrum decoding (V0)");
#endif
        return;
    }

    // Read in the counts until we run out of data or reach MAX_CHANNELS
    while ((br.available() > 0) && (br.available() >= sizeof(uint32_t)) && 
           (spectrum.count_size < Spectrum::MAX_CHANNELS))
    {
        uint32_t count;
        if (br.readUint32(&count))
        {
            // Double-check bounds before writing
            if (spectrum.count_size < Spectrum::MAX_CHANNELS)
            {
                spectrum.counts[spectrum.count_size++] = count;
            }
            else
            {
#ifdef DEC_DEBUG_WARNING
                RC_LOGW("Warning: Reached maximum spectrum channels");
#endif
                break;
            }
        }
        else
        {
#ifdef DEC_DEBUG_ERROR
            RC_LOGE("Error reading uint32 in decodeCountsV0");
#endif
            break;
        }
    }
}

// Helper function for spectrum decoding (version 1)
void decodeCountsV1(BytesBuffer& br, Spectrum& spectrum)
{
    // Clear the counts before starting
    spectrum.count_size = 0;
    uint32_t last = 0;

    // Safety check - make sure we have data available
    if (br.available() < sizeof(uint16_t))
    {
#ifdef DEC_DEBUG_WARNING
        RC_LOGW("Warning: Not enough data for spectrum decoding");
#endif
        return;
    }

    while ((br.available() >= sizeof(uint16_t)) && (spectrum.count_size < Spectrum::MAX_CHANNELS))
    {
        uint16_t u16;
        if (!br.readUint16(&u16))
        {
#ifdef DEC_DEBUG_ERROR
            RC_LOGE("Error reading u16 in decodeCountsV1");
#endif
            break;
        }

        uint16_t cnt = (u16 >> 4) & 0x0FFF;
        uint8_t vlen = u16 & 0x0F;

        // Add safety check for unreasonable count values
        if (cnt > 4096)
        {
#ifdef DEC_DEBUG_WARNING
            RC_LOGW("Warning: Suspicious count value in spectrum: %u", (unsigned)cnt);
#endif
            cnt = 0; // Skip this block
        }

        for (uint16_t i = 0; i < cnt && spectrum.count_size < Spectrum::MAX_CHANNELS; i++)
        {
            uint32_t v = 0;

            if (vlen == 0)
            {
                v = 0;
            }
            else if (vlen == 1)
            {
                uint8_t val;
                if (!br.readUint8(&val))
                {
#ifdef DEC_DEBUG_ERROR
                    RC_LOGE("Error reading uint8 in decodeCountsV1");
#endif
                    goto end_decoding;
                }
                v = val;
            }
            else if (vlen == 2)
            {
                int8_t val;
                if (!br.readInt8(&val))
                {
#ifdef DEC_DEBUG_ERROR
                    RC_LOGE("Error reading int8 in decodeCountsV1");
#endif
                    goto end_decoding;
                }
                v = last + val;
            }
            else if (vlen == 3)
            {
                int16_t val;
                if (!br.readInt16(&val))
                {
#ifdef DEC_DEBUG_ERROR
                    RC_LOGE("Error reading int16 in decodeCountsV1");
#endif
                    goto end_decoding;
                }
                v = last + val;
            }
            else if (vlen == 4)
            {
                if (br.available() < 3)
                {
#ifdef DEC_DEBUG_ERROR
                    RC_LOGE("Error: Not enough data for vlen=4");
#endif
                    goto end_decoding;
                }
                uint8_t a, b;
                int8_t c;
                if ((!br.readUint8(&a)) || (!br.readUint8(&b)) || (!br.readInt8(&c)))
                {
#ifdef DEC_DEBUG_ERROR
                    RC_LOGE("Error reading 3-byte value in decodeCountsV1");
#endif
                    goto end_decoding;
                }
                v = last + ((c << 16) | (b << 8) | a);
            }
            else if (vlen == 5)
            {
                int32_t val;
                if (!br.readInt32(&val))
                {
#ifdef DEC_DEBUG_ERROR
                    RC_LOGE("Error reading int32 in decodeCountsV1");
#endif
                    goto end_decoding;
                }
                v = last + val;
            }
            else
            {
#ifdef DEC_DEBUG_ERROR
                RC_LOGE("Error: Unsupported vlen in decodeCountsV1: %u", (unsigned)vlen);
#endif
                break;
            }

            last = v;

            // Extra safety check before adding to spectrum
            if (spectrum.count_size < Spectrum::MAX_CHANNELS)
            {
                spectrum.counts[spectrum.count_size++] = v;
            }
        }
    }

end_decoding:
    ; // Null statement to avoid syntax error
    // No additional cleanup needed
}

void decodeSpectrum(BytesBuffer& br, uint8_t format_version, Spectrum& spectrum)
{
    // Clear the spectrum to start with a clean state
    spectrum.clear();

    // Check if we have enough data for the spectrum header
    // We need 4 bytes for duration and 3x4 bytes for a0, a1, a2
    if (br.available() < (4 + 3*4))
    {
#ifdef DEC_DEBUG_ERROR
        RC_LOGE("Error: Not enough data for spectrum header");
#endif
        return;
    }

    // Read spectrum header with error checking
    uint32_t duration;
    if (!br.readUint32(&duration))
    {
#ifdef DEC_DEBUG_ERROR
        RC_LOGE("Error reading spectrum duration");
#endif
        return;
    }

    float a0, a1, a2;
    if ((!br.readFloat(&a0)) || (!br.readFloat(&a1)) || (!br.readFloat(&a2)))
    {
#ifdef DEC_DEBUG_ERROR
        RC_LOGE("Error reading spectrum calibration coefficients");
#endif
        return;
    }

    // Only assign values after we've successfully read them
    spectrum.duration_sec = duration;
    spectrum.a0 = a0;
    spectrum.a1 = a1;
    spectrum.a2 = a2;

    // Decode counts based on format version
    if (format_version == 0)
    {
        decodeCountsV0(br, spectrum);
    }
    else if (format_version == 1)
    {
        decodeCountsV1(br, spectrum);
    }
    else
    {
#ifdef DEC_DEBUG_WARNING
        RC_LOGW("Warning: Unsupported spectrum format version: %u", (unsigned)format_version);
#endif
    }

    // Add a safety check on the count_size
    if (spectrum.count_size > Spectrum::MAX_CHANNELS)
    {
#ifdef DEC_DEBUG_ERROR
        RC_LOGE("Error: Spectrum count_size exceeds MAX_CHANNELS!");
#endif
        spectrum.count_size = Spectrum::MAX_CHANNELS;
    }
}

// Payload sizes (bytes after the 7-byte header) for single-sample records,
// keyed by gid.  These come from the authoritative python decoder:
// gid 0 '<ffHHHB'(15), 1 '<ff'(8), 2 '<IffHH'(16), 3 '<IfHHH'(14), 4 '<IH'(6).
static const uint8_t gid0_single_payload[5] = {15, 8, 16, 14, 6};
// Per-sample payload size for multi-sample groups (eid==1), keyed by gid:
// 1 '<ff'(8), 2 '<IffHH'(16), 3 '<IfHHH'(14), 4 '<IH'(6).
static const uint8_t gid1_sample_payload[5] = {0, 8, 16, 14, 6};

// Alarm / off-scale events carry an extra rate + error field after the usual
// event header (ev, mask, flags).  Count-rate events carry a count rate, the
// others carry a dose rate.  Matches the python reference decoder.
static bool isRateEvent(uint8_t event)
{
    switch (event)
    {
    case 9:   // DOSE_RATE_ALARM1
    case 10:  // DOSE_RATE_ALARM2
    case 11:  // DOSE_RATE_OFFSCALE
    case 12:  // DOSE_ALARM1
    case 13:  // DOSE_ALARM2
    case 14:  // DOSE_OFFSCALE
    case 20:  // COUNT_RATE_ALARM1
    case 21:  // COUNT_RATE_ALARM2
    case 22:  // COUNT_RATE_OFFSCALE
        return true;
    default:
        return false;
    }
}

static bool isCountRateEvent(uint8_t event)
{
    return (event == 20) || (event == 21) || (event == 22);
}

// Need at least `bytes` more bytes than the record header; bail cleanly if not.
#define REC_NEED(bytes) \
    do { if (br.available() < (bytes)) { br.setPosition(rec_pos); return 0; } } while (0)

size_t decodeDataBufRecord(BytesBuffer& br, uint32_t base_time_sec, std::vector<DataItem*>& out)
{
    // We need at least the 7-byte record header before we can do anything.
    if (br.available() < 7)
    {
        return 0;
    }

    size_t rec_pos = br.getPosition();
    uint8_t seq, eid, gid;
    int32_t ts_offset;

    if (!br.readUint8(&seq) || !br.readUint8(&eid) || !br.readUint8(&gid) ||
        !br.readInt32(&ts_offset))
    {
        br.setPosition(rec_pos);
        return 0;
    }

    // The device's ts_offset grows over the session (it is relative to connect
    // time), so base_time_sec is the host "now" at connect, fixed for the whole
    // session. This is NOT refreshed per read: refreshing added the elapsed
    // session time to every record, drifting all timestamps forward.
    uint32_t timestamp = base_time_sec + (ts_offset * 10) / 1000;

    // Firmware quirk: the device timestamps live RealTimeData (gid 0) records
    // ~128s in the past relative to its current time, so gid 0 needs a +128s
    // correction (this is what makes `radiacode mon` track the host clock).
    // Every other record type is timestamped at its capture time and must NOT
    // be shifted - applying the blanket +128 overcorrected raw/db/dc/rare by
    // ~47-129s.
    if (eid == 0 && gid == 0)
    {
        timestamp += 128;
    }

    if (eid == 0)
    {
        switch (gid)
        {
        case 0: // GRP_RealTimeData '<ffHHHB' (15 bytes)
        {
            REC_NEED(15);
            RealTimeData* data = new RealTimeData();
            data->timestamp = timestamp;
            uint16_t cr_err, dr_err;
            if (!br.readFloat(&data->count_rate) || !br.readFloat(&data->dose_rate) ||
                !br.readUint16(&cr_err) || !br.readUint16(&dr_err) ||
                !br.readUint16(&data->flags) || !br.readUint8(&data->real_time_flags))
            {
                delete data;
                br.setPosition(rec_pos);
                return 0;
            }
            data->count_rate_err = cr_err / 10.0f;
            data->dose_rate_err = dr_err / 10.0f;
            out.push_back(data);
            return 7 + 15;
        }
        case 1: // GRP_RawData '<ff' (8 bytes)
        {
            REC_NEED(8);
            RawData* data = new RawData();
            data->timestamp = timestamp;
            if (!br.readFloat(&data->count_rate) || !br.readFloat(&data->dose_rate))
            {
                delete data;
                br.setPosition(rec_pos);
                return 0;
            }
            out.push_back(data);
            return 7 + 8;
        }
        case 2: // GRP_DoseRateDB '<IffHH' (16 bytes)
        {
            REC_NEED(16);
            DoseRateDB* data = new DoseRateDB();
            data->timestamp = timestamp;
            uint16_t dr_err;
            if (!br.readUint32(&data->count) || !br.readFloat(&data->count_rate) ||
                !br.readFloat(&data->dose_rate) || !br.readUint16(&dr_err) ||
                !br.readUint16(&data->flags))
            {
                delete data;
                br.setPosition(rec_pos);
                return 0;
            }
            data->dose_rate_err = dr_err / 10.0f;
            out.push_back(data);
            return 7 + 16;
        }
        case 3: // GRP_RareData '<IfHHH' (14 bytes)
        {
            REC_NEED(14);
            RareData* data = new RareData();
            data->timestamp = timestamp;
            uint16_t temperature_raw, charge_raw;
            if (!br.readUint32(&data->duration) || !br.readFloat(&data->dose) ||
                !br.readUint16(&temperature_raw) || !br.readUint16(&charge_raw) ||
                !br.readUint16(&data->flags))
            {
                delete data;
                br.setPosition(rec_pos);
                return 0;
            }
            data->temperature = (temperature_raw - 2000) / 100.0f;
            data->charge_level = charge_raw / 100.0f;
            out.push_back(data);
            return 7 + 14;
        }
        case 4: // GRP_DoseCounter '<IH' (6 bytes)
        {
            REC_NEED(6);
            DoseCounter* data = new DoseCounter();
            data->timestamp = timestamp;
            if (!br.readUint32(&data->dose_counter) || !br.readUint16(&data->flags))
            {
                delete data;
                br.setPosition(rec_pos);
                return 0;
            }
            out.push_back(data);
            return 7 + 6;
        }
        case 5: // GRP_ScheduleData '<IffHH' (16 bytes) - consume but discard
        case 6: // GRP_AccelData '<HHH' (6 bytes) - consume but discard
        {
            size_t payload = (gid == 5) ? 16 : 6;
            REC_NEED(payload);
            br.setPosition(br.getPosition() + payload);
            return 7 + payload;
        }
        case 7: // GRP_Event '<BBH[+fH]' (4 or 10 bytes)
        {
            // Need the base header first to know whether a rate payload follows.
            REC_NEED(4);
            uint8_t event_id, param1; uint16_t fl;
            if (!br.readUint8(&event_id) || !br.readUint8(&param1) || !br.readUint16(&fl))
            {
                br.setPosition(rec_pos);
                return 0;
            }
            Event* data = new Event();
            data->timestamp = timestamp;
            data->event = static_cast<EventId>(event_id);
            data->event_param1 = param1;
            data->flags = fl;
            if (isRateEvent(event_id))
            {
                REC_NEED(6);
                float rate; uint16_t rate_err;
                if (!br.readFloat(&rate) || !br.readUint16(&rate_err))
                {
                    delete data;
                    br.setPosition(rec_pos);
                    return 0;
                }
                if (isCountRateEvent(event_id))
                {
                    data->count_rate = rate;
                    data->count_rate_err = rate_err / 10.0f;
                    data->has_count_rate = true;
                }
                else
                {
                    data->dose_rate = rate;
                    data->dose_rate_err = rate_err / 10.0f;
                    data->has_dose_rate = true;
                }
                out.push_back(data);
                return 7 + 4 + 6;
            }
            out.push_back(data);
            return 7 + 4;
        }
        case 8: // GRP_RawCountRate '<fH' (6 bytes)
        case 9: // GRP_RawDoseRate '<fH' (6 bytes)
        {
            REC_NEED(6);
            RawData* data = new RawData();
            data->timestamp = timestamp;
            float rate; uint16_t fl;
            if (!br.readFloat(&rate) || !br.readUint16(&fl))
            {
                delete data;
                br.setPosition(rec_pos);
                return 0;
            }
            data->count_rate = (gid == 8) ? rate : 0.0f;
            data->dose_rate = (gid == 9) ? rate : 0.0f;
            out.push_back(data);
            return 7 + 6;
        }
        default:
            br.setPosition(rec_pos);
            return 0; // unknown eid==0 gid -> stop
        }
    }
    else if (eid == 1)
    {
        // Multi-sample groups: [samples u16][time u32] then samples*granularity
        // bytes.  Each sample is expanded into its own record, matching the
        // python reference decoder.
        size_t granularity = 0;
        if (gid == 1)      granularity = 8;   // GRP_RawData
        else if (gid == 2) granularity = 16;  // GRP_DoseRateDB
        else if (gid == 3) granularity = 14;  // GRP_RareData
        else if (gid == 4) granularity = 6;   // GRP_DoseCounter
        else
        {
            br.setPosition(rec_pos);
            return 0; // unknown eid==1 gid -> stop
        }

        // Validate the multi-block before decoding: a corrupt samples_num would
        // otherwise greedily consume the rest of the buffer as samples.
        if (br.available() < 6)
        {
            br.setPosition(rec_pos);
            return 0;
        }
        uint16_t samples_num;
        uint32_t smpl_time_ms;
        if (!br.readUint16(&samples_num) || !br.readUint32(&smpl_time_ms))
        {
            br.setPosition(rec_pos);
            return 0;
        }
        size_t body = (size_t)samples_num * granularity;
        if (br.available() < body)
        {
            br.setPosition(rec_pos);
            return 0; // body does not fit -> malformed record
        }

        for (uint16_t i = 0; i < samples_num; i++)
        {
            if (gid == 1) // GRP_RawData
            {
                RawData* data = new RawData();
                data->timestamp = timestamp;
                if (!br.readFloat(&data->count_rate) || !br.readFloat(&data->dose_rate))
                {
                    delete data;
                    br.setPosition(rec_pos);
                    return 0;
                }
                out.push_back(data);
            }
            else if (gid == 2) // GRP_DoseRateDB
            {
                DoseRateDB* data = new DoseRateDB();
                data->timestamp = timestamp;
                uint16_t dr_err;
                if (!br.readUint32(&data->count) || !br.readFloat(&data->count_rate) ||
                    !br.readFloat(&data->dose_rate) || !br.readUint16(&dr_err) ||
                    !br.readUint16(&data->flags))
                {
                    delete data;
                    br.setPosition(rec_pos);
                    return 0;
                }
                data->dose_rate_err = dr_err / 10.0f;
                out.push_back(data);
            }
            else if (gid == 3) // GRP_RareData
            {
                RareData* data = new RareData();
                data->timestamp = timestamp;
                uint16_t temperature_raw, charge_raw;
                if (!br.readUint32(&data->duration) || !br.readFloat(&data->dose) ||
                    !br.readUint16(&temperature_raw) || !br.readUint16(&charge_raw) ||
                    !br.readUint16(&data->flags))
                {
                    delete data;
                    br.setPosition(rec_pos);
                    return 0;
                }
                data->temperature = (temperature_raw - 2000) / 100.0f;
                data->charge_level = charge_raw / 100.0f;
                out.push_back(data);
            }
            else if (gid == 4) // GRP_DoseCounter
            {
                DoseCounter* data = new DoseCounter();
                data->timestamp = timestamp;
                if (!br.readUint32(&data->dose_counter) || !br.readUint16(&data->flags))
                {
                    delete data;
                    br.setPosition(rec_pos);
                    return 0;
                }
                out.push_back(data);
            }
        }
        return 7 + 6 + body;
    }
    else
    {
        // Unknown eid/gid combination.
        br.setPosition(rec_pos);
        return 0;
    }
}

std::vector<DataItem*> decodeDataBuf(BytesBuffer& br, uint32_t base_time_sec)
{
    std::vector<DataItem*> ret;

    while (true)
    {
        size_t consumed = decodeDataBufRecord(br, base_time_sec, ret);
        if (consumed == 0)
        {
            break; // need more data or hit an unknown record type
        }
    }

    return ret;
}
