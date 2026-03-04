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
    int material=0;
    int pst=0;

    int phase=0;
    for(int i=0;i<64;i++){
        Piece p=bd.b[i];
        if(isNone(p) || p.t==PieceType::King || p.t==PieceType::Pawn) continue;
        if(p.t==PieceType::Knight || p.t==PieceType::Bishop) phase += 1;
        else if(p.t==PieceType::Rook) phase += 2;
        else if(p.t==PieceType::Queen) phase += 4;
    }
    phase = std::clamp(phase, 0, 24);
    bool endgameKing = (phase <= 8);

    int whiteBishops=0, blackBishops=0;
    int wpFile[8]{}, bpFile[8]{};

    for(int i=0;i<64;i++){
        Piece p=bd.b[i];
        if(isNone(p)) continue;
        int base = pieceValue(p.t);
        if(p.c==Color::White) material += base;
        else material -= base;

        int idxW = (p.c==Color::White) ? i : mirrorIndex(i);
        int ps = pstScore(p.t, idxW, endgameKing);
        if(p.c==Color::White) pst += ps;
        else pst -= ps;

        if(p.t==PieceType::Bishop){
            if(p.c==Color::White) whiteBishops++;
            else blackBishops++;
        }
        if(p.t==PieceType::Pawn){
            int f=i%8;
            if(p.c==Color::White) wpFile[f]++;
            else bpFile[f]++;
        }
    }

    int bishopPair = 0;
    if(whiteBishops>=2) bishopPair += 30;
    if(blackBishops>=2) bishopPair -= 30;

    int pawnStruct=0;
    for(int f=0;f<8;f++){
        if(wpFile[f]>=2) pawnStruct -= 12*(wpFile[f]-1);
        if(bpFile[f]>=2) pawnStruct += 12*(bpFile[f]-1);

        if(wpFile[f]>0){
            bool left = (f>0 && wpFile[f-1]>0);
            bool right= (f<7 && wpFile[f+1]>0);
            if(!left && !right) pawnStruct -= 10;
        }
        if(bpFile[f]>0){
            bool left = (f>0 && bpFile[f-1]>0);
            bool right= (f<7 && bpFile[f+1]>0);
            if(!left && !right) pawnStruct += 10;
        }
    }

    int mobility=0;
    {
        Board t=bd;
        t.stm=Color::White;
        std::vector<Move> w; t.genPseudoMoves(w);
        t.stm=Color::Black;
        std::vector<Move> b; t.genPseudoMoves(b);
        mobility = (int(w.size()) - int(b.size())) * 2;
    }

    int kingSafety=0;
    if(!endgameKing){
        int wK = bd.findKing(Color::White);
        int bK = bd.findKing(Color::Black);

        auto kingCentrePenalty = [&](int kIdx, Color c)->int{
            if(kIdx<0) return 0;
            int f=kIdx%8, r=kIdx/8;
            int df = std::abs(f-4);
            int pen = 0;
            if(df<=1 && (r==0 || r==7)) pen += 10;
            if(df<=1 && (r==1 || r==6)) pen += 20;
            if(df<=1 && (r==2 || r==5)) pen += 35;
            return pen;
        };

        kingSafety -= kingCentrePenalty(wK, Color::White);
        kingSafety += kingCentrePenalty(bK, Color::Black);

        bool wCanCastle = (bd.castling & 0b0011);
        bool bCanCastle = (bd.castling & 0b1100);
        if(!wCanCastle) kingSafety -= 10;
        if(!bCanCastle) kingSafety += 10;
    }

    int scoreWhite = material + pst + bishopPair + pawnStruct + mobility + kingSafety;
    return (bd.stm==Color::White) ? scoreWhite : -scoreWhite;
}

// ======================== Search (ID + TT + QS + Ordering) ========================
struct SearchStats {
    u64 nodes=0;
    u64 qnodes=0;
    int depthReached=0;
    int bestScore=0;
    int timeMs=0;
};

struct SearchContext {
    TranspositionTable tt;
    SearchStats stats;
    std::chrono::steady_clock::time_point start;
    int timeLimitMs=1000;
    bool stop=false;
    const std::atomic<bool>* abortFlag=nullptr;

    Move killer[128][2]{};
    int history[2][64][64]{};
    std::vector<u64> gameHistory; // position hashes from actual game (includes current root)
    std::vector<u64> repetition;
};

