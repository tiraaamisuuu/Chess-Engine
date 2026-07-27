#pragma once

#include "board.hpp"
#include "nnue.hpp"

// ======================== Evaluation (PST + extras) ========================
inline int mirrorIndex(int idx){
    int f = idx%8, r=idx/8;
    int mr = 7-r;
    return mr*8 + f;
}

inline const int PST_PAWN[64]={
     0,  0,  0,  0,  0,  0,  0,  0,
    50, 50, 50, 55, 55, 50, 50, 50,
    10, 10, 20, 30, 30, 20, 10, 10,
     5,  5, 10, 25, 25, 10,  5,  5,
     0,  0,  0, 20, 20,  0,  0,  0,
     5, -5,-10,  0,  0,-10, -5,  5,
     5, 10, 10,-20,-20, 10, 10,  5,
     0,  0,  0,  0,  0,  0,  0,  0
};
inline const int PST_KNIGHT[64]={
   -50,-40,-30,-30,-30,-30,-40,-50,
   -40,-20,  0,  5,  5,  0,-20,-40,
   -30,  5, 10, 15, 15, 10,  5,-30,
   -30,  0, 15, 20, 20, 15,  0,-30,
   -30,  5, 15, 20, 20, 15,  5,-30,
   -30,  0, 10, 15, 15, 10,  0,-30,
   -40,-20,  0,  0,  0,  0,-20,-40,
   -50,-40,-30,-30,-30,-30,-40,-50
};
inline const int PST_BISHOP[64]={
   -20,-10,-10,-10,-10,-10,-10,-20,
   -10,  5,  0,  0,  0,  0,  5,-10,
   -10, 10, 10, 10, 10, 10, 10,-10,
   -10,  0, 10, 10, 10, 10,  0,-10,
   -10,  5,  5, 10, 10,  5,  5,-10,
   -10,  0,  5, 10, 10,  5,  0,-10,
   -10,  0,  0,  0,  0,  0,  0,-10,
   -20,-10,-10,-10,-10,-10,-10,-20
};
inline const int PST_ROOK[64]={
     0,  0,  5, 10, 10,  5,  0,  0,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
     5, 10, 10, 10, 10, 10, 10,  5,
     0,  0,  0,  0,  0,  0,  0,  0
};
inline const int PST_QUEEN[64]={
   -20,-10,-10, -5, -5,-10,-10,-20,
   -10,  0,  0,  0,  0,  0,  0,-10,
   -10,  0,  5,  5,  5,  5,  0,-10,
    -5,  0,  5,  5,  5,  5,  0, -5,
     0,  0,  5,  5,  5,  5,  0, -5,
   -10,  5,  5,  5,  5,  5,  0,-10,
   -10,  0,  5,  0,  0,  0,  0,-10,
   -20,-10,-10, -5, -5,-10,-10,-20
};
inline const int PST_KING_MG[64]={
   -30,-40,-40,-50,-50,-40,-40,-30,
   -30,-40,-40,-50,-50,-40,-40,-30,
   -30,-40,-40,-50,-50,-40,-40,-30,
   -30,-40,-40,-50,-50,-40,-40,-30,
   -20,-30,-30,-40,-40,-30,-30,-20,
   -10,-20,-20,-20,-20,-20,-20,-10,
    20, 20,  0,  0,  0,  0, 20, 20,
    20, 30, 10,  0,  0, 10, 30, 20
};
inline const int PST_KING_EG[64]={
   -50,-40,-30,-20,-20,-30,-40,-50,
   -30,-20,-10,  0,  0,-10,-20,-30,
   -30,-10, 20, 30, 30, 20,-10,-30,
   -30,-10, 30, 40, 40, 30,-10,-30,
   -30,-10, 30, 40, 40, 30,-10,-30,
   -30,-10, 20, 30, 30, 20,-10,-30,
   -30,-30,  0,  0,  0,  0,-30,-30,
   -50,-30,-30,-30,-30,-30,-30,-50
};

