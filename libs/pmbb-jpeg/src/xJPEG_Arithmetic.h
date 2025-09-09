/*
    SPDX-FileCopyrightText: 2020-2024 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
    */
#pragma once
#include "xCommonDefJPEG.h"
#include "xBitstream.h"
#include <array>   // For std::array
#include <cstdint> // For fixed-width types, e.g. uint16_t, uint8_t
#include <functional> // For std::function
#include <map>
#include <numeric>   // For std::accumulate
#include <stdexcept> // For std::out_of_range
#include <queue>
#include <vector>
#include "xJFIF.h"
using namespace PMBB_NAMESPACE::JPEG;

#define X_PMBB_JPEG_MULTI_LEVEL_LOOKAHEAD 0

namespace PMBB_NAMESPACE::JPEG {

    // A function to convert a hexadecimal sequence to a bit vector.
    std::vector<bool> hexToBitVector(const std::string& hexString);

    //=====================================================================================================================================================================================
    // layer 0 - probability model

        // Table D.3 – Qe values and probability estimation state machine
    struct StateEntry
    {
        uint16_t    m_Qe;        // The probability estimate value for Qe
        uint8_t     m_nextLPS;   // Index of the next state after LPS encoding
        uint8_t     m_nextMPS;   // Index of the next state after MPS encoding
        bool        m_switchMPS; // Flag indicating whether MPS/LPS sense should be swapped
    };

    class xArithCoreModel
    {
    public:
        // The statistics areas are initialized to an MPS sense of 0 and to a Qe index of 0 as defined by Table D.3
        xArithCoreModel() : m_ProbIndex(0), m_MPS(0) {}
        void init(uint8_t init_index = 0, uint8_t init_mps = 0)
        {
            m_ProbIndex = init_index;
            m_MPS = init_mps;
        }

        uint8_t getProbIndex() const { return m_ProbIndex; }
        uint8_t getMPS() const { return m_MPS; }
        uint16_t getQe() const;
        void updateMPS();
        void updateLPS();

        // Methods for updating the model that now only accept a symbol(LPS or MPS)
        void update(bool is_mps);

    private:
        uint8_t m_ProbIndex; // Current index in the probability table
        uint8_t m_MPS;      // Current Most Probable Symbol (MPS) sense (0 or 1)
    };

    //=====================================================================================================================================================================================
    // layer 1 - arithmetic codec core - Annex D

    // Static accessor for Qe values, matching the legacy c_Qe[] interface
    class xArithCoreCommon
    {
    public:
        // Legacy Qe table for compatibility with code expecting c_Qe
        static const std::array<uint32_t, 113> c_Qe;

        uint32_t getA() const { return m_A; }
        uint32_t getC() const { return m_C; }
        int32_t  getCT() const { return m_CT; }
        uint32_t getST() const { return m_ST; }

    protected:
        xByteBuffer* m_ByteBuffer = nullptr; // it should be a pointer
        // MSB                                LSB
        uint32_t m_A;               // 00000000, 00000000, aaaaaaaa, aaaaaaaa
        uint32_t m_C;               // 0000cbbb, bbbbbsss, xxxxxxxx, xxxxxxxx
        // a - fractional bits in the A-register (the current probability interval value)
        // b - indicate the bit positions from which the completed bytes of data are removed from the C-register
        // c - carry bit
        // s - optional spacer bits which provide useful constraints on carry-over
        // x - fractional bits in the code register
        int32_t  m_CT;             // Bit counter for output byte
        uint32_t m_ST;             // Byte being constructed for output
        //any register conventions which allow resolution of carry-over in the encoder and which produce the same entropy-coded segment may be used

        //Except at the time of initialization, bit 15 of the A - register is always set and bit 16 is always clear(the LSB is bit 0).
    };

    //=====================================================================================================================================================================================

    template <class T_Bitstream>
    class xArithCoreEncT : public xArithCoreCommon
    {
    public:
        xArithCoreEncT(T_Bitstream& bitstream) : m_Bitstream(bitstream), m_pending_bytes(0)
            {
                m_A  = 0;
                m_C  = 0;
                m_CT = 0;
                m_ST = 0;
            }

        void initialize()
        {
            m_A = 0x10000;
            m_C = 0;
            m_CT = 11; // 11 bits are available in the buffer before the first byte is full.
            m_ST = 0;
            m_pending_bytes = 0;
        }

