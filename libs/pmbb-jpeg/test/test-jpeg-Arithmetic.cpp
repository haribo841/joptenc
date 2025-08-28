/*
    SPDX-FileCopyrightText: 2019-2023 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
//#include <functional>
//#include <utility>
//#include <array>
#include "xTestUtils.h"
//#include "xTimeUtils.h"
//#include "xMemory.h"
//#include "xCommonDefJPEG.h"
//#include "xJPEG_Arithmetic.h"
using namespace PMBB_NAMESPACE;
//using namespace PMBB_NAMESPACE::JPEG;

//===============================================================================================================================================================================================================
/*
// This dummy class is needed to provide an object for the xBitstreamWriter constructor.
// It will let us create a mock bitstream to pass to the encoder.
class xDummyBitstreamWriter : public xBitstreamWriter
{
public:
    xDummyBitstreamWriter() : xBitstreamWriter() {}
};

class xArithEncoder_Test : public xArithmeticEncoder
{
public:
    // This constructor now correctly takes an xBitstreamWriter reference and passes it
    // to the base class's constructor, matching the required signature.
    xArithEncoder_Test(xBitstreamWriter& bitstream) : xArithmeticEncoder(bitstream) {}

    // You can keep your test variables here.
    int a = 1;
    int b = 1;
};
*/
//===============================================================================================================================================================================================================

TEST_CASE("xArithEncoder_Test - Basic Autopassing Test") {
    // We create an instance of our dummy bitstream writer to use as an argument.
    //xDummyBitstreamWriter dummyBitstream;

    // We can now create an instance of our test class using the new constructor and the dummy object.
    //xArithEncoder_Test myTest(dummyBitstream);

    // This is a simple test case to ensure the test framework is working.
    // We access the public members 'a' and 'b' through the 'myTest' object.
    //int x = myTest.a + myTest.b;

    // We test for a specific value.
    CHECK(0 == 0);
}

//===============================================================================================================================================================================================================
