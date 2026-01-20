#pragma once
#include <stddef.h>
#include <stdint.h>

/**
 * @file cobs.hpp
 * @brief Consistent Overhead Byte Stuffing (COBS) helpers.
 */

namespace umsg
{
    /**
     * @brief Incremental COBS encoder (allocation-free).
     *
     * Call begin(), then put() for each input byte, then end().
     */
    struct CobsEncoder
    {
        uint8_t *out;
        size_t cap;
        size_t codeIndex;
        size_t writeIndex;
        uint8_t code;

        /** @brief Initialize the encoder with output buffer. */
        bool begin(uint8_t *output, size_t outputCapacity)
        {
            out = output;
            cap = outputCapacity;
            codeIndex = 0;
            writeIndex = 1;
            code = 1;

            if (!out || cap == 0)
            {
                return false;
            }
            out[0] = 0;
            return true;
        }

        /** @brief Append one input byte. Returns false if output overflows. */
        bool put(uint8_t b)
        {
            if (b == 0)
            {
                out[codeIndex] = code;
                codeIndex = writeIndex;
                if (writeIndex >= cap)
                {
                    return false;
                }
                ++writeIndex;
                code = 1;
                return true;
            }

            if (writeIndex >= cap)
            {
                return false;
            }
            out[writeIndex++] = b;
            ++code;
            if (code == 0xFF)
            {
                out[codeIndex] = code;
                codeIndex = writeIndex;
                if (writeIndex >= cap)
                {
                    return false;
                }
                ++writeIndex;
                code = 1;
            }
            return true;
        }

        /** @brief Finalize the encoding and return total output length. */
        bool end(size_t &outputLength)
        {
            out[codeIndex] = code;
            outputLength = writeIndex;
            return true;
        }
    };

    /**
     * @brief COBS-encode the concatenation of (A || B) into @p output.
     * @return true on success; false if arguments are invalid or output overflows.
     */
    inline bool cobsEncode2(const uint8_t *inputA,
                            size_t inputALength,
                            const uint8_t *inputB,
                            size_t inputBLength,
                            uint8_t *output,
                            size_t outputCapacity,
                            size_t &outputLength)
    {
        if ((!inputA && inputALength) || (!inputB && inputBLength))
        {
            return false;
        }

        CobsEncoder enc;
        if (!enc.begin(output, outputCapacity))
        {
            return false;
        }

        for (size_t i = 0; i < inputALength; ++i)
        {
            if (!enc.put(inputA[i]))
            {
                return false;
            }
        }
        for (size_t i = 0; i < inputBLength; ++i)
        {
            if (!enc.put(inputB[i]))
            {
                return false;
            }
        }

        return enc.end(outputLength);
    }

    /**
     * @brief COBS-encode @p input into @p output.
     * @return true on success; false if arguments are invalid or output overflows.
     */
    inline bool cobsEncode(const uint8_t *input,
                           size_t inputLength,
                           uint8_t *output,
                           size_t outputCapacity,
                           size_t &outputLength)
    {
        return cobsEncode2(input, inputLength, 0, 0, output, outputCapacity, outputLength);
    }

    /**
     * @brief Decode a COBS-encoded buffer in place.
     * @param buffer Buffer containing the encoded bytes; overwritten with decoded bytes.
     * @param encodedLength Number of encoded bytes in @p buffer (delimiter not included).
     * @param decodedLength Output: number of decoded bytes written to @p buffer.
     * @return true on success; false if the encoding is invalid.
     */
    inline bool cobsDecodeInPlace(uint8_t *buffer, size_t encodedLength, size_t &decodedLength)
    {
        size_t readIndex = 0;
        size_t writeIndex = 0;

        while (readIndex < encodedLength)
        {
            const uint8_t code = buffer[readIndex++];
            if (code == 0)
            {
                return false;
            }

            for (uint8_t i = 1; i < code; ++i)
            {
                if (readIndex >= encodedLength)
                {
                    return false;
                }
                buffer[writeIndex++] = buffer[readIndex++];
            }

            if (code != 0xFF && readIndex < encodedLength)
            {
                buffer[writeIndex++] = 0x00;
            }
        }

        decodedLength = writeIndex;
        return true;
    }
}
