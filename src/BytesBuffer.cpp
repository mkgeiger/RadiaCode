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

#include "BytesBuffer.h"
#include "RadiaCodeLog.h"

#include <algorithm>
#include <cstring>

// Debugging switches
#define BUF_DEBUG_WARNING
#define BUF_DEBUG_ERROR

// Define the static shared buffer for BytesBuffer
uint8_t BytesBuffer::_shared_buffer[BytesBuffer::MAX_BUFFER_SIZE] = {0};

BytesBuffer::BytesBuffer(void)
{
    _size = 0;
    _position = 0;
    _capacity = MAX_BUFFER_SIZE; // Use our fixed maximum size
    // Use the shared static buffer
    _fixed_data = _shared_buffer;
    // Clear just the first few bytes for efficiency
    memset(_fixed_data, 0, 16);
}

BytesBuffer::BytesBuffer(const uint8_t* data, size_t length)
{
    _size = 0; // Initialize to 0 first
    _position = 0;
    _capacity = MAX_BUFFER_SIZE;

    // Use the shared static buffer
    _fixed_data = _shared_buffer;

    // First zero out just the beginning of the buffer for safety and efficiency
    memset(_fixed_data, 0, 16);

    // Now handle the data copy with careful checks
    if (data == nullptr)
    {
#ifdef BUF_DEBUG_WARNING
        RC_LOGW("Warning: Null data pointer in BytesBuffer constructor");
#endif
        return; // Leave _size as 0
    }

    if (length == 0)
    {
        return; // Nothing to copy, leave _size as 0
    }

    // Check for reasonable buffer size
    if (length > MAX_BUFFER_SIZE)
    {
#ifdef BUF_DEBUG_WARNING
        RC_LOGW("Warning: BytesBuffer truncating data in constructor");
#endif
        _size = MAX_BUFFER_SIZE;
    }
    else
    {
        _size = length;
    }

    // Only copy the valid amount
    memcpy(_fixed_data, data, _size);
}

// Copy constructor
BytesBuffer::BytesBuffer(const BytesBuffer& other)
{
    _size = other._size;
    _position = other._position;
    _capacity = MAX_BUFFER_SIZE; // Always use our fixed max size

    // Use the shared buffer
    _fixed_data = _shared_buffer;

    // Copy the data
    if (other._fixed_data && (_size > 0))
    {
        memcpy(_fixed_data, other._fixed_data, _size);
    }
}

// Assignment operator
BytesBuffer& BytesBuffer::operator=(const BytesBuffer& other)
{
    if (this != &other) // Self-assignment check
    {
        // Copy from other
        _size = other._size;
        _position = other._position;
        _capacity = MAX_BUFFER_SIZE; // Always use our fixed max size

        // Make sure we use the shared buffer
        _fixed_data = _shared_buffer;

        // Copy the data with safety checks
        if (other._fixed_data && (_size > 0))
        {
            memcpy(_fixed_data, other._fixed_data, _size);
        }
    }
    return *this;
}

BytesBuffer::~BytesBuffer(void)
{
    // Reset state
    _size = 0;
    _position = 0;
}

bool BytesBuffer::readUint8(uint8_t* value)
{
    // Strong bounds checking
    if ((_position >= _size) || (value == nullptr))
    {
#ifdef BUF_DEBUG_ERROR
        RC_LOGE("Error: Buffer overflow prevented in readUint8");
#endif
        if (value != nullptr)
        {
            *value = 0;
        }
        return false;
    }

    *value = _fixed_data[_position];
    _position += sizeof(uint8_t);
    return true;
}

bool BytesBuffer::readUint16(uint16_t* value)
{
    // Strong bounds checking
    if ((_position >= _size) || ((_position + sizeof(uint16_t)) > _size) || (value == nullptr))
    {
#ifdef BUF_DEBUG_ERROR
        RC_LOGE("Error: Buffer overflow prevented in readUint16");
#endif
        if (value != nullptr)
        {
            *value = 0;
        }
        return false;
    }

    // Little-endian read with extra safety
    uint16_t temp = 0;

    // Check each byte index separately for extra safety
    if (_position < _size)
    {
        temp |= (uint16_t)_fixed_data[_position];
    }
    if ((_position + 1) < _size)
    {
        temp |= (uint16_t)_fixed_data[_position + 1] << 8;
    }

    *value = temp;
    _position += sizeof(uint16_t);
    return true;
}

