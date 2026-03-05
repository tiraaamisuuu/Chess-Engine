#pragma once

#include "board.hpp"

// ======================== Evaluation (PST + extras) ========================
static int mirrorIndex(int idx){
    int f = idx%8, r=idx/8;
    int mr = 7-r;
    return mr*8 + f;
}

static const int PST_PAWN[64]={
     0,  0,  0,  0,  0,  0,  0,  0,
    50, 50, 50, 55, 55, 50, 50, 50,
    10, 10, 20, 30, 30, 20, 10, 10,
     5,  5, 10, 25, 25, 10,  5,  5,
     0,  0,  0, 20, 20,  0,  0,  0,
     5, -5,-10,  0,  0,-10, -5,  5,
     5, 10, 10,-20,-20, 10, 10,  5,
     0,  0,  0,  0,  0,  0,  0,  0
};
static const int PST_KNIGHT[64]={
   -50,-40,-30,-30,-30,-30,-40,-50,
   -40,-20,  0,  5,  5,  0,-20,-40,
   -30,  5, 10, 15, 15, 10,  5,-30,
   -30,  0, 15, 20, 20, 15,  0,-30,
   -30,  5, 15, 20, 20, 15,  5,-30,
   -30,  0, 10, 15, 15, 10,  0,-30,
   -40,-20,  0,  0,  0,  0,-20,-40,
   -50,-40,-30,-30,-30,-30,-40,-50
};
static const int PST_BISHOP[64]={
   -20,-10,-10,-10,-10,-10,-10,-20,
   -10,  5,  0,  0,  0,  0,  5,-10,
   -10, 10, 10, 10, 10, 10, 10,-10,
   -10,  0, 10, 10, 10, 10,  0,-10,
   -10,  5,  5, 10, 10,  5,  5,-10,
   -10,  0,  5, 10, 10,  5,  0,-10,
   -10,  0,  0,  0,  0,  0,  0,-10,
   -20,-10,-10,-10,-10,-10,-10,-20
};
static const int PST_ROOK[64]={
     0,  0,  5, 10, 10,  5,  0,  0,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
     5, 10, 10, 10, 10, 10, 10,  5,
     0,  0,  0,  0,  0,  0,  0,  0
};
static const int PST_QUEEN[64]={
   -20,-10,-10, -5, -5,-10,-10,-20,
   -10,  0,  0,  0,  0,  0,  0,-10,
   -10,  0,  5,  5,  5,  5,  0,-10,
    -5,  0,  5,  5,  5,  5,  0, -5,
     0,  0,  5,  5,  5,  5,  0, -5,
   -10,  5,  5,  5,  5,  5,  0,-10,
   -10,  0,  5,  0,  0,  0,  0,-10,
   -20,-10,-10, -5, -5,-10,-10,-20
};
static const int PST_KING_MG[64]={
   -30,-40,-40,-50,-50,-40,-40,-30,
   -30,-40,-40,-50,-50,-40,-40,-30,
   -30,-40,-40,-50,-50,-40,-40,-30,
   -30,-40,-40,-50,-50,-40,-40,-30,
   -20,-30,-30,-40,-40,-30,-30,-20,
   -10,-20,-20,-20,-20,-20,-20,-10,
    20, 20,  0,  0,  0,  0, 20, 20,
    20, 30, 10,  0,  0, 10, 30, 20
};
static const int PST_KING_EG[64]={
   -50,-40,-30,-20,-20,-30,-40,-50,
   -30,-20,-10,  0,  0,-10,-20,-30,
   -30,-10, 20, 30, 30, 20,-10,-30,
   -30,-10, 30, 40, 40, 30,-10,-30,
   -30,-10, 30, 40, 40, 30,-10,-30,
   -30,-10, 20, 30, 30, 20,-10,-30,
   -30,-30,  0,  0,  0,  0,-30,-30,
   -50,-30,-30,-30,-30,-30,-30,-50
};

static int pstScore(PieceType t, int idxWhitePerspective, bool endgameKing){
    switch(t){
        case PieceType::Pawn: return PST_PAWN[idxWhitePerspective];
        case PieceType::Knight: return PST_KNIGHT[idxWhitePerspective];
        case PieceType::Bishop: return PST_BISHOP[idxWhitePerspective];
        case PieceType::Rook: return PST_ROOK[idxWhitePerspective];
        case PieceType::Queen: return PST_QUEEN[idxWhitePerspective];
        case PieceType::King: return endgameKing ? PST_KING_EG[idxWhitePerspective] : PST_KING_MG[idxWhitePerspective];
        default: return 0;
    }
}

