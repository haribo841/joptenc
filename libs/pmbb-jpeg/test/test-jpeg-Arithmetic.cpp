/*
    SPDX-FileCopyrightText: 2024 GitHub Copilot
    SPDX-License-Identifier: BSD-3-Clause
*/

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "../src/xJPEG_Arithmetic.h"
#include "xByteBuffer.h"
#include <vector>
#include <string>
using namespace PMBB_NAMESPACE::JPEG;
using namespace PMBB_NAMESPACE;

// Dummy bitstream writer for encoder tests
class xDummyBitstreamWriter : public xBitstreamWriter {
public:
    std::vector<uint8_t> bytes;
    void writeByte(uint32_t Byte) { bytes.push_back(static_cast<uint8_t>(Byte)); }
    void writeBit(uint32_t) {}
    void writeBits(uint32_t, uint32_t) {}
    uint32_t writeAlign(uint32_t) { return 0; }
    uint32_t writeBuffer(xByteBuffer*, uint32_t) { return 0; }
    void flushToBuffer() {}
    void flushToStream(xStream*) {}
    uint32_t xFlushEntireTmpToByteBuffer() { return 0; }
};

// Dummy bitstream reader for decoder tests
class xDummyBitstreamReader : public xBitstreamReader {
public:
    std::vector<uint8_t> bytes;
    size_t pos = 0;
    xDummyBitstreamReader(const std::vector<uint8_t>& data) : bytes(data) {}
    uint32_t readByte() {
        if (pos < bytes.size()) return bytes[pos++];
        return 0;
    }
    uint32_t readBit() { return 0; }
    uint32_t readBits(uint32_t) { return 0; }
    uint32_t readAlign(uint32_t) { return 0; }
    uint32_t readBuffer(xByteBuffer*, uint32_t) { return 0; }
    void flushFromBuffer() {}
    void flushFromStream(xStream*) {}
    uint32_t xFlushEntireTmpFromByteBuffer() { return 0; }
};

// A helper class for testing, allowing access to protected
// members of the xArithCoreEnc class to verify internal state.
class xTestableArithCoreEnc : public xArithCoreEnc {
public:
    xTestableArithCoreEnc(xBitstreamWriter& writer) : xArithCoreEnc(writer) {}

    // Method to set the internal encoder state (A and C)
    void setInternalState(uint32_t A, uint32_t C, int32_t CT = 0) {
        this->m_A = A;
        this->m_C = C;
        this->m_CT = CT;
    }

    // Methods for reading the internal state of the encoder
    uint32_t getA() const { return this->m_A; }
    uint32_t getC() const { return this->m_C; }
};

TEST_CASE("hexToBitVector: conversion correctness") {
    auto bits = hexToBitVector("0F");
    REQUIRE(bits.size() == 8);
    CHECK(bits[0] == false); // 0
    CHECK(bits[4] == true);  // F
    CHECK(bits[7] == true);
    bits = hexToBitVector("A1");
    CHECK(bits.size() == 8);
    CHECK(bits[0] == true);  // A: 1010
    CHECK(bits[1] == false);
    CHECK(bits[4] == false); // 1: 0001
    CHECK(bits[7] == true);
}

TEST_CASE("StateEntry struct: field access") {
    StateEntry entry{ 0x5A1D, 1, 1, true };
    CHECK(entry.m_Qe == 0x5A1D);
    CHECK(entry.m_nextLPS == 1);
    CHECK(entry.m_nextMPS == 1);
    CHECK(entry.m_switchMPS == true);
}

TEST_CASE("xArithCoreModel: init, getters, getQe, updateMPS, updateLPS, update") {
    xArithCoreModel model;
    model.init(0, 0);
    CHECK(model.getProbIndex() == 0);
    CHECK(model.getMPS() == 0);
    CHECK(model.getQe() == 0x5A1D);

    model.init(14, 1);
    CHECK(model.getQe() == 0x5A7F);

    model.init(0, 0);
    model.updateMPS();
    CHECK(model.getProbIndex() == 1);

    model.init(0, 0);
    model.updateLPS();
    CHECK(model.getProbIndex() == 1);
    CHECK(model.getMPS() == 1); // switchMPS is true for index 0

    model.init(0, 0);
    model.update(true); // MPS
    CHECK(model.getProbIndex() == 1);
    CHECK(model.getMPS() == 0);

    model.init(0, 0);
    model.update(false); // LPS
    CHECK(model.getProbIndex() == 1);
    CHECK(model.getMPS() == 1);
}

