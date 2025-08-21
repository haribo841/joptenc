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
bool xArithDecoder::xInitTables(const xJFIF::xArithTable& ArithTable)
{
  const xJFIF::xArithTable::tCodeL& TabCodeLengths = ArithTable.getCodeLengths();
  const xJFIF::tByteV&             TabCodeSymbols = ArithTable.getCodeSymbols();

  uint32 TmpCode[257];
  uint8  TmpLen [257];

  //copy code symbols from ArithTable
  memset(m_CodeSymbols, 0, 256);
  memcpy(m_CodeSymbols, TabCodeSymbols.data(), TabCodeSymbols.size());

  //generate length
  int32 NumLengths = xFillTmpLengths(TmpLen, TabCodeLengths);
  if(NumLengths == NOT_VALID) { return false; }

  //generate codes
  bool Result = xFillTmpCodes(TmpCode, TmpLen, NumLengths);
  if(!Result) { return false; }

  {
    int32 p = 0;
    for(int32 l = 1; l <= 16; l++)
    {
      if(TabCodeLengths[l - 1])
      {
        m_ValOffset[l] = p - (int32)TmpCode[p];
        p += TabCodeLengths[l - 1];
        m_MaxCode[l] = TmpCode[p - 1];
      }
      else
      {
        m_MaxCode[l] = -1;
      }
    }
    m_ValOffset[17] = 0;
    m_MaxCode  [17] = 0xFFFFFL;
  }

#if X_PMBB_JPEG_MULTI_LEVEL_LOOKAHEAD
  for(int32 i = 0; i < (1 << c_LookAhead1st); i++)
  {
    m_Lookup1st[i] = (uint16)((c_LookAhead1st + 1) << c_LookAhead1st);
  }
  {
    int32 p = 0;
    for(int32 l = 1; l <= c_LookAhead1st; l++)
    {
      for(uint32 i = 1; i <= (int32)TabCodeLengths[l - 1]; i++, p++)
      {
        int32 LookBits = TmpCode[p] << (c_LookAhead1st - l);
        for(int32 ctr = 1 << (c_LookAhead1st - l); ctr > 0; ctr--)
        {
          m_Lookup1st[LookBits] = (l << c_LookAhead1st) | TabCodeSymbols[p];
          LookBits++;
        }
      }
    }
  }

  for(int32 i = 0; i < (1 << c_LookAhead2nd); i++)
  {
    m_Lookup2nd[i] = (uint32)((c_LookAhead2nd + 1) << c_LookAhead2nd);
  }
  {
    int32 p = 0;
    for(uint32 l = 1; l <= c_LookAhead2nd; l++)
    {
      for(int32 i = 1; i <= (int32)TabCodeLengths[l - 1]; i++, p++)
      {
        int32 LookBits = TmpCode[p] << (c_LookAhead2nd - l);
        for(int32 ctr = 1 << (c_LookAhead2nd - l); ctr > 0; ctr--)
        {
          m_Lookup2nd[LookBits] = ((l << c_LookAhead2nd) | TabCodeSymbols[p]);
          LookBits++;
        }
      }
    }
  }

#else //X_PMBB_JPEG_MULTI_LEVEL_LOOKAHEAD

  for(int32 i = 0; i < (1 << c_LookAhead); i++)
  {
    m_Lookup[i] = (c_LookAhead + 1) << c_LookAhead;
  }
  {
    int32 p = 0;
    for(int32 l = 1; l <= c_LookAhead; l++)
    {
      for(int32 i = 1; i <= (int32)TabCodeLengths[l - 1]; i++, p++)
      {
        int32 LookBits = TmpCode[p] << (c_LookAhead - l);
        for(int32 ctr = 1 << (c_LookAhead - l); ctr > 0; ctr--)
        {
          m_Lookup[LookBits] = (l << c_LookAhead) | TabCodeSymbols[p];
          LookBits++;
        }
      }
    }
  }

#endif //X_PMBB_JPEG_MULTI_LEVEL_LOOKAHEAD

  if (ArithTable.isDC())
  {
    for (int32 i = 0; i < NumLengths; i++)
    {
      int sym = TabCodeSymbols[i];
      if(sym < 0 || sym > 15) { return false; }
    }
  }

  return true;
}

