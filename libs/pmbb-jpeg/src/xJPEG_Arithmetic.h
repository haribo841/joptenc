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
        //void updateMPS();
        //void updateLPS();

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
		xArithCoreEncT(T_Bitstream& bitstream) : m_Bitstream(bitstream), m_B(0)
            {
                m_A  = 0;
                m_C  = 0;
                m_CT = 0;
                m_ST = 0;
                m_renormalization_occurred = false;
                m_ByteBuffer = nullptr;
            }

        void initialize()
        {
            m_A = 0x10000;
            m_C = 0;
            m_CT = 11; // 11 bits are available in the buffer before the first byte is full.
            m_ST = 0; // Stack counter/Count of pending bits to be written
            m_B = 0;
            m_renormalization_occurred = false; // Reset flag on initialization
        }

        void encodeBinMP(uint32_t BinValue, xArithCoreModel& CtxModel) //Code_0(S) + Code_1(S) - Encodes a single binary symbol using the provided context model/index
            //S is a context-index which identifies a particular conditional probability estimate used in coding the binary decision
        {
            // Get MPS and Qe (LPS probability)
            uint8_t  ProbIndex = CtxModel.getProbIndex();
            uint8_t  MPS = CtxModel.getMPS();
            const auto& QeEntry = xQeModel::xQeModel::getInstance().getEntry(ProbIndex);
            uint16_t Qe = xQeModel::getQeValue(ProbIndex); //QeEntry.m_Qe;

            // Calculating the subinterval size for MPS
            uint32_t AMps = m_A - Qe;

            bool is_mps = (BinValue == MPS);
            bool conditional_exchange = false;
            // Reset flags at the beginning of each symbol encoding.
            // This will ensure that the flag reflects the state *only* for the current operation.
            m_renormalization_occurred = false;

            if (is_mps) // MPS Path
            {
                // Completely changed structure to avoid uint underflow when m_A < Qe.
                // Logic is now separated into safe paths.
                if (m_A < Qe)
                {
                    // This is a forced conditional exchange because A is too small.
                    // LPS coding logic is applied to MPS.
                    // C_new = C + (A - Qe), A_new = Qe
                    // To avoid overflow, we calculate it as: C_new = C - (Qe - A)
                    // Unification of the arithmetic operation with the second path of conditional exchange.
                    //m_C -= (Qe - m_A); //m_C += m_A - Qe;
                    //uint32_t newC = static_cast<uint32_t>(static_cast<uint64_t>(m_C) + static_cast<uint64_t>(m_A) - static_cast<uint64_t>(Qe));
                    //m_C = newC;
                    m_C = safeAddAminusQ(m_C, m_A, Qe);
                    if ((m_C >> 24) != 0) { // adjust threshold if you want earlier detection
                        printf("[ASSERT-WARN] large high bits in C after update: C=0x%08X (A=0x%04X Qe=0x%04X)\n",
                            m_C, m_A, Qe);
                    }
                    m_A = Qe;
                    conditional_exchange = true;
                }
                else
                {
                    // m_A >= Qe, so subtraction is safe.
                    uint32_t AMps_local = m_A - Qe;
                    if (AMps_local < Qe)
                    {
                        // This is the second, standard type of conditional exchange.
                        m_C += AMps_local;
                        if ((m_C >> 24) != 0) { // adjust threshold if you want earlier detection
                            printf("[ASSERT-WARN] large high bits in C after update: C=0x%08X (A=0x%04X Qe=0x%04X)\n",
                                m_C, m_A, Qe);
                        }
                        m_A = Qe;
                        conditional_exchange = true;
                    }
                    else
                    {
                        // Standard MPS encoding.
                        m_A = AMps_local;
                    }
                }
            }
            else // LPS Path
            {
                // Unify logic with MPS to prevent overflow, when m_A < Qe.
                // This case should not occur if the encoder is in the correct state.
                /*
                if (m_A < Qe)
                {
                    m_C -= (Qe - m_A);
                }
                else
                {
                    m_C += m_A - Qe;
                }
                m_A = Qe;
                */
                //m_C = static_cast<uint32_t>(static_cast<uint64_t>(m_C) + static_cast<uint64_t>(m_A) - static_cast<uint64_t>(Qe));
                m_C = safeAddAminusQ(m_C, m_A, Qe);
                if ((m_C >> 24) != 0) { // adjust threshold if you want earlier detection
                    printf("[ASSERT-WARN] large high bits in C after update: C=0x%08X (A=0x%04X Qe=0x%04X)\n",
                        m_C, m_A, Qe);
                }
                m_A = Qe;
            }
            if (m_A < 0x8000)
            {
                renormalize();
            }
            // Model update logic must distinguish between true LPS, conditional exchange, and true MPS.
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
                if (conditional_exchange) // MPS with conditional exchange
                {
                    // Update index as if it were an LPS, but DO NOT flip MPS sense.
                    CtxModel.update(false);
                }
                else if (m_renormalization_occurred) // True MPS with renormalization
                {
                    // Update the probability index.
                    CtxModel.update(true);
                }
                // If it's a true MPS without renormalization, the model state is not changed.
            }
        }

        /**
        * @brief: Returns whether renormalization has occurred
        * since the last call to this method and resets the flag.
        * @return true if renormalization has occurred, false otherwise.
        */
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
                uint32_t beforeC = m_C;
                uint32_t beforeA = m_A;
				uint16_t Qe = xQeModel::getQeValue(0); //xQeModel::getInstance().getEntry(0).m_Qe; // For logging purposes, we use index 0 as a reference.
                int shifts = 0;
                printf("[ENC-UPD] iter=%d beforeC=0x%08X A=0x%04X Qe=0x%04X afterC=0x%08X\n",
                    current_iter, beforeC, beforeA, Qe, m_C);
                if ((m_C >> 20) != 0) {
                    printf("[WARN] high bits in C: C=0x%08X T=%u at iter=%d\n", m_C, (m_C >> 20), current_iter);
                }
                m_A <<= 1;  // Double the compartment size
                m_C <<= 1;  // Move lower border left
                m_CT--;     // Decrement bit counter to fill byte
                printf("[RENORM] iter=%d shift#=%d A=0x%04X C=0x%08X CT=%d\n", current_iter, shifts, m_A, m_C, m_CT);
                if (m_CT == 0)
                {
                    // When the counter reaches zero, we need to write a byte
                    writeByte(); // This function handles bit buffering and output.
                }
                current_iter++;
            } while (m_A < 0x8000); // Repeat until A is large enough
        }

        void writeByte()
        {
            uint32_t T = (m_C >> 20);
			static int current_iter = 0; // For debugging purposes
            printf("[BYTE_OUT] iter=%d T=%u m_B(before)=0x%02X m_ST=%d -> branch=%s\n",
                current_iter, T, m_B, m_ST, (T > 0xFF ? "carry" : ((T & 0xFF) == 0xFF ? "stack" : "normal")));
            if (T > 0xFF)
            {
                // carry to previous byte
                uint8_t oldB = m_B;
                uint8_t newB = static_cast<uint8_t>(oldB + 1); // increment (may wrap)
                m_Bitstream.writeByte(newB);

                // If previous byte was 0xFF, an extra 0x00 stuffing byte must be written.
                if (oldB == 0xFF) {
                    m_Bitstream.writeByte(0x00); // Stuff_0 for carry
                }

                // Output stacked zeros (these represent previously deferred 0xFF results)
                for (int i = 0; i < m_ST; ++i) {
                    m_Bitstream.writeByte(0x00);
                }
                m_ST = 0;

                // Update B for next iteration
                m_B = static_cast<uint8_t>(T & 0xFF);
            }
            else if ((T & 0xFF) == 0xFF)
            {
                // Defer writing 0xFF: increment the stack
                m_ST++;
            }

            else
            {
                // Normal byte: write previous B
                m_Bitstream.writeByte(m_B);

                // If we had stacked deferred 0xFFs, write them with stuffing 0x00 after each 0xFF
                if (m_ST > 0)
                {
                    for (int i = 0; i < m_ST; ++i)
                    {
                        m_Bitstream.writeByte(0xFF);
                        m_Bitstream.writeByte(0x00); // Stuffing after every 0xFF
                    }
                    m_ST = 0;
                }

                m_B = static_cast<uint8_t>(T & 0xFF);
            }

            // keep only lower 20 bits of C (rest used via T)
            m_C &= 0xFFFFF;
            m_CT = 7;
        }

        /**
        * @brief: Improved method for setting the byte buffer.
        */
        void setByteBuffer(xByteBuffer* buffer) {
            m_ByteBuffer = buffer; // Setting a pointer in the base class
            m_Bitstream.bindByteBuffer(buffer);
        }

        private:
            /**
            * @brief: Implements the Clear_final_bits procedure from Figure D.14 of the JPEG standard.
            */
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

            /**
            * @brief: Implements the Discard_final_zeros procedure from Figure D.15 of the JPEG standard.
            */
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

        /**
        * @brief: Replaces the previous finish() method. Implements the Flush procedure from Figure D.13.
        *
        * Correctly terminates the arithmetic encoding process according to the JPEG standard.
        */
        void finish()
        {
            // Step 1: Execute the Clear_final_bits procedure
            clear_final_bits();

            // Step 2: Shift register C left by CT bits (C = SLL C CT)
            m_C <<= m_CT;

            // Step 3: Call the Byte_out procedure
            writeByte();

            // Step 4: Shift register C left by 8 bits (C = SLL C 8)
            m_C <<= 8;

            // Step 5: Call Byte_out and Discard_final_zeros
            writeByte();
            discard_final_zeros();

            // Make sure all data from the writer's temporary buffer has been written
            m_Bitstream.flushToBuffer();
        }


    protected:
        T_Bitstream& m_Bitstream;
        uint8_t  m_B; // Previous byte
        uint8_t m_BypassCount = 0;
        bool m_renormalization_occurred;
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

        inline const StateEntry& getEntry(size_t index) const
        {
            if (index >= getNumStates())
            {
                throw std::out_of_range("State index is out of range.");
            }
            return xQeModel::STATIC_STATE_TABLE[index];
        }

        static constexpr size_t getNumStates() { return NUM_STATES; }

        /**
  * @brief Zwraca wartość Qe dla danego indeksu stanu.
  * Definicja funkcji znajduje się BEZPOŚREDNIO tutaj, w pliku nagłówkowym,
  * co jest wymagane dla funkcji inline.
  */
        static inline uint16_t getQeValue(uint8_t stateIndex)
        {
            // Upewnij się, że STATIC_STATE_TABLE jest widoczna tutaj.
            // Zazwyczaj jest zdefiniowana jako prywatna statyczna stała w tej samej klasie.
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
