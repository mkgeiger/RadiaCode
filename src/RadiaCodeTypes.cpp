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

#include "RadiaCodeTypes.h"
#include "RadiaCodeLog.h"

#include <cstring>

// Debugging switches
#define RCTYPES_DEBUG_WARNING

// Define the static shared buffer for the Spectrum class
// This moves the large array out of the stack and into static memory
uint32_t Spectrum::shared_counts[Spectrum::MAX_CHANNELS] = {0};

// Constructor
Spectrum::Spectrum(void) : duration_sec(0), a0(0), a1(0), a2(0), count_size(0)
{
    // Use the shared static buffer
    counts = shared_counts;
    // Zero out only the part of the array we'll use
    memset(counts, 0, sizeof(uint32_t) * MAX_CHANNELS);
}

// Destructor - nothing to do with fixed arrays
Spectrum::~Spectrum(void)
{
    // No cleanup needed for fixed arrays
}

// Copy constructor
Spectrum::Spectrum(const Spectrum& other) : 
    duration_sec(other.duration_sec), 
    a0(other.a0),
    a1(other.a1),
    a2(other.a2),
    count_size(0)
{
    // Initialize to 0 first for safety

    // Use the shared buffer
    counts = shared_counts;

    // Make sure the source count_size is valid
    size_t safe_count = other.count_size;
    if (safe_count > MAX_CHANNELS)
    {
#ifdef RCTYPES_DEBUG_WARNING
        RC_LOGW("Warning: Copy constructor truncating spectrum data");
#endif
        safe_count = MAX_CHANNELS;
    }

    // Set the count_size after validation
    count_size = safe_count;

    // Safe memcpy with validated size
    if ((count_size > 0) && (other.counts != nullptr))
    {
        memcpy(counts, other.counts, count_size * sizeof(uint32_t));
    }
}

// Assignment operator
Spectrum& Spectrum::operator=(const Spectrum& other)
{
    if (this != &other)
    {
        duration_sec = other.duration_sec;
        a0 = other.a0;
        a1 = other.a1;
        a2 = other.a2;

        // Ensure we're using the shared buffer
        counts = shared_counts;

        // Make sure the source count_size is valid
        size_t safe_count = other.count_size;
        if (safe_count > MAX_CHANNELS)
        {
#ifdef RCTYPES_DEBUG_WARNING
            RC_LOGW("Warning: Assignment operator truncating spectrum data");
#endif
            safe_count = MAX_CHANNELS;
        }

        // Set the count_size after validation
        count_size = safe_count;

        // Safe memcpy with validated size
        if ((count_size > 0) && (other.counts != nullptr))
        {
            memcpy(counts, other.counts, count_size * sizeof(uint32_t));
        }
    }
    return *this;
}

// Clear method - just reset the size
void Spectrum::clear(void)
{
    duration_sec = 0;
    a0 = 0;
    a1 = 0;
    a2 = 0;
    count_size = 0;

    // Ensure we're using the shared buffer
    counts = shared_counts;

    // We'll zero out just the first few elements for safety
    // Full zeroing is not needed since we track the count_size
    memset(counts, 0, sizeof(uint32_t) * 16);  // Zero first 16 elements
}

// Vector-like interface methods for compatibility
size_t Spectrum::size(void) const
{
    return count_size;
}

bool Spectrum::empty(void) const
{
    return count_size == 0;
}

void Spectrum::push_back(uint32_t value)
{
    if (count_size < MAX_CHANNELS)
    {
        counts[count_size++] = value;
    }
    else
    {
        // Only print the warning once to avoid flooding the log
        static bool warning_printed = false;
        if (!warning_printed)
        {
#ifdef RCTYPES_DEBUG_WARNING
            RC_LOGW("Warning: Spectrum array full, ignoring additional data");
#endif
            warning_printed = true;
        }
    }
}

uint32_t Spectrum::at(size_t index) const
{
    if (index < count_size)
    {
        return counts[index];
    }
    return 0; // Safety fallback
}
