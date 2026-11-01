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

// Decode a single DATA_BUF record from the front of the buffer.
//
// The decoded records are appended to `out` (the caller owns and must delete
// the heap-allocated DataItems).  Multi-sample groups (eid==1) are expanded so
// each sample becomes its own record, matching the python reference decoder.
//
// Returns the number of bytes consumed (0 if the buffer does not yet hold a
// complete record, or the record type is not recognized).  When 0 is returned
// the buffer position is left unchanged so the caller can wait for more bytes
// or stop.
size_t decodeDataBufRecord(BytesBuffer& br, uint32_t base_time_sec, std::vector<DataItem*>& out);

// Decode a whole DATA_BUF buffer into a flat list of DataItems.
//
// Malformed / truncated records are skipped and decoding resumes at the next
// plausible record boundary, so a single glitch does not discard the rest of
// the buffer.  Returns only when the buffer needs more bytes or no further
// plausible record is found.
std::vector<DataItem*> decodeDataBuf(BytesBuffer& br, uint32_t base_time_sec);

// After a malformed record, scan forward for the next plausible record header
// and reposition the buffer there.  Returns true if a re-sync point was found,
// leaving the buffer positioned at that record's header.  A candidate is only
// accepted if the record parses *and* is followed by another plausible record,
// which rejects most false positives in garbage.
bool resyncDataBuf(BytesBuffer& br);

// Decode spectrum data directly into an existing Spectrum object
void decodeSpectrum(BytesBuffer& br, uint8_t format_version, Spectrum& spectrum);

#endif
