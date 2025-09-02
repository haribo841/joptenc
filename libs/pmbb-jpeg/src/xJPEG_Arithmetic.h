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

            /* void updateMPS()
            {
                const auto& entry = xQeModel::getInstance().getEntry(m_ProbIndex);
                m_ProbIndex = entry.next_index_mps;
                // MPS sense does not change on MPS path
            }

            void updateLPS()
            {
                const auto& entry = xQeModel::getInstance().getEntry(m_ProbIndex);
                m_ProbIndex = entry.next_index_lps;
                if (entry.switch_mps)
                {
                    m_MPS ^= 1; // Toggle MPS sense
                }*/

    private:
        uint8_t m_ProbIndex; // Current index in the probability table
        uint8_t m_MPS;      // Current Most Probable Symbol (MPS) sense (0 or 1)
    };

    //=====================================================================================================================================================================================
    // layer 1 - arithmetic codec core - Annex D

    class xArithCoreCommon
    {
    private:
        xByteBuffer* m_ByteBuffer = nullptr; // it should be a pointer
        // MSB                                LSB
        uint64_t m_A;               // 00000000, 00000000, aaaaaaaa, aaaaaaaa
        uint64_t m_C;               // 0000cbbb, bbbbbsss, xxxxxxxx, xxxxxxxx
        // a - fractional bits in the A-register (the current probability interval value)
        // b - indicate the bit positions from which the completed bytes of data are removed from the C-register
        // c - carry bit
        // s - optional spacer bits which provide useful constraints on carry-over
        // x - fractional bits in the code register

        //any register conventions which allow resolution of carry-over in the encoder and which produce the same entropy-coded segment may be used

        //Except at the time of initialization, bit 15 of the A - register is always set and bit 16 is always clear(the LSB is bit 0).
    };

    //=====================================================================================================================================================================================

    class xArithCoreEnc : public xArithCoreCommon
    {
    public:
        xArithCoreEnc(xBitstreamWriter& bitstream) : m_Bitstream(bitstream), m_ST(0), m_CT(0), m_pending_bits(0), m_bFF(false), m_bByteAvailable(false), m_A(0), m_C(0) {}
        void initialize()
        {
            m_A = 0x10000;
            m_C = 0;
            m_CT = 12; // 4 "pause" bits + 8 data bits
            m_ST = 0;
        }

        void encodeBinMP(uint32_t BinValue, xArithCoreModel& CtxModel); //Code_0(S) + Code_1(S) - Encodes a single binary symbol using the provided context model/index
        //S is a context-index which identifies a particular conditional probability estimate used in coding the binary decision
        void finish();// const std::function<void(bool)>& bit_writer); // Finalizes the encoding process by writing the remaining bits - "Flush"
        void renormalize(); // Renormalizes the interval and outputs any determined bits
		void writeByte(); // Writes a byte to the bitstream, handling special cases like 0xFF

        uint32_t getA() const { return m_A; }
        uint32_t getC() const { return m_C; }

    private:
        uint32_t m_ST;             // Byte being constructed for output
        int32_t  m_pending_bits;   // Count of pending bits to be written
        bool     m_bFF;            // Flag indicating if the last byte was 0xFF
        bool     m_bByteAvailable; // Flag indicating if a byte is available for output

    protected:
        xBitstreamWriter& m_Bitstream;
        uint32_t m_A;
        uint32_t m_C;
        int32_t  m_CT;             // Bit counter for output byte
        uint8_t m_BypassCount = 0;
        //xArithCoreModel m_CtxModel;
    };

    //=====================================================================================================================================================================================
    class xArithCoreDec : public xArithCoreCommon
	{
    public:
        //xArithmeticDecoder(xBitstreamReader& bitstream) : m_Bitstream(bitstream) {}

		void start(const std::function<bool()>& bit_reader); // Initializes the decoder state
		uint32_t decodeBinMP(const xArithCoreModel& Model); // Decodes a single binary symbol using the provided context model

	private:
		uint32_t m_ST; // Current byte being processed
		int32_t  m_CT; // Bit counter for the current byte
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
