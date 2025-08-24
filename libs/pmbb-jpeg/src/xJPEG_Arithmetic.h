/*
    SPDX-FileCopyrightText: 2020-2024 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/
#pragma once
#include "xCommonDefJPEG.h"
#include "xJFIF.h"
#include "xBitstream.h"
#include <cstdint>
#include <iostream>
#include <map>
#include <numeric>   // For std::accumulate
#include <stdexcept> // For std::runtime_error
#include <queue>
#include <vector>

#define X_PMBB_JPEG_MULTI_LEVEL_LOOKAHEAD 0

namespace PMBB_NAMESPACE::JPEG {

 //=====================================================================================================================================================================================

class xArithCommon
{
public:
  static bool xInitArithTables   (uint8* ArithLen, uint32* ArithCode, const xJFIF::xArithTable& ArithTable);
  static void xAvoidZeroLenCodes(uint8* ArithLen, int32 TableSize);

  static int32 xFillTmpLengths(uint8 * Lenghts, const xJFIF::xArithTable::tCodeL& TabCodeLengths);
  static int32 xFillTmpCodes  (uint32* Codes  , const uint8* Lenghts, int32 NumLengths);
};

//=====================================================================================================================================================================================

class xArithDecoder : public xArithCommon
{
public:
    xArithDecoder() = default;

    /**
     * @brief Starts the decoder and initializes its state from the bitstream.
     * @param bitstream The bitstream to read from.
     */
    void start(xBitstreamReader* bitstream);

    /**
     * @brief Decodes a single symbol using the provided probability model.
     * @param model The probability model to use for decoding.
     * @return The decoded symbol.
     */
    int32_t decode(const xArithmeticProbabilityModel& model);

    /**
     * @brief A convenience function to decode a DC coefficient.
     * It decodes the prefix code and then reads the suffix bits.
     * @param bitstream The bitstream to read from.
     * @param model The probability model for DC coefficients.
     * @return The decoded DC value.
     */
    int32_t readDC(xBitstreamReader* bitstream, const xArithmeticProbabilityModel& model);

private:
    /**
     * @brief Renormalizes the range [m_low, m_high] by shifting out common
     * most significant bits and reading new bits into m_value.
     */
    void renormalize();

    /**
     * @brief Reads the suffix bits for a decoded symbol and computes the final value.
     * @param bitstream The bitstream to read from.
     * @param numBits The number of suffix bits to read.
     * @return The final coefficient value.
     */
    int32_t readSufix(xBitstreamReader* bitstream, int32_t numBits);

    // State variables for the decoder
    uint64_t m_low;
    uint64_t m_high;
    uint64_t m_value; // Holds the current value from the bitstream
    xBitstreamReader* m_bitstream = nullptr;

    // Constants matching the encoder
    static constexpr uint64_t TOP_VALUE = 0xFFFFFFFFFFFFFFFFULL;
    static constexpr uint64_t FIRST_QUARTER = 0x4000000000000000ULL;
    static constexpr uint64_t THIRD_QUARTER = 0xC000000000000000ULL;
};

//=====================================================================================================================================================================================
/**
 * @class xArithmeticEncoder
 * @brief Performs arithmetic encoding of a sequence of symbols using 64-bit integer math.
 */
class xArithmeticEncoder
{
public:
    xArithmeticEncoder() = default;

    /**
     * @brief Initializes the encoder and associates it with a bitstream.
     */
    void start(xBitstreamWriter* bitstream)
    {
        m_bitstream = bitstream;
        m_low = 0;
        m_high = TOP_VALUE;
        m_pending_bits = 0;
    }

    /**
     * @brief Encodes a single symbol.
     * @param symbol The symbol to be encoded.
     * @param model The probability model to use for encoding.
     */
    void encode(int32_t symbol, const xArithmeticProbabilityModel& model)
    {
        // 1. Update the range based on the symbol's probability ("zoom in")
        const uint64_t range = m_high - m_low + 1;
        const SymbolModel& s = model.getSymbolModel(symbol);
        const uint64_t totalCount = model.getTotalCount();

        m_high = m_low + (range * s.upperCount) / totalCount - 1;
        m_low = m_low + (range * s.lowerCount) / totalCount;

        // 2. Renormalize the range and output any determined bits
        renormalize();
    }

