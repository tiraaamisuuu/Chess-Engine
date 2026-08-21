#pragma once

#include "board.hpp"

inline int seePieceValue(PieceType type){
    return type == PieceType::King ? 20000 : pieceValue(type);
}

inline int leastValuableAttacker(const std::array<Piece, 64>& pieces, Color side, int target){
    const int targetRank = target / 8;
    const int targetFile = target % 8;

    const int pawnRank = targetRank + (side == Color::White ? -1 : 1);
    if(pawnRank >= 0 && pawnRank < 8){
        for(const int fileOffset : {-1, 1}){
            const int file = targetFile + fileOffset;
            if(file < 0 || file > 7) continue;
            const int square = pawnRank * 8 + file;
            const Piece piece = pieces[static_cast<size_t>(square)];
            if(piece.c == side && piece.t == PieceType::Pawn) return square;
        }
    }

    static constexpr int knightOffsets[8][2] = {
        {1,2},{2,1},{-1,2},{-2,1},{1,-2},{2,-1},{-1,-2},{-2,-1}
    };
    for(const auto& offset : knightOffsets){
        const int file = targetFile + offset[0];
        const int rank = targetRank + offset[1];
        if(file < 0 || file > 7 || rank < 0 || rank > 7) continue;
        const int square = rank * 8 + file;
        const Piece piece = pieces[static_cast<size_t>(square)];
        if(piece.c == side && piece.t == PieceType::Knight) return square;
    }

    auto rayAttacker = [&](PieceType wanted, bool diagonal)->int{
        static constexpr int directions[8][2] = {
            {1,1},{1,-1},{-1,1},{-1,-1},{1,0},{-1,0},{0,1},{0,-1}
        };
        const int begin = diagonal ? 0 : 4;
        const int end = diagonal ? 4 : 8;
        for(int direction = begin; direction < end; direction++){
            int file = targetFile + directions[direction][0];
            int rank = targetRank + directions[direction][1];
            while(file >= 0 && file < 8 && rank >= 0 && rank < 8){
                const int square = rank * 8 + file;
                const Piece piece = pieces[static_cast<size_t>(square)];
                if(!isNone(piece)){
                    if(piece.c == side && piece.t == wanted) return square;
                    break;
                }
                file += directions[direction][0];
                rank += directions[direction][1];
            }
        }
        return -1;
    };

    int attacker = rayAttacker(PieceType::Bishop, true);
    if(attacker >= 0) return attacker;
    attacker = rayAttacker(PieceType::Rook, false);
    if(attacker >= 0) return attacker;
    attacker = rayAttacker(PieceType::Queen, true);
    if(attacker >= 0) return attacker;
    attacker = rayAttacker(PieceType::Queen, false);
    if(attacker >= 0) return attacker;

    for(int fileOffset = -1; fileOffset <= 1; fileOffset++){
        for(int rankOffset = -1; rankOffset <= 1; rankOffset++){
            if(fileOffset == 0 && rankOffset == 0) continue;
            const int file = targetFile + fileOffset;
            const int rank = targetRank + rankOffset;
            if(file < 0 || file > 7 || rank < 0 || rank > 7) continue;
            const int square = rank * 8 + file;
            const Piece piece = pieces[static_cast<size_t>(square)];
            if(piece.c == side && piece.t == PieceType::King) return square;
        }
    }
    return -1;
}

// Swap-off evaluation on the destination square. Pinned attackers are treated
// conservatively as available, so SEE is used for ordering rather than as a
// hard legality decision.
inline int staticExchangeEvaluation(const Board& board, const Move& move){
    if(!(move.isCapture || move.isEnPassant) && move.promo == PieceType::None) return 0;

    std::array<Piece, 64> pieces = board.b;
    Piece moving = pieces[move.from];
    if(isNone(moving)) return 0;

    int capturedValue = 0;
    if(move.isEnPassant){
        const int capturedSquare = static_cast<int>(move.to) + (moving.c == Color::White ? -8 : 8);
        capturedValue = seePieceValue(pieces[static_cast<size_t>(capturedSquare)].t);
        pieces[static_cast<size_t>(capturedSquare)] = Piece{};
    } else if(move.isCapture){
        capturedValue = seePieceValue(pieces[move.to].t);
    }

    std::array<int, 32> gains{};
    int depth = 0;
    gains[0] = capturedValue;
    if(move.promo != PieceType::None){
        gains[0] += seePieceValue(move.promo) - seePieceValue(PieceType::Pawn);
        moving.t = move.promo;
    }

    pieces[move.from] = Piece{};
    pieces[move.to] = moving;
    int occupantValue = seePieceValue(moving.t);
    Color side = other(moving.c);

    while(depth + 1 < static_cast<int>(gains.size())){
        const int attackerSquare = leastValuableAttacker(pieces, side, move.to);
        if(attackerSquare < 0) break;
        Piece attacker = pieces[static_cast<size_t>(attackerSquare)];
        int promotionGain = 0;
        const int targetRank = move.to / 8;
        if(attacker.t == PieceType::Pawn &&
           ((attacker.c == Color::White && targetRank == 7) ||
            (attacker.c == Color::Black && targetRank == 0))){
            promotionGain = seePieceValue(PieceType::Queen) - seePieceValue(PieceType::Pawn);
            attacker.t = PieceType::Queen;
        }

        depth++;
        gains[depth] = occupantValue + promotionGain - gains[depth - 1];
        pieces[static_cast<size_t>(attackerSquare)] = Piece{};
        pieces[move.to] = attacker;
        occupantValue = seePieceValue(attacker.t);
        side = other(side);
    }

    while(depth > 0){
        gains[depth - 1] = -std::max(-gains[depth - 1], gains[depth]);
        depth--;
    }
    return gains[0];
}
