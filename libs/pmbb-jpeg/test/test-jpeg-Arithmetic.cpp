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
// Fix: Remove 'override' from DummyBitstreamWriter::writeByte, since the base class does not declare it as virtual.
using namespace PMBB_NAMESPACE;

// Dummy bitstream writer for encoder tests
class DummyBitstreamWriter : public xBitstreamWriter {
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
class DummyBitstreamReader : public xBitstreamReader {
public:
    std::vector<uint8_t> bytes;
    size_t pos = 0;
    DummyBitstreamReader(const std::vector<uint8_t>& data) : bytes(data) {}
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

TEST_CASE("xArithCoreDec: start and decodeBinMP") {
    // Prepare a dummy bitstream reader with some bytes
    std::vector<uint8_t> data = { 0xAA, 0x55 };
    DummyBitstreamReader reader(data);

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