    /**
     * @brief Finalizes the encoding process, writing any remaining bits.
     */
    void finish()
    {
        // Output enough bits to uniquely resolve the final interval.
        m_pending_bits++;
        if (m_low < FIRST_QUARTER) { writeBit(0); }
        else { writeBit(1); }

        m_bitstream->flush();
    }

private:
    inline void xArithDecoder::start(xBitstreamReader* bitstream)
    {
    }
    inline int32_t xArithDecoder::decode(const xArithmeticProbabilityModel& model)
    {
        return 0;
    }
    inline int32_t xArithDecoder::readDC(xBitstreamReader* bitstream, const xArithmeticProbabilityModel& model)
    {
        return 0;
    }
    void renormalize()
    {
        while (true)
        {
            // Case 1: MSBs of low and high are the same. Output the bit.
            if ((m_low >> 63) == (m_high >> 63))
            {
                writeBit(m_low >> 63);
                m_low <<= 1;
                m_high = (m_high << 1) | 1;
            }
            // Case 2: Underflow. The range is straddling the midpoint.
            else if ((m_low >= FIRST_QUARTER) && (m_high < THIRD_QUARTER))
            {
                m_pending_bits++;
                m_low = (m_low - FIRST_QUARTER) * 2;
                m_high = (m_high - FIRST_QUARTER) * 2 + 1;
            }
            // Case 3: Range is wide enough. Stop renormalizing for now.
            else
            {
                break;
            }
        }
    }

    inline int32_t xArithDecoder::readSufix(xBitstreamReader* bitstream, int32_t numBits)
    {
        return 0;
    }

    void writeBit(bool bit)
    {
        m_bitstream->writeBit(bit);
        // Write any pending bits now that the ambiguity is resolved
        for (; m_pending_bits > 0; m_pending_bits--)
        {
            m_bitstream->writeBit(!bit);
        }
    }

    // State variables
    uint64_t m_low;
    uint64_t m_high;
    uint64_t m_pending_bits;
    xBitstreamWriter* m_bitstream = nullptr;

    // Constants for 64-bit arithmetic as described
    static constexpr uint64_t TOP_VALUE = 0xFFFFFFFFFFFFFFFFULL;
    static constexpr uint64_t FIRST_QUARTER = 0x4000000000000000ULL;
    static constexpr uint64_t THIRD_QUARTER = 0xC000000000000000ULL;
};

//=====================================================================================================================================================================================

class xArithEncoderDC : public xArithCommon
{
    xArithmeticProbabilityModel m_model;
    xArithmeticEncoder          m_encoder;
    xBitstreamWriter* m_bitstream = nullptr;

public:
    // Initializes the encoder with a probability model built from symbol counts.
    bool init(const xArithCounterDC& counter)
    {
        m_model.build(counter.getSymbolCount(), xJPEG_Constants::c_MaxNumCodeSymbolsDC);
        return true;
    }

    // Starts the encoding process and links to a bitstream.
    void start(xBitstreamWriter* bitstream)
    {
        m_bitstream = bitstream;
        m_encoder.start(bitstream);
    }

    // Encodes a DC symbol and writes the suffix bits.
    void writeDC(int32_t numBits, uint32_t remainder)
    {
        m_encoder.encode(numBits, m_model);
        if (numBits > 0) { m_bitstream->writeBits(remainder, numBits); }
    }

    // Finalizes the bitstream.
    void finish()
    {
        m_encoder.finish();
    }
};

class xArithEncoderAC : public xArithCommon
{
protected:
    xArithmeticProbabilityModel m_model;
    xArithmeticEncoder          m_encoder;
    xBitstreamWriter* m_bitstream = nullptr;

public:
    // Initializes the encoder with a probability model built from symbol counts.
    bool init(const xArithCounterAC& counter)
    {
        m_model.build(counter.getSymbolCount(), xJPEG_Constants::c_MaxNumCodeSymbolsAC);
        return true;
    }

    // Starts the encoding process and links to a bitstream.
    void start(xBitstreamWriter* bitstream)
    {
        m_bitstream = bitstream;
        m_encoder.start(bitstream);
    }

    // Encodes an AC symbol and writes the suffix bits.
    void writeAC(int32_t code, int32_t numBits, uint32_t remainder)
    {
        m_encoder.encode(code, m_model);
        if (numBits > 0) { m_bitstream->writeBits(remainder, numBits); }
    }

    // Encodes the Zero Run Length (ZRL) special symbol.
    void writeZRL() { m_encoder.encode(0xF0, m_model); }

    // Encodes the End of Block (EOB) special symbol.
    void writeEOB() { m_encoder.encode(0x00, m_model); }

    // Finalizes the bitstream.
    void finish() { m_encoder.finish(); }
};

//=====================================================================================================================================================================================

class xArithEstimatorDC: public xArithCommon
{
protected:
  uint8 m_ArithLen [xJPEG_Constants::c_MaxNumCodeSymbolsDC];

public:
  bool  init  (const xJFIF::xArithTable& ArithTable);
  int32 calcDC(int32 NumBits) const { return m_ArithLen[NumBits] + NumBits; }
};

