/*
    SPDX-FileCopyrightText: 2019-2023 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <functional>
#include <utility>
#include <array>
#include "xTestUtils.h"
#include "xTimeUtils.h"
#include "xMemory.h"
#include "xCommonDefJPEG.h"
#include "xJPEG_Entropy.h"

using namespace PMBB_NAMESPACE;
using namespace PMBB_NAMESPACE::JPEG;

/*/===============================================================================================================================================================================================================

class xArithEncoderDC_Test : public xArithEncoderDC
{
public:
  const uint32* getArithCode() const { return m_ArithCode; }
  const uint8*  getArithLen () const { return m_ArithLen ; }
};

class xArithEncoderAC_Test : public xArithEncoderAC
{
public:
  const uint32* getArithCode() const { return m_ArithCode; }
  const uint8*  getArithLen () const { return m_ArithLen ; }
};

//===============================================================================================================================================================================================================

void testArithemticInitialization(xJFIF::xArithTable::eArithClass ArithClass, eCmp Component)
{
  xJFIF::xArithTable HT;
  HT.InitDefault(0, ArithClass, Component);

  const uint8*  ArithLenDefault  = nullptr;
  const uint16* ArithCodeDefault = nullptr;
  if(ArithClass == xJFIF::xArithTable::eArithClass::DC && Component == eCmp::LM) { ArithLenDefault = xJPEG_Constants::c_ArithLenDefaultLumaDC  ; ArithCodeDefault = xJPEG_Constants::c_ArithCodeDefaultLumaDC  ; }
  if(ArithClass == xJFIF::xArithTable::eArithClass::AC && Component == eCmp::LM) { ArithLenDefault = xJPEG_Constants::c_ArithLenDefaultLumaAC  ; ArithCodeDefault = xJPEG_Constants::c_ArithCodeDefaultLumaAC  ; }
  if(ArithClass == xJFIF::xArithTable::eArithClass::DC && Component != eCmp::LM) { ArithLenDefault = xJPEG_Constants::c_ArithLenDefaultChromaDC; ArithCodeDefault = xJPEG_Constants::c_ArithCodeDefaultChromaDC; }
  if(ArithClass == xJFIF::xArithTable::eArithClass::AC && Component != eCmp::LM) { ArithLenDefault = xJPEG_Constants::c_ArithLenDefaultChromaAC; ArithCodeDefault = xJPEG_Constants::c_ArithCodeDefaultChromaAC; }
  REQUIRE(ArithLenDefault  != nullptr);
  REQUIRE(ArithCodeDefault != nullptr);

  if(ArithClass == xJFIF::xArithTable::eArithClass::DC)
  {
    xArithEncoderDC_Test HE;
    HE.init(HT);

    for(int32 i = 0; i < xJPEG_Constants::c_MaxNumCodeSymbolsDC; i++)
    {
      uint8 TstArithLen = HE.getArithLen()[i];
      uint8 RefArithLen = ArithLenDefault[i];
      CHECK(TstArithLen == RefArithLen);
      uint32 TstArithCode = HE.getArithCode()[i];
      uint32 RefArithCode = ArithCodeDefault[i];
      CHECK(TstArithCode == RefArithCode);
    }
  }

  if(ArithClass == xJFIF::xArithTable::eArithClass::AC)
  {
    xArithEncoderAC_Test HE;
    HE.init(HT);

    for(int32 i = 0; i < xJPEG_Constants::c_MaxNumCodeSymbolsAC; i++)
    {
      uint8 TstArithLen = HE.getArithLen()[i];
      uint8 RefArithLen = ArithLenDefault[i];
      CHECK(TstArithLen == RefArithLen);
      uint32 TstArithCode = HE.getArithCode()[i];
      uint32 RefArithCode = ArithCodeDefault[i];
      CHECK(TstArithCode == RefArithCode);
    }
  }
}

//===============================================================================================================================================================================================================

