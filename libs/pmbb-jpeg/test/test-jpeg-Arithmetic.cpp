/*
    SPDX-FileCopyrightText: 2019-2023 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <cstdint> // Required for uint32_t
#include <vector>
#include <array>
namespace PMBB_NAMESPACE {
    //===============================================================================================================================================================================================================
    // Definitions of helper classes (Mocks/Dummies) for testing purposes
    //===============================================================================================================================================================================================================

    // Minimal base class definition to enable compilation.
    class xBitstreamWriter
    {
    public:
        virtual ~xBitstreamWriter() = default;
        // In the future, virtual methods may be added here to track calls.
        virtual void writeByte(uint8_t byte) = 0;
    };

    // An empty (dummy) implementation of xBitstreamWriter, needed for the constructor of the class under test.
    class xDummyBitstreamWriter : public xBitstreamWriter {};

    // Mock xBitstreamWriter, which stores the written bytes in a vector.
    class xMockBitstreamWriter : public xBitstreamWriter {
    public:
        std::vector<uint8_t> m_WrittenBytes;
        void writeByte(uint8_t byte) override {
            m_WrittenBytes.push_back(byte);
        }
    };

    // Minimal definition of a common base class for the arithmetic core.
    class xArithCoreCommon
    {
    public:
        virtual ~xArithCoreCommon() = default;
    };

    // Implementation of the Qe model (probability table) as a singleton.
    class xQeModel
    {
    public:
        struct Entry { uint16_t m_Qe; uint8_t m_nextMPS; uint8_t m_nextLPS; bool m_switchMPS; };
    private:
        static constexpr uint8_t c_NumStates = 47;
        const std::array<Entry, c_NumStates> m_QeTable = { {
            {0x59EB, 1, 1, true}, {0x5522, 2, 6, false}, {0x504F, 3, 9, false}, {0x4B85, 4, 12, false},
            {0x4639, 5, 14, false}, {0x415E, 29, 17, false}, {0x3CDE, 7, 18, false}, {0x3821, 8, 20, false},
            {0x338A, 9, 22, false}, {0x2F30, 10, 24, false}, {0x2B15, 11, 25, false}, {0x274A, 12, 27, false},
            {0x23B9, 13, 28, false}, {0x206C, 29, 30, false}, {0x1D6B, 15, 31, false}, {0x1A9D, 16, 33, false},
            {0x1807, 17, 34, false}, {0x15A4, 18, 35, false}, {0x136D, 19, 36, false}, {0x1161, 20, 37, false},
            {0x0F8B, 21, 38, false}, {0x0DD4, 22, 39, false}, {0x0C41, 23, 40, false}, {0x0AC9, 24, 41, false},
            {0x0972, 25, 42, false}, {0x083F, 26, 43, false}, {0x0728, 27, 44, false}, {0x0631, 28, 45, false},
            {0x0551, 29, 45, false}, {0x048B, 30, 46, false}, {0x03D8, 31, 46, false}, {0x0334, 32, 46, false},
            {0x02A0, 33, 46, false}, {0x0218, 34, 46, false}, {0x019A, 35, 46, false}, {0x012A, 36, 46, false},
            {0x00C1, 37, 46, false}, {0x0061, 38, 46, false}, {0x0031, 39, 46, false}, {0x0018, 40, 46, false},
            {0x0008, 41, 46, false}, {0x0004, 42, 46, false}, {0x0002, 43, 46, false}, {0x0001, 44, 46, false},
            {0x59EB, 45, 45, false}, {0x59EB, 46, 46, false}
        } };
        xQeModel() = default;

    public:
        static const xQeModel& getInstance() { static const xQeModel instance; return instance; }
        const Entry& getEntry(uint8_t ProbIndex) const { return m_QeTable[ProbIndex]; }
    };

    // Context model implementation.
    class xArithCoreModel
    {
    private:
        uint8_t m_ProbIndex = 0;
        uint8_t m_MPS = 0;

    public:
        void init(uint8_t ProbIndex, uint8_t MPS) { m_ProbIndex = ProbIndex; m_MPS = MPS; }
        uint8_t getProbIndex() const { return m_ProbIndex; }
        uint8_t getMPS() const { return m_MPS; }

        void update(bool is_mps)
        {
            const auto& entry = xQeModel::getInstance().getEntry(m_ProbIndex);
            if (is_mps)
            {
                m_ProbIndex = entry.m_nextMPS;
            }
            else
            {
                m_ProbIndex = entry.m_nextLPS;
                if (entry.m_switchMPS)
                {
                    m_MPS ^= 1;
                }
            }
        }
    };

    //===============================================================================================================================================================================================================
    // Tested class
    //===============================================================================================================================================================================================================

    class xArithCoreEnc : public xArithCoreCommon
    {
    public:
        xArithCoreEnc(xBitstreamWriter& bitstream) : m_Bitstream(bitstream) {}
        void initialize();
        void encodeBinMP(uint32_t BinValue, xArithCoreModel& CtxModel);

    protected:
        void renormalize();
        void writeByte();

        xBitstreamWriter& m_Bitstream;
        // Registers A and C should be 32-bit to correctly store the value 0x10000.
        uint32_t m_A = 0;
        uint32_t m_C = 0;
        int32_t  m_CT = 12;
    };

    void xArithCoreEnc::initialize()
    {
        m_A = 0x10000;
        m_C = 0;
        m_CT = 12;
    }

    void xArithCoreEnc::encodeBinMP(uint32_t BinValue, xArithCoreModel& CtxModel) {
        uint8_t  ProbIndex = CtxModel.getProbIndex();
        uint8_t  MPS = CtxModel.getMPS();
        const auto& QeEntry = xQeModel::getInstance().getEntry(ProbIndex);
        uint16_t Qe = QeEntry.m_Qe;

        m_A -= Qe;
        bool is_mps = (BinValue == MPS);

        if (is_mps)
        {
            if (m_A < 0x8000)
            {
                if (m_A < Qe)
                {
                    m_C += m_A;
                    m_A = Qe;
                }
                renormalize();
            }
        }
        else
        {
            m_C += m_A;
            m_A = Qe;
            renormalize();
        }
        CtxModel.update(is_mps);
    }

    void xArithCoreEnc::renormalize()
    {
        do {
            m_A <<= 1;
            m_C <<= 1;
            m_CT--;
            if (m_CT == 0)
            {
                writeByte();
            }
        } while (m_A < 0x8000);
    }

    void xArithCoreEnc::writeByte() {
        if (m_C > 0xFFFF) {
            m_Bitstream.writeByte(0xFF);
            m_C &= 0xFFFF;
        }
        m_Bitstream.writeByte(m_C >> 8);
        m_C &= 0xFF;
        m_CT = 8;
    }

    //===============================================================================================================================================================================================================
    // A friend class for testing, allowing access to protected members.
    //===============================================================================================================================================================================================================
    class xArithCoreEnc_Test : public xArithCoreEnc
    {
    public:
        // Inheriting a constructor from a base class
        xArithCoreEnc_Test(xBitstreamWriter& bitstream) : xArithCoreEnc(bitstream) {}

        // Public methods (getters) for reading the values ​​of protected fields m_A and m_C
        uint32_t getA() const { return m_A; }
        uint32_t getC() const { return m_C; }
        int32_t  getCT() const { return m_CT; }

        void setState(uint32_t A, uint32_t C, int32_t CT) { m_A = A; m_C = C; m_CT = CT; }
    };


    //===============================================================================================================================================================================================================
    // Unit tests
    //===============================================================================================================================================================================================================

    TEST_CASE("xArithCoreEnc - Procedure Initenc (initialize)")
    {
        // --- Arrange ---

        // We create a dummy object of the bit stream.
        //xDummyBitstreamWriter dummyBitstream;
        // We create an instance of the test class, passing a dummy to it.
        //xArithCoreEnc_Test testEncoder(dummyBitstream);
        xMockBitstreamWriter mockBitstream;
        xArithCoreEnc_Test testEncoder(mockBitstream);

        // --- Act ---

        // We call the tested method - encoder initialization.
        testEncoder.initialize();

        // --- Assert ---

        // We check whether the registers have been set to the correct initial values.
        // According to the standard, register A (interval) is initialized to 0x10000.
        CHECK(testEncoder.getA() == 0x10000);
        // Register C (lower bound of the range) is initialized to 0.
        CHECK(testEncoder.getC() == 0);
        CHECK(testEncoder.getCT() == 12);
    }

    TEST_CASE("xArithCoreEnc - encodeBinMP (Code_0 and Code_1)")
    {
        xMockBitstreamWriter mockBitstream;
        xArithCoreEnc_Test testEncoder(mockBitstream);
        xArithCoreModel ctxModel;

        SUBCASE("Encoding MPS (Most Probable Symbol) without renormalization")
        {
            // Arrange
            ctxModel.init(0, 0); // p_idx=0 (Qe=0x59EB), MPS=0
            testEncoder.initialize(); // A=0x10000, C=0, CT=12

            // Act
            testEncoder.encodeBinMP(0, ctxModel); // Encode BinValue=0 (which is MPS)

            // Assert
            // A = 0x10000 - 0x59EB = 0xA615
            // A (0xA615) >= 0x8000, so no renormalization
            // C remains 0
            CHECK(testEncoder.getA() == 0xA615);
            CHECK(testEncoder.getC() == 0);
            CHECK(testEncoder.getCT() == 12);

            // Model update for MPS: p_idx becomes nextMPS for index 0, which is 1
            CHECK(ctxModel.getProbIndex() == 1);
            CHECK(ctxModel.getMPS() == 0); // MPS doesn't switch
            CHECK(mockBitstream.m_WrittenBytes.empty());
        }

        SUBCASE("Encoding LPS (Less Probable Symbol) with renormalization")
        {
            // Arrange
            ctxModel.init(0, 0); // p_idx=0 (Qe=0x59EB), MPS=0
            testEncoder.initialize(); // A=0x10000, C=0, CT=12

            // Act
            testEncoder.encodeBinMP(1, ctxModel); // Encode BinValue=1 (which is LPS)

            // Assert
            // LPS path:
            // C = C + A = 0 + 0x10000 = 0x10000
            // A = Qe = 0x59EB
            // Renormalization is triggered because A (0x59EB) < 0x8000
            // do-while loop runs once:
            // A = 0x59EB << 1 = 0xB3D6
            // C = 0x10000 << 1 = 0x20000
            // CT = 12 - 1 = 11
            // Loop terminates as A (0xB3D6) >= 0x8000
            CHECK(testEncoder.getA() == 0xB3D6);
            CHECK(testEncoder.getC() == 0x20000);
            CHECK(testEncoder.getCT() == 11);

            // Model update for LPS: p_idx becomes nextLPS for index 0, which is 1
            // switchMPS is true, so MPS flips from 0 to 1
            CHECK(ctxModel.getProbIndex() == 1);
            CHECK(ctxModel.getMPS() == 1);
            CHECK(mockBitstream.m_WrittenBytes.empty()); // No byte written yet
        }

        SUBCASE("Encoding LPS that triggers writeByte")
        {
            // Arrange
            ctxModel.init(46, 0); // p_idx=46 (Qe=0x59EB), MPS=0
            // Set a state where renormalization will cause CT to become 0
            testEncoder.setState(0x8000, 0, 4); // A is just at the limit, C=0, CT=4

            // Act: Encode LPS, which will reduce A and start renormalization
            testEncoder.encodeBinMP(1, ctxModel); // Encode BinValue=1 (LPS)

            // Assert
            // Initial: A=0x8000, C=0, CT=4
            // A = A - Qe = 0x8000 - 0x59EB = 0x2615
            // This is MPS path, let's trace LPS
            // C = C + A = 0 + 0x8000 = 0x8000
            // A = Qe = 0x59EB
            // Renormalize: A < 0x8000
            // 1. A=0xB3D6, C=0x10000, CT=3
            // Now A >= 0x8000, loop ends.
            // Let's force more loops with a smaller Qe.

            // --- Re-arranging for a better test case ---
            ctxModel.init(20, 0); // p_idx=20 (Qe=0x0F8B), MPS=0
            testEncoder.setState(0x8000, 0x1234, 3); // A=0x8000, C=0x1234, CT=3

            // Act
            testEncoder.encodeBinMP(1, ctxModel); // Encode LPS

            // Trace LPS path:
            // C = C + A = 0x1234 + 0x8000 = 0x9234
            // A = Qe = 0x0F8B
            // Renormalize, A < 0x8000. Loop will run 3 times.
            // 1. A=0x1F16, C=0x12468, CT=2
            // 2. A=0x3E2C, C=0x248D0, CT=1
            // 3. A=0x7C58, C=0x491A0, CT=0. writeByte() is called!
            //    writeByte(): C > 0xFFFF is true.
            //    writes 0xFF. C becomes 0x91A0.
            //    writes C >> 8 = 0x91. C becomes 0xA0.
            //    CT becomes 8.
            // 4. A=0xF8B0, C=0x140, CT=7. Loop terminates.

            CHECK(testEncoder.getA() == 0xF8B0);
            CHECK(testEncoder.getC() == 0x140);
            CHECK(testEncoder.getCT() == 7);

            REQUIRE(mockBitstream.m_WrittenBytes.size() == 2);
            CHECK(mockBitstream.m_WrittenBytes[0] == 0xFF);
            CHECK(mockBitstream.m_WrittenBytes[1] == 0x91);
        }
    }
}