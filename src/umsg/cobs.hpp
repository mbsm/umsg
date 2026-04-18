#pragma once
#include <stddef.h>
#include <stdint.h>

/**
 * @file cobs.hpp
 * @brief Consistent Overhead Byte Stuffing (COBS) helpers.
 * @ingroup umsg
 *
 * This header provides allocation-free COBS helpers used by the wire protocol.
 * The wire delimiter (packet terminator) is handled by `Framer`, not by these helpers.
 */

namespace umsg
{
    namespace detail
    {
        /**
         * @brief Streaming COBS encoder state. Implementation detail of the public
         *        `cobsEncode` / `cobsEncode2` helpers below.
         */
        struct CobsEncState
        {
            uint8_t *out;
            size_t cap;
            size_t codeIndex;
            size_t writeIndex;
            uint8_t code;

            bool begin(uint8_t *output, size_t outputCapacity)
            {
                if (!output || outputCapacity == 0) return false;
                out = output;
                cap = outputCapacity;
                codeIndex = 0;
                writeIndex = 1;
                code = 1;
                out[0] = 0;
                return true;
            }

            bool put(uint8_t b)
            {
                if (b == 0)
                {
                    out[codeIndex] = code;
                    codeIndex = writeIndex;
                    if (writeIndex >= cap) return false;
                    ++writeIndex;
                    code = 1;
                    return true;
                }
                if (writeIndex >= cap) return false;
                out[writeIndex++] = b;
                if (++code == 0xFF)
                {
                    out[codeIndex] = code;
                    codeIndex = writeIndex;
                    if (writeIndex >= cap) return false;
                    ++writeIndex;
                    code = 1;
                }
                return true;
            }

            void finish(size_t &outputLength)
            {
                out[codeIndex] = code;
                outputLength = writeIndex;
            }
        };
    }

    /**
     * @brief COBS-encode the concatenation of (A || B) into @p output.
     *
     * Used by the Framer to encode `(frame || crc32)` without a temporary contiguous buffer.
     * The produced encoding does not include a trailing `0x00` delimiter.
     *
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
        if ((!inputA && inputALength) || (!inputB && inputBLength)) return false;

        detail::CobsEncState enc;
        if (!enc.begin(output, outputCapacity)) return false;

        for (size_t i = 0; i < inputALength; ++i)
        {
            if (!enc.put(inputA[i])) return false;
        }
        for (size_t i = 0; i < inputBLength; ++i)
        {
            if (!enc.put(inputB[i])) return false;
        }

        enc.finish(outputLength);
        return true;
    }

    /**
     * @brief COBS-encode @p input into @p output.
     *
     * The produced encoding does not include a trailing `0x00` delimiter.
     *
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
        *
        * @warning The decode is performed in-place; @p buffer must be writable.
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