inline int pstScore(PieceType t, int idxWhitePerspective, int phase){
    switch(t){
        case PieceType::Pawn: return PST_PAWN[idxWhitePerspective];
        case PieceType::Knight: return PST_KNIGHT[idxWhitePerspective];
        case PieceType::Bishop: return PST_BISHOP[idxWhitePerspective];
        case PieceType::Rook: return PST_ROOK[idxWhitePerspective];
        case PieceType::Queen: return PST_QUEEN[idxWhitePerspective];
        case PieceType::King: {
            const int middleGame = PST_KING_MG[idxWhitePerspective];
            const int endGame = PST_KING_EG[idxWhitePerspective];
            return (middleGame * phase + endGame * (24 - phase)) / 24;
        }
        default: return 0;
    }
}

// Count pseudo-legal moves without constructing Move objects. Classical
// evaluation only needs the mobility total, so routing through genPseudoMoves
// paid for vector writes and full move metadata at every evaluated node.
inline int pseudoMobility(const Board& bd, Color us){
    int count = 0;
    for(int square = 0; square < 64; square++){
        const Piece piece = bd.b[static_cast<size_t>(square)];
        if(isNone(piece) || piece.c != us) continue;
        const int rank = square / 8;
        const int file = square % 8;

        if(piece.t == PieceType::Pawn){
            const int direction = us == Color::White ? 1 : -1;
            const int startRank = us == Color::White ? 1 : 6;
            const int promotionRank = us == Color::White ? 7 : 0;
            const int nextRank = rank + direction;
            if(nextRank >= 0 && nextRank < 8){
                const int one = nextRank * 8 + file;
                if(isNone(bd.b[static_cast<size_t>(one)])){
                    count += nextRank == promotionRank ? 4 : 1;
                    if(rank == startRank){
                        const int two = (rank + 2 * direction) * 8 + file;
                        if(isNone(bd.b[static_cast<size_t>(two)])) count++;
                    }
                }
                for(const int fileOffset : {-1, 1}){
                    const int targetFile = file + fileOffset;
                    if(targetFile < 0 || targetFile >= 8) continue;
                    const int target = nextRank * 8 + targetFile;
                    const Piece occupant = bd.b[static_cast<size_t>(target)];
                    if(!isNone(occupant) && occupant.c != us){
                        count += nextRank == promotionRank ? 4 : 1;
                    }
                    if(bd.epSquare == target){
                        const Piece adjacent = bd.b[static_cast<size_t>(rank * 8 + targetFile)];
                        if(adjacent.t == PieceType::Pawn && adjacent.c != us) count++;
                    }
                }
            }
            continue;
        }

        if(piece.t == PieceType::Knight){
            static constexpr int offsets[8][2] = {
                {1,2}, {2,1}, {-1,2}, {-2,1}, {1,-2}, {2,-1}, {-1,-2}, {-2,-1}
            };
            for(const auto& offset : offsets){
                const int targetFile = file + offset[0];
                const int targetRank = rank + offset[1];
                if(targetFile < 0 || targetFile >= 8 || targetRank < 0 || targetRank >= 8) continue;
                const Piece target = bd.b[static_cast<size_t>(targetRank * 8 + targetFile)];
                if(isNone(target) || target.c != us) count++;
            }
            continue;
        }

        if(piece.t == PieceType::Bishop || piece.t == PieceType::Rook || piece.t == PieceType::Queen){
            auto countRay = [&](int fileStep, int rankStep){
                int targetFile = file + fileStep;
                int targetRank = rank + rankStep;
                while(targetFile >= 0 && targetFile < 8 && targetRank >= 0 && targetRank < 8){
                    const Piece target = bd.b[static_cast<size_t>(targetRank * 8 + targetFile)];
                    if(isNone(target)){
                        count++;
                    } else {
                        if(target.c != us) count++;
                        break;
                    }
                    targetFile += fileStep;
                    targetRank += rankStep;
                }
            };
            if(piece.t == PieceType::Bishop || piece.t == PieceType::Queen){
                countRay(1, 1); countRay(1, -1); countRay(-1, 1); countRay(-1, -1);
            }
            if(piece.t == PieceType::Rook || piece.t == PieceType::Queen){
                countRay(1, 0); countRay(-1, 0); countRay(0, 1); countRay(0, -1);
            }
            continue;
        }

        if(piece.t == PieceType::King){
            for(int fileOffset = -1; fileOffset <= 1; fileOffset++){
                for(int rankOffset = -1; rankOffset <= 1; rankOffset++){
                    if(fileOffset == 0 && rankOffset == 0) continue;
                    const int targetFile = file + fileOffset;
                    const int targetRank = rank + rankOffset;
                    if(targetFile < 0 || targetFile >= 8 || targetRank < 0 || targetRank >= 8) continue;
                    const Piece target = bd.b[static_cast<size_t>(targetRank * 8 + targetFile)];
                    if(isNone(target) || target.c != us) count++;
                }
            }

            if(us == Color::White && square == 4){
                if((bd.castling & 0b0001) && isNone(bd.b[5]) && isNone(bd.b[6]) &&
                   bd.b[7].t == PieceType::Rook && bd.b[7].c == Color::White &&
                   !bd.inCheck(Color::White) && !bd.isSquareAttacked(5, Color::Black) &&
                   !bd.isSquareAttacked(6, Color::Black)) count++;
                if((bd.castling & 0b0010) && isNone(bd.b[3]) && isNone(bd.b[2]) && isNone(bd.b[1]) &&
                   bd.b[0].t == PieceType::Rook && bd.b[0].c == Color::White &&
                   !bd.inCheck(Color::White) && !bd.isSquareAttacked(3, Color::Black) &&
                   !bd.isSquareAttacked(2, Color::Black)) count++;
            } else if(us == Color::Black && square == 60){
                if((bd.castling & 0b0100) && isNone(bd.b[61]) && isNone(bd.b[62]) &&
                   bd.b[63].t == PieceType::Rook && bd.b[63].c == Color::Black &&
                   !bd.inCheck(Color::Black) && !bd.isSquareAttacked(61, Color::White) &&
                   !bd.isSquareAttacked(62, Color::White)) count++;
                if((bd.castling & 0b1000) && isNone(bd.b[59]) && isNone(bd.b[58]) && isNone(bd.b[57]) &&
                   bd.b[56].t == PieceType::Rook && bd.b[56].c == Color::Black &&
                   !bd.inCheck(Color::Black) && !bd.isSquareAttacked(59, Color::White) &&
                   !bd.isSquareAttacked(58, Color::White)) count++;
            }
        }
    }
    return count;
}

