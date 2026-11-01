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

#ifndef RadiaCodeTransport_h
#define RadiaCodeTransport_h

#include "BytesBuffer.h"
#include "RadiaCodeTypes.h"
#include <vector>

class RadiaCodeTransport
{
    public:
        virtual ~RadiaCodeTransport(void) {}
        virtual BytesBuffer execute(const uint8_t* request, size_t length) = 0;

        // Whether the transport's underlying connection is alive. An empty
        // DATA_BUF read is normal ("caught up"), so connection loss must be
        // detected from the transport, not from the absence of records.
        virtual bool isConnected(void) const { return false; }

        // Streaming DATA_BUF support. Base implementations are no-ops; only
        // transports that can decode records incrementally override these.
        virtual void setDataBufStream(uint32_t base_time_sec)
        {
            (void)base_time_sec;
        }
        virtual std::vector<DataItem*> drainDataBufRecords(void)
        {
            return std::vector<DataItem*>();
        }
};

#endif
