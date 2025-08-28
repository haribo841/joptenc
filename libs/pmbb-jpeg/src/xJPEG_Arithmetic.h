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
    public:
        xArithCoreCommon();
        ~xArithCoreCommon();

        void setByteBuffer(xByteBuffer* ByteBuffer) { m_ByteBuffer = ByteBuffer; }

    private:
        xByteBuffer* m_ByteBuffer = nullptr;
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
        xArithCoreEnc(xBitstreamWriter& bitstream) : m_Bitstream(bitstream) {}
        void initialize()
        {
            m_A = 0x10000;
            m_C = 0;
            //m_Bitstream.finalize();
        }

        void encodeBinMP(uint32_t BinValue, xArithCoreModel& CtxModel); //Code_0(S) + Code_1(S) - Encodes a single binary symbol using the provided context model/index
        //S is a context-index which identifies a particular conditional probability estimate used in coding the binary decision
        /*void finish(const std::function<void(bool)>& bit_writer) // Finalizes the encoding process by writing the remaining bits - "Flush"
        {
            encodeBinMP(m_MPS, m_CtxModel); // Finish coding, uses the model
            m_Bitstream.flush();
        }*/
        void renormalize(); // Renormalizes the interval and outputs any determined bits
		void writeByte(); // Writes a byte to the bitstream, handling special cases like 0xFF

    private:
        uint32_t m_ST;             // Byte being constructed for output
        int32_t  m_CT;             // Bit counter for output byte
        int32_t  m_pending_bits;   // Count of pending bits to be written
        bool     m_bFF;            // Flag indicating if the last byte was 0xFF
        bool     m_bByteAvailable; // Flag indicating if a byte is available for output

    protected:
        xBitstreamWriter& m_Bitstream;
        uint16_t m_A = 0;
        uint16_t m_C = 0;
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

    class xArithmeticEncoder
    {
    public:
        xArithmeticEncoder(xBitstreamWriter& bitstream) : m_ArithCoreEnc(bitstream) {}

        void initialize(uint8_t init_index = 0, uint8_t init_mps = 0)
        {
            m_ArithCoreEnc.initialize();
            //m_ArithCoreEnc.m_CtxModel.init(init_index, init_mps);
        }

        /**
         * @class xArithmeticEncoder
         * @brief A wrapper for the arithmetic encoding engine
         * ​​that manages writing to the bitstream.
         */

        /*void encodeBin(uint8_t BinValue, uint8_t ContextIndex);
        {
             We pass the call to the engine, providing a lambda
            // that knows how to write a bit to our stream.
            m_engine.encode(symbol, model, [this](bool bit) {
                this->writeBit(bit);
                });
        }*/

        void finish()
        {
            /*m_engine.finish([this](bool bit) {
                // In the original code, writeBit was not used in finish(),
                // but for consistency and to handle pending_bits,
                // we delegate it in the same way.
                m_bitstream->writeBit(bit);
                });
            m_bitstream->flush();*/
        }

    private:
        xArithCoreEnc m_ArithCoreEnc;
    };

    //=====================================================================================================================================================================================

    /**
     * @class xArithmeticEncoderCore
     * ​​@brief The core of the arithmetic coding algorithm.
     * * This class manages the state (low, high, pending_bits) and performs
     * all computations. It does not handle writing to the stream, instead, 
     * it calls the passed 'bit_writer' function each time a bit is ready to be written.
    */

    class ArithmeticEncoderCore {
    public:
        //xArithmeticEncoderCore() = default;

        // A function to write a bit that is provided by external code
        using tBitWriter = std::function<void(bool)>;

        /**
         * @brief Initializes the encoder state.
         */
        void start();

        /**
        * @brief Encodes a single symbol.
        * @param symbol The symbol to encode.
        * @param model The probability model used for encoding.
        * @param bit_writer The function to call to write the bits.
         */
        //void encode(int32_t symbol, const xArithmeticProbabilityModel& model, const tBitWriter& bit_writer);

        /**
         * @brief Finalizes the encoding process by writing the re
         
         
         
         ing bits.
         * @param bit_writer The function to be called to write the bits.
         */
        void finish(const tBitWriter& bit_writer);

    private:
        void renormalize(const tBitWriter& bit_writer);

        // State variables - the heart of the arithmetic encoder
        uint64_t m_low;
        uint64_t m_high;
        uint64_t m_pending_bits;

        // Algorithm constants
        static constexpr uint64_t TOP_VALUE = 0xFFFFFFFFFFFFFFFFULL;
        static constexpr uint64_t FIRST_QUARTER = 0x4000000000000000ULL;
        static constexpr uint64_t THIRD_QUARTER = 0xC000000000000000ULL;
    };

    class xBitStream {
    public:
        virtual ~xBitStream() = default;
        virtual void writeBit(bool bit) = 0;
        virtual void finalize() = 0; // Finalizes the stream, e.g., by padding the last byte
        virtual const std::vector<uint8_t>& getBuffer() const = 0;
    };

    /*to be removed class xContextModel {
    public:
        // A structure representing the state of a single context ("Bin")
        struct ContextState {
            uint8_t index{ 0 }; // Index to the probability table
            bool mps{ false };  // Sense MPS (false for 0, true for 1)
        };

        explicit xContextModel(size_t num_contexts);

        bool getMps(size_t context_index) const; // Returns the MPS meaning for a given context

        uint8_t getStateIndex(size_t context_index) const; // Returns the internal state index for the given context.

        uint16_t getQeValue(size_t context_index) const; // Returns the probability estimate (Qe) for a given context.

        const ContextState& getContextState(size_t context_index) const; // Gets the state for a given context

        void updateContextState(size_t context_index, uint8_t new_index, bool new_mps); // Updates the state for a given context

        void updateForMps(size_t context_index); // Updates the context state after encoding an MPS symbol.

        void updateForLps(size_t context_index); // Updates the context state after encoding an LPS symbol.

    private:
        std::vector<ContextState> contexts;
        xQeModel& qe_static_model; // Reference to a static transition table
    };*/

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

    class xArithCommon
    {
    public:
        //static bool xInitArithTables(uint8* ArithLen, uint32* ArithCode, const xJFIF::xArithTable& ArithTable);
        static void xAvoidZeroLenCodes(uint8* ArithLen, int32 TableSize);

        //static int32 xFillTmpLengths(uint8* Lenghts, const xJFIF::xArithTable::tCodeL& TabCodeLengths);
        static int32 xFillTmpCodes(uint32* Codes, const uint8* Lenghts, int32 NumLengths);
    };

    //=====================================================================================================================================================================================

    class xArithDecoder : public xArithCommon
    {
    public:
        xArithDecoder() = default;

        /**
         * @brief Starts the decoder and initializes its state from the bitstream.
         * @param bitstream The bitstream to read from.
         */
        void start(xBitstreamReader* bitstream);

        /**
         * @brief Decodes a single symbol using the provided probability model.
         * @param model The probability model to use for decoding.
         * @return The decoded symbol.
         */
        //int64_t decode(const xArithmeticProbabilityModel& model);

        /**
         * @brief A convenience function to decode a DC coefficient.
         * It decodes the prefix code and then reads the suffix bits.
         * @param bitstream The bitstream to read from.
         * @param model The probability model for DC coefficients.
         * @return The decoded DC value.
         */
        //int64_t readDC(xBitstreamReader* bitstream, const xArithmeticProbabilityModel& model);

    private:
        /**
         * @brief Reads the suffix bits for a decoded symbol and computes the final value.
         * @param bitstream The bitstream to read from.
         * @param numBits The number of suffix bits to read.
         * @return The final coefficient value.
         */
        int64_t readSufix(xBitstreamReader* bitstream, int64_t numBits);

        // Composition instead of inheritance/direct implementation
        //xArithmeticDecoderCore m_engine;
        xBitstreamReader* m_bitstream = nullptr;
    };

    //=====================================================================================================================================================================================

    class xArithmeticDecoderCore
    {
    public:
        xArithmeticDecoderCore() = default;

        // A function to read a bit that is provided by external code
        using tBitReader = std::function<bool()>;

        /**
         * @brief Starts the decoder and initializes its state.
         * @param bit_reader The function to call to read bits.
         */
        void start(const tBitReader& bit_reader);

        /**
         * @brief Decodes a single symbol using the provided probability model.
         * @param model The probability model to use for decoding.
         * @return The decoded symbol.
         */
        //int64_t decode(const xArithmeticProbabilityModel& model, const tBitReader& bit_reader);

    private:
        /**
         * @brief Renormalizes the range [m_low, m_high] by shifting out common
         * most significant bits and reading new bits into m_value.
         * @param bit_reader The function to call to read bits.
         */
        void renormalize(const tBitReader& bit_reader);

        // State variables for the decoder
        uint64_t m_low;
        uint64_t m_high;
        uint64_t m_value; // Holds the current value from the bitstream

        // Constants matching the encoder
        static constexpr uint64_t TOP_VALUE = 0xFFFFFFFFFFFFFFFFULL;
        static constexpr uint64_t FIRST_QUARTER = 0x4000000000000000ULL;
        static constexpr uint64_t THIRD_QUARTER = 0xC000000000000000ULL;
    };

    //=====================================================================================================================================================================================

    class xArithEncoderDC : public xArithCommon
    {
    protected:
        uint32 m_ArithCode[xJPEG_Constants::c_MaxNumCodeSymbolsDC];
        uint8  m_ArithLen[xJPEG_Constants::c_MaxNumCodeSymbolsDC];

    public:
        //bool init(const xJFIF::xArithTable& ArithTable) { if (ArithTable.getClass() != xJFIF::xArithTable::eArithClass::DC) { return false; } return xInitArithTables(m_ArithLen, m_ArithCode, ArithTable); }
        void writeDC(xBitstreamWriter* Bitstream, int32 NumBits, uint32 Remainder) { Bitstream->writeBits(m_ArithCode[NumBits], m_ArithLen[NumBits]); if (NumBits) { Bitstream->writeBits(Remainder, NumBits); } }
    };

    class xArithEncoderAC : public xArithCommon
    {
    protected:
        uint32 m_ArithCode[xJPEG_Constants::c_MaxNumCodeSymbolsAC];
        uint8  m_ArithLen[xJPEG_Constants::c_MaxNumCodeSymbolsAC];

    public:
        //bool init(xJFIF::xArithTable& ArithTable) { if (ArithTable.getClass() != xJFIF::xArithTable::eArithClass::AC) { return false; } return xInitArithTables(m_ArithLen, m_ArithCode, ArithTable); }
        void writeAC(xBitstreamWriter* Bitstream, int32 Code, int32 NumBits, uint32 Remainder) { Bitstream->writeBits(m_ArithCode[Code], m_ArithLen[Code]); Bitstream->writeBits(Remainder, NumBits); }
        void writeZRL(xBitstreamWriter* Bitstream) { Bitstream->writeBits(m_ArithCode[0xF0], m_ArithLen[0xF0]); }
        void writeEOB(xBitstreamWriter* Bitstream) { Bitstream->writeBits(m_ArithCode[0x00], m_ArithLen[0x00]); }
    };

    //=====================================================================================================================================================================================

    class xArithEstimatorDC : public xArithCommon
    {
    protected:
        uint8 m_ArithLen[xJPEG_Constants::c_MaxNumCodeSymbolsDC];

    public:
        //bool  init(const xJFIF::xArithTable& ArithTable);
        int32 calcDC(int32 NumBits) const { return m_ArithLen[NumBits] + NumBits; }
    };

    class xArithEstimatorAC : public xArithCommon
    {
    protected:
        uint8  m_ArithLen[xJPEG_Constants::c_MaxNumCodeSymbolsAC];

    public:
        //bool  init(const xJFIF::xArithTable& ArithTable);
        int32 calcAC(int32 Code, int32 NumBits) const { return m_ArithLen[Code] + NumBits; }
        int32 calcZRL() const { return m_ArithLen[0xF0]; }
        int32 calcEOB() const { return m_ArithLen[0x00]; }
    };

    //=====================================================================================================================================================================================

    class xArithCounterDC
    {
    protected:
        static constexpr int32 c_NCS = xJPEG_Constants::c_MaxNumCodeSymbolsDC;
        uint32 m_SymbolCount[c_NCS];

    public:
        bool init() { memset(m_SymbolCount, 0, c_NCS * sizeof(uint32)); return true; }
        void countDC(int32 Code) { m_SymbolCount[Code]++; }
        void acc(const xArithCounterDC* Other) { for (int32 i = 0; i < c_NCS; i++) { m_SymbolCount[i] += Other->m_SymbolCount[i]; } }
        const uint32* getSymbolCount() const { return m_SymbolCount; }
    };

    class xArithCounterAC
    {
    protected:
        static constexpr int32 c_NCS = xJPEG_Constants::c_MaxNumCodeSymbolsAC;
        uint32 m_SymbolCount[c_NCS];

    public:
        bool init() { memset(m_SymbolCount, 0, c_NCS * sizeof(uint32)); return true; }
        void countAC(int32 Code) { m_SymbolCount[Code]++; }
        void countZRL() { m_SymbolCount[0xF0]++; }
        void countEOB() { m_SymbolCount[0x00]++; }
        void acc(const xArithCounterAC* Other) { for (int32 i = 0; i < c_NCS; i++) { m_SymbolCount[i] += Other->m_SymbolCount[i]; } }
        const uint32* getSymbolCount() const { return m_SymbolCount; }
    };

    //=====================================================================================================================================================================================

    class xArithmeticTabBuilder
    {
    public:
        static void buildLengthTable(uint8_t* LengthTable, const uint32_t* SymbolCount, int32_t Size);
    };

    //=====================================================================================================================================================================================

}; //end of namespace PMBB::JPEG