inline int evaluateClassical(const Board& bd){
    int material = 0;
    int pst = 0;

    int phase = 0;
    for(int i=0;i<64;i++){
        const Piece p = bd.b[i];
        if(isNone(p) || p.t==PieceType::King || p.t==PieceType::Pawn) continue;
        if(p.t==PieceType::Knight || p.t==PieceType::Bishop) phase += 1;
        else if(p.t==PieceType::Rook) phase += 2;
        else if(p.t==PieceType::Queen) phase += 4;
    }
    phase = std::clamp(phase, 0, 24);

    int whiteBishops = 0, blackBishops = 0;
    int wpFile[8]{}, bpFile[8]{};
    std::array<int, 64> wPawns{}, bPawns{}, wRooks{}, bRooks{};
    int wPawnCount = 0, bPawnCount = 0, wRookCount = 0, bRookCount = 0;

    for(int i=0;i<64;i++){
        const Piece p = bd.b[i];
        if(isNone(p)) continue;

        const int base = pieceValue(p.t);
        if(p.c==Color::White) material += base;
        else material -= base;

        // PST data is stored rank 8 first, while the board uses a1 == 0.
        const int idxW = (p.c==Color::White) ? mirrorIndex(i) : i;
        const int ps = pstScore(p.t, idxW, phase);
        if(p.c==Color::White) pst += ps;
        else pst -= ps;

        if(p.t==PieceType::Bishop){
            if(p.c==Color::White) whiteBishops++;
            else blackBishops++;
        } else if(p.t==PieceType::Pawn){
            const int f = i % 8;
            if(p.c==Color::White){
                wpFile[f]++;
                wPawns[static_cast<size_t>(wPawnCount++)] = i;
            } else {
                bpFile[f]++;
                bPawns[static_cast<size_t>(bPawnCount++)] = i;
            }
        } else if(p.t==PieceType::Rook){
            if(p.c==Color::White) wRooks[static_cast<size_t>(wRookCount++)] = i;
            else bRooks[static_cast<size_t>(bRookCount++)] = i;
        }
    }

    int bishopPair = 0;
    if(whiteBishops >= 2) bishopPair += 30;
    if(blackBishops >= 2) bishopPair -= 30;

    int pawnStruct = 0;
    for(int f=0; f<8; f++){
        if(wpFile[f] >= 2) pawnStruct -= 12 * (wpFile[f] - 1);
        if(bpFile[f] >= 2) pawnStruct += 12 * (bpFile[f] - 1);

        if(wpFile[f] > 0){
            const bool left = (f > 0 && wpFile[f-1] > 0);
            const bool right = (f < 7 && wpFile[f+1] > 0);
            if(!left && !right) pawnStruct -= 10;
        }
        if(bpFile[f] > 0){
            const bool left = (f > 0 && bpFile[f-1] > 0);
            const bool right = (f < 7 && bpFile[f+1] > 0);
            if(!left && !right) pawnStruct += 10;
        }
    }

    int passedPawns = 0;
    static const int passedBonusByRank[8] = {0, 5, 10, 20, 35, 60, 90, 0};
    for(int pawnIndex = 0; pawnIndex < wPawnCount; pawnIndex++){
        const int sq = wPawns[static_cast<size_t>(pawnIndex)];
        const int f = sq % 8;
        const int r = sq / 8;
        bool blocked = false;
        for(int rr = r + 1; rr < 8 && !blocked; rr++){
            for(int ff = std::max(0, f - 1); ff <= std::min(7, f + 1); ff++){
                const Piece p = bd.b[rr*8 + ff];
                if(!isNone(p) && p.c==Color::Black && p.t==PieceType::Pawn){
                    blocked = true;
                    break;
                }
            }
        }
        if(!blocked){
            passedPawns += passedBonusByRank[r];
        }
    }
    for(int pawnIndex = 0; pawnIndex < bPawnCount; pawnIndex++){
        const int sq = bPawns[static_cast<size_t>(pawnIndex)];
        const int f = sq % 8;
        const int r = sq / 8;
        bool blocked = false;
        for(int rr = r - 1; rr >= 0 && !blocked; rr--){
            for(int ff = std::max(0, f - 1); ff <= std::min(7, f + 1); ff++){
                const Piece p = bd.b[rr*8 + ff];
                if(!isNone(p) && p.c==Color::White && p.t==PieceType::Pawn){
                    blocked = true;
                    break;
                }
            }
        }
        if(!blocked){
            const int progress = 7 - r;
            passedPawns -= passedBonusByRank[progress];
        }
    }

    int rookFiles = 0;
    auto rookFileBonus = [&](int sq, Color c)->int{
        const int f = sq % 8;
        const int ownPawns = (c==Color::White) ? wpFile[f] : bpFile[f];
        const int oppPawns = (c==Color::White) ? bpFile[f] : wpFile[f];
        if(ownPawns == 0 && oppPawns == 0) return 24;
        if(ownPawns == 0) return 12;
        return 0;
    };
    for(int rookIndex = 0; rookIndex < wRookCount; rookIndex++){
        rookFiles += rookFileBonus(wRooks[static_cast<size_t>(rookIndex)], Color::White);
    }
    for(int rookIndex = 0; rookIndex < bRookCount; rookIndex++){
        rookFiles -= rookFileBonus(bRooks[static_cast<size_t>(rookIndex)], Color::Black);
    }

    const int mobility = (pseudoMobility(bd, Color::White) - pseudoMobility(bd, Color::Black)) * 2;

    int kingSafety = 0;
    if(phase > 0){
        const int wK = bd.findKing(Color::White);
        const int bK = bd.findKing(Color::Black);

        auto kingCentrePenalty = [&](int kIdx)->int{
            if(kIdx < 0) return 0;
            const int f = kIdx % 8;
            const int r = kIdx / 8;
            const int df = std::abs(f - 4);
            int pen = 0;
            if(df <= 1 && (r==0 || r==7)) pen += 10;
            if(df <= 1 && (r==1 || r==6)) pen += 20;
            if(df <= 1 && (r==2 || r==5)) pen += 35;
            return pen;
        };

        auto kingShieldScore = [&](int kIdx, Color c)->int{
            if(kIdx < 0) return 0;
            const int f = kIdx % 8;
            const int r = kIdx / 8;
            const int dir = (c==Color::White) ? 1 : -1;
            const int sr = r + dir;
            if(sr < 0 || sr > 7) return 0;

            int score = 0;
            for(int df = -1; df <= 1; df++){
                const int nf = f + df;
                if(nf < 0 || nf > 7) continue;
                const Piece p = bd.b[sr*8 + nf];
                if(!isNone(p) && p.c==c && p.t==PieceType::Pawn) score += 8;
                else score -= 6;
            }
            return score;
        };

        kingSafety -= kingCentrePenalty(wK);
        kingSafety += kingCentrePenalty(bK);
        kingSafety += kingShieldScore(wK, Color::White);
        kingSafety -= kingShieldScore(bK, Color::Black);

        const bool wCanCastle = (bd.castling & 0b0011) != 0;
        const bool bCanCastle = (bd.castling & 0b1100) != 0;
        if(!wCanCastle) kingSafety -= 10;
        if(!bCanCastle) kingSafety += 10;
        kingSafety = (kingSafety * phase) / 24;
    }

    int scoreWhite = material + pst + bishopPair + pawnStruct + passedPawns + rookFiles + mobility + kingSafety;
    return (bd.stm==Color::White) ? scoreWhite : -scoreWhite;
}