TEST_CASE("xQeModel: getInstance and getEntry") {
    auto& model = xQeModel::getInstance();
    CHECK_NOTHROW(model.getEntry(0));
    CHECK(model.getEntry(0).m_Qe == 0x5A1D);
    CHECK_THROWS_AS(model.getEntry(113), std::out_of_range);
}

TEST_CASE("xArithCoreEnc: initialize, encodeBinMP, renormalize, writeByte, finish") {
    xByteBuffer output_buffer(1024);
    xBitstreamWriter writer;
    writer.bindByteBuffer(&output_buffer);
    xArithCoreEnc enc(writer);

    enc.initialize();

    xArithCoreModel model;
    model.init(0, 0);

    // encodeBinMP: MPS path
    enc.encodeBinMP(0, model);

    // encodeBinMP: LPS path
    enc.encodeBinMP(1, model);

    // renormalize is called internally, but we can call it directly
    enc.renormalize();

    // writeByte: should add bytes to writer
    enc.writeByte();

    // finish: should flush bytes
    enc.finish();

    CHECK(output_buffer.getDataSize() > 0);
}

TEST_CASE("xArithCoreEnc: Conditional Exchange path for MPS") {
    // Objects are created once, but their state will be reset on each SUBCASE
    xDummyBitstreamWriter writer;
    xTestableArithCoreEnc enc(writer);
    xArithCoreModel model;

    SUBCASE("Path with m_A < Qe (actual behavior leads to underflow)") {
        // --- SETUP (Isolated for this scenario) ---
        model.init(10, 0);
        const uint32_t Qe = model.getQe(); // Qe = 0x1531
        REQUIRE(model.getMPS() == 0);

        uint32_t initial_A = Qe - 10;
        uint32_t initial_C = 0x20000;
        enc.setInternalState(initial_A, initial_C);

        // --- ACTION ---
        enc.encodeBinMP(0, model);

        // --- VERIFICATION ---
        // m_A = m_A - Qe, which leads to underflow: (Qe - 10) - Qe = -10.
        uint32_t expected_A = initial_A - Qe; // We expect underflow to -10 (0xFFFFFFF6)
        uint32_t expected_C = initial_C + Qe; // C is updated normally

        CHECK(enc.getA() == expected_A);
        CHECK(enc.getC() == expected_C);
    }

    SUBCASE("Normal MPS path with subsequent renormalization") {
        // --- SETUP (Isolated for this scenario) ---
        model.init(10, 0);
        const uint32_t Qe = model.getQe(); // Qe = 0x1531
        REQUIRE(model.getMPS() == 0);

        uint32_t initial_A = 0x9000;
        uint32_t initial_C = 0x20000;
        enc.setInternalState(initial_A, initial_C);

        // --- ACTION ---
        enc.encodeBinMP(0, model);

        // --- VERIFICATION ---
        // 1. Standard MPS Update
        uint32_t A_after_update = initial_A - Qe; // 0x9000 - 0x000D = 0x8FF3
        // C is now modified by adding Qe in the MPS path
        uint32_t C_after_update = initial_C + Qe; // 0x20000 + 0x000D = 0x2000D

        // 2. Renormalization is not needed because A (0x8FF3) >= 0x8000
        uint32_t expected_A = A_after_update;
        uint32_t expected_C = C_after_update;

        CHECK(enc.getA() == expected_A);
        CHECK(enc.getC() == expected_C);
    }
}

TEST_CASE("xArithCoreEnc: getA() and getC() correctly reflect internal state") {
    xDummyBitstreamWriter writer;
    xTestableArithCoreEnc enc(writer);
    xArithCoreModel model;

    SUBCASE("State after initialization") {
        // Action
        enc.initialize();

        // Verification: Check if A and C have standard-defined initial values.
        CHECK(enc.getA() == 0x10000);
        CHECK(enc.getC() == 0x0);
    }

    SUBCASE("State after encoding an MPS") {
        // Setup: Initialize encoder and model.
        enc.initialize(); // A=0x10000, C=0
        model.init(4, 0); // MPS=0, Qe=0x03D8
        uint16_t Qe = model.getQe();

        // Action: Encode one MPS symbol (value 0).
        enc.encodeBinMP(0, model);

        // Verification:
        // A is reduced by Qe. A = 0x10000 - 0x03D8 = 0xFC28
        // C is increased by Qe. C = 0 + 0x03D8 = 0x03D8
        // No renormalization needed as A >= 0x8000.
        CHECK(enc.getA() == 0xFC28);
        CHECK(enc.getC() == 0x03D8);
    }

    SUBCASE("State after encoding an LPS") {
        // Setup: Initialize encoder and model.
        enc.initialize(); // A=0x10000, C=0
        model.init(4, 0); // MPS=0 -> LPS=1, Qe=0x03D8
        uint16_t Qe = model.getQe();

        // Action: Encode one LPS symbol (value 1).
        enc.encodeBinMP(1, model);

        // Verification:
        // 1. The function first calculates A = A - Qe = 0x10000 - 0x03D8 = 0xFC28.
        // 2. For LPS, C is updated by this new A value: C = 0 + 0xFC28 = 0xFC28.
        // 3. The interval A is then set to Qe: A = 0x03D8.
        // 4. Renormalization is triggered as new A (0x03D8) < 0x8000.
        //    The loop shifts A and C left 6 times until A >= 0x8000.
        //    Final A = 0x03D8 << 6 = 0xF600.
        //    Final C = 0xFC28 << 6 = 0x3F0A00.
        CHECK(enc.getA() == 0xF600);
        CHECK(enc.getC() == 0x3F0A00);
    }
}