        void encodeBinMP(uint32_t BinValue, xArithCoreModel& CtxModel) //Code_0(S) + Code_1(S) - Encodes a single binary symbol using the provided context model/index
            //S is a context-index which identifies a particular conditional probability estimate used in coding the binary decision
        {
            // Get MPS and Qe (LPS probability)
            uint8_t  ProbIndex = CtxModel.getProbIndex();
            uint8_t  MPS = CtxModel.getMPS();
            const auto& QeEntry = xQeModel::getInstance().getEntry(ProbIndex);
            uint16_t Qe = QeEntry.m_Qe; //qe_value;

            // Calculating the subinterval size for MPS
            uint32_t AMps = m_A - Qe;

            // Decide whether the encoded symbol is MPS or LPS
            bool is_mps = (BinValue == MPS);

            if (is_mps) // MPS Path
            {
                // Checking the condition of a conditional exchange
                if (m_A < Qe || AMps < Qe)
                {
                    // Conditional exchange (LPS and MPS switch roles)
                    // This path is identical to the LPS encoding
                    m_C += AMps;
                    m_A = Qe;
                }
                else
                {
                    // Standard MPS encoding
                    // The bug was here. In this arithmetic coding variant,
                    // the MPS occupies the upper sub-interval. Therefore, C must be
                    // advanced past the lower (LPS) sub-interval.
                    m_A = AMps;
                }
            }
            else // LPS Path
            {
                // The lower limit of C is shifted by the size of the MPS interval
                m_C += AMps;
                // The size of the A interval becomes the size of the LPS interval
                m_A = Qe;
            }
            if (m_A < 0x8000)
            {
                renormalize();
            }

            // Updating the statistical model after symbol encoding
            CtxModel.update(is_mps);
        }

        void finish() // Finalizes the encoding process by writing the remaining bits - "Flush"
        {
            uint32_t TempC = m_C + m_A;
            m_C |= 0xFFFFF;
            if (TempC < m_C)
            {
                m_ST++;
            }

            if (m_pending_bytes > 0)
            {
                m_Bitstream.writeByte(m_ST);
                for (int i = 0; i < m_pending_bytes; ++i)
                {
                    m_Bitstream.writeByte(0xFF);
                }
            }
            else {
                m_Bitstream.writeByte(m_ST);
            }

            m_Bitstream.writeByte((m_C >> 12) & 0xFF);
            m_Bitstream.writeByte((m_C >> 4) & 0xFF);

            m_Bitstream.flushToBuffer();
        }

        void renormalize() // Renormalizes the interval and outputs any determined bits
        {
            do {
                m_A <<= 1;  // Double the compartment size
                m_C <<= 1;  // Move lower border left
                m_CT--;     // Decrement bit counter to fill byte
                if (m_CT == 0)
                {
                    // When the counter reaches zero, we need to write a byte
                    writeByte(); // This function handles bit buffering and output.
                }

            } while (m_A < 0x8000); // Repeat until A is large enough
        }

        void writeByte() // Writes a byte to the bitstream, handling special cases like 0xFF
        {
            if (m_ST == 0xFF)
            {
                m_pending_bytes++;
                m_ST = (m_C >> 20) & 0xFF;
                m_C &= 0xFFFFF;
                m_CT = 7;
            }
            else
            {
                if (m_pending_bytes > 0)
                {
                    m_Bitstream.writeByte(m_ST);
                    for (int i = 0; i < m_pending_bytes; ++i)
                    {
                        m_Bitstream.writeByte(0xFF);
                    }
                    m_pending_bytes = 0;
                }
                m_ST = (m_C >> 20) & 0xFF;
                m_C &= 0xFFFFF;
                m_CT = 7;
            }
        }

        void setByteBuffer(xByteBuffer* buffer) {
            m_Bitstream.bindByteBuffer(buffer);
        }

    protected:
        T_Bitstream& m_Bitstream;
        int32_t  m_pending_bytes;   // Count of pending bits to be written
        uint8_t m_BypassCount = 0;
    };

    // Alias ​​for backward compatibility in the rest of the code
    using xArithCoreEnc = xArithCoreEncT<xBitstreamWriter>;

    //=====================================================================================================================================================================================
    class xArithCoreDec : public xArithCoreCommon
	{
    public:
        //xArithmeticDecoder(xBitstreamReader& bitstream) : m_Bitstream(bitstream) {}
        void start();
		void start(const std::function<bool()>& bit_reader); // Initializes the decoder state
		uint32_t decodeBinMP(const xArithCoreModel& Model); // Decodes a single binary symbol using the provided context model
        void setByteBuffer(xByteBuffer* buffer);
        void finish();
	private:
		uint32_t m_value; // Current value from the input stream
		bool     m_bFF; // Flag indicating if the last byte was 0xFF
        bool     m_bByteAvailable; // Flag indicating if a byte is available for input
        xBitstreamReader m_Bitstream;
	};

	//TODO - test using - K.4 Additional information on arithmetic coding
    //=====================================================================================================================================================================================
    // layer 2 - coefficient codding (engine - Annex D)TBD) //F.1.4
    //=====================================================================================================================================================================================

    //=====================================================================================================================================================================================

    class xQeModel {
    public:
        xQeModel(const xQeModel&) = delete;
        xQeModel& operator=(const xQeModel&) = delete;

        static xQeModel& getInstance();

        const StateEntry& getEntry(size_t index) const;

        static constexpr size_t getNumStates() { return NUM_STATES; }

    private:
        xQeModel();

        static constexpr size_t NUM_STATES = 113;

        std::array<StateEntry, NUM_STATES> state_table;
    };

    //=====================================================================================================================================================================================
}; //end of namespace PMBB::JPEG