static int evaluate(const Board& bd){
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
    const bool endgameKing = (phase <= 8);

    int whiteBishops = 0, blackBishops = 0;
    int wpFile[8]{}, bpFile[8]{};
    std::vector<int> wPawns, bPawns, wRooks, bRooks;
    wPawns.reserve(8); bPawns.reserve(8); wRooks.reserve(2); bRooks.reserve(2);

    for(int i=0;i<64;i++){
        const Piece p = bd.b[i];
        if(isNone(p)) continue;

        const int base = pieceValue(p.t);
        if(p.c==Color::White) material += base;
        else material -= base;

        const int idxW = (p.c==Color::White) ? i : mirrorIndex(i);
        const int ps = pstScore(p.t, idxW, endgameKing);
        if(p.c==Color::White) pst += ps;
        else pst -= ps;

        if(p.t==PieceType::Bishop){
            if(p.c==Color::White) whiteBishops++;
            else blackBishops++;
        } else if(p.t==PieceType::Pawn){
            const int f = i % 8;
            if(p.c==Color::White){
                wpFile[f]++;
                wPawns.push_back(i);
            } else {
                bpFile[f]++;
                bPawns.push_back(i);
            }
        } else if(p.t==PieceType::Rook){
            if(p.c==Color::White) wRooks.push_back(i);
            else bRooks.push_back(i);
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
    for(int sq : wPawns){
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
    for(int sq : bPawns){
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
    for(int sq : wRooks) rookFiles += rookFileBonus(sq, Color::White);
    for(int sq : bRooks) rookFiles -= rookFileBonus(sq, Color::Black);

    int mobility = 0;
    {
        Board t = bd;
        t.stm = Color::White;
        std::vector<Move> w; t.genPseudoMoves(w);
        t.stm = Color::Black;
        std::vector<Move> b; t.genPseudoMoves(b);
        mobility = (int(w.size()) - int(b.size())) * 2;
    }

    int kingSafety = 0;
    if(!endgameKing){
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
    }

    int scoreWhite = material + pst + bishopPair + pawnStruct + passedPawns + rookFiles + mobility + kingSafety;
    return (bd.stm==Color::White) ? scoreWhite : -scoreWhite;
}

// ======================== Search (ID + TT + QS + Ordering) ========================
struct SearchStats {
    u64 nodes=0;
    u64 qnodes=0;
    int depthReached=0;
    int bestScore=0;
    int timeMs=0;
    int configuredThreads=1;
    int workersUsed=1;
    int hardwareThreads=1;
};

struct SearchContext {
    TranspositionTable tt;
    SearchStats stats;
    std::chrono::steady_clock::time_point start;
    int timeLimitMs=1000;
    bool stop=false;
    const std::atomic<bool>* abortFlag=nullptr;

    Move killer[128][2]{};
    Move countermove[2][64][64]{};
    Move plyMove[128]{};
    int history[2][64][64]{};
    int captureHistory[2][7][64]{};
    int staticEvalByPly[128]{};
    std::vector<u64> gameHistory; // position hashes from actual game (includes current root)
    std::vector<u64> repetition;
};

static bool sameMove(const Move& a, const Move& b){
    return a.from==b.from && a.to==b.to && a.promo==b.promo && a.isCastle==b.isCastle && a.isEnPassant==b.isEnPassant;
}

static Move invalidMove(){
    Move m{};
    m.from = 64;
    m.to = 64;
    return m;
}

static int mvvLvaScore(const Board& bd, const Move& m){
    Piece a = bd.at(m.from);
    int attacker = pieceValue(a.t);
    int victim = 0;
    if(m.isEnPassant){
        victim = pieceValue(PieceType::Pawn);
    } else if(m.isCapture){
        Piece v = bd.at(m.to);
        victim = pieceValue(v.t);
    }
    return victim*10 - attacker;
}

static int scoreMove(const Board& bd, SearchContext& ctx, const Move& m, const Move& ttMove, int ply, const Move& prevMove){
    if(ttMove.from < 64 && ttMove.from==m.from && ttMove.to==m.to && ttMove.promo==m.promo) return 1000000;

    const int side = (bd.stm==Color::White)?0:1;
    const Piece attacker = bd.at(m.from);
    const int attackerType = std::clamp(int(attacker.t), 0, 6);

    if(m.promo != PieceType::None){
        int s = 140000 + pieceValue(m.promo) * 4;
        if(m.isCapture || m.isEnPassant){
            s += mvvLvaScore(bd, m);
            s += ctx.captureHistory[side][attackerType][m.to];
        }
        return s;
    }

    if(m.isCapture || m.isEnPassant){
        return 100000 + mvvLvaScore(bd, m) + ctx.captureHistory[side][attackerType][m.to];
    }

    if(ply<128){
        if(sameMove(m, ctx.killer[ply][0])) return 90000;
        if(sameMove(m, ctx.killer[ply][1])) return 80000;
    }

    if(prevMove.from < 64 && prevMove.to < 64){
        const Move& cm = ctx.countermove[side][prevMove.from][prevMove.to];
        if(sameMove(m, cm)) return 85000;
    }
    return ctx.history[side][m.from][m.to];
}

static inline bool timeUp(SearchContext& ctx){
    if(ctx.stop) return true;
    if(ctx.abortFlag && ctx.abortFlag->load(std::memory_order_relaxed)){
        ctx.stop = true;
        return true;
    }
    auto now = std::chrono::steady_clock::now();
    int ms = (int)std::chrono::duration_cast<std::chrono::milliseconds>(now - ctx.start).count();
    if(ms >= ctx.timeLimitMs){
        ctx.stop=true;
        return true;
    }
    return false;
}

static const int INF = 100000000;
static const int MATE = 1000000;

static int scoreToTT(int score, int ply){
    if(score >= MATE - 10000) return score + ply;
    if(score <= -MATE + 10000) return score - ply;
    return score;
}

static int scoreFromTT(int score, int ply){
    if(score >= MATE - 10000) return score - ply;
    if(score <= -MATE + 10000) return score + ply;
    return score;
}

static bool hasNonPawnMaterial(const Board& bd, Color side){
    for(const auto& p : bd.b){
        if(isNone(p) || p.c != side) continue;
        if(p.t != PieceType::King && p.t != PieceType::Pawn) return true;
    }
    return false;
}

static int nonKingPieceCount(const Board& bd, Color side){
    int count = 0;
    for(const auto& p : bd.b){
        if(isNone(p) || p.c != side || p.t == PieceType::King) continue;
        count++;
    }
    return count;
}

static bool isThreefoldRepetition(const Board& bd, const SearchContext& ctx){
    if(ctx.repetition.empty()) return false;

    const size_t n = ctx.repetition.size();
    const size_t maxBack = std::min<size_t>(size_t(std::max(0, bd.halfmoveClock)), n - 1);

    int seen = 0;
    for(size_t back = 0; back <= maxBack; back++){
        const size_t idx = n - 1 - back;
        if(ctx.repetition[idx] == bd.hash){
            seen++;
            if(seen >= 3) return true;
        }
    }
    return false;
}

static int quiescence(Board& bd, SearchContext& ctx, int alpha, int beta, int ply){
    if(timeUp(ctx)) return 0;
    ctx.stats.qnodes++;

    alpha = std::max(alpha, -MATE + ply);
    beta = std::min(beta, MATE - ply - 1);
    if(alpha >= beta) return alpha;

    if(isThreefoldRepetition(bd, ctx)) return 0;

    const bool inCheck = bd.inCheck(bd.stm);
    if(inCheck){
        std::vector<Move> evasions;
        bd.genLegalMoves(evasions);
        if(evasions.empty()) return -MATE + ply;

        int best = -INF;
        for(const Move& m : evasions){
            Undo u{};
            if(!bd.makeMove(m, u)) continue;
            ctx.repetition.push_back(bd.hash);
            int score = -quiescence(bd, ctx, -beta, -alpha, ply + 1);
            ctx.repetition.pop_back();
            bd.undoMove(u);

            if(score > best) best = score;
            if(score > alpha) alpha = score;
            if(alpha >= beta) return score;
        }
        return best;
    }

    int stand = evaluate(bd);
    if(stand >= beta) return stand;
    if(stand > alpha) alpha = stand;

    std::vector<Move> pseudo;
    bd.genPseudoMoves(pseudo);

    std::vector<Move> moves;
    moves.reserve(pseudo.size());
    for(const Move& m : pseudo){
        if(!(m.isCapture || m.isEnPassant || m.promo != PieceType::None)) continue;

        if(!m.isEnPassant && m.promo == PieceType::None){
            int victim = 0;
            if(m.isCapture){
                const Piece v = bd.at(m.to);
                victim = pieceValue(v.t);
            }
            const int deltaMargin = 120;
            if(stand + victim + deltaMargin < alpha){
                continue;
            }
        }
        moves.push_back(m);
    }

    std::sort(moves.begin(), moves.end(), [&](const Move& a, const Move& b){
        auto tactical = [&](const Move& m){
            int s = mvvLvaScore(bd, m);
            if(m.promo != PieceType::None) s += 2000 + pieceValue(m.promo);
            return s;
        };
        return tactical(a) > tactical(b);
    });

    for(const Move& m : moves){
        Undo u{};
        if(!bd.makeMove(m, u)) continue;
        ctx.repetition.push_back(bd.hash);
        int score = -quiescence(bd, ctx, -beta, -alpha, ply + 1);
        ctx.repetition.pop_back();
        bd.undoMove(u);

        if(score >= beta) return score;
        if(score > alpha) alpha = score;
    }

    return alpha;
}

static int negamax(Board& bd, SearchContext& ctx, int depth, int alpha, int beta, int ply, const Move& prevMove, bool allowNullMove){
    if(timeUp(ctx)) return 0;
    ctx.stats.nodes++;

    // Mate-distance pruning keeps mate scores consistent and trims impossible windows.
    alpha = std::max(alpha, -MATE + ply);
    beta = std::min(beta, MATE - ply - 1);
    if(alpha >= beta) return alpha;
    const bool pvNode = (beta - alpha) > 1;

    if(bd.insufficientMaterial()) return 0;
    if(bd.halfmoveClock >= 100) return 0;
    if(isThreefoldRepetition(bd, ctx)) return 0;

    bool inCheck = bd.inCheck(bd.stm);
    int staticEval = 0;
    if(!inCheck){
        staticEval = evaluate(bd);
        if(ply < 128) ctx.staticEvalByPly[ply] = staticEval;
    } else if(ply < 128){
        ctx.staticEvalByPly[ply] = -INF;
    }

    bool improving = false;
    if(!inCheck && ply >= 2 && ply < 128){
        improving = staticEval > ctx.staticEvalByPly[ply - 2];
    }

    if(!inCheck && depth <= 3){
        const int rfpMargin = 95 * depth;
        if(staticEval - rfpMargin >= beta){
            return staticEval;
        }
    }

    Move ttMove = invalidMove();
    if(auto* e = ctx.tt.probe(bd.hash)){
        if(e->key==bd.hash){
            ttMove = e->best;
            if(e->depth >= depth){
                int s = scoreFromTT(e->score, ply);
                if(e->flag==TTFlag::Exact) return s;
                if(e->flag==TTFlag::Lower) alpha = std::max(alpha, s);
                else if(e->flag==TTFlag::Upper) beta = std::min(beta, s);
                if(alpha >= beta) return s;
            }
        }
    }

    if(!inCheck && depth >= 6 && ttMove.from >= 64){
        const int iidDepth = std::max(1, depth - 2);
        (void)negamax(bd, ctx, iidDepth, alpha, beta, ply, prevMove, false);
        if(ctx.stop) return 0;
        if(auto* e = ctx.tt.probe(bd.hash)){
            if(e->key == bd.hash){
                ttMove = e->best;
            }
        }
    }

    if(depth <= 0){
        return quiescence(bd, ctx, alpha, beta, ply);
    }

    if(!inCheck && !pvNode && depth <= 2){
        const int razorMargin = 180 + 120 * depth;
        if(staticEval + razorMargin <= alpha){
            return quiescence(bd, ctx, alpha, beta, ply);
        }
    }

    // Null-move pruning: aggressive cut when position is quiet enough and side has material.
    if(allowNullMove && !pvNode && depth >= 3 && !inCheck &&
       hasNonPawnMaterial(bd, bd.stm) && nonKingPieceCount(bd, bd.stm) >= 2){
        Board nb = bd;
        nb.epSquare = -1;
        nb.stm = other(nb.stm);
        nb.recomputeHash();
        ctx.repetition.push_back(nb.hash);
        int reduction = 2 + depth / 4;
        if(depth >= 7) reduction++;
        if(!improving) reduction++;
        if(staticEval >= beta + 120) reduction++;
        reduction = std::clamp(reduction, 2, std::max(2, depth - 2));
        int score = -negamax(nb, ctx, depth - 1 - reduction, -beta, -beta + 1, ply + 1, invalidMove(), false);
        ctx.repetition.pop_back();
        if(ctx.stop) return 0;
        if(score >= beta){
            if(depth >= 6){
                const int verifyDepth = std::max(0, depth - 1 - reduction);
                int verify = negamax(bd, ctx, verifyDepth, beta - 1, beta, ply, prevMove, false);
                if(ctx.stop) return 0;
                if(verify >= beta) return beta;
            } else {
                return beta;
            }
        }
    }

    std::vector<Move> moves;
    bd.genLegalMoves(moves);

    if(moves.empty()){
        if(bd.inCheck(bd.stm)) return -MATE + ply;
        return 0;
    }

    if(inCheck && moves.size() == 1 && depth < 10){
        depth++;
    }

    std::sort(moves.begin(), moves.end(), [&](const Move& a, const Move& b){
        return scoreMove(bd, ctx, a, ttMove, ply, prevMove) > scoreMove(bd, ctx, b, ttMove, ply, prevMove);
    });

    int best = -INF;
    Move bestM{};

    int originalAlpha = alpha;
    const int side = (bd.stm==Color::White)?0:1;
    std::vector<Move> quietTried;
    std::vector<Move> tacticalTried;
    quietTried.reserve(moves.size());
    tacticalTried.reserve(moves.size());

    for(size_t i=0;i<moves.size();i++){
        const Move& m = moves[i];
        bool isQuiet = !(m.isCapture || m.isEnPassant) && (m.promo==PieceType::None);

        if(!inCheck && !pvNode && isQuiet){
            if(depth <= 3){
                const size_t futilitySkipAfter = size_t(4 + depth * 3);
                const int futilityMargin = 90 + 120 * depth + (improving ? 20 : 0);
                if(i >= futilitySkipAfter && staticEval + futilityMargin <= alpha){
                    continue;
                }
            }

            if(depth <= 4){
                const size_t lmpThreshold = size_t(3 + depth * depth + (improving ? 2 : 0));
                if(i >= lmpThreshold){
                    continue;
                }
            }
        }

        if(isQuiet) quietTried.push_back(m);
        else tacticalTried.push_back(m);

        Undo u{};
        if(!bd.makeMove(m,u)) continue;

        if(ply < 128) ctx.plyMove[ply] = m;
        ctx.repetition.push_back(bd.hash);

        int newDepth = depth - 1;
        bool givesCheck = bd.inCheck(bd.stm);
        if(givesCheck){
            newDepth++;
        }
        int score = 0;

        int reduction = 0;
        if(newDepth >= 3 && i >= 3 && isQuiet && !givesCheck){
            reduction = 1;
            if(i >= 4) reduction++;
            if(i >= 8) reduction++;
            if(newDepth >= 5) reduction++;
            if(newDepth >= 8 && i >= 12) reduction++;
            if(!improving) reduction++;
            if(pvNode) reduction--;
            if(sameMove(m, ctx.killer[ply][0])) reduction--;
            if(ctx.history[side][m.from][m.to] > 18000) reduction -= 2;
            else if(ctx.history[side][m.from][m.to] > 9000) reduction--;
            reduction = std::clamp(reduction, 0, std::max(0, newDepth - 1));
        }

        if(i == 0){
            score = -negamax(bd, ctx, newDepth, -beta, -alpha, ply + 1, m, true);
        } else {
            // PVS + LMR: search late quiet moves reduced on a null-window first.
            const int scoutDepth = std::max(0, newDepth - reduction);
            score = -negamax(bd, ctx, scoutDepth, -alpha - 1, -alpha, ply + 1, m, true);

            if(!ctx.stop && reduction > 0 && score > alpha){
                score = -negamax(bd, ctx, newDepth, -alpha - 1, -alpha, ply + 1, m, true);
            }

            if(!ctx.stop && score > alpha && score < beta){
                score = -negamax(bd, ctx, newDepth, -beta, -alpha, ply + 1, m, true);
            }
        }

        ctx.repetition.pop_back();
        bd.undoMove(u);

        if(ctx.stop) return 0;

        if(score > best){
            best = score;
            bestM = m;
        }

        alpha = std::max(alpha, score);
        if(alpha >= beta){
            if(isQuiet && ply<128){
                if(!sameMove(ctx.killer[ply][0], m)){
                    ctx.killer[ply][1] = ctx.killer[ply][0];
                    ctx.killer[ply][0] = m;
                }
                const int bonus = depth * depth * 16;
                ctx.history[side][m.from][m.to] = std::min(90000, ctx.history[side][m.from][m.to] + bonus);
                for(const Move& qm : quietTried){
                    if(sameMove(qm, m)) continue;
                    ctx.history[side][qm.from][qm.to] = std::max(-90000, ctx.history[side][qm.from][qm.to] - (bonus / 2));
                }
                if(prevMove.from < 64 && prevMove.to < 64){
                    ctx.countermove[side][prevMove.from][prevMove.to] = m;
                }
            } else if(ply < 128){
                const Piece attacker = bd.at(m.from);
                const int attackerType = std::clamp(int(attacker.t), 0, 6);
                const int bonus = depth * depth * 20;
                int& slot = ctx.captureHistory[side][attackerType][m.to];
                slot = std::min(90000, slot + bonus);

                for(const Move& tm : tacticalTried){
                    if(sameMove(tm, m)) continue;
                    const Piece triedPiece = bd.at(tm.from);
                    const int triedType = std::clamp(int(triedPiece.t), 0, 6);
                    int& penaltySlot = ctx.captureHistory[side][triedType][tm.to];
                    penaltySlot = std::max(-90000, penaltySlot - (bonus / 3));
                }
            }
            break;
        }
    }

    TTFlag flag = TTFlag::Exact;
    if(best <= originalAlpha) flag = TTFlag::Upper;
    else if(best >= beta) flag = TTFlag::Lower;
    ctx.tt.store(bd.hash, depth, scoreToTT(best, ply), flag, bestM);

    return best;
}

static Move searchBestMoveSingle(Board& bd, SearchContext& ctx, int maxDepth, int timeLimitMs){
    ctx.stats = {};
    ctx.stats.configuredThreads = 1;
    ctx.stats.workersUsed = 1;
    ctx.stats.hardwareThreads = std::max(1, int(std::thread::hardware_concurrency()));
    ctx.start = std::chrono::steady_clock::now();
    ctx.timeLimitMs = timeLimitMs;
    ctx.stop = false;
    ctx.tt.newSearch();
    const Move noMove = invalidMove();

    for(int s=0; s<2; s++){
        for(int from=0; from<64; from++){
            for(int to=0; to<64; to++){
                ctx.history[s][from][to] = (ctx.history[s][from][to] * 7) / 8;
            }
        }
        for(int pt=0; pt<7; pt++){
            for(int to=0; to<64; to++){
                ctx.captureHistory[s][pt][to] = (ctx.captureHistory[s][pt][to] * 7) / 8;
            }
        }
    }
    for(int ply = 0; ply < 128; ply++){
        ctx.plyMove[ply] = noMove;
        ctx.staticEvalByPly[ply] = -INF;
    }

    ctx.repetition = ctx.gameHistory;
    if(ctx.repetition.empty() || ctx.repetition.back() != bd.hash){
        ctx.repetition.push_back(bd.hash);
    }

    std::vector<Move> rootMoves;
    bd.genLegalMoves(rootMoves);
    if(rootMoves.empty()) return Move{};

    Move bestMove = rootMoves[0];
    int bestScore = 0;

    for(int d=1; d<=maxDepth; d++){
        if(timeUp(ctx)) break;

        int window = INF;
        if(d >= 3 && std::abs(bestScore) < MATE / 2){
            window = 40;
        }

        int acceptedScore = -INF;
        Move acceptedMove = bestMove;

        while(!ctx.stop){
            if(timeUp(ctx)) break;

            int alpha = -INF;
            int beta = INF;
            const bool aspiration = (window < INF);
            if(aspiration){
                alpha = bestScore - window;
                beta = bestScore + window;
            }

            Move ttMove = noMove;
            if(auto* e = ctx.tt.probe(bd.hash)){
                if(e->key==bd.hash) ttMove = e->best;
            }

            std::sort(rootMoves.begin(), rootMoves.end(), [&](const Move& a, const Move& b){
                auto rootOrderScore = [&](const Move& m){
                    int s = scoreMove(bd, ctx, m, ttMove, 0, noMove);
                    if(sameMove(m, bestMove)) s += 200000;
                    return s;
                };
                return rootOrderScore(a) > rootOrderScore(b);
            });

            int localBest = -INF;
            Move localMove = rootMoves[0];
            int alphaRun = alpha;

            for(size_t i=0; i<rootMoves.size(); i++){
                const Move& m = rootMoves[i];
                if(timeUp(ctx)) break;
                Undo u{};
                if(!bd.makeMove(m,u)) continue;

                ctx.repetition.push_back(bd.hash);
                int score = 0;
                if(i == 0){
                    score = -negamax(bd, ctx, d - 1, -beta, -alphaRun, 1, m, true);
                } else {
                    score = -negamax(bd, ctx, d - 1, -alphaRun - 1, -alphaRun, 1, m, true);
                    if(!ctx.stop && score > alphaRun && score < beta){
                        score = -negamax(bd, ctx, d - 1, -beta, -alphaRun, 1, m, true);
                    }
                }
                ctx.repetition.pop_back();
                bd.undoMove(u);

                if(ctx.stop) break;

                if(score > localBest){
                    localBest = score;
                    localMove = m;
                }
                alphaRun = std::max(alphaRun, score);
                if(alphaRun >= beta){
                    break;
                }
            }

            if(ctx.stop) break;
            if(localBest == -INF) break;

            const bool failLow = aspiration && (localBest <= alpha);
            const bool failHigh = aspiration && (localBest >= beta);
            if(failLow || failHigh){
                if(window >= INF / 4){
                    window = INF;
                } else {
                    window *= 2;
                }
                continue;
            }

            acceptedScore = localBest;
            acceptedMove = localMove;
            break;
        }

        if(!ctx.stop && acceptedScore != -INF){
            bestScore = acceptedScore;
            bestMove = acceptedMove;
            ctx.stats.depthReached = d;
            ctx.stats.bestScore = bestScore;
        }
    }

    auto end = std::chrono::steady_clock::now();
    ctx.stats.timeMs = (int)std::chrono::duration_cast<std::chrono::milliseconds>(end - ctx.start).count();
    return bestMove;
}

static Move searchBestMoveParallel(Board& bd, SearchContext& ctx, int maxDepth, int timeLimitMs, int threadCount){
    ctx.stats = {};
    ctx.stats.configuredThreads = std::max(1, threadCount);
    ctx.stats.hardwareThreads = std::max(1, int(std::thread::hardware_concurrency()));
    ctx.start = std::chrono::steady_clock::now();
    ctx.timeLimitMs = timeLimitMs;
    ctx.stop = false;
    ctx.tt.newSearch();
    const Move noMove = invalidMove();

    for(int s=0; s<2; s++){
        for(int from=0; from<64; from++){
            for(int to=0; to<64; to++){
                ctx.history[s][from][to] = (ctx.history[s][from][to] * 7) / 8;
            }
        }
        for(int pt=0; pt<7; pt++){
            for(int to=0; to<64; to++){
                ctx.captureHistory[s][pt][to] = (ctx.captureHistory[s][pt][to] * 7) / 8;
            }
        }
    }
    for(int ply = 0; ply < 128; ply++){
        ctx.plyMove[ply] = noMove;
        ctx.staticEvalByPly[ply] = -INF;
    }

    ctx.repetition = ctx.gameHistory;
    if(ctx.repetition.empty() || ctx.repetition.back() != bd.hash){
        ctx.repetition.push_back(bd.hash);
    }
    const std::vector<u64> rootRepetition = ctx.repetition;

    std::vector<Move> rootMoves;
    bd.genLegalMoves(rootMoves);
    if(rootMoves.empty()) return Move{};

    Move bestMove = rootMoves[0];
    int bestScore = 0;
    std::vector<int> rootScores(rootMoves.size(), 0);

    const int workers = std::max(1, std::min<int>(std::clamp(threadCount, 1, 64), int(rootMoves.size())));
    ctx.stats.workersUsed = workers;
    const size_t ttBytes = std::max<size_t>(sizeof(TTEntry), ctx.tt.table.size() * sizeof(TTEntry));
    const size_t ttPerThreadMB = std::max<size_t>(16, (ttBytes / size_t(workers)) / (1024ull * 1024ull));

    std::vector<SearchContext> workerCtx;
    workerCtx.resize(static_cast<size_t>(workers));
    for(int w = 0; w < workers; w++){
        workerCtx[size_t(w)].tt.resizeMB(ttPerThreadMB);
        std::memcpy(workerCtx[size_t(w)].killer, ctx.killer, sizeof(ctx.killer));
        std::memcpy(workerCtx[size_t(w)].countermove, ctx.countermove, sizeof(ctx.countermove));
        std::memcpy(workerCtx[size_t(w)].history, ctx.history, sizeof(ctx.history));
        std::memcpy(workerCtx[size_t(w)].captureHistory, ctx.captureHistory, sizeof(ctx.captureHistory));
    }

    int bestWorker = 0;

    for(int d=1; d<=maxDepth; d++){
        if(timeUp(ctx)) break;

        std::vector<size_t> order(rootMoves.size());
        for(size_t i=0; i<rootMoves.size(); i++) order[i] = i;
        std::sort(order.begin(), order.end(), [&](size_t a, size_t b){
            const bool aBest = sameMove(rootMoves[a], bestMove);
            const bool bBest = sameMove(rootMoves[b], bestMove);
            if(aBest != bBest) return aBest;
            if(rootScores[a] != rootScores[b]) return rootScores[a] > rootScores[b];
            return a < b;
        });

        std::vector<int> scores(rootMoves.size(), -INF);
        std::vector<int> owners(rootMoves.size(), -1);
        std::vector<u64> depthNodes(rootMoves.size(), 0);
        std::vector<u64> depthQNodes(rootMoves.size(), 0);
        std::atomic<size_t> nextIndex{0};

        std::vector<std::thread> threads;
        threads.reserve(size_t(workers));

        for(int w = 0; w < workers; w++){
            threads.emplace_back([&, w](){
                SearchContext& local = workerCtx[size_t(w)];
                local.start = ctx.start;
                local.timeLimitMs = ctx.timeLimitMs;
                local.stop = false;
                local.abortFlag = ctx.abortFlag;
                local.tt.newSearch();

                while(true){
                    if(timeUp(local)) break;
                    size_t ord = nextIndex.fetch_add(1, std::memory_order_relaxed);
                    if(ord >= order.size()) break;

                    const size_t idx = order[ord];
                    const Move m = rootMoves[idx];

                    Board child = bd;
                    Undo u{};
                    if(!child.makeMove(m, u)){
                        scores[idx] = -INF;
                        owners[idx] = w;
                        continue;
                    }

                    local.repetition = rootRepetition;
                    local.repetition.push_back(child.hash);

                    const u64 prevNodes = local.stats.nodes;
                    const u64 prevQNodes = local.stats.qnodes;
                    int score = -negamax(child, local, d - 1, -INF, INF, 1, m, true);
                    depthNodes[idx] = local.stats.nodes - prevNodes;
                    depthQNodes[idx] = local.stats.qnodes - prevQNodes;
                    scores[idx] = score;
                    owners[idx] = w;
                }
            });
        }

        for(std::thread& t : threads){
            if(t.joinable()) t.join();
        }

        bool incomplete = false;
        int localBest = -INF;
        size_t localBestIdx = 0;
        int localBestWorker = bestWorker;
        for(size_t idx = 0; idx < rootMoves.size(); idx++){
            if(scores[idx] == -INF){
                incomplete = true;
                continue;
            }
            rootScores[idx] = scores[idx];
            if(scores[idx] > localBest){
                localBest = scores[idx];
                localBestIdx = idx;
                localBestWorker = std::max(0, owners[idx]);
            }
            ctx.stats.nodes += depthNodes[idx];
            ctx.stats.qnodes += depthQNodes[idx];
        }

        auto now = std::chrono::steady_clock::now();
        const int elapsedMs = (int)std::chrono::duration_cast<std::chrono::milliseconds>(now - ctx.start).count();
        const bool externalAbort = ctx.abortFlag && ctx.abortFlag->load(std::memory_order_relaxed);
        const bool timedOut = elapsedMs >= ctx.timeLimitMs;
        if((timedOut || externalAbort) && incomplete){
            ctx.stop = true;
            break;
        }

        if(localBest != -INF){
            bestScore = localBest;
            bestMove = rootMoves[localBestIdx];
            bestWorker = localBestWorker;
            ctx.stats.depthReached = d;
            ctx.stats.bestScore = bestScore;
        }

        if(timedOut || externalAbort){
            ctx.stop = true;
            break;
        }
    }

    bestWorker = std::clamp(bestWorker, 0, workers - 1);
    std::memcpy(ctx.killer, workerCtx[size_t(bestWorker)].killer, sizeof(ctx.killer));
    std::memcpy(ctx.countermove, workerCtx[size_t(bestWorker)].countermove, sizeof(ctx.countermove));
    std::memcpy(ctx.history, workerCtx[size_t(bestWorker)].history, sizeof(ctx.history));
    std::memcpy(ctx.captureHistory, workerCtx[size_t(bestWorker)].captureHistory, sizeof(ctx.captureHistory));

    auto end = std::chrono::steady_clock::now();
    ctx.stats.timeMs = (int)std::chrono::duration_cast<std::chrono::milliseconds>(end - ctx.start).count();
    return bestMove;
}

static Move searchBestMove(Board& bd, SearchContext& ctx, int maxDepth, int timeLimitMs, int threadCount=1){
    if(threadCount <= 1){
        return searchBestMoveSingle(bd, ctx, maxDepth, timeLimitMs);
    }
    return searchBestMoveParallel(bd, ctx, maxDepth, timeLimitMs, threadCount);
}

static float drawWrappedText(sf::RenderTarget& target,
                             const sf::Font& font,
                             const std::string& text,
                             unsigned characterSize,
                             sf::Vector2f pos,
                             float maxWidth,
                             sf::Color col)
{
    sf::Text t;
    t.setFont(font);
    t.setCharacterSize(characterSize);
    t.setFillColor(col);

    const float lineSpacing = font.getLineSpacing(characterSize);
    float y = pos.y;

    auto flushLine = [&](const std::string& line){
        if(line.empty()) return;
        t.setString(line);
        setCrispTextPosition(t, sf::Vector2f(pos.x, y));
        target.draw(t);
        y += lineSpacing;
    };

    auto fits = [&](const std::string& candidate)->bool{
        t.setString(candidate);
        return t.getLocalBounds().width <= maxWidth;
    };

    std::string currentLine;
    std::string currentWord;

    auto commitWord = [&](){
        if(currentWord.empty()) return;

        if(currentLine.empty()){
            if(fits(currentWord)){
                currentLine = currentWord;
                currentWord.clear();
                return;
            }
        } else {
            std::string trial = currentLine + " " + currentWord;
            if(fits(trial)){
                currentLine = trial;
                currentWord.clear();
                return;
            }
        }

        flushLine(currentLine);
        currentLine = currentWord;
        currentWord.clear();
    };

    for(char ch : text){
        if(ch == '\n'){
            commitWord();
            flushLine(currentLine);
            currentLine.clear();
            continue;
        }
        if(ch == ' '){
            commitWord();
            continue;
        }
        currentWord.push_back(ch);
    }

    commitWord();
    flushLine(currentLine);

    return y - pos.y;
}

static std::string extractPVFromTT(Board bd, SearchContext& ctx, int maxPlies=12){
    std::string pv;
    std::vector<u64> seen;
    seen.reserve((size_t)maxPlies+2);

    for(int ply=0; ply<maxPlies; ply++){
        if(std::find(seen.begin(), seen.end(), bd.hash) != seen.end()) break;
        seen.push_back(bd.hash);

        TTEntry* e = ctx.tt.probe(bd.hash);
        if(!e || e->key != bd.hash) break;

        Move m = e->best;

        std::vector<Move> leg;
        bd.genLegalMoves(leg);

        auto it = std::find_if(leg.begin(), leg.end(), [&](const Move& x){
            return x.from==m.from && x.to==m.to && x.promo==m.promo;
        });
        if(it == leg.end()) break;

        Undo u{};
        if(!bd.makeMove(*it, u)) break;

        if(!pv.empty()) pv += " ";
        pv += moveToUCI(*it);
    }
    return pv;
}

static u64 perft(Board& bd, int depth){
    if(depth <= 0) return 1;

    std::vector<Move> legal;
    bd.genLegalMoves(legal);
    if(depth == 1) return static_cast<u64>(legal.size());

    u64 nodes = 0;
    for(const Move& m : legal){
        Undo u{};
        if(!bd.makeMove(m, u)) continue;
        nodes += perft(bd, depth - 1);
        bd.undoMove(u);
    }
    return nodes;
}

static std::vector<std::pair<std::string, u64>> perftDivide(Board& bd, int depth){
    std::vector<std::pair<std::string, u64>> out;
    if(depth <= 0) return out;

    std::vector<Move> legal;
    bd.genLegalMoves(legal);
    out.reserve(legal.size());
    for(const Move& m : legal){
        Undo u{};
        if(!bd.makeMove(m, u)) continue;
        const u64 nodes = (depth == 1) ? 1 : perft(bd, depth - 1);
        bd.undoMove(u);
        out.emplace_back(moveToUCI(m), nodes);
    }
    return out;
}

struct PerftCase {
    const char* name;
    const char* fen;
    std::vector<std::pair<int, u64>> expectations;
};

static int runPerftSuite(const Zobrist& zob, int maxDepthPerCase = 4){
    const std::vector<PerftCase> cases = {
        {"Start Position", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", {
            {1, 20ULL}, {2, 400ULL}, {3, 8902ULL}, {4, 197281ULL}
        }},
        {"Kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/2pP4/1p2P3/2N2N2/PPQBBPPP/R3K2R w KQkq - 0 1", {
            {1, 45ULL}, {2, 1947ULL}, {3, 85877ULL}, {4, 3617140ULL}
        }},
        {"Position 3", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", {
            {1, 14ULL}, {2, 191ULL}, {3, 2812ULL}, {4, 43238ULL}
        }},
        {"Position 4", "r3k2r/Pppp1ppp/1b3nbN/nP6/B1P1P3/5N2/Pp1P1PPP/R2Q1RK1 w kq - 0 1", {
            {1, 30ULL}, {2, 1160ULL}, {3, 35941ULL}, {4, 1371859ULL}
        }},
        {"Position 5", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", {
            {1, 44ULL}, {2, 1486ULL}, {3, 62379ULL}, {4, 2103487ULL}
        }},
        {"Position 6", "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/2NP1N2/PPP1QPPP/R4RK1 w - - 0 10", {
            {1, 44ULL}, {2, 1987ULL}, {3, 83034ULL}, {4, 3596057ULL}
        }}
    };

    int failures = 0;
    for(const auto& tc : cases){
        Board bd;
        bd.setZobrist(&zob);
        if(!bd.loadFEN(tc.fen)){
            std::cout << "[FAIL] " << tc.name << " (invalid FEN)\n";
            failures++;
            continue;
        }

        std::cout << "\n== " << tc.name << " ==\n";
        for(const auto& [depth, expected] : tc.expectations){
            if(depth > maxDepthPerCase) continue;
            auto t0 = std::chrono::steady_clock::now();
            const u64 got = perft(bd, depth);
            auto t1 = std::chrono::steady_clock::now();
            const int ms = (int)std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
            const double nps = (ms > 0) ? (double(got) * 1000.0 / double(ms)) : 0.0;

            const bool ok = (got == expected);
            std::cout << "d" << depth
                      << " expected=" << expected
                      << " got=" << got
                      << " time=" << ms << "ms"
                      << " nps=" << static_cast<long long>(nps)
                      << (ok ? " [OK]" : " [FAIL]")
                      << "\n";
            if(!ok) failures++;
        }
    }

    if(failures == 0){
        std::cout << "\nPerft suite: PASS\n";
        return 0;
    }
    std::cout << "\nPerft suite: FAIL (" << failures << " mismatches)\n";
    return 1;
}

struct BenchmarkPosition {
    const char* name;
    const char* fen;
};

static int runSearchBenchmark(const Zobrist& zob, int depth, int perPositionTimeMs, int ttSizeMB = 256, int threads = 1){
    const std::vector<BenchmarkPosition> positions = {
        {"Start", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"},
        {"Middlegame 1", "r2q1rk1/pp2bppp/2np1n2/2p1p1B1/2P1P3/2NP1N2/PP2QPPP/R4RK1 w - - 0 10"},
        {"Middlegame 2", "r1bq1rk1/pp2bppp/2n1pn2/2pp4/2P5/2NP1NP1/PP2PPBP/R1BQ1RK1 w - - 0 9"},
        {"Endgame", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1"}
    };

    u64 totalNodes = 0;
    int totalMs = 0;
    std::cout << "Benchmark: depth=" << depth
              << " timeLimit=" << perPositionTimeMs
              << "ms threads=" << threads
              << " positions=" << positions.size() << "\n";

    for(const auto& p : positions){
        Board bd;
        bd.setZobrist(&zob);
        if(!bd.loadFEN(p.fen)){
            std::cout << "[FAIL] " << p.name << " invalid FEN\n";
            return 1;
        }

        SearchContext ctx;
        ctx.tt.resizeMB(static_cast<size_t>(ttSizeMB));
        ctx.gameHistory = {bd.hash};

        const Move best = searchBestMove(bd, ctx, depth, perPositionTimeMs, threads);
        const double nps = (ctx.stats.timeMs > 0)
            ? (double(ctx.stats.nodes) * 1000.0 / double(ctx.stats.timeMs))
            : 0.0;

        std::cout << std::left << std::setw(12) << p.name
                  << " best=" << moveToUCI(best)
                  << " depth=" << ctx.stats.depthReached
                  << " score=" << ctx.stats.bestScore
                  << " nodes=" << ctx.stats.nodes
                  << " qnodes=" << ctx.stats.qnodes
                  << " time=" << ctx.stats.timeMs << "ms"
                  << " nps=" << static_cast<long long>(nps)
                  << "\n";

        totalNodes += ctx.stats.nodes;
        totalMs += ctx.stats.timeMs;
    }

    const double totalNps = (totalMs > 0) ? (double(totalNodes) * 1000.0 / double(totalMs)) : 0.0;
    std::cout << "Benchmark summary: nodes=" << totalNodes
              << " time=" << totalMs << "ms"
              << " nps=" << static_cast<long long>(totalNps)
              << "\n";
    return 0;
}