void testCustomArithmeticTableGeneration(const uint8* ArithLenghts, const uint8* CodeLengths, xJFIF::xArithTable::eArithClass ArithClass)
{
  const int32 MaxNumCodeSymbols = xJFIF::xArithTable::getMaxNumCodeSymbols(ArithClass);

  //generate simulated symbol conts based on code length
  std::vector<uint32> SymbolCount(MaxNumCodeSymbols);
  for(int32 i = 0; i < MaxNumCodeSymbols; i++)
  {
    int32  CodeLen  = ArithLenghts[i];
    uint32 EstCount = CodeLen != 0 ? 1<<(18 - CodeLen) : 0; // same as 2^18 * 2^(-CodeLen)
    SymbolCount[i]  = EstCount;
  }

  //design custom code table 
  std::vector<uint8> LengthTable(MaxNumCodeSymbols + 1);
  xArithmeticTabBuilder::buildLengthTable(LengthTable.data(), SymbolCount.data(), MaxNumCodeSymbols);
  xJFIF::xArithTable HT;
  HT.InitCustom(0, ArithClass, LengthTable.data());

  //check if DHT code length entries are the same (checking DTH code sumbol is useless - they can differ)
  for(int32 i = 0; i < xJPEG_Constants::c_NumCodeLenghts; i++)
  {
    CHECK(HT.getCodeLengths()[i] == CodeLengths[i]);
  }

  //check if decoded Arithmetic code lenghts are same (checking decoded code sumbols is useless - they can differ)
  if(ArithClass == xJFIF::xArithTable::eArithClass::DC)
  {
    xArithEncoderDC_Test HE;
    HE.init(HT);

    for(int32 i = 0; i < MaxNumCodeSymbols; i++)
    {
      uint8 TstArithLen = HE.getArithLen()[i];
      uint8 RefArithLen = ArithLenghts[i];
      CHECK(TstArithLen == RefArithLen);
    }
  }  
  if(ArithClass == xJFIF::xArithTable::eArithClass::AC)
  {
    xArithEncoderAC_Test HE;
    HE.init(HT);

    for(int32 i = 0; i < MaxNumCodeSymbols; i++)
    {
      uint8 TstArithLen = HE.getArithLen()[i];
      uint8 RefArithLen = ArithLenghts[i];
      CHECK(TstArithLen == RefArithLen);
    }
  }
}

//===============================================================================================================================================================================================================

TEST_CASE("testArithmeticInitialization")
{
  testArithmeticInitialization(xJFIF::xArithTable::eArithClass::DC, eCmp::LM);
  testArithmeticInitialization(xJFIF::xArithTable::eArithClass::AC, eCmp::LM);
  testArithmeticInitialization(xJFIF::xArithTable::eArithClass::DC, eCmp::CB);
  testArithmeticInitialization(xJFIF::xArithTable::eArithClass::AC, eCmp::CB);
}

TEST_CASE("testCustomArithmeticTableGeneration")
{
  testCustomArithmeticTableGeneration(xJPEG_Constants::c_ArithLenDefaultLumaDC  , xJPEG_Constants::m_CodeLengthLumaDC  , xJFIF::xArithTable::eArithClass::DC);
  testCustomArithmeticTableGeneration(xJPEG_Constants::c_ArithLenDefaultLumaAC  , xJPEG_Constants::m_CodeLengthLumaAC  , xJFIF::xArithTable::eArithClass::AC);
  testCustomArithmeticTableGeneration(xJPEG_Constants::c_ArithLenDefaultChromaDC, xJPEG_Constants::m_CodeLengthChromaDC, xJFIF::xArithTable::eArithClass::DC);
  testCustomArithmeticTableGeneration(xJPEG_Constants::c_ArithLenDefaultChromaAC, xJPEG_Constants::m_CodeLengthChromaAC, xJFIF::xArithTable::eArithClass::AC);
}

//===============================================================================================================================================================================================================*/
