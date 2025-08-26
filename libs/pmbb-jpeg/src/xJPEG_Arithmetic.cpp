/*
    SPDX-FileCopyrightText: 2020-2024 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/
#include "xJPEG_Arithmetic.h"

namespace PMBB_NAMESPACE::JPEG {

//=============================================================================================================================================================================
// xArithCommon
//=====================================================================================================================================================================================
bool xArithCommon::xInitArithTables(uint8* ArithLen, uint32* ArithCode, const xJFIF::xArithTable& ArithTable)
{
  const xJFIF::xArithTable::tCodeL& TabCodeLengths = ArithTable.getCodeLengths();
  const xJFIF::tByteV&             TabCodeSymbols = ArithTable.getCodeSymbols();

  uint32 TmpCode[257];
  uint8  TmpLen [257];

  //generate lengths
  int32 NumLengths = xFillTmpLengths(TmpLen, TabCodeLengths);
  if(NumLengths == NOT_VALID) { return false; }

  //generate codes
  bool Result = xFillTmpCodes(TmpCode, TmpLen, NumLengths);
  if(!Result) { return false; }

  //writeout
  int32 TabLen = ArithTable.getClass() == xJFIF::xArithTable::eArithClass::DC ? 16 : 256;
  memset(ArithLen , 0, TabLen*sizeof(uint8 ));
  if(ArithCode != nullptr) { memset(ArithCode, 0, TabLen * sizeof(uint32)); }


  int32 MaxSymbol = ArithTable.isDC() ? 15 : 255;
  for (int32 p = 0; p < NumLengths; p++)
  {
    int32 idx = TabCodeSymbols[p];
    if(idx < 0 || idx > MaxSymbol || ArithLen[idx]) { return false; }    
    ArithLen [idx] = TmpLen [p];
    if(ArithCode != nullptr) { ArithCode[idx] = TmpCode[p]; }
  }


  return true;
}
int32 xArithCommon::xFillTmpLengths(uint8* Lenghts, const xJFIF::xArithTable::tCodeL& TabCodeLengths)
{
  int32 p = 0;
  for(int32 l = 1; l <= 16; l++)
  {
    int32 i = TabCodeLengths[l - 1];
    if(i < 0 || p + i > 256) { return NOT_VALID; }
    while(i--) { Lenghts[p++] = (uint8)l; }
  }
  Lenghts[p] = 0;

  return p;
}
int32 xArithCommon::xFillTmpCodes(uint32* Codes, const uint8* Lenghts, int32 /*NumLengths*/)
{
  uint32 Code = 0;
  uint32 si   = Lenghts[0];
  int32  p    = 0;
  while(Lenghts[p])
  {
    while(Lenghts[p] == si)
    {      
      Codes[p++] = Code;
      Code++;
    }
    if(Code >= ((uint32)1 << si)) { return false; }
    Code <<= 1;
    si++;
  }
  return true;
}
void xArithCommon::xAvoidZeroLenCodes(uint8* ArithLen, int32 TableSize)
{
  for(int32 i = 0; i < TableSize; i++)
  {
    if(ArithLen[i] == 0) { ArithLen[i] = 16; }
  }
}

//=============================================================================================================================================================================
// xArithDecoder
//=====================================================================================================================================================================================
void xArithDecoder::start(xBitstreamReader* bitstream)
{
    m_bitstream = bitstream;
    m_low = 0;
    m_high = TOP_VALUE;

    // Read the initial bits from the stream to fill the value buffer.
    m_value = 0;
    for (int i = 0; i < 64; ++i)
    {
        m_value = (m_value << 1) | m_bitstream->readBit();
    }
}

int64_t xArithDecoder::decode(const xArithmeticProbabilityModel& model)
{
    // Step 1: Find the symbol
    // This is done by mapping the current value in the [low, high] range
    // to the corresponding cumulative frequency in the model.
    const uint64_t range = m_high - m_low + 1;
    const uint64_t totalCount = model.getTotalCount();
    const uint64_t scaled_value = ((m_value - m_low + 1) * totalCount - 1) / range;

    // Find the symbol 's' where: lowerCount[s] <= scaled_value < upperCount[s]
    int64_t symbol = model.findSymbol(scaled_value);
    const SymbolModel& s = model.getSymbolModel(symbol);

    // Step 2: Update the range
    // Narrow the range to match the decoded symbol's sub-interval.
    m_high = m_low + (range * s.upperCount) / totalCount - 1;
    m_low = m_low + (range * s.lowerCount) / totalCount;

    // Step 3: Renormalize
    // Keep the interval wide enough to maintain precision.
    renormalize();

    return symbol;
}

void xArithDecoder::renormalize()
{
    while (true)
    {
        // Case 1: MSBs of low and high are the same. Shift them out.
        if ((m_low >> 63) == (m_high >> 63))
        {
            m_low <<= 1;
            m_high = (m_high << 1) | 1;
            m_value = (m_value << 1) | m_bitstream->readBit();
        }
        // Case 2: Underflow is possible. The range is straddling the midpoint.
        else if ((m_low >= FIRST_QUARTER) && (m_high < THIRD_QUARTER))
        {
            m_low = (m_low - FIRST_QUARTER) * 2;
            m_high = (m_high - FIRST_QUARTER) * 2 + 1;
            m_value = (m_value - FIRST_QUARTER) * 2 | m_bitstream->readBit();
        }
        // Case 3: Range is wide enough. Stop.
        else
        {
            break;
        }
    }
}