class xArithEstimatorAC : public xArithCommon
{
protected:
  uint8  m_ArithLen [xJPEG_Constants::c_MaxNumCodeSymbolsAC];

public:
  bool  init   (const xJFIF::xArithTable& ArithTable);
  int32 calcAC (int32 Code, int32 NumBits) const { return m_ArithLen[Code] + NumBits; }
  int32 calcZRL() const { return m_ArithLen[0xF0]; }
  int32 calcEOB() const { return m_ArithLen[0x00]; }
};

//=====================================================================================================================================================================================

class xArithCounterDC
{
protected:
  static constexpr int32 c_NCS = xJPEG_Constants::c_MaxNumCodeSymbolsDC;
  uint32 m_SymbolCount[c_NCS];

public:
  bool init   (          ) { memset(m_SymbolCount, 0, c_NCS * sizeof(uint32)); return true; }
  void countDC(int32 Code) { m_SymbolCount[Code]++; }
  void acc    (const xArithCounterDC* Other) { for(int32 i = 0; i < c_NCS; i++) { m_SymbolCount[i] += Other->m_SymbolCount[i]; } }
  const uint32* getSymbolCount() const { return m_SymbolCount; }
};

class xArithCounterAC
{
protected:
  static constexpr int32 c_NCS = xJPEG_Constants::c_MaxNumCodeSymbolsAC;
  uint32 m_SymbolCount[c_NCS];

public:
  bool init    (          ) { memset(m_SymbolCount, 0, c_NCS * sizeof(uint32)); return true; }
  void countAC (int32 Code) { m_SymbolCount[Code]++; }
  void countZRL(          ) { m_SymbolCount[0xF0]++; }
  void countEOB(          ) { m_SymbolCount[0x00]++; }
  void acc     (const xArithCounterAC* Other) { for(int32 i = 0; i < c_NCS; i++) { m_SymbolCount[i] += Other->m_SymbolCount[i]; } }
  const uint32* getSymbolCount() const { return m_SymbolCount; }
};

//=====================================================================================================================================================================================
// A structure that stores the cumulative counts for a single symbol.
struct SymbolModel
{
    uint64_t lowerCount; // Beginning of the range (cumulative count)
    uint64_t upperCount; // End of the range (lowerCount + symbol count)
};
/**
 * @class xArithmeticProbabilityModel
 * @brief Builds and stores a statistical model for arithmetic coding.
 *
 * This class computes cumulative frequencies for each symbol, which are
 * required by the arithmetic encoder to partition the coding range.
 */
class xArithmeticProbabilityModel
{
    xArithmeticProbabilityModel() = default;

    /**
     * @brief Builds the probability model from an array of symbol counts.
     * @param SymbolCount Pointer to an array with symbol frequencies.
     * @param Size The number of symbols in the alphabet.
     */
public:
    void build(const uint32_t* SymbolCount, int32_t Size)
    {
        if (Size <= 0) {
            m_totalCount = 0;
            m_model.clear();
            return;
        }

        // 1. Sum all symbol counts to get the total. Use uint64_t to prevent overflow.
        m_totalCount = std::accumulate(SymbolCount, SymbolCount + Size, 0ULL);

        if (m_totalCount == 0)
        {
            throw std::runtime_error("Cannot build a probability model with zero total frequency.");
        }

        // 2. Calculate the cumulative counts for each symbol.
        m_model.resize(Size);
        uint64_t cumulativeCount = 0;
        for (int32_t i = 0; i < Size; ++i)
        {
            m_model[i].lowerCount = cumulativeCount;
            cumulativeCount += SymbolCount[i];
            m_model[i].upperCount = cumulativeCount;
        }
    }

    const SymbolModel& getSymbolModel(int32_t symbol) const
    {
        if (symbol < 0 || static_cast<size_t>(symbol) >= m_model.size())
        {
            throw std::out_of_range("Symbol index is out of range.");
        }
        return m_model[symbol];
    }

    uint64_t getTotalCount() const { return m_totalCount; }

    int64_t findSymbol(uint64_t scaled_value) const
    {
        // Perform a binary search or linear scan to find the symbol
        // For simplicity, a linear scan is shown here. A binary search would be faster.
        for (size_t i = 0; i < m_model.size(); ++i)
        {
            if (scaled_value < m_model[i].upperCount)
            {
                return static_cast<int64_t>(i);
            }
        }
        // Should not be reached if scaled_value is valid
        throw std::runtime_error("Could not find symbol for the given scaled value.");
    }

private:
    uint64_t m_totalCount = 0;
    std::vector<SymbolModel> m_model;
};
//=====================================================================================================================================================================================

} //end of namespace PMBB::JPEG