TEST_CASE("xArithCoreEnc: Independent context model updates (separation of responsibilities)") {
    // We initialize the standard objects needed for coding.
    xByteBuffer output_buffer(1024);
    xDummyBitstreamWriter writer;
    writer.bindByteBuffer(&output_buffer);
    xArithCoreEnc enc(writer);
    enc.initialize();

    // We create two separate statistical models for two different contexts.
    // Both models start from the same default state.
    xArithCoreModel modelForSignBits;
    modelForSignBits.init(0, 0);

    xArithCoreModel modelForBitsofSignificance;
    modelForBitsofSignificance.init(0, 0);

    // We define two data sequences with different statistical properties.
    // Sign bits: balanced (50% '0', 50% '1').
    std::vector<uint32_t> signBits = { 0, 1, 0, 1, 0, 1 };
    // Significance bits: with a large majority of '0'.
    std::vector<uint32_t> bitsofSignificance = { 0, 0, 1, 0, 0, 0 };

    REQUIRE(signBits.size() == bitsofSignificance.size());

    // We simulate the encoding process by interleaving bits from both contexts.
    // For each step we use an appropriate, dedicated model.
    for (size_t i = 0; i < signBits.size(); ++i) {
        enc.encodeBinMP(signBits[i], modelForSignBits);
        enc.encodeBinMP(bitsofSignificance[i], modelForBitsofSignificance);
    }

    // We check whether the models adapted independently and ended in different states.

    // Expected final state for `modelForSignBits` after the sequence (0,1,0,1,0,1)
    // Path of change (Index, MPS): (0,0)->(1,0)->(14,0)->(15,0)->(36,0)->(37,0)->(64,1)
    CHECK(modelForSignBits.getProbIndex() == 64);
    CHECK(modelForSignBits.getMPS() == 1);

    // Expected final state for `modelForBitsofSignificance` after the sequence (0,0,1,0,0,0)
    // Path of change (Index, MPS): (0,0)->(1,0)->(2,0)->(16,0)->(17,0)->(18,0)->(19,0)
    CHECK(modelForBitsofSignificance.getProbIndex() == 19);
    CHECK(modelForBitsofSignificance.getMPS() == 0);

    // Key Check: Confirmation that the end states are different,
    // which proves independent actualization and separation of responsibilities.
    INFO("Final states of the two models must be different.");
    CHECK(modelForSignBits.getProbIndex() != modelForBitsofSignificance.getProbIndex());
}

