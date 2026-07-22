#pragma once

#include "board.hpp"

struct TimeBudget {
    int softMs = 1000;
    int hardMs = 1000;
};

inline int countNonKingPieces(const Board& board){
    int pieces = 0;
    for(const Piece& p : board.b){
        if(isNone(p) || p.t == PieceType::King) continue;
        pieces++;
    }
    return pieces;
}

inline int positionComplexityScale(const Board& board, int legalMoves){
    if(legalMoves <= 1) return 35;

    int scale = 100;
    const int pieces = countNonKingPieces(board);
    if(board.inCheck(board.stm)) scale += 20;

    if(legalMoves >= 36) scale += 18;
    else if(legalMoves >= 28) scale += 10;
    else if(legalMoves <= 6) scale -= 18;
    else if(legalMoves <= 10) scale -= 10;

    if(pieces >= 20) scale += 8;
    else if(pieces <= 8) scale -= 8;

    return std::clamp(scale, 35, 145);
}

inline TimeBudget pickGuiTimeBudget(const Board& board, int requestedMs){
    Board probe = board;
    std::vector<Move> legal;
    probe.genLegalMoves(legal);
    const int legalMoves = static_cast<int>(legal.size());
    const int complexityScale = positionComplexityScale(board, legalMoves);

    const int base = std::clamp(requestedMs, 100, 180000);
    int soft = (base * complexityScale) / 100;
    soft = std::clamp(soft, std::max(100, base / 3), std::min(180000, base + base / 2));
    if(legalMoves <= 1) soft = std::min(soft, std::max(60, base / 5));
    const int hard = std::clamp(soft + std::max(250, soft / 4), soft, 180000);
    return TimeBudget{soft, hard};
}