#if X_PMBB_JPEG_MULTI_LEVEL_LOOKAHEAD
int32 readPrefix(xBitstreamReader* Bitstream)
{
  uint32 Peek1st    = Bitstream->peekBits(c_LookAhead1st);
  uint32 Lockup1st  = m_Lookup1st[Peek1st];
  uint32 NumBits1st = Lockup1st >> c_LookAhead1st;

  if(NumBits1st <= c_LookAhead1st)
  {
    Bitstream->skipBits(NumBits1st);
    return Lockup1st & ((1 << c_LookAhead1st) - 1);
  }
  else
  {
    uint32 Peek2nd    = Bitstream->peekBits(c_LookAhead2nd);
    uint32 Lockup2nd  = m_Lookup2nd[Peek2nd];
    uint32 NumBits2nd = Lockup2nd >> c_LookAhead2nd;
    if(NumBits2nd <= c_LookAhead2nd)
    {
      Bitstream->skipBits(NumBits2nd);
      return Lockup2nd & ((1 << c_LookAhead2nd) - 1);
    }
    else
    {
      int32 S = Bitstream->readBits(NumBits2nd);
      while(S > m_MaxCode[NumBits2nd])
      {
        S <<= 1;
        S |= Bitstream->readBit();
        NumBits2nd++;
      }
      S = m_CodeSymbols[(S + m_ValOffset[NumBits2nd]) & 0xFF];
      return S;
    }
  }
}
#else //X_PMBB_JPEG_MULTI_LEVEL_LOOKAHEAD
int32 xArithDecoder::readPrefix(xBitstreamReader* Bitstream)
{
  uint32 Peek    = Bitstream->peekBits(c_LookAhead);
  uint32 Lockup  = m_Lookup[Peek];
  uint32 NumBits = Lockup >> c_LookAhead;

  if(NumBits <= c_LookAhead)
  {
    Bitstream->skipBits(NumBits);
    return Lockup & ((1 << c_LookAhead) - 1);
  }
  else
  {
    int32 S = Bitstream->readBits(NumBits);
    while(S > m_MaxCode[NumBits])
    {
      S <<= 1;
      S |= Bitstream->readBit();
      NumBits++;
    }
    S = m_CodeSymbols[(S + m_ValOffset[NumBits]) & 0xFF];
    return S;
  }    
}
#endif //X_PMBB_JPEG_MULTI_LEVEL_LOOKAHEAD


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
// xArithmeticTabBuilder
//=====================================================================================================================================================================================

void xArithmeticTabBuilder::buildLengthTable(uint8* LengthTable, const uint32* SymbolCount, int32 Size)
{
  std::priority_queue<xArithTree*, std::vector<xArithTree*>, Comparator > ArithmeticTree;

  //Before starting the procedure, the values of FREQ are collected for V = 0 to 255 and the FREQ value for V = 256 is set to 1 to reserve one code point
  ArithmeticTree.push(new xArithTree((int16)Size, 1));
	//insert values
	for(int32 i=0; i< Size; i++)
	{
		if(SymbolCount[i])
		{
			ArithmeticTree.push(new xArithTree((int16)(i), SymbolCount[i]));
		}
	}

	//build Arithmetic tree
  while(ArithmeticTree.size() > 1)
  {
    xArithTree* R = ArithmeticTree.top(); ArithmeticTree.pop();
    xArithTree* L = ArithmeticTree.top(); ArithmeticTree.pop();
    ArithmeticTree.push(new xArithTree(L, R));
  }
  xArithTree* Root = ArithmeticTree.top(); ArithmeticTree.pop();

	//generate codes
  memset(LengthTable, 0, Size+1);
  xCalcCodeLengths(LengthTable, Root, 0);
  delete Root; Root = nullptr;
}

void xArithmeticTabBuilder::xCalcCodeLengths(uint8* LengthTable, xArithTree* Node, int32 Length)
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
flt64 xArithmeticTabBuilder::xCalcAvgCodeLength(const uint8* CodeLength, const uint32* SymbolCount, int32 Size)
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
flt64 xArithmeticTabBuilder::calcAvgCodeLength(const xJFIF::xArithTable& ArithTable, const uint32* SymbolCount)
{
  const int32 MaxNumCodesymbols = ArithTable.getMaxNumCodeSymbols();
  std::vector<uint8>ArithLengths(MaxNumCodesymbols);
  xArithCommon::xInitArithTables(ArithLengths.data(), nullptr, ArithTable);
  return xCalcAvgCodeLength(ArithLengths.data(), SymbolCount, MaxNumCodesymbols);
}

//=====================================================================================================================================================================================

} //end of namespace PMBB::JPEG