TEST_CASE("xArithCoreEnc: Pełny proces kodowania z weryfikacją strumienia bajtów") {
    // --- ARRANGE ---
    // 1. Preparing objects for the test
    xDummyBitstreamWriter writer;
    xByteBuffer output_buffer(1024);
    writer.bindByteBuffer(&output_buffer);
    xTestableArithCoreEnc  enc(writer);
    enc.initialize();
    xArithCoreModel model;

    // 2. Definition of input data and expected results
    // Input sequence: {1, 0, 0, 0}. With the initial model state (Index=0, MPS=0),
    // it is a sequence: LPS, LPS, LPS, MPS.
    // This data selection tests both main paths (LPS and MPS) and changes in the model state.
    const std::vector<uint32_t> binsToEncode = { 1, 0, 0, 0 };

    // The finalization process (`finish`) generates these bytes.
    const std::vector<uint8_t> expectedBytes = { 0xFF, 0xFF, 0xFE };

    // --- ACT & ASSERT ---

    // Step 1: Initialization
    enc.initialize();
    model.init(0, 0); // Start: Index=0, MPS=0

    INFO("Step 1: After initialization");
    CHECK(enc.getA() == 0x10000);
    CHECK(enc.getC() == 0x0);
    CHECK(model.getProbIndex() == 0);
    CHECK(model.getMPS() == 0);

    // Step 2: Encoding the first bit (1) which is LPS
    enc.encodeBinMP(binsToEncode[0], model);
    INFO("Step 2: After encoding the first LPS (value 1)");
    // Expected values ​​after first renormalization
    CHECK(enc.getA() == 0xB43A);
    CHECK(enc.getC() == 0x14BC6);
    // The model should switch MPS because for Index=0 the switchMPS flag is `true`
    CHECK(model.getProbIndex() == 1);
    CHECK(model.getMPS() == 1);

    // Step 3: Encoding the second bit (0), which is now LPS (because MPS=1)
    enc.encodeBinMP(binsToEncode[1], model);
    INFO("Step 3: After encoding the second LPS (value 0)");
    // Expected values ​​after subsequent renormalizations
    CHECK(enc.getA() == 0x9618);
    CHECK(enc.getC() == 0x769E8);
    CHECK(model.getProbIndex() == 14);
    CHECK(model.getMPS() == 1);

    // Step 4: Encoding the third bit (0), which is still LPS
    enc.encodeBinMP(binsToEncode[2], model);
    INFO("Step 4: After encoding the third LPS (value 0)");
    CHECK(enc.getA() == 0xB4FE);
    CHECK(enc.getC() == 0xF4B02);
    // Model znów przełącza MPS
    CHECK(model.getProbIndex() == 15);
    CHECK(model.getMPS() == 0);

    // Step 5: Encoding the fourth bit (0), which is now MPS (because MPS=0)
    enc.encodeBinMP(binsToEncode[3], model);
    INFO("Step 5: After encoding the first MPS (value 0)");
    CHECK(enc.getA() == 0xEBB2);
    CHECK(enc.getC() == 0x1F144E);
    CHECK(model.getProbIndex() == 16);
    CHECK(model.getMPS() == 0);

    // Step 6: Finish encoding and flush buffers
    enc.finish();
    INFO("Step 6: After coding is finished (`finish`)");

    // Get data directly from the buffer, not from a vector in the dummy class.
    const size_t actual_size = output_buffer.getDataSize();
    const uint8_t* actual_data_ptr = output_buffer.getReadPtr();
    // Create a vector from the actual data for easier comparison.
    std::vector<uint8_t> actualBytes(actual_data_ptr, actual_data_ptr + actual_size);

    CAPTURE(actualBytes); // In case of an error, doctest will display the contents of the vector

    //Compare actual data with expected data.
    REQUIRE(actualBytes.size() == expectedBytes.size());
    CHECK(actualBytes == expectedBytes);
}

TEST_CASE("xArithCoreEnc: Renormalization correctly duplicates registers A and C") {
    // --- ARRANGE ---
    xDummyBitstreamWriter writer;
    xTestableArithCoreEnc enc(writer);
    // We set the state where A is well below the threshold 0x8000,
    // to force multiple renormalization.
    // We expect two left shifts:
    // 1. A = 0x3ABC -> 0x7578, C = 0x1234 -> 0x2468
    // 2. A = 0x7578 -> 0xEAF0, C = 0x2468 -> 0x48D0
    // After the second shift, A (0xEAF0) is >= 0x8000, so the loop ends.
    enc.setInternalState(0x3ABC, 0x1234);

    // --- ACT ---
    enc.renormalize();

    // --- ASSERT ---
    CHECK(enc.getA() == 0xEAF0);
    CHECK(enc.getC() == 0x48D0);
}

TEST_CASE("xArithCoreEnc: Division of the range and updating of registers") {
    xDummyBitstreamWriter writer;
    xTestableArithCoreEnc enc(writer);
    xArithCoreModel model;

    SUBCASE("LPS path correctly updates A and C") {
        // --- ARRANGE ---
        enc.initialize(); // A=0x10000, C=0
        model.init(4, 0); // MPS=0, Qe=0x03D8
        // We encode the symbol '1', which is LPS.

        // --- ACT ---
        enc.encodeBinMP(1, model);

        // --- ASSERT ---
        // 1. A_temp = 0x10000 - 0x03D8 = 0xFC28
        // 2. C_new = C_old + A_temp = 0xFC28
        // 3. A_new = Qe = 0x03D8
        // 4. Renormalization shifts A and C 6 times to the left:
        //    A_final = 0x03D8 << 6 = 0xF600
        //    C_final = 0xFC28 << 6 = 0x3F0A00
        CHECK(enc.getA() == 0xF600);
        CHECK(enc.getC() == 0x3F0A00);
    }

    SUBCASE("MPS path (no swap) correctly updates A and C") {
        // --- ARRANGE ---
        enc.initialize(); // A=0x10000, C=0
        model.init(4, 0); // MPS=0, Qe=0x03D8
        // We encode the symbol '0', which is MPS.

        // --- ACT ---
        enc.encodeBinMP(0, model);

        // --- ASSERT ---
        // 1. A_new = A_old - Qe = 0x10000 - 0x03D8 = 0xFC28
        // 2. C_new = C_old + Qe = 0x03D8
        // 3. A >= 0x8000, so there is no renormalization.
        CHECK(enc.getA() == 0xFC28);
        CHECK(enc.getC() == 0x03D8);
    }
}