int64_t xArithDecoder::readSufix(xBitstreamReader* bitstream, int64_t numBits)
{
    if (numBits == 0) { return 0; }
    int64_t R = bitstream->readBits(numBits);
    // This logic correctly converts the JPEG suffix to a signed value.
    int64_t Value = R + (((R - (1 << (numBits - 1))) >> 31) & ((((uint64_t)-1) << numBits) + 1));
    return Value;
}

int64_t xArithDecoder::readDC(xBitstreamReader* bitstream, const xArithmeticProbabilityModel& model)
{
    // The decoded symbol represents the number of bits in the suffix.
    int64_t numBits = decode(model);
    return readSufix(bitstream, numBits);
}

//=============================================================================================================================================================================
// xArithEstimator
//=====================================================================================================================================================================================
bool xArithEstimatorDC::init(const xJFIF::xArithTable& ArithTable)
{
  if(ArithTable.getClass() != xJFIF::xArithTable::eArithClass::DC) { return false; }
  bool InitCorrect = xInitArithTables(m_ArithLen, nullptr, ArithTable);
  if(!InitCorrect) { return false; }
  xAvoidZeroLenCodes(m_ArithLen, xJPEG_Constants::c_MaxNumCodeSymbolsDC);
  return true;
}
bool xArithEstimatorAC::init(const xJFIF::xArithTable& ArithTable)
{
  if(ArithTable.getClass() != xJFIF::xArithTable::eArithClass::AC) { return false; }
  bool InitCorrect = xInitArithTables(m_ArithLen, nullptr, ArithTable);
  if(!InitCorrect) { return false; }
  xAvoidZeroLenCodes(m_ArithLen, xJPEG_Constants::c_MaxNumCodeSymbolsAC);
  return true;
}

//=====================================================================================================================================================================================
// xArithmeticProbabilityModel
//=====================================================================================================================================================================================

void xArithmeticProbabilityModel::buildLengthTable(uint8* LengthTable, const uint32* SymbolCount, int32 Size)
{
  std::priority_queue<xArithModel*, std::vector<xArithModel*>, Comparator > ArithmeticTree;

  //Before starting the procedure, the values of FREQ are collected for V = 0 to 255 and the FREQ value for V = 256 is set to 1 to reserve one code point
  ArithmeticTree.push(new xArithModel((int16)Size, 1));
	//insert values
	for(int32 i=0; i< Size; i++)
	{
		if(SymbolCount[i])
		{
			ArithmeticTree.push(new xArithModel((int16)(i), SymbolCount[i]));
		}
	}

	//build Arithmetic tree
  while(ArithmeticTree.size() > 1)
  {
    xArithModel* R = ArithmeticTree.top(); ArithmeticTree.pop();
    xArithModel* L = ArithmeticTree.top(); ArithmeticTree.pop();
    ArithmeticTree.push(new xArithModel(L, R));
  }
  xArithModel* Root = ArithmeticTree.top(); ArithmeticTree.pop();

	//generate codes
  memset(LengthTable, 0, Size+1);
  xCalcCodeLengths(LengthTable, Root, 0);
  delete Root; Root = nullptr;
}

void xArithmeticProbabilityModel::xCalcCodeLengths(uint8* LengthTable, xArithModel* Node, int32 Length)
{
	if(Node->m_Left==nullptr && Node->m_Right==nullptr)
	{
    int32 Symbol = Node->m_Symbol;
    assert(Symbol >= 0 && Symbol <= (int32)std::numeric_limits<uint8>::max() + 1);
    LengthTable[Symbol] = (uint8)Length;
	}
	else
	{
    xCalcCodeLengths(LengthTable, Node->m_Left , Length+1);
    xCalcCodeLengths(LengthTable, Node->m_Right, Length+1);
	}
}
flt64 xArithmeticProbabilityModel::xCalcAvgCodeLength(const uint8* CodeLength, const uint32* SymbolCount, int32 Size)
{
  int64 TotalCount  = 0;
  int64 TotalLength = 0;
  for(int32 i = 0; i < Size; i++)
  {
    //assert((SymbolCount[i] == 0 && CodeLength[i] == 0) || (SymbolCount[i] != 0 && CodeLength[i] != 0));
    TotalCount  += SymbolCount[i];
    TotalLength += (int64)SymbolCount[i] * (int64)CodeLength[i];
  }
  flt64 AvgCodeLength = (flt64)TotalLength / (flt64)TotalCount;

  //flt64 Entropy = 0;
  //for(int32 i = 0; i < Size; i++)
  //{
  //  if(SymbolCount[i] > 0)
  //  {
  //    flt64 Probability = (flt64)SymbolCount[i] / (flt64)TotalCount;
  //    Entropy -= Probability * (log(Probability) / log(2));
  //  }
  //}

  return AvgCodeLength;
}
flt64 xArithmeticProbabilityModel::calcAvgCodeLength(const xJFIF::xArithTable& ArithTable, const uint32* SymbolCount)
{
  const int32 MaxNumCodesymbols = ArithTable.getMaxNumCodeSymbols();
  std::vector<uint8>ArithLengths(MaxNumCodesymbols);
  xArithCommon::xInitArithTables(ArithLengths.data(), nullptr, ArithTable);
  return xCalcAvgCodeLength(ArithLengths.data(), SymbolCount, MaxNumCodesymbols);
}

//=====================================================================================================================================================================================

} //end of namespace PMBB::JPEG