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

#ifndef Decoders_h
#define Decoders_h

#include "RadiaCodeTypes.h"
#include "BytesBuffer.h"
#include <vector>

// RealTimeData (gid 0) body size, bytes after the 7-byte header.  Firmware
// <= 4.14 uses 15 bytes '<ffHHHB'; firmware 4.15 appends one further byte
// (observed always 0x00, meaning undetermined) after rt_flags, making the body
// 16 bytes.  The first 15 bytes keep the same layout in both versions.
#define RADIACODE_RT_LEGACY_SIZE 15
#define RADIACODE_RT_NEW_SIZE    16

// Pick the RealTimeData body size (15 or 16) that best explains `data`.  A
// buffer containing RealTimeData records parses cleanly end-to-end under
// exactly one of the two sizes; the other misaligns on the first such record.
// When no RealTimeData record is present both sizes parse identically and the
// legacy size is returned (the choice is then irrelevant for this buffer).
// For a corrupt or truncated buffer (neither size reaches the end) the size
// that decoded more records wins.  When `conclusive` is non-null it is set to
// true only when the buffer actually distinguishes the two sizes, so a
// streaming caller can defer locking in its choice until then.
int detectRealtimeSize(const uint8_t* data, size_t n, bool* conclusive);

// Decode a single DATA_BUF record from the front of the buffer.
//
// The decoded records are appended to `out` (the caller owns and must delete
// the heap-allocated DataItems).  Multi-sample groups (eid==1) are expanded so
// each sample becomes its own record, matching the python reference decoder.
// Samples are timestamped dt + i*smpl_time*10 ms (oldest to newest) rather
// than all sharing the group timestamp.
//
// `realtime_size` is the RealTimeData (gid 0) body size: 15 for firmware
// <= 4.14, 16 for firmware 4.15.
//
// Returns the number of bytes consumed (0 if the buffer does not yet hold a
// complete record, or the record type is not recognized).  When 0 is returned
// the buffer position is left unchanged so the caller can wait for more bytes
// or stop.
size_t decodeDataBufRecord(BytesBuffer& br, uint32_t base_time_sec, std::vector<DataItem*>& out,
                           int realtime_size = RADIACODE_RT_LEGACY_SIZE);

// Decode a whole DATA_BUF buffer into a flat list of DataItems.
//
// Malformed / truncated records are skipped and decoding resumes at the next
// plausible record boundary, so a single glitch does not discard the rest of
// the buffer.  Returns only when the buffer needs more bytes or no further
// plausible record is found.  `realtime_size` selects the RealTimeData body
// size; 0 (the default) auto-detects it from the buffer so one decoder handles
// both firmware versions.
std::vector<DataItem*> decodeDataBuf(BytesBuffer& br, uint32_t base_time_sec, int realtime_size = 0);

// After a malformed record, scan forward for the next plausible record header
// and reposition the buffer there.  Returns true if a re-sync point was found,
// leaving the buffer positioned at that record's header.  A candidate is only
// accepted if the record parses *and* is followed by another plausible record,
// which rejects most false positives in garbage.
bool resyncDataBuf(BytesBuffer& br, int realtime_size = RADIACODE_RT_LEGACY_SIZE);

// Decode spectrum data directly into an existing Spectrum object
void decodeSpectrum(BytesBuffer& br, uint8_t format_version, Spectrum& spectrum);

#endif