class PositionEvaluator {
public:
    int evaluate(const Board& board) const {
        return useNnue_ && nnue_.loaded() ? nnue_.evaluate(board) : evaluateClassical(board);
    }

    int evaluate(const Board& board, const NnueAccumulator& accumulator) const {
        return useNnue_ && nnue_.loaded()
            ? nnue_.evaluate(board, accumulator)
            : evaluateClassical(board);
    }

    void refreshAccumulator(const Board& board, NnueAccumulator& accumulator) const {
        if(usingNnue()) nnue_.refresh(board, accumulator);
        else accumulator.valid = false;
    }

    void updateAccumulator(const Board& board, const Undo& undo,
                           const NnueAccumulator& parent, NnueAccumulator& child) const {
        if(usingNnue()) nnue_.updateAfterMove(board, undo, parent, child);
        else child.valid = false;
    }

    void copyAccumulator(const NnueAccumulator& source, NnueAccumulator& destination) const {
        if(usingNnue()) nnue_.copyAccumulator(source, destination);
        else destination.valid = false;
    }

    bool loadNnue(const std::filesystem::path& path, std::string* error = nullptr){
        const bool loaded = nnue_.load(path, error);
        if(!loaded) useNnue_ = false;
        return loaded;
    }

    bool setUseNnue(bool enabled){
        useNnue_ = enabled && nnue_.loaded();
        return useNnue_ == enabled;
    }

    bool usingNnue() const { return useNnue_ && nnue_.loaded(); }
    bool hasNnue() const { return nnue_.loaded(); }
    const NnueNetwork& nnue() const { return nnue_; }

private:
    NnueNetwork nnue_;
    bool useNnue_ = false;
};
