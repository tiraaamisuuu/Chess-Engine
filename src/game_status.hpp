#pragma once

#include "board.hpp"

enum class GameTermination {
    Ongoing,
    WhiteCheckmated,
    BlackCheckmated,
    Stalemate,
    InsufficientMaterial,
    FiftyMoveRule,
    ThreefoldRepetition
};

struct GameStatus {
    GameTermination termination = GameTermination::Ongoing;

    bool finished() const { return termination != GameTermination::Ongoing; }
    bool draw() const {
        return termination == GameTermination::Stalemate ||
               termination == GameTermination::InsufficientMaterial ||
               termination == GameTermination::FiftyMoveRule ||
               termination == GameTermination::ThreefoldRepetition;
    }
};

inline int positionOccurrenceCount(u64 hash, const std::vector<u64>& history){
    return static_cast<int>(std::count(history.begin(), history.end(), hash));
}

inline GameStatus assessGameStatus(Board& board, const std::vector<u64>& history){
    std::vector<Move> legal;
    board.genLegalMoves(legal);
    if(legal.empty()){
        if(board.inCheck(board.stm)){
            return GameStatus{board.stm == Color::White
                ? GameTermination::WhiteCheckmated
                : GameTermination::BlackCheckmated};
        }
        return GameStatus{GameTermination::Stalemate};
    }
    if(board.insufficientMaterial()) return GameStatus{GameTermination::InsufficientMaterial};
    if(board.halfmoveClock >= 100) return GameStatus{GameTermination::FiftyMoveRule};
    if(positionOccurrenceCount(board.hash, history) >= 3){
        return GameStatus{GameTermination::ThreefoldRepetition};
    }
    return GameStatus{};
}

inline const char* gameTerminationName(GameTermination termination){
    switch(termination){
        case GameTermination::WhiteCheckmated: return "White is checkmated";
        case GameTermination::BlackCheckmated: return "Black is checkmated";
        case GameTermination::Stalemate: return "Stalemate";
        case GameTermination::InsufficientMaterial: return "Insufficient material";
        case GameTermination::FiftyMoveRule: return "Fifty-move rule";
        case GameTermination::ThreefoldRepetition: return "Threefold repetition";
        default: return "Game in progress";
    }
}
