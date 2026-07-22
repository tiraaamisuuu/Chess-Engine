#pragma once

#include "board.hpp"

class NnueNetwork {
public:
    static constexpr u32 FormatVersion = 1;
    static constexpr u32 FeatureCount = 64U * 10U * 64U; // HalfKP: king, relative piece, square.

    bool load(const std::filesystem::path& path, std::string* error = nullptr){
        auto fail = [&](const std::string& message){
            clear();
            if(error) *error = message;
            return false;
        };

        std::ifstream input(path, std::ios::binary);
        if(!input) return fail("could not open network file");

        std::array<char, 8> magic{};
        input.read(magic.data(), static_cast<std::streamsize>(magic.size()));
        const std::array<char, 8> expected{{'T','N','N','U','E','1','\0','\0'}};
        if(magic != expected) return fail("invalid NNUE magic");

        u32 version = 0;
        u32 features = 0;
        u32 hidden = 0;
        int32_t hiddenScale = 0;
        int32_t outputScale = 0;
        int32_t outputBias = 0;
        if(!readValue(input, version) || !readValue(input, features) || !readValue(input, hidden) ||
           !readValue(input, hiddenScale) || !readValue(input, outputScale) || !readValue(input, outputBias)){
            return fail("truncated NNUE header");
        }
        if(version != FormatVersion) return fail("unsupported NNUE version");
        if(features != FeatureCount) return fail("network feature layout does not match HalfKP v1");
        if(hidden == 0 || hidden > 1024) return fail("invalid NNUE hidden size");
        if(hiddenScale <= 0 || outputScale <= 0) return fail("invalid NNUE quantization scale");

        const size_t inputWeightCount = static_cast<size_t>(features) * hidden;
        const size_t outputWeightCount = static_cast<size_t>(hidden) * 2;
        std::vector<int32_t> newHiddenBias(hidden);
        std::vector<int16_t> newInputWeights(inputWeightCount);
        std::vector<int16_t> newOutputWeights(outputWeightCount);
        if(!readArray(input, newHiddenBias) || !readArray(input, newInputWeights) ||
           !readArray(input, newOutputWeights)){
            return fail("truncated NNUE weights");
        }

        hiddenSize_ = hidden;
        hiddenScale_ = hiddenScale;
        outputScale_ = outputScale;
        outputBias_ = outputBias;
        hiddenBias_ = std::move(newHiddenBias);
        inputWeights_ = std::move(newInputWeights);
        outputWeights_ = std::move(newOutputWeights);
        source_ = path.string();
        return true;
    }

    void clear(){
        hiddenSize_ = 0;
        hiddenScale_ = 0;
        outputScale_ = 0;
        outputBias_ = 0;
        hiddenBias_.clear();
        inputWeights_.clear();
        outputWeights_.clear();
        source_.clear();
    }

    bool loaded() const { return hiddenSize_ != 0; }
    u32 hiddenSize() const { return hiddenSize_; }
    const std::string& source() const { return source_; }

    int evaluate(const Board& board) const {
        if(!loaded()) return 0;
        const Color firstPerspective = board.stm;
        const Color secondPerspective = other(board.stm);
        const int firstKing = board.findKing(firstPerspective);
        const int secondKing = board.findKing(secondPerspective);
        if(firstKing < 0 || secondKing < 0) return 0;

        thread_local std::vector<int32_t> firstAccumulator;
        thread_local std::vector<int32_t> secondAccumulator;
        firstAccumulator = hiddenBias_;
        secondAccumulator = hiddenBias_;
        addActiveFeatures(board, firstPerspective, firstAccumulator);
        addActiveFeatures(board, secondPerspective, secondAccumulator);

        int64_t output = outputBias_;
        for(size_t hidden = 0; hidden < hiddenSize_; hidden++){
            const int32_t first = std::clamp(firstAccumulator[hidden], 0, hiddenScale_);
            const int32_t second = std::clamp(secondAccumulator[hidden], 0, hiddenScale_);
            output += static_cast<int64_t>(first) * outputWeights_[hidden];
            output += static_cast<int64_t>(second) * outputWeights_[hiddenSize_ + hidden];
        }
        const int64_t divisor = static_cast<int64_t>(hiddenScale_) * outputScale_;
        return static_cast<int>(std::clamp<int64_t>(output / divisor, -32000, 32000));
    }

private:
    template<typename T>
    static bool readValue(std::ifstream& input, T& value){
        input.read(reinterpret_cast<char*>(&value), static_cast<std::streamsize>(sizeof(T)));
        return static_cast<bool>(input);
    }

    template<typename T>
    static bool readArray(std::ifstream& input, std::vector<T>& values){
        if(values.empty()) return true;
        input.read(reinterpret_cast<char*>(values.data()),
                   static_cast<std::streamsize>(values.size() * sizeof(T)));
        return static_cast<bool>(input);
    }

    static int pieceBucket(PieceType type){
        switch(type){
            case PieceType::Pawn: return 0;
            case PieceType::Knight: return 1;
            case PieceType::Bishop: return 2;
            case PieceType::Rook: return 3;
            case PieceType::Queen: return 4;
            default: return -1;
        }
    }

    static int orientSquare(int square, Color perspective){
        if(perspective == Color::White) return square;
        const int file = square % 8;
        const int rank = 7 - square / 8;
        return rank * 8 + file;
    }

    static size_t featureIndex(const Board& board, Color perspective, int pieceSquare, const Piece& piece){
        const int king = orientSquare(board.findKing(perspective), perspective);
        const int relativeColor = piece.c == perspective ? 0 : 1;
        const int bucket = relativeColor * 5 + pieceBucket(piece.t);
        const int square = orientSquare(pieceSquare, perspective);
        return static_cast<size_t>(king * 640 + bucket * 64 + square);
    }

    void addActiveFeatures(const Board& board, Color perspective, std::vector<int32_t>& accumulator) const {
        for(int square = 0; square < 64; square++){
            const Piece piece = board.b[static_cast<size_t>(square)];
            if(isNone(piece) || piece.t == PieceType::King) continue;
            const int bucket = pieceBucket(piece.t);
            if(bucket < 0) continue;
            const size_t feature = featureIndex(board, perspective, square, piece);
            const size_t offset = feature * hiddenSize_;
            for(size_t hidden = 0; hidden < hiddenSize_; hidden++){
                accumulator[hidden] += inputWeights_[offset + hidden];
            }
        }
    }

    u32 hiddenSize_ = 0;
    int32_t hiddenScale_ = 0;
    int32_t outputScale_ = 0;
    int32_t outputBias_ = 0;
    std::vector<int32_t> hiddenBias_;
    std::vector<int16_t> inputWeights_;
    std::vector<int16_t> outputWeights_;
    std::string source_;
};
