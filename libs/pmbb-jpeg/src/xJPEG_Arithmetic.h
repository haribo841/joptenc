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
protected:
#if X_PMBB_JPEG_MULTI_LEVEL_LOOKAHEAD
  static const int32 c_LookAhead1st =  8; //fixed size
  static const int32 c_LookAhead2nd = 16; //fixed size
#else
  static const int32 c_LookAhead    = 10; //can be up to 16
#endif

  uint8  m_CodeSymbols[256];
  int32  m_MaxCode    [18 ]; //largest code of length k (-1 if none)
  int32  m_ValOffset  [18 ]; //Arithval[] offset for codes of length k
#if X_PMBB_JPEG_MULTI_LEVEL_LOOKAHEAD
  uint16 m_Lookup1st  [1<<c_LookAhead1st];
  uint32 m_Lookup2nd  [1<<c_LookAhead2nd];
#else
  uint32  m_Lookup    [1<<c_LookAhead   ];
#endif

public:
  bool init(const xJFIF::xArithTable& ArithTable)
  {     
    //return xCreateDerrivedDecoder(m_CodeSymbols, m_MaxCode, m_ValOffset, m_Lookup, ArithTable);
    return xInitTables(ArithTable);
  }
  int32 readPrefix(xBitstreamReader* Bitstream);
  int32 readSufix(xBitstreamReader* Bitstream, int32 NumBits)
  {
    int32 R = Bitstream->readBits(NumBits);
    int32 Value = R + (((R - (1 << (NumBits - 1))) >> 31) & ((((uint32)-1) << NumBits) + 1));
    return Value;
  }
  int32 readDC(xBitstreamReader* Bitstream)
  {
    int32 DC = readPrefix(Bitstream);
    if(DC) { DC = readSufix(Bitstream, DC); }
    return DC;
  }

protected:
  bool xInitTables(const xJFIF::xArithTable& ArithTable);

};

//=====================================================================================================================================================================================

class xArithEncoderDC : public xArithCommon
{
protected:
  uint32 m_ArithCode[xJPEG_Constants::c_MaxNumCodeSymbolsDC];
  uint8  m_ArithLen [xJPEG_Constants::c_MaxNumCodeSymbolsDC];

public:
  bool init    (const xJFIF::xArithTable& ArithTable) { if(ArithTable.getClass() != xJFIF::xArithTable::eArithClass::DC) { return false; } return xInitArithTables(m_ArithLen, m_ArithCode, ArithTable); }
  void writeDC (xBitstreamWriter* Bitstream, int32 NumBits, uint32 Remainder) { Bitstream->writeBits(m_ArithCode[NumBits], m_ArithLen[NumBits]); if(NumBits) { Bitstream->writeBits(Remainder, NumBits); } }
};

class xArithEncoderAC : public xArithCommon
{
protected:
  uint32 m_ArithCode[xJPEG_Constants::c_MaxNumCodeSymbolsAC];
  uint8  m_ArithLen [xJPEG_Constants::c_MaxNumCodeSymbolsAC];

public:
  bool init    (xJFIF::xArithTable& ArithTable) { if(ArithTable.getClass() != xJFIF::xArithTable::eArithClass::AC) { return false; } return xInitArithTables(m_ArithLen, m_ArithCode, ArithTable); }
  void writeAC (xBitstreamWriter* Bitstream, int32 Code, int32 NumBits, uint32 Remainder) { Bitstream->writeBits(m_ArithCode[Code], m_ArithLen[Code]); Bitstream->writeBits(Remainder, NumBits); }
  void writeZRL(xBitstreamWriter* Bitstream) { Bitstream->writeBits(m_ArithCode[0xF0], m_ArithLen[0xF0]); }
  void writeEOB(xBitstreamWriter* Bitstream) { Bitstream->writeBits(m_ArithCode[0x00], m_ArithLen[0x00]); }
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
// A structure that stores the probability model for a single symbol.
// // Defines its subinterval in the range [0, 1.0).
struct SymbolProbability
{
    uint64_t lowerRange; // Beginning of the range (cumulative probability for this symbol)
    uint64_t upperRange; // End of the range (lowerRange + probability for this symbol)
};
/**
 * @class xArithmeticModelBuilder
 * @brief Builds and stores a statistical model for arithmetic coding.
 *
 * This class replaces xArithmeticTabBuilder. Instead of generating variable-length bitcodes
 * (as in Huffman coding), it computes
 * cumulative probabilities for each symbol.
 * Each symbol is assigned a unique subinterval in the range [0.0, 1.0) transformed to int,
 * whose width corresponds to its probability of occurrence.
 */
class xArithmeticModelBuilder
{
public:
    ProbabilityModel(const uint32_t* SymbolCount, int32_t Size)
    {
        if (Size <= 0) {
            return; // Empty model
        }

    // 1. Count all symbols to get the grand total. 
    // We use uint64_t to avoid overflow with large files.
    m_totalNumberOfSymbols = std::accumulate(SymbolCount, SymbolCount + Size, 0ULL);

    if (m_totalNumberOfSymbols == 0)
    {
        // If there are no symbols, the model cannot be built.
        // You can throw an exception or leave the model empty.
        throw std::runtime_error("Cannot build a probability model with zero symbols.");
    }

    // 2. Calculate the probabilities and create a table of cumulative probabilities.
    m_model.resize(Size);
    uint64_t cumulativeProbability = 0;

        for (int32_t i = 0; i < Size; ++i)
        {
            // If a symbol is not present, its range has zero width.
            if (SymbolCount[i] > 0)
            {
                //P(symbol) = number_of_occurrences(symbol) / total_number_of_symbols
			    uint64_t probability = static_cast<uint64_t>(SymbolCount[i]) / m_totalNumberOfSymbols; //ugh int division - need to scale

                m_model[i].lowerRange = cumulativeProbability;
                cumulativeProbability += probability;
                m_model[i].upperRange = cumulativeProbability;
            }
            else
            {
                // The symbol does not occur, so we assign it an empty range.
                m_model[i].lowerRange = cumulativeProbability;
                m_model[i].upperRange = cumulativeProbability;
            }
        }
    }

    /**
     * @brief Returns the probability model for a given symbol.
     * @param symbol Symbol index.
     * @return Constant reference to a structure with a probability range.
     */
        const SymbolProbability& getSymbolModel(int32_t symbol) const
        {
            if (symbol >= m_model.size())
            {
                throw std::out_of_range("The symbol is outside the scope of the model.");
            }
            return m_model[symbol];
        }

        /**
         * @brief Returns the total number of symbols counted.
         */
    uint64_t getTotalCount() const
    {
        return m_totalNumberOfSymbols;
    }

private:
    uint64_t m_totalNumberOfSymbols = 0;
    std::vector<SymbolProbability> m_model;
};
//=====================================================================================================================================================================================

} //end of namespace PMBB::JPEG