bool BytesBuffer::readUint32(uint32_t* value)
{
    // Stronger bounds checking
    if ((_position >= _size) || ((_position + sizeof(uint32_t)) > _size) || (value == nullptr))
    {
#ifdef BUF_DEBUG_ERROR
        RC_LOGE("Error: Buffer overflow prevented in readUint32");
#endif
        if (value != nullptr)
        {
            *value = 0; // Set a safe default
        }
        return false;
    }

    // Little-endian read with additional safety checks
    uint32_t temp = 0;

    // Check each byte index separately for extra safety
    if (_position < _size)
    {
        temp |= (uint32_t)_fixed_data[_position];
    }
    if ((_position + 1) < _size)
    {
        temp |= (uint32_t)_fixed_data[_position + 1] << 8;
    }
    if ((_position + 2) < _size)
    {
        temp |= (uint32_t)_fixed_data[_position + 2] << 16;
    }
    if ((_position + 3) < _size)
    {
        temp |= (uint32_t)_fixed_data[_position + 3] << 24;
    }

    *value = temp;
    _position += sizeof(uint32_t);
    return true;
}

bool BytesBuffer::readInt8(int8_t* value)
{
    // Strong bounds checking
    if ((_position >= _size) || (value == nullptr))
    {
#ifdef BUF_DEBUG_ERROR
        RC_LOGE("Error: Buffer overflow prevented in readInt8");
#endif
        if (value != nullptr)
        {
            *value = 0;
        }
        return false;
    }

    *value = static_cast<int8_t>(_fixed_data[_position]);
    _position += sizeof(int8_t);
    return true;
}

bool BytesBuffer::readInt16(int16_t* value)
{
    // Strong bounds checking
    if ((_position >= _size) || ((_position + sizeof(int16_t)) > _size) || (value == nullptr))
    {
#ifdef BUF_DEBUG_ERROR
        RC_LOGE("Error: Buffer overflow prevented in readInt16");
#endif
        if (value != nullptr)
        {
            *value = 0;
        }
        return false;
    }

    // Little-endian read with extra safety
    int16_t temp = 0;

    // Check each byte index separately for extra safety
    if (_position < _size)
    {
        temp |= (int16_t)_fixed_data[_position];
    }
    if ((_position + 1) < _size)
    {
        temp |= (int16_t)_fixed_data[_position + 1] << 8;
    }

    *value = temp;
    _position += sizeof(int16_t);
    return true;
}

bool BytesBuffer::readInt32(int32_t* value)
{
    // Strong bounds checking
    if ((_position >= _size) || ((_position + sizeof(int32_t)) > _size) || (value == nullptr))
    {
#ifdef BUF_DEBUG_ERROR
        RC_LOGE("Error: Buffer overflow prevented in readInt32");
#endif
        if (value != nullptr)
        {
            *value = 0;
        }
        return false;
    }

    // Little-endian read with extra safety
    int32_t temp = 0;
    
    // Check each byte index separately for extra safety
    if (_position < _size)
    {
        temp |= (int32_t)_fixed_data[_position];
    }
    if ((_position + 1) < _size)
    {
        temp |= (int32_t)_fixed_data[_position + 1] << 8;
    }
    if ((_position + 2) < _size)
    {
        temp |= (int32_t)_fixed_data[_position + 2] << 16;
    }
    if ((_position + 3) < _size)
    {
        temp |= (int32_t)_fixed_data[_position + 3] << 24;
    }

    *value = temp;
    _position += sizeof(int32_t);
    return true;
}

bool BytesBuffer::readFloat(float* value)
{
    // Stronger bounds checking
    if ((_position >= _size) || ((_position + sizeof(float)) > _size) || (value == nullptr))
    {
#ifdef BUF_DEBUG_ERROR
        RC_LOGE("Error: Buffer overflow prevented in readFloat");
#endif
        *value = 0.0f; // Set a safe default
        return false;
    }

    // Read as little-endian (RadiaCode protocol uses little-endian) with bounds checking
    uint32_t temp = 0;

    // Check each byte index separately for extra safety
    if (_position < _size)
    {
        temp |= (uint32_t)_fixed_data[_position];
    }
    if ((_position + 1) < _size)
    {
        temp |= (uint32_t)_fixed_data[_position + 1] << 8;
    }
    if ((_position + 2) < _size)
    {
        temp |= (uint32_t)_fixed_data[_position + 2] << 16;
    }
    if ((_position + 3) < _size)
    {
        temp |= (uint32_t)_fixed_data[_position + 3] << 24;
    }

    memcpy(value, &temp, sizeof(float));

    _position += sizeof(float);
    return true;
}

size_t BytesBuffer::readBytes(uint8_t* buffer, size_t length)
{
    size_t bytesToRead = std::min(length, _size - _position);

    if (bytesToRead > 0)
    {
        memcpy(buffer, _fixed_data + _position, bytesToRead);
        _position += bytesToRead;
    }

    return bytesToRead;
}

bool BytesBuffer::peekBytes(uint8_t* buffer, size_t offset, size_t length)
{
    // Strong bounds checking
    if (buffer == nullptr)
    {
#ifdef BUF_DEBUG_ERROR
        RC_LOGE("Error: Null buffer in peekBytes");
#endif
        return false;
    }

    if (offset >= _size)
    {
#ifdef BUF_DEBUG_ERROR
        RC_LOGE("Error: Offset out of bounds in peekBytes");
#endif
        return false;
    }
    
    // Check if we would read past the end
    if ((offset + length) > _size)
    {
#ifdef BUF_DEBUG_WARNING
        RC_LOGW("Warning: Truncating peekBytes read from %zu to %zu", length, _size - offset);
#endif
        // Only copy up to the available data
        length = _size - offset;
    }

    // Only perform the copy if we have data to copy
    if (length > 0)
    {
        memcpy(buffer, _fixed_data + offset, length);
    }

    return (length > 0);
}