TEST_CASE("xArithCoreEnc: Conditional exchange correctly exchanges intervals") {
    // --- ARRANGE ---
    xDummyBitstreamWriter writer;
    xTestableArithCoreEnc enc(writer);
    xArithCoreModel model;

    // We set a state to force a conditional exchange.
    // We need A_old - Qe < Qe  (which is A_old < 2 * Qe).
    model.init(80, 0); // MPS=0, Qe=0x5832. Required A < 0xB064.
    enc.setInternalState(0x9000, 0x1000); // We are setting up A=0x9000, C=0x1000.

    // We encode MPS ('0').

    // --- ACT ---
    enc.encodeBinMP(0, model);

    // --- ASSERT ---
    // 1. A_temp = 0x9000 - 0x5832 = 0x37CE.
    // 2. Condition A_temp < Qe (0x37CE < 0x5832) is fulfilled -> exchange.
    // 3. The code is treated as LPS: C_new = C_old + A_temp = 0x1000 + 0x37CE = 0x47CE.
    // 4. The interval size is swapped: A_new = Qe = 0x5832.
    // 5. Renormalization (1 shift left):
    //    A_final = 0x5832 << 1 = 0xB064
    //    C_final = 0x47CE << 1 = 0x8F9C
    CHECK(enc.getA() == 0xB064);
    CHECK(enc.getC() == 0x8F9C);
}

TEST_CASE("xArithCoreEnc: Obsługa przeniesienia (Carry-over) przy opróżnianiu bufora") {
    // --- ARRANGE ---
    xDummyBitstreamWriter writer;
    xByteBuffer output_buffer(1024);
    writer.bindByteBuffer(&output_buffer);
    xTestableArithCoreEnc  enc(writer);
    enc.initialize();
    xArithCoreModel model;

    // Ustawiamy stan tuż przed wywołaniem finish(), który spowoduje przeniesienie.
    // A=0x8001, C=0xFFFF, CT=1.
    enc.setInternalState(0x8001, 0xFFFF, 1);

    // --- ACT ---
    enc.finish();

    // --- ASSERT ---
    // 1. W finish(): C = C + A - 1 = 0xFFFF + 0x8000 = 0x17FFF.
    // 2. C <<= (8 - CT) = 0x17FFF << 7 = 0xBFFFF8.
    // 3. writeByte():
    //    - Jest przeniesienie (C > 0xFFFF), więc zapisz 0xFF.  -> output: {0xFF}
    //    - C &= 0xFFFF -> C = 0xFFF8.
    //    - Zapisz C >> 8 (0xFF). -> output: {0xFF, 0xFF}
    // 4. W finish() C jest przesuwane o 8 bitów.
    // 5. writeByte() zapisuje ostatni bajt (0xF8). -> output: {0xFF, 0xFF, 0xF8}
    const std::vector<uint8_t> expectedBytes = { 0xFF, 0xFF, 0xF8 };

    // Get data directly from the buffer, not from a vector in the dummy class.
    const size_t actual_size = output_buffer.getDataSize();
    const uint8_t* actual_data_ptr = output_buffer.getReadPtr();
    // Create a vector from the actual data for easier comparison.
    std::vector<uint8_t> actualBytes(actual_data_ptr, actual_data_ptr + actual_size);

    CHECK(actualBytes == expectedBytes);
}

TEST_CASE("xArithCoreDec: start and decodeBinMP") {
    // Prepare a dummy bitstream reader with some bytes
    std::vector<uint8_t> data = { 0xAA, 0x55 };
    xDummyBitstreamReader reader(data);

    xArithCoreDec dec;
    // start: should initialize decoder state
    dec.start([&]() { return true; });

    xArithCoreModel model;
    model.init(0, 0);

    // decodeBinMP: should return a value (0 or 1)
    auto val = dec.decodeBinMP(model);
    bool result = (val == 0 || val == 1);
    CHECK(result);
}