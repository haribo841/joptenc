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
    const std::array<StateEntry, xQeModel::getNumStates()> xQeModel::STATIC_STATE_TABLE =
    {
        {
            // Qe_Value, Next_LPS, Next_MPS, Switch_MPS, Index
            {0x5a1d,   1,   1, 1 }, //   0
            {0x2586,  14,   2, 0 }, //   1
            {0x1114,  16,   3, 0 }, //   2
            {0x080b,  18,   4, 0 }, //   3
            {0x03d8,  20,   5, 0 }, //   4
            {0x01da,  23,   6, 0 }, //   5
            {0x00e5,  25,   7, 0 }, //   6
            {0x006f,  28,   8, 0 }, //   7
            {0x0036,  30,   9, 0 }, //   8
            {0x001a,  33,  10, 0 }, //   9
            {0x000d,  35,  11, 0 }, //  10
            {0x0006,   9,  12, 0 }, //  11
            {0x0003,  10,  13, 0 }, //  12
            {0x0001,  12,  13, 0 }, //  13
            {0x5a7f,  15,  15, 1 }, //  14
            {0x3f25,  36,  16, 0 }, //  15
            {0x2cf2,  38,  17, 0 }, //  16
            {0x207c,  39,  18, 0 }, //  17
            {0x17b9,  40,  19, 0 }, //  18
            {0x1182,  42,  20, 0 }, //  19
            {0x0cef,  43,  21, 0 }, //  20
            {0x09a1,  45,  22, 0 }, //  21
            {0x072f,  46,  23, 0 }, //  22
            {0x055c,  48,  24, 0 }, //  23
            {0x0406,  49,  25, 0 }, //  24
            {0x0303,  51,  26, 0 }, //  25
            {0x0240,  52,  27, 0 }, //  26
            {0x01b1,  54,  28, 0 }, //  27
            {0x0144,  56,  29, 0 }, //  28
            {0x00f5,  57,  30, 0 }, //  29
            {0x00b7,  59,  31, 0 }, //  30
            {0x008a,  60,  32, 0 }, //  31
            {0x0068,  62,  33, 0 }, //  32
            {0x004e,  63,  34, 0 }, //  33
            {0x003b,  32,  35, 0 }, //  34
            {0x002c,  33,   9, 0 }, //  35
            {0x5ae1,  37,  37, 1 }, //  36
            {0x484c,  64,  38, 0 }, //  37
            {0x3a0d,  65,  39, 0 }, //  38
            {0x2ef1,  67,  40, 0 }, //  39
            {0x261f,  68,  41, 0 }, //  40
            {0x1f33,  69,  42, 0 }, //  41
            {0x19a8,  70,  43, 0 }, //  42
            {0x1518,  72,  44, 0 }, //  43
            {0x1177,  73,  45, 0 }, //  44
            {0x0e74,  74,  46, 0 }, //  45
            {0x0bfb,  75,  47, 0 }, //  46
            {0x09f8,  77,  48, 0 }, //  47
            {0x0861,  78,  49, 0 }, //  48
            {0x0706,  79,  50, 0 }, //  49
            {0x05cd,  48,  51, 0 }, //  50
            {0x04de,  50,  52, 0 }, //  51
            {0x040f,  50,  53, 0 }, //  52
            {0x0363,  51,  54, 0 }, //  53
            {0x02d4,  52,  55, 0 }, //  54
            {0x025c,  53,  56, 0 }, //  55
            {0x01f8,  54,  57, 0 }, //  56next page
            {0x01a4,  55,  58, 0 }, //  57
            {0x0160,  56,  59, 0 }, //  58
            {0x0125,  57,  60, 0 }, //  59
            {0x00f6,  58,  61, 0 }, //  60
            {0x00cb,  59,  62, 0 }, //  61
            {0x00ab,  61,  63, 0 }, //  62
            {0x008f,  61,  32, 0 }, //  63
            {0x5b12,  65,  65, 1 }, //  64
            {0x4d04,  80,  66, 0 }, //  65
            {0x412c,  81,  67, 0 }, //  66
            {0x37d8,  82,  68, 0 }, //  67
            {0x2fe8,  83,  69, 0 }, //  68
            {0x293c,  84,  70, 0 }, //  69
            {0x2379,  86,  71, 0 }, //  70
            {0x1edf,  87,  72, 0 }, //  71
            {0x1aa9,  87,  73, 0 }, //  72
            {0x174e,  72,  74, 0 }, //  73
            {0x1424,  72,  75, 0 }, //  74
            {0x119c,  74,  76, 0 }, //  75
            {0x0f6b,  74,  77, 0 }, //  76
            {0x0d51,  75,  78, 0 }, //  77
            {0x0bb6,  77,  79, 0 }, //  78
            {0x0a40,  77,  48, 0 }, //  79
            {0x5832,  80,  81, 1 }, //  80
            {0x4d1c,  88,  82, 0 }, //  81
            {0x438e,  89,  83, 0 }, //  82
            {0x3bdd,  90,  84, 0 }, //  83
            {0x34ee,  91,  85, 0 }, //  84
            {0x2eae,  92,  86, 0 }, //  85
            {0x299a,  93,  87, 0 }, //  86
            {0x2516,  86,  71, 0 }, //  87
            {0x5570,  88,  89, 1 }, //  88
            {0x4ca9,  95,  90, 0 }, //  89
            {0x44d9,  96,  91, 0 }, //  90
            {0x3e22,  97,  92, 0 }, //  91
            {0x3824,  99,  93, 0 }, //  92
            {0x32b4,  99,  94, 0 }, //  93
            {0x2e17,  93,  86, 0 }, //  94
            {0x56a8,  95,  96, 1 }, //  95
            {0x4f46, 101,  97, 0 }, //  96
            {0x47e5, 102,  98, 0 }, //  97
            {0x41cf, 103,  99, 0 }, //  98
            {0x3c3d, 104, 100, 0 }, //  99
            {0x375e,  99,  93, 0 }, // 100
            {0x5231, 105, 102, 0 }, // 101
            {0x4c0f, 106, 103, 0 }, // 102
            {0x4639, 107, 104, 0 }, // 103
            {0x415e, 103,  99, 0 }, // 104
            {0x5627, 105, 106, 1 }, // 105
            {0x50e7, 108, 107, 0 }, // 106
            {0x4b85, 109, 103, 0 }, // 107
            {0x5597, 110, 109, 0 }, // 108
            {0x504f, 111, 107, 0 }, // 109
            {0x5a10, 110, 111, 1 }, // 110
            {0x5522, 112, 109, 0 }, // 111
            {0x59eb, 112, 111, 1 }, // 112
        }
    };

    xQeModel& xQeModel::getInstance()
    {
        static xQeModel instance;
        return instance;
    }

    uint16_t xArithCoreModel::getQe() const {
        return xQeModel::getInstance().getEntry(m_ProbIndex).m_Qe;
    }

    void xArithCoreModel::update(bool is_mps)
    {
        if (is_mps)
        {
            m_ProbIndex = xQeModel::STATIC_STATE_TABLE[m_ProbIndex].m_nextMPS;
        }
        else // is LPS
        {
            m_ProbIndex = xQeModel::STATIC_STATE_TABLE[m_ProbIndex].m_nextLPS;
        }
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
        //m_Bitstream.bindByteBuffer(buffer);
    }

    void xArithCoreDec::finish() {
        // TODO: implementation
    }

    //=============================================================================================================================================================================
    //=====================================================================================================================================================================================
    //end of namespace PMBB::JPEG
}
