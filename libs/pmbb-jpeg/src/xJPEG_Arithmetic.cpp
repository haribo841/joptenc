/*
    SPDX-FileCopyrightText: 2020-2024 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
    */
#include "xJPEG_Arithmetic.h"
#include <stdexcept>
namespace PMBB_NAMESPACE::JPEG {
    std::vector<bool> hexToBitVector(const std::string& hexString) {
        std::vector<bool> bitVector;
        for (char hexDigit : hexString) {
            uint8_t value = 0;
            if (hexDigit >= '0' && hexDigit <= '9') {
                value = hexDigit - '0';
            }
            else if (hexDigit >= 'A' && hexDigit <= 'F') {
                value = hexDigit - 'A' + 10;
            }
            else if (hexDigit >= 'a' && hexDigit <= 'f') {
                value = hexDigit - 'a' + 10;
            }
            for (int i = 3; i >= 0; --i) {
                bitVector.push_back((value >> i) & 1);
            }
        }
        return bitVector;
    }
    // Table D.3 – Qe values and probability estimation state machine
    const std::array<StateEntry, xQeModel::getNumStates()> STATIC_STATE_TABLE =
    {
        {
            // Index,  Qe_Value, Next_LPS, Next_MPS, Switch_MPS
            {0x5A1D, 1, 1, true},      // 0
            {0x2586, 14, 2, false},    // 1
            {0x1114, 16, 3, false},    // 2
            {0x080B, 18, 4, false},    // 3
            {0x03D8, 20, 5, false},    // 4
            {0x01DA, 23, 6, false},    // 5
            {0x00E5, 25, 7, false},    // 6
            {0x006F, 28, 8, false},    // 7
            {0x0036, 30, 9, false},    // 8
            {0x001A, 33, 10, false},   // 9
            {0x000D, 35, 11, false},   // 10
            {0x0006, 9, 12, false},    // 11
            {0x0003, 10, 13, false},   // 12
            {0x0001, 12, 13, false},   // 13
            {0x5A7F, 15, 15, true},    // 14
            {0x3F25, 36, 16, false},   // 15
            {0x2CF2, 38, 17, false},   // 16
            {0x207C, 39, 18, false},   // 17
            {0x17B9, 40, 19, false},   // 18
            {0x1182, 42, 20, false},   // 19
            {0x0CEF, 43, 21, false},   // 20
            {0x09A1, 45, 22, false},   // 21
            {0x072F, 46, 23, false},   // 22
            {0x055C, 48, 24, false},   // 23
            {0x0406, 49, 25, false},   // 24
            {0x0303, 51, 26, false},   // 25
            {0x0240, 52, 27, false},   // 26
            {0x01B1, 54, 28, false},   // 27
            {0x0144, 56, 29, false},   // 28
            {0x00F5, 57, 30, false},   // 29
            {0x00B7, 59, 31, false},   // 30
            {0x008A, 60, 32, false},   // 31
            {0x0068, 62, 33, false},   // 32
            {0x004E, 63, 34, false},   // 33
            {0x003B, 32, 35, false},   // 34
            {0x002C, 33, 9, false},    // 35
            {0x5AE1, 37, 37, false},   // 36
            {0x484C, 64, 38, true},    // 37
            {0x3A0D, 65, 39, false},   // 38
            {0x2EF1, 67, 40, false},   // 39
            {0x261F, 68, 41, false},   // 40
            {0x1F33, 69, 42, false},   // 41
            {0x19A8, 70, 43, false},   // 42
            {0x1518, 72, 44, false},   // 43
            {0x1177, 73, 45, false},   // 44
            {0x0E74, 74, 46, false},   // 45
            {0x0BFB, 75, 47, false},   // 46
            {0x09F8, 77, 48, false},   // 47
            {0x0861, 78, 49, false},   // 48
            {0x0706, 79, 50, false},   // 49
            {0x05CD, 48, 51, false},   // 50
            {0x04DE, 50, 52, false},   // 51
            {0x040F, 50, 53, false},   // 52
            {0x0363, 51, 54, false},   // 53
            {0x02D4, 52, 55, false},   // 54
            {0x025C, 53, 56, false},   // 55
            {0x01F8, 54, 57, false},   // 56 next page
            {0x01A4, 55, 58, false},   // 57
            {0x0160, 56, 59, false},   // 58
            {0x0125, 57, 60, false},   // 59
            {0x00F6, 58, 61, false},   // 60
            {0x00CB, 59, 62, false},   // 61
            {0x00AB, 61, 63, false},   // 62
            {0x008F, 61, 32, false},   // 63
            {0x5B12, 65, 65, true},    // 64
            {0x4D04, 80, 66, false},   // 65
            {0x412C, 81, 67, false},   // 66
            {0x37D8, 82, 68, false},   // 67
            {0x2FE8, 83, 69, false},   // 68
            {0x293C, 84, 70, false},   // 69
            {0x2379, 86, 71, false},   // 70
            {0x1EDF, 87, 72, false},   // 71
            {0x1AA9, 87, 73, false},   // 72
            {0x174E, 72, 74, false},   // 73
            {0x1424, 72, 75, false},   // 74
            {0x119C, 74, 76, false},   // 75
            {0x0F6B, 74, 77, false},   // 76
            {0x0D51, 75, 78, false},   // 77
            {0x0BB6, 77, 79, false},   // 78
            {0x0A40, 77, 48, false},   // 79
            {0x5832, 80, 81, true},    // 80
            {0x4D1C, 88, 82, false},   // 81
            {0x438E, 89, 83, false},   // 82
            {0x3BDD, 90, 84, false},   // 83
            {0x34EE, 91, 85, false},   // 84
            {0x2EAE, 92, 86, false},   // 85
            {0x299A, 93, 87, false},   // 86
            {0x2516, 86, 71, false},   // 87
            {0x5570, 88, 89, true},    // 88
            {0x4CA9, 95, 90, false},   // 89
            {0x44D9, 96, 91, false},   // 90
            {0x3E22, 97, 92, false},   // 91
            {0x3824, 99, 93, false},   // 92
            {0x32B4, 99, 94, false},   // 93
            {0x2E17, 93, 96, false},   // 94
            {0x56A8, 95, 96, true},    // 95
            {0x4F46, 101, 97, false},  // 96
            {0x47E5, 102, 98, false},  // 97
            {0x41CF, 103, 99, false},  // 98
            {0x3C3D, 104, 100, false}, // 99
            {0x3787, 99,93, false},    // 100
            {0x5231, 105, 102, false}, // 101
            {0x4C0F, 106, 103, false}, // 102
            {0x4639, 107, 104, false}, // 103
            {0x415E, 103, 99, false},  // 104
            {0x5627, 105, 106, true},  // 105
            {0x50E7, 108, 107, false}, // 106
            {0x4B85, 109, 103, false}, // 107
            {0x5597, 110, 109, false}, // 108
            {0x504F, 111, 107, false}, // 109
            {0x5A10, 110, 111, true},  // 110
            {0x5522, 112, 109, false}, // 111
            {0x59EB, 112, 111, true}   // 112
        }
    };