std::string BytesBuffer::readString(void)
{
    uint8_t length;
    if (!readUint8(&length))
    {
        return std::string("");
    }

    if ((_position + length) > _size)
    {
        return std::string("");
    }

    std::string result;
    result.reserve(length);

    for (uint8_t i = 0; i < length; i++)
    {
        result += (char)_fixed_data[_position++];
    }

    return result;
}

bool BytesBuffer::writeUint8(uint8_t value)
{
    if (!ensureCapacity(sizeof(uint8_t)))
    {
        return false;
    }

    _fixed_data[_position] = value;
    _position += sizeof(uint8_t);

    if (_position > _size)
    {
        _size = _position;
    }

    return true;
}

bool BytesBuffer::writeUint16(uint16_t value)
{
    if (!ensureCapacity(sizeof(uint16_t)))
    {
        return false;
    }

    // Little-endian write
    _fixed_data[_position] = value & 0xFF;
    _fixed_data[_position + 1] = (value >> 8) & 0xFF;
    _position += sizeof(uint16_t);

    if (_position > _size)
    {
        _size = _position;
    }

    return true;
}

bool BytesBuffer::writeUint32(uint32_t value)
{
    if (!ensureCapacity(sizeof(uint32_t)))
    {
        return false;
    }

    // Little-endian write
    _fixed_data[_position] = value & 0xFF;
    _fixed_data[_position + 1] = (value >> 8) & 0xFF;
    _fixed_data[_position + 2] = (value >> 16) & 0xFF;
    _fixed_data[_position + 3] = (value >> 24) & 0xFF;
    _position += sizeof(uint32_t);

    if (_position > _size)
    {
        _size = _position;
    }

    return true;
}

bool BytesBuffer::writeInt8(int8_t value)
{
    return (writeUint8(static_cast<uint8_t>(value)));
}

bool BytesBuffer::writeInt16(int16_t value)
{
    return (writeUint16(static_cast<uint16_t>(value)));
}

bool BytesBuffer::writeInt32(int32_t value)
{
    return (writeUint32(static_cast<uint32_t>(value)));
}

bool BytesBuffer::writeFloat(float value)
{
    if (!ensureCapacity(sizeof(float)))
    {
        return false;
    }

    // We need to be careful with alignment, so copy the bytes through a properly aligned variable
    uint32_t temp;
    memcpy(&temp, &value, sizeof(float));
    memcpy(_fixed_data + _position, &temp, sizeof(float));

    _position += sizeof(float);

    if (_position > _size)
    {
        _size = _position;
   }

    return true;
}

size_t BytesBuffer::writeBytes(const uint8_t* buffer, size_t length)
{
    if (!ensureCapacity(length))
    {
        return 0; // Return 0 bytes written if capacity exceeded
    }

    memcpy(_fixed_data + _position, buffer, length);
    _position += length;

    if (_position > _size)
    {
        _size = _position;
    }

    return length;
}

bool BytesBuffer::writeString(const std::string& str)
{
    uint8_t length = static_cast<uint8_t>(str.length());

    if (!writeUint8(length))
    {
        return false;
    }

    for (uint8_t i = 0; i < length; i++)
    {
        if (!writeUint8(static_cast<uint8_t>(str[i])))
        {
            return false;
        }
    }

    return true;
}

size_t BytesBuffer::available(void) const
{
    return (_size - _position);
}

size_t BytesBuffer::getPosition(void) const
{
    return _position;
}

size_t BytesBuffer::getSize(void) const
{
    return _size;
}

void BytesBuffer::setPosition(size_t position)
{
    _position = std::min(position, _size);
}

void BytesBuffer::reset(void)
{
    _position = 0;
}

void BytesBuffer::setSize(size_t size)
{
    if (size > _capacity)
    {
        ensureCapacity(size - _capacity);
    }
    _size = size;
    if (_position > _size)
    {
        _position = _size;
    }
}

const uint8_t* BytesBuffer::getData(void) const
{
    return _fixed_data;
}

bool BytesBuffer::ensureCapacity(size_t additionalBytes)
{
    size_t required = _position + additionalBytes;

    if (required <= _capacity)
    {
        return true;
    }

#ifdef BUF_DEBUG_ERROR
    // With a fixed buffer, we can't grow beyond MAX_BUFFER_SIZE
    RC_LOGE("Error: BytesBuffer capacity exceeded. Required: %zu, Maximum: %zu", required, (size_t)MAX_BUFFER_SIZE);
#endif

    // Return false to indicate failure
    return false;
}