static bool sameMove(const Move& a, const Move& b){
    return a.from==b.from && a.to==b.to && a.promo==b.promo && a.isCastle==b.isCastle && a.isEnPassant==b.isEnPassant;
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

static int scoreMove(const Board& bd, SearchContext& ctx, const Move& m, const Move& ttMove, int ply){
    if(ttMove.from==m.from && ttMove.to==m.to && ttMove.promo==m.promo) return 1000000;

    if(m.isCapture || m.isEnPassant){
        return 100000 + mvvLvaScore(bd, m);
    }

    if(ply<128){
        if(sameMove(m, ctx.killer[ply][0])) return 90000;
        if(sameMove(m, ctx.killer[ply][1])) return 80000;
    }

    int side = (bd.stm==Color::White)?0:1;
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

static int quiescence(Board& bd, SearchContext& ctx, int alpha, int beta){
    if(timeUp(ctx)) return 0;
    ctx.stats.qnodes++;

    int stand = evaluate(bd);
    if(stand >= beta) return beta;
    if(stand > alpha) alpha = stand;

    std::vector<Move> pseudo;
    bd.genPseudoMoves(pseudo);

    std::vector<Move> moves;
    moves.reserve(pseudo.size());
    for(const auto& m: pseudo){
        if(m.isCapture || m.isEnPassant || m.promo!=PieceType::None){
            Undo u{};
            if(bd.makeMove(m,u)){
                moves.push_back(m);
                bd.undoMove(u);
            }
        }
    }

    std::sort(moves.begin(), moves.end(), [&](const Move& a, const Move& b){
        return mvvLvaScore(bd,a) > mvvLvaScore(bd,b);
    });

    for(const auto& m: moves){
        Undo u{};
        if(!bd.makeMove(m,u)) continue;
        int score = -quiescence(bd, ctx, -beta, -alpha);
        bd.undoMove(u);

        if(score >= beta) return beta;
        if(score > alpha) alpha = score;
    }

    return alpha;
}

static int negamax(Board& bd, SearchContext& ctx, int depth, int alpha, int beta, int ply){
    if(timeUp(ctx)) return 0;
    ctx.stats.nodes++;

    // Mate-distance pruning keeps mate scores consistent and trims impossible windows.
    alpha = std::max(alpha, -MATE + ply);
    beta = std::min(beta, MATE - ply - 1);
    if(alpha >= beta) return alpha;

    if(bd.insufficientMaterial()) return 0;
    if(bd.halfmoveClock >= 100) return 0;
    if(isThreefoldRepetition(bd, ctx)) return 0;

    bool inCheck = bd.inCheck(bd.stm);
    int staticEval = 0;
    if(!inCheck) staticEval = evaluate(bd);

    Move ttMove{};
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

    if(depth <= 0){
        return quiescence(bd, ctx, alpha, beta);
    }

    // Null-move pruning: aggressive cut when position is quiet enough and side has material.
    if(depth >= 3 && !inCheck && hasNonPawnMaterial(bd, bd.stm)){
        Board nb = bd;
        nb.epSquare = -1;
        nb.stm = other(nb.stm);
        nb.recomputeHash();
        ctx.repetition.push_back(nb.hash);
        int reduction = 2 + depth/4;
        int score = -negamax(nb, ctx, depth - 1 - reduction, -beta, -beta + 1, ply + 1);
        ctx.repetition.pop_back();
        if(ctx.stop) return 0;
        if(score >= beta) return beta;
    }

    std::vector<Move> moves;
    bd.genLegalMoves(moves);

    if(moves.empty()){
        if(bd.inCheck(bd.stm)) return -MATE + ply;
        return 0;
    }

    std::sort(moves.begin(), moves.end(), [&](const Move& a, const Move& b){
        return scoreMove(bd, ctx, a, ttMove, ply) > scoreMove(bd, ctx, b, ttMove, ply);
    });

    int best = -INF;
    Move bestM{};

    int originalAlpha = alpha;

    for(size_t i=0;i<moves.size();i++){
        const Move& m = moves[i];
        bool isQuiet = !(m.isCapture || m.isEnPassant) && (m.promo==PieceType::None);

        if(!inCheck && depth <= 2 && isQuiet && i >= 6){
            int futility = staticEval + (110 * depth);
            if(futility <= alpha){
                continue;
            }
        }

        if(!inCheck && depth <= 3 && isQuiet && i >= size_t(10 + depth*2)){
            continue;
        }

        Undo u{};
        if(!bd.makeMove(m,u)) continue;

        ctx.repetition.push_back(bd.hash);

        int newDepth = depth - 1;
        bool givesCheck = bd.inCheck(bd.stm);
        if(givesCheck){
          newDepth++;
        }
        int score=0;

        int reduction = 0;
        if(newDepth >= 3 && i >= 3 && isQuiet && !givesCheck){
            reduction = 1;
            if(newDepth >= 5) reduction++;
            if(i >= 8) reduction++;
            reduction = std::min(reduction, std::max(0, newDepth - 1));
        }

        if(reduction > 0){
            score = -negamax(bd, ctx, newDepth - reduction, -alpha-1, -alpha, ply+1);
            if(score > alpha){
                score = -negamax(bd, ctx, newDepth, -beta, -alpha, ply+1);
            }
        } else {
            score = -negamax(bd, ctx, newDepth, -beta, -alpha, ply+1);
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
                int side = (bd.stm==Color::White)?0:1;
                ctx.history[side][m.from][m.to] = std::min(90000, ctx.history[side][m.from][m.to] + depth*depth*8);
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

static Move searchBestMove(Board& bd, SearchContext& ctx, int maxDepth, int timeLimitMs){
    ctx.stats = {};
    ctx.start = std::chrono::steady_clock::now();
    ctx.timeLimitMs = timeLimitMs;
    ctx.stop = false;
    ctx.tt.newSearch();

    for(int s=0; s<2; s++){
        for(int from=0; from<64; from++){
            for(int to=0; to<64; to++){
                ctx.history[s][from][to] = (ctx.history[s][from][to] * 7) / 8;
            }
        }
    }

    ctx.repetition = ctx.gameHistory;
    if(ctx.repetition.empty() || ctx.repetition.back() != bd.hash){
        ctx.repetition.push_back(bd.hash);
    }

    std::vector<Move> rootMoves;
    bd.genLegalMoves(rootMoves);
    if(rootMoves.empty()) return Move{};

    Move bestMove = rootMoves[0];
    int bestScore = -INF;

    for(int d=1; d<=maxDepth; d++){
        if(timeUp(ctx)) break;

        int alpha = -INF;
        int beta  = INF;
        if(d >= 3 && std::abs(bestScore) < MATE/2){
            alpha = bestScore - 50;
            beta  = bestScore + 50;
        }

        Move ttMove{};
        if(auto* e = ctx.tt.probe(bd.hash)){
            if(e->key==bd.hash) ttMove = e->best;
        }

        std::sort(rootMoves.begin(), rootMoves.end(), [&](const Move& a, const Move& b){
            return scoreMove(bd, ctx, a, ttMove, 0) > scoreMove(bd, ctx, b, ttMove, 0);
        });

        int localBest=-INF;
        Move localMove = rootMoves[0];

        for(size_t i=0; i<rootMoves.size(); i++){
            const Move& m = rootMoves[i];
            if(timeUp(ctx)) break;
            Undo u{};
            if(!bd.makeMove(m,u)) continue;

            ctx.repetition.push_back(bd.hash);
            int score = 0;
            if(i == 0){
                score = -negamax(bd, ctx, d-1, -beta, -alpha, 1);
            } else {
                score = -negamax(bd, ctx, d-1, -alpha-1, -alpha, 1);
                if(score > alpha && score < beta){
                    score = -negamax(bd, ctx, d-1, -beta, -alpha, 1);
                }
            }
            ctx.repetition.pop_back();

            bd.undoMove(u);

            if(ctx.stop) break;

            if(score > localBest){
                localBest = score;
                localMove = m;
            }
            alpha = std::max(alpha, score);

            if(alpha >= beta){
                alpha = -INF;
                beta = INF;
                Undo u2{};
                if(bd.makeMove(m,u2)){
                    ctx.repetition.push_back(bd.hash);
                    int score2 = -negamax(bd, ctx, d-1, -INF, INF, 1);
                    ctx.repetition.pop_back();
                    bd.undoMove(u2);
                    if(!ctx.stop && score2 > localBest){
                        localBest = score2;
                        localMove = m;
                    }
                }
                break;
            }
        }

        if(!ctx.stop){
            bestScore = localBest;
            bestMove = localMove;
            ctx.stats.depthReached = d;
            ctx.stats.bestScore = bestScore;
        }
    }

    auto end = std::chrono::steady_clock::now();
    ctx.stats.timeMs = (int)std::chrono::duration_cast<std::chrono::milliseconds>(end - ctx.start).count();
    return bestMove;
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