    const std::array<uint32_t, 113> xArithCoreCommon::c_Qe = {
            0x5A1D,
            0x2586,
            0x1114,
            0x080B,
            0x03D8,
            0x01DA,
            0x00E5,
            0x006F,
            0x0036,
            0x001A,
            0x000D,
            0x0006,
            0x0003,
            0x0001,
            0x5A7F,
            0x3F25,
            0x2CF2,
            0x207C,
            0x17B9,
            0x1182,
            0x0CEF,
            0x09A1,
            0x072F,
            0x055C,
            0x0406,
            0x0303,
            0x0240,
            0x01B1,
            0x0144,
            0x00F5,
            0x00B7,
            0x008A,
            0x0068,
            0x004E,
            0x003B,
            0x002C,
            0x5AE1,
            0x484C,
            0x3A0D,
            0x2EF1,
            0x261F,
            0x1F33,
            0x19A8,
            0x1518,
            0x1177,
            0x0E74,
            0x0BFB,
            0x09F8,
            0x0861,
            0x0706,
            0x05CD,
            0x04DE,
            0x040F,
            0x0363,
            0x02D4,
            0x025C,
            0x01F8,
            0x01A4,
            0x0160,
            0x0125,
            0x00F6,
            0x00CB,
            0x00AB,
            0x008F,
            0x5B12,
            0x4D04,
            0x412C,
            0x37D8,
            0x2FE8,
            0x293C,
            0x2379,
            0x1EDF,
            0x1AA9,
            0x174E,
            0x1424,
            0x119C,
            0x0F6B,
            0x0D51,
            0x0BB6,
            0x0A40,
            0x5832,
            0x4D1C,
            0x438E,
            0x3BDD,
            0x34EE,
            0x2EAE,
            0x299A,
            0x2516,
            0x5570,
            0x4CA9,
            0x44D9,
            0x3E22,
            0x3824,
            0x32B4,
            0x2E17,
            0x56A8,
            0x4F46,
            0x47E5,
            0x41CF,
            0x3C3D,
            0x3787,
            0x5231,
            0x4C0F,
            0x4639,
            0x415E,
            0x5627,
            0x50E7,
            0x4B85,
            0x5597,
            0x504F,
            0x5A10,
            0x5522,
            0x59EB
    };

    uint16_t xArithCoreModel::getQe() const {
        return STATIC_STATE_TABLE[m_ProbIndex].m_Qe;
    }

    void xArithCoreModel::update(bool is_mps)
    {
        if (is_mps)
        {
            updateMPS();
        }
        else
        {
            updateLPS();
        }
    }

    void xArithCoreModel::updateMPS() {
        m_ProbIndex = STATIC_STATE_TABLE[m_ProbIndex].m_nextMPS;
    }

    void xArithCoreModel::updateLPS() {
        if (STATIC_STATE_TABLE[m_ProbIndex].m_switchMPS) {
            m_MPS = 1 - m_MPS;
        }
        m_ProbIndex = STATIC_STATE_TABLE[m_ProbIndex].m_nextLPS;
    }

    // context-index S is determined by the statistical model and is, in general, a function of the previous coding decisions
    // each value of S identifies a particular conditional probability estimate which is used in encoding the binary decision
//=====================================================================================================================================================================================
// layer 1 - arithmetic codec core - Annex D        
//=============================================================================================================================================================================

    void xArithCoreDec::start() {
        // TODO: implementation
    }

    void xArithCoreDec::start(std::function<bool(void)> const& callback) {
        // TODO: implementation
    }

    unsigned int xArithCoreDec::decodeBinMP(xArithCoreModel const& model) {
        // TODO: implementation
        return 0;
    }

    void xArithCoreDec::setByteBuffer(xByteBuffer* buffer) {
        m_Bitstream.bindByteBuffer(buffer);
    }

    void xArithCoreDec::finish() {
        // TODO: implementation
    }

//=============================================================================================================================================================================

    xQeModel::xQeModel() : state_table(STATIC_STATE_TABLE) {}

    xQeModel& xQeModel::getInstance() {
        static xQeModel instance;
        return instance;
    }

    const StateEntry& xQeModel::getEntry(size_t index) const {
        if (index >= NUM_STATES) {
            throw std::out_of_range("Index sis out of range (0-112).");
        }
        return state_table[index];
    }
//=====================================================================================================================================================================================
//end of namespace PMBB::JPEG
}
