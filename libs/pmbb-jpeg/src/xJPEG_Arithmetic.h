/*
    SPDX-FileCopyrightText: 2020-2024 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
    */
#pragma once
#include "xCommonDefJPEG.h"
#include "xBitstream.h"
#include "xByteBuffer.h"
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
        void flipMPS() { m_MPS = 1 - m_MPS; }

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
        //static const std::array<uint32_t, 113> c_Qe;

        uint32_t getA()  const { return m_A;  }
        uint32_t getC()  const { return m_C;  }
        uint32_t getCT() const { return m_CT; }
        uint32_t getST() const { return m_ST; }

    protected:
        xByteBuffer* m_ByteBuffer = nullptr; // it should be a pointer
                                    // MSB                                LSB
        uint32_t m_A;               // 00000000, 00000000, aaaaaaaa, aaaaaaaa
        uint8_t  m_B;               // Previous byte
        uint32_t m_C;               // 0000cbbb, bbbbbsss, xxxxxxxx, xxxxxxxx
        // a - fractional bits in the A-register (the current probability interval value)
        // b - indicate the bit positions from which the completed bytes of data are removed from the C-register
        // c - carry bit
        // s - optional spacer bits which provide useful constraints on carry-over
        // x - fractional bits in the code register
        uint32_t  m_CT;             // Bit counter for output byte
        uint32_t  m_ST;             // Byte being constructed for output
        bool     m_isFirstByte;     // true until first byte is output via Byte_out()
        bool     m_renormalization_occurred;
        uint8_t m_BypassCount = 0;
        //any register conventions which allow resolution of carry-over in the encoder and which produce the same entropy-coded segment may be used

        //Except at the time of initialization, bit 15 of the A - register is always set and bit 16 is always clear(the LSB is bit 0).
    };

    //=====================================================================================================================================================================================

    template <class T_Bitstream>
    class xArithCoreEncT : public xArithCoreCommon
    {
    public:
        xArithCoreEncT(T_Bitstream& bitstream) : m_Bitstream(bitstream)
        {
            m_A  = 0;
		    m_B  = 0;
            m_C  = 0;
            m_CT = 0;
            m_ST = 0;
            m_isFirstByte = true;
            m_renormalization_occurred = false;
            m_ByteBuffer = nullptr;
        }
        void initialize()
        {
            m_A = 0x10000;
            m_C = 0;
            m_CT = 11;
            m_ST = 0;
            m_B = 0;
            m_isFirstByte = true;
            m_renormalization_occurred = false;
        }
        void encodeBinMP(uint32_t BinValue, xArithCoreModel& CtxModel) //Code_0(S) + Code_1(S) - Encodes a single binary symbol using the provided context model/index
            //S is a context-index which identifies a particular conditional probability estimate used in coding the binary decision
        {
            uint8_t  ProbIndex = CtxModel.getProbIndex();
            uint8_t  MPS = CtxModel.getMPS();
            const auto& QeEntry = xQeModel::xQeModel::getInstance().getEntry(ProbIndex);
            uint16_t Qe = QeEntry.m_Qe;
            m_A -= Qe;
            bool is_mps = (BinValue == MPS);
            bool modelUpdate = false;
            bool doRenormalization = false;
            m_renormalization_occurred = false;
            if (is_mps) // MPS Path
            {
                if (m_A < 0x8000)
                {
                    if (m_A < Qe)
                    {
                        m_C += m_A;
                        m_A = Qe;
                    }
                    else
                    {
                    }
                    doRenormalization = true;
                    modelUpdate = true;
                }
            }
            else // LPS Path
            {
                if (m_A < Qe)
                {
                }
                else {
                    m_C += m_A;
                    m_A = Qe;
                }
                doRenormalization = true;
                modelUpdate = true;
            }
            if (modelUpdate) {
                if (!is_mps) // True LPS
                {
                    // For a true LPS, check if the MPS sense should be flipped.
                    if (xQeModel::getInstance().getEntry(CtxModel.getProbIndex()).m_switchMPS)
                    {
                        CtxModel.flipMPS();
                    }
                    // Update the probability index.
                    CtxModel.update(false);
                }
                else // BinValue == MPS
                {
                    if (modelUpdate) // MPS with conditional exchange
                    {
                        CtxModel.update(true);
                    }
                }
            }
            if (doRenormalization) {
                renormalize();
			}

        }
        bool getAndClearRenormalizationFlag()
        {
            bool flag_status = m_renormalization_occurred;
            m_renormalization_occurred = false;
            return flag_status;
        }
        uint8_t getB() const { return m_B; }
        void renormalize() // Renormalizes the interval and outputs any determined bits
        {
            int shifts = 0;
            int current_iter = 0; // For debugging purposes

            m_renormalization_occurred = true;
            do {
                m_A <<= 1;  // Double the compartment size
                m_C <<= 1;  // Move lower border left
                m_CT--;     // Decrement bit counter to fill byte
                if (m_CT == 0)
                {
					Byte_out(); // Output a byte if the counter is zero
                    m_CT = 8;
                }
                current_iter++;
            } while (m_A < 0x8000); // Repeat until A is large enough
        }
        void Byte_out() // Replaces writeByte()
        {
            uint32_t T = (m_C >> 19);

            // Step 2: T > X'FF' ? -> Checking if a carry has occurred.
            if (T > 0xFF)// Carry-over occurred
            {
                // On carry, the first byte logic is simpler as we are forced to output.
                // We assume carry on the very first byte is an unlikely edge case.
                m_B++; // B = B + 1

                // Store the previous byte and handle Stuff_0 if B becomes 0xFF.
                m_Bitstream.writeByte(m_B);
                if (m_B == 0xFF)
                {
                    m_Bitstream.writeByte(0x00); // Stuff_0
                }

                // Output_stacked_zeros: write as many zeros as there were stacked 0xFF.
                for (uint32_t i = 0; i < m_ST; ++i)
                {
                    m_Bitstream.writeByte(0x00);
                }
                m_ST = 0;

                // Update B for the next call, ignoring the carry bit.
                m_B = static_cast<uint8_t>(T & 0xFF);
                m_isFirstByte = false; // A byte has now been processed
            }
            else // --- Path "No" (no carry) ---
            {
                // Step 3: T = X'FF' ?
                if ((T & 0xFF) == 0xFF)
                {
                    // --- Path "Yes" (byte is 0xFF) ---
                    m_ST++; // ST = ST + 1: push a byte onto the stack.
                }
                else
                {
                    // --- Path "No" (plain byte) ---
                    //Write the previous byte only if this is not the first call.
                        if (!m_isFirstByte)
                        {
                            m_Bitstream.writeByte(m_B);
                            if (m_B == 0xFF) // Stuff_0 support for B itself.
                            {
                                m_Bitstream.writeByte(0x00);
                            }
                        }

                    // Write all set aside bytes 0xFF, each with a required zero (Stuff_0).
                    for (uint32_t i = 0; i < m_ST; ++i)
                    {
                        m_Bitstream.writeByte(0xFF);
                        m_Bitstream.writeByte(0x00);
                    }
                    m_ST = 0;

                    // Update B for the next call.
                    m_B = static_cast<uint8_t>(T & 0xFF);
                    m_isFirstByte = false; // The first real byte is now pending in m_B
                }
            }
            m_C &= 0x7FFFF;
        }
        void setByteBuffer(xByteBuffer* buffer) {
            m_ByteBuffer = buffer; // Setting a pointer in the base class
            m_Bitstream.bindByteBuffer(buffer);
        }

        private:
            void clear_final_bits()
            {
                uint32_t T = m_C + m_A - 1;
                T &= 0xFFFF0000;

                if (T < m_C)
                {
                    T += 0x8000;
                }
                m_C = T;
            }
            void discard_final_zeros()
            {
                if (!m_ByteBuffer) { return; }

                uint32_t newSize = m_ByteBuffer->getDataSize();
                uint8_t* data = m_ByteBuffer->getReadPtr();

                while (newSize > 0)
                {
                    if (data[newSize - 1] != 0) {
                        break;
                    }
                    if (newSize > 1 && data[newSize - 2] == 0xFF) {
                        break;
                    }
                    newSize--;
                }

                if (newSize < m_ByteBuffer->getDataSize())
                {
                    m_ByteBuffer->setDataSize(newSize);
                }
            }

    public:
        void finish()
        {
            // Step 1: Execute the Clear_final_bits procedure
            clear_final_bits();

            // Step 2: Shift register C left by CT bits (C = SLL C CT)
            m_C <<= m_CT;

            // Step 3: Call the Byte_out procedure
            Byte_out();

            // Step 4: Shift register C left by 8 bits (C = SLL C 8)
            m_C <<= 8;

            // Step 5: Call Byte_out and Discard_final_zeros
            Byte_out();
            discard_final_zeros();
        }

    protected:
        T_Bitstream& m_Bitstream;
        //uint8_t  m_B; // Previous byte
        // Safe arithmetic helper: computes C + A - Qe using a 64-bit intermediate
        inline uint32_t safeAddAminusQ(uint32_t C, uint32_t A, uint32_t Qe)
        {
            uint64_t tmp = static_cast<uint64_t>(C);
            tmp += static_cast<uint64_t>(A);
            tmp -= static_cast<uint64_t>(Qe);
            // keep lower 32 bits (behavior consistent with existing uint32_t members)
            return static_cast<uint32_t>(tmp & 0xFFFFFFFFu);
        }
    };
    using xArithCoreEnc = xArithCoreEncT<xBitstreamWriter>;
    //=====================================================================================================================================================================================
    class xArithCoreDec : public xArithCoreCommon
	{
    public:
        void init(uint8_t* buffer, size_t size);
        void Initdec();                                          // Initialize the decoder
		//void start(const std::function<bool()>& bit_reader);   // Initializes the decoder state
        uint32 MPS             (const xArithCoreModel& S); // more probable symbol for context-index S
        uint32 Decode                (xArithCoreModel& S); // Decode a binary decision with context-index S
        uint32 Cond_LPS_exchange     (xArithCoreModel& S); // Decoder LPS path conditional exchange procedure
        uint32 Cond_MPS_exchange     (xArithCoreModel& S); // Decoder MPS path conditional exchange procedure
        void   Estimate_QeS_after_MPS(xArithCoreModel& S);
        void   Estimate_QeS_after_LPS(xArithCoreModel& S);
        void   Renorm_d();                                 // Decoder renormalization procedure
		void   Byte_in();
        void   Unstuff_0();
        void   setByteBuffer(xByteBuffer* buffer);
        void   finish();
        // Metoda do powiązania dekodera z buforem danych wejściowych
        // Composes CLow and Cx into a full C register (32-bit)
        uint32_t MakeC(uint16_t CLow, uint16_t Cx);

        // Decomposes the full C register into CLow and Cx
        inline void SplitC(uint32_t C, uint16_t& CLow, uint16_t& Cx)
        {
            // CLow = lower 16 bits
            CLow = static_cast<uint16_t>(C & 0xFFFF);
            // Cx = upper 16 bits
            Cx = static_cast<uint16_t>((C >> 16) & 0xFFFF);
        }

	private:           
        uint8_t* m_pBufferStart = nullptr; // początek bufora
        size_t   m_BufferSize = 0;       // rozmiar bufora
        uint8_t* m_BP = nullptr; // aktualna pozycja w buforze
        uint8_t* m_BPST = nullptr; // byte przed startem segmentu
        bool     m_bFoundEOI = false;   // koniec danych?

        // rejestry arytmetyczne
        uint32_t m_A = 0;
        uint32_t m_C = 0;
        uint32_t m_CLow = 0;
        uint32_t m_Cx = 0;
        uint32_t m_CT = 0;
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

        inline const StateEntry& getEntry(size_t index) const
        {
            if (index >= getNumStates())
            {
                throw std::out_of_range("State index is out of range.");
            }
            return xQeModel::STATIC_STATE_TABLE[index];
        }

        static constexpr size_t getNumStates() { return NUM_STATES; }

        // Returns the Qe value for a given state index.
        static inline uint16_t getQeValue(uint8_t stateIndex)
        {
            return xQeModel::STATIC_STATE_TABLE[stateIndex].m_Qe;
        }
        static constexpr size_t NUM_STATES = 113;
        std::array<StateEntry, NUM_STATES> state_table;
        static const std::array<StateEntry, 113> STATIC_STATE_TABLE;

	private:
		xQeModel() : state_table(STATIC_STATE_TABLE) {}
    };
    //=====================================================================================================================================================================================
}; //end of namespace PMBB::JPEG
