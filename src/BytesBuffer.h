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

#ifndef BytesBuffer_h
#define BytesBuffer_h

#include <cstddef>
#include <cstdint>
#include <string>

class BytesBuffer
{
    public:
        // Define a buffer size large enough for a full raw spectrum read
        static const size_t MAX_BUFFER_SIZE = 8192;
        
        BytesBuffer(void);
        BytesBuffer(const uint8_t* data, size_t length);
        BytesBuffer(const BytesBuffer& other);  // Copy constructor
        BytesBuffer& operator=(const BytesBuffer& other);  // Assignment operator
        ~BytesBuffer(void);

        // Reading methods
        bool readUint8(uint8_t* value);
        bool readUint16(uint16_t* value);
        bool readUint32(uint32_t* value);
        bool readInt8(int8_t* value);
        bool readInt16(int16_t* value);
        bool readInt32(int32_t* value);
        bool readFloat(float* value);
        size_t readBytes(uint8_t* buffer, size_t length);
        bool peekBytes(uint8_t* buffer, size_t offset, size_t length);
        std::string readString(void);

        // Writing methods
        bool writeUint8(uint8_t value);
        bool writeUint16(uint16_t value);
        bool writeUint32(uint32_t value);
        bool writeInt8(int8_t value);
        bool writeInt16(int16_t value);
        bool writeInt32(int32_t value);
        bool writeFloat(float value);
        size_t writeBytes(const uint8_t* buffer, size_t length);
        bool writeString(const std::string& str);

        // Utility methods
        size_t available(void) const;
        size_t getPosition(void) const;
        size_t getSize(void) const;
        void setPosition(size_t position);
        void reset(void);
        void setSize(size_t size);
        const uint8_t* getData(void) const;

    private:
        // Use static shared buffer to avoid stack overflow
        static uint8_t _shared_buffer[MAX_BUFFER_SIZE];
        // Pointer to the buffer we're using (shared or allocated)
        uint8_t* _fixed_data;
        size_t _size;
        size_t _capacity;
        size_t _position;

        bool ensureCapacity(size_t additionalBytes); // Returns false if capacity exceeded
};

#endif
