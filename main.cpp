// main.cpp  (SFML 2.6.x)

#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>

#include <iomanip>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <optional>
#include <random>
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <filesystem>
#include <sstream>
#include <iostream>
#include <fstream>
#include <thread>
#include <atomic>
#include <mutex>
#include <cstdio>
#include <cstdlib>

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

static sf::Vector2f snap(sf::Vector2f p) { return sf::Vector2f(std::round(p.x), std::round(p.y)); }
static void setCrispTextPosition(sf::Text& t, sf::Vector2f p){
    const sf::FloatRect b = t.getLocalBounds();
    t.setPosition(snap(sf::Vector2f(p.x - b.left, p.y - b.top)));
}
static std::string trim(std::string s){
    while(!s.empty() && (s.back()=='\n' || s.back()=='\r' || s.back()==' ' || s.back()=='\t')) s.pop_back();
    size_t i = 0;
    while(i < s.size() && (s[i]==' ' || s[i]=='\t' || s[i]=='\n' || s[i]=='\r')) ++i;
    return s.substr(i);
}
static std::string shellQuote(const std::string& s){
    std::string out = "'";
    for(char c : s){
        if(c=='\'') out += "'\\''";
        else out.push_back(c);
    }
    out.push_back('\'');
    return out;
}

// ======================== Squares / Coords ========================
struct Square { int file=0, rank=0; }; // 0..7
static bool operator==(const Square& a, const Square& b){ return a.file==b.file && a.rank==b.rank; }
static bool inBounds(const Square& s){ return s.file>=0 && s.file<8 && s.rank>=0 && s.rank<8; }
static int sqToIndex(const Square& s){ return s.rank*8 + s.file; }
static Square indexToSq(int idx){ return Square{idx%8, idx/8}; }
static std::string sqName(const Square& s){
    return std::string() + char('a'+s.file) + char('1'+s.rank);
}

// Visual board: rank 7 at top visually unless flipped
static sf::Vector2f squareToPixel(const Square& s, float tile, sf::Vector2f origin, bool flip){
    int vr = flip ? s.rank : (7 - s.rank);
    int vf = flip ? (7 - s.file) : s.file;
    return sf::Vector2f(origin.x + vf*tile, origin.y + vr*tile);
}
static std::optional<Square> pixelToSquare(sf::Vector2f p, float tile, sf::Vector2f origin, bool flip){
    float x=p.x-origin.x, y=p.y-origin.y;
    if(x<0||y<0) return std::nullopt;
    int vf=int(x/tile), vr=int(y/tile);
    if(vf<0||vf>7||vr<0||vr>7) return std::nullopt;
    int file = flip ? (7 - vf) : vf;
    int rank = flip ? vr : (7 - vr);
    return Square{file, rank};
}

static sf::Color lighten(sf::Color c, int add){
    auto clamp=[](int v){ return std::max(0,std::min(255,v)); };
    return sf::Color(
        static_cast<sf::Uint8>(clamp(int(c.r)+add)),
        static_cast<sf::Uint8>(clamp(int(c.g)+add)),
        static_cast<sf::Uint8>(clamp(int(c.b)+add)),
        c.a
    );
}

// ======================== Chess Types ========================
enum class Color : u8 { White=0, Black=1 };
static Color other(Color c){ return c==Color::White ? Color::Black : Color::White; }

enum class PieceType : u8 { None=0, Pawn, Knight, Bishop, Rook, Queen, King };

struct Piece {
    PieceType t = PieceType::None;
    Color c = Color::White;
};
static bool isNone(const Piece& p){ return p.t==PieceType::None; }

static int pieceValue(PieceType t){
    switch(t){
        case PieceType::Pawn:   return 100;
        case PieceType::Knight: return 320;
        case PieceType::Bishop: return 330;
        case PieceType::Rook:   return 500;
        case PieceType::Queen:  return 900;
        case PieceType::King:   return 0;
        default: return 0;
    }
}

static std::string pieceName(PieceType t){
    switch(t){
        case PieceType::Pawn:   return "pawn";
        case PieceType::Knight: return "knight";
        case PieceType::Bishop: return "bishop";
        case PieceType::Rook:   return "rook";
        case PieceType::Queen:  return "queen";
        case PieceType::King:   return "king";
        default: return "";
    }
}
static std::string pieceKey(const Piece& p){
    if(p.t==PieceType::None) return "";
    std::string col = (p.c==Color::White) ? "white_" : "black_";
    return col + pieceName(p.t);
}

struct Move {
    u8 from=0, to=0;
    PieceType promo = PieceType::None;
    bool isCapture=false;
    bool isEnPassant=false;
    bool isCastle=false;
};

struct Undo {
    Move m{};
    Piece captured{};
    int epSquare=-1;
    u8 castling=0;
    int halfmoveClock=0;
    u64 hash=0;
};

static std::string moveToUCI(const Move& m){
    Square a = indexToSq(m.from);
    Square b = indexToSq(m.to);
    std::string s = sqName(a) + sqName(b);
    if(m.promo!=PieceType::None){
        char pc='q';
        if(m.promo==PieceType::Rook) pc='r';
        if(m.promo==PieceType::Bishop) pc='b';
        if(m.promo==PieceType::Knight) pc='n';
        s.push_back(pc);
    }
    return s;
}

static char sanPieceChar(PieceType t){
    switch(t){
        case PieceType::Knight: return 'N';
        case PieceType::Bishop: return 'B';
        case PieceType::Rook:   return 'R';
        case PieceType::Queen:  return 'Q';
        case PieceType::King:   return 'K';
        default: return '?';
    }
}

// ======================== Zobrist + TT ========================
struct Zobrist {
    // [color][pieceType][square]
    u64 psq[2][7][64]{};
    u64 sideToMove{};
    u64 castling[16]{};
    u64 epFile[9]{}; // 0..7 file, 8 = "no ep"

    Zobrist(){
        std::mt19937_64 rng(0xC0FFEE1234ULL);
        auto r64 = [&](){ return rng(); };

        for(int c=0;c<2;c++)
            for(int pt=0;pt<7;pt++)
                for(int s=0;s<64;s++)
                    psq[c][pt][s]=r64();

        sideToMove = r64();
        for(int i=0;i<16;i++) castling[i]=r64();
        for(int i=0;i<9;i++) epFile[i]=r64();
    }
};

enum class TTFlag : u8 { Exact=0, Lower=1, Upper=2 };

struct TTEntry {
    u64 key=0;
    int16_t score=0;
    int8_t depth=0;
    TTFlag flag=TTFlag::Exact;
    Move best{};
};

struct TranspositionTable {
    std::vector<TTEntry> table;
    size_t mask=0;

    void resizeMB(size_t mb){
        size_t bytes = mb*1024ull*1024ull;
        size_t n = std::max<size_t>(1, bytes / sizeof(TTEntry));
        size_t p=1;
        while(p < n) p<<=1;
        table.assign(p, TTEntry{});
        mask = p-1;
    }

    TTEntry* probe(u64 key){
        if(table.empty()) return nullptr;
        return &table[size_t(key) & mask];
    }

    void store(u64 key, int depth, int score, TTFlag flag, const Move& best){
        if(table.empty()) return;
        TTEntry& e = table[size_t(key) & mask];
        if(e.key==0 || e.key==key || depth >= e.depth){
            e.key=key;
            e.depth=(int8_t)std::clamp(depth, 0, 127);
            e.score=(int16_t)std::clamp(score, -32767, 32767);
            e.flag=flag;
            e.best=best;
        }
    }
};

// ======================== Board ========================
struct Board {
    std::array<Piece, 64> b{};
    Color stm = Color::White;

    int epSquare = -1;          // en passant target square index or -1
    u8 castling = 0b1111;       // 1=WK,2=WQ,4=BK,8=BQ
    int halfmoveClock = 0;      // 50-move heuristic
    u64 hash = 0;

    const Zobrist* z = nullptr;

    void clear(){
        for(auto& p : b) p = Piece{};
        stm = Color::White;
        epSquare = -1;
        castling = 0b1111;
        halfmoveClock = 0;
        hash = 0;
    }

    void reset(){
        clear();
        auto set = [&](int file, int rank, Color c, PieceType t){
            b[rank*8 + file] = Piece{t,c};
        };

        // White
        set(0,0,Color::White,PieceType::Rook);
        set(1,0,Color::White,PieceType::Knight);
        set(2,0,Color::White,PieceType::Bishop);
        set(3,0,Color::White,PieceType::Queen);
        set(4,0,Color::White,PieceType::King);
        set(5,0,Color::White,PieceType::Bishop);
        set(6,0,Color::White,PieceType::Knight);
        set(7,0,Color::White,PieceType::Rook);
        for(int f=0; f<8; f++) set(f,1,Color::White,PieceType::Pawn);

        // Black
        set(0,7,Color::Black,PieceType::Rook);
        set(1,7,Color::Black,PieceType::Knight);
        set(2,7,Color::Black,PieceType::Bishop);
        set(3,7,Color::Black,PieceType::Queen);
        set(4,7,Color::Black,PieceType::King);
        set(5,7,Color::Black,PieceType::Bishop);
        set(6,7,Color::Black,PieceType::Knight);
        set(7,7,Color::Black,PieceType::Rook);
        for(int f=0; f<8; f++) set(f,6,Color::Black,PieceType::Pawn);

        stm = Color::White;
        epSquare = -1;
        castling = 0b1111;
        halfmoveClock=0;

        recomputeHash();
    }

    Piece at(int idx) const { return b[idx]; }
    Piece& atRef(int idx) { return b[idx]; }

    void setZobrist(const Zobrist* zz){
        z = zz;
        recomputeHash();
    }

    void recomputeHash(){
        if(!z){ hash=0; return; }
        u64 h=0;
        for(int i=0;i<64;i++){
            Piece p=b[i];
            if(isNone(p)) continue;
            int c = (p.c==Color::White)?0:1;
            int pt = (int)p.t;
            h ^= z->psq[c][pt][i];
        }
        if(stm==Color::Black) h ^= z->sideToMove;
        h ^= z->castling[castling & 0xF];
        int epF = 8;
        if(epSquare>=0) epF = epSquare % 8;
        h ^= z->epFile[epF];
        hash = h;
    }

    int findKing(Color c) const {
        for(int i=0;i<64;i++){
            auto p=b[i];
            if(p.t==PieceType::King && p.c==c) return i;
        }
        return -1;
    }

    bool isSquareAttacked(int sq, Color by) const {
        int r = sq/8, f = sq%8;

        int pr = r + ((by==Color::White) ? -1 : 1);
        if(pr>=0 && pr<8){
            if(f-1>=0){
                Piece p=b[pr*8 + (f-1)];
                if(p.t==PieceType::Pawn && p.c==by) return true;
            }
            if(f+1<8){
                Piece p=b[pr*8 + (f+1)];
                if(p.t==PieceType::Pawn && p.c==by) return true;
            }
        }

        static const int kD[8][2]={{1,2},{2,1},{-1,2},{-2,1},{1,-2},{2,-1},{-1,-2},{-2,-1}};
        for(auto& d: kD){
            int nf=f+d[0], nr=r+d[1];
            if(nf>=0&&nf<8&&nr>=0&&nr<8){
                Piece p=b[nr*8+nf];
                if(p.t==PieceType::Knight && p.c==by) return true;
            }
        }

        for(int df=-1; df<=1; df++){
            for(int dr=-1; dr<=1; dr++){
                if(df==0&&dr==0) continue;
                int nf=f+df, nr=r+dr;
                if(nf>=0&&nf<8&&nr>=0&&nr<8){
                    Piece p=b[nr*8+nf];
                    if(p.t==PieceType::King && p.c==by) return true;
                }
            }
        }

        auto ray = [&](int df, int dr, PieceType a, PieceType b2)->bool{
            int nf=f+df, nr=r+dr;
            while(nf>=0&&nf<8&&nr>=0&&nr<8){
                Piece p=this->b[nr*8+nf];
                if(!isNone(p)){
                    if(p.c==by && (p.t==a || p.t==b2)) return true;
                    return false;
                }
                nf+=df; nr+=dr;
            }
            return false;
        };

        if(ray(1,1,PieceType::Bishop,PieceType::Queen)) return true;
        if(ray(1,-1,PieceType::Bishop,PieceType::Queen)) return true;
        if(ray(-1,1,PieceType::Bishop,PieceType::Queen)) return true;
        if(ray(-1,-1,PieceType::Bishop,PieceType::Queen)) return true;

        if(ray(1,0,PieceType::Rook,PieceType::Queen)) return true;
        if(ray(-1,0,PieceType::Rook,PieceType::Queen)) return true;
        if(ray(0,1,PieceType::Rook,PieceType::Queen)) return true;
        if(ray(0,-1,PieceType::Rook,PieceType::Queen)) return true;

        return false;
    }

    bool inCheck(Color c) const {
        int k = findKing(c);
        if(k<0) return false;
        return isSquareAttacked(k, other(c));
    }

    void genPseudoMoves(std::vector<Move>& out) const {
        out.clear();
        Color us = stm;

        auto push = [&](int from, int to, bool cap=false, bool ep=false, bool castle=false, PieceType promo=PieceType::None){
            Move m;
            m.from=(u8)from; m.to=(u8)to;
            m.isCapture=cap; m.isEnPassant=ep; m.isCastle=castle; m.promo=promo;
            out.push_back(m);
        };

        for(int i=0;i<64;i++){
            Piece p=b[i];
            if(isNone(p) || p.c!=us) continue;
            int r=i/8, f=i%8;

            if(p.t==PieceType::Pawn){
                int dir = (us==Color::White) ? 1 : -1;
                int startRank = (us==Color::White) ? 1 : 6;
                int promoRank = (us==Color::White) ? 7 : 0;

                int nr = r + dir;
                if(nr>=0 && nr<8){
                    int one = nr*8 + f;
                    if(isNone(b[one])){
                        if(nr==promoRank){
                            push(i, one, false, false, false, PieceType::Queen);
                            push(i, one, false, false, false, PieceType::Rook);
                            push(i, one, false, false, false, PieceType::Bishop);
                            push(i, one, false, false, false, PieceType::Knight);
                        } else {
                            push(i, one);
                            if(r==startRank){
                                int twoR = r + 2*dir;
                                int two = twoR*8 + f;
                                if(twoR>=0 && twoR<8 && isNone(b[two])) push(i, two);
                            }
                        }
                    }
                }

                for(int df : {-1,1}){
                    int nf=f+df, tr=r+dir;
                    if(nf<0||nf>=8||tr<0||tr>=8) continue;
                    int to=tr*8+nf;

                    if(!isNone(b[to]) && b[to].c!=us){
                        if(tr==promoRank){
                            push(i,to,true,false,false,PieceType::Queen);
                            push(i,to,true,false,false,PieceType::Rook);
                            push(i,to,true,false,false,PieceType::Bishop);
                            push(i,to,true,false,false,PieceType::Knight);
                        } else push(i,to,true);
                    }

                    if(epSquare==to){
                        int adj = r*8 + nf;
                        if(!isNone(b[adj]) && b[adj].t==PieceType::Pawn && b[adj].c!=us){
                            push(i,to,true,true,false);
                        }
                    }
                }

            } else if(p.t==PieceType::Knight){
                static const int d[8][2]={{1,2},{2,1},{-1,2},{-2,1},{1,-2},{2,-1},{-1,-2},{-2,-1}};
                for(auto& dd: d){
                    int nf=f+dd[0], nr=r+dd[1];
                    if(nf<0||nf>=8||nr<0||nr>=8) continue;
                    int to=nr*8+nf;
                    if(isNone(b[to])) push(i,to);
                    else if(b[to].c!=us) push(i,to,true);
                }

            } else if(p.t==PieceType::Bishop || p.t==PieceType::Rook || p.t==PieceType::Queen){
                auto slide=[&](int df,int dr){
                    int nf=f+df, nr=r+dr;
                    while(nf>=0&&nf<8&&nr>=0&&nr<8){
                        int to=nr*8+nf;
                        if(isNone(b[to])) push(i,to);
                        else {
                            if(b[to].c!=us) push(i,to,true);
                            break;
                        }
                        nf+=df; nr+=dr;
                    }
                };

                if(p.t==PieceType::Bishop || p.t==PieceType::Queen){
                    slide(1,1); slide(1,-1); slide(-1,1); slide(-1,-1);
                }
                if(p.t==PieceType::Rook || p.t==PieceType::Queen){
                    slide(1,0); slide(-1,0); slide(0,1); slide(0,-1);
                }

            } else if(p.t==PieceType::King){
                for(int df=-1; df<=1; df++){
                    for(int dr=-1; dr<=1; dr++){
                        if(df==0&&dr==0) continue;
                        int nf=f+df, nr=r+dr;
                        if(nf<0||nf>=8||nr<0||nr>=8) continue;
                        int to=nr*8+nf;
                        if(isNone(b[to])) push(i,to);
                        else if(b[to].c!=us) push(i,to,true);
                    }
                }

                if(us==Color::White && i==4){
                    if((castling & 0b0001) && isNone(b[5]) && isNone(b[6]) &&
                       b[7].t==PieceType::Rook && b[7].c==Color::White){
                        if(!inCheck(Color::White) &&
                           !isSquareAttacked(5, Color::Black) &&
                           !isSquareAttacked(6, Color::Black))
                            push(4,6,false,false,true);
                    }
                    if((castling & 0b0010) && isNone(b[3]) && isNone(b[2]) && isNone(b[1]) &&
                       b[0].t==PieceType::Rook && b[0].c==Color::White){
                        if(!inCheck(Color::White) &&
                           !isSquareAttacked(3, Color::Black) &&
                           !isSquareAttacked(2, Color::Black))
                            push(4,2,false,false,true);
                    }
                }
                if(us==Color::Black && i==60){
                    if((castling & 0b0100) && isNone(b[61]) && isNone(b[62]) &&
                       b[63].t==PieceType::Rook && b[63].c==Color::Black){
                        if(!inCheck(Color::Black) &&
                           !isSquareAttacked(61, Color::White) &&
                           !isSquareAttacked(62, Color::White))
                            push(60,62,false,false,true);
                    }
                    if((castling & 0b1000) && isNone(b[59]) && isNone(b[58]) && isNone(b[57]) &&
                       b[56].t==PieceType::Rook && b[56].c==Color::Black){
                        if(!inCheck(Color::Black) &&
                           !isSquareAttacked(59, Color::White) &&
                           !isSquareAttacked(58, Color::White))
                            push(60,58,false,false,true);
                    }
                }
            }
        }
    }

    bool makeMove(const Move& m, Undo& u){
        u.m = m;
        u.epSquare = epSquare;
        u.castling = castling;
        u.halfmoveClock = halfmoveClock;
        u.hash = hash;
        u.captured = Piece{};

        Piece moving = b[m.from];
        if(isNone(moving)) return false;

        bool resetHalf = (moving.t==PieceType::Pawn) || m.isCapture || m.isEnPassant;
        halfmoveClock = resetHalf ? 0 : (halfmoveClock + 1);

        if(z){
            int oldEpF = (epSquare>=0) ? (epSquare%8) : 8;
            hash ^= z->epFile[oldEpF];
            hash ^= z->castling[castling & 0xF];
            if(stm==Color::Black) hash ^= z->sideToMove;
        }

        epSquare = -1;

        if(m.isEnPassant){
            int dir = (moving.c==Color::White) ? -8 : 8;
            int capSq = int(m.to) + dir;
            u.captured = b[capSq];
            if(z && !isNone(u.captured)){
                int cc = (u.captured.c==Color::White)?0:1;
                hash ^= z->psq[cc][(int)u.captured.t][capSq];
            }
            b[capSq] = Piece{};
        } else if(m.isCapture){
            u.captured = b[m.to];
            if(z && !isNone(u.captured)){
                int cc = (u.captured.c==Color::White)?0:1;
                hash ^= z->psq[cc][(int)u.captured.t][(int)m.to];
            }
        }

        if(z){
            int mc = (moving.c==Color::White)?0:1;
            hash ^= z->psq[mc][(int)moving.t][(int)m.from];
        }

        b[m.to] = b[m.from];
        b[m.from] = Piece{};

        if(z){
            int mc = (moving.c==Color::White)?0:1;
            hash ^= z->psq[mc][(int)moving.t][(int)m.to];
        }

        if(m.promo != PieceType::None){
            if(z){
                int mc = (moving.c==Color::White)?0:1;
                hash ^= z->psq[mc][(int)PieceType::Pawn][(int)m.to];
                hash ^= z->psq[mc][(int)m.promo][(int)m.to];
            }
            b[m.to].t = m.promo;
        }

        if(m.isCastle){
            if(moving.c==Color::White){
                if(m.to==6){
                    Piece rook=b[7];
                    if(z){
                        int rc=0;
                        hash ^= z->psq[rc][(int)rook.t][7];
                        hash ^= z->psq[rc][(int)rook.t][5];
                    }
                    b[5]=b[7]; b[7]=Piece{};
                } else if(m.to==2){
                    Piece rook=b[0];
                    if(z){
                        int rc=0;
                        hash ^= z->psq[rc][(int)rook.t][0];
                        hash ^= z->psq[rc][(int)rook.t][3];
                    }
                    b[3]=b[0]; b[0]=Piece{};
                }
            } else {
                if(m.to==62){
                    Piece rook=b[63];
                    if(z){
                        int rc=1;
                        hash ^= z->psq[rc][(int)rook.t][63];
                        hash ^= z->psq[rc][(int)rook.t][61];
                    }
                    b[61]=b[63]; b[63]=Piece{};
                } else if(m.to==58){
                    Piece rook=b[56];
                    if(z){
                        int rc=1;
                        hash ^= z->psq[rc][(int)rook.t][56];
                        hash ^= z->psq[rc][(int)rook.t][59];
                    }
                    b[59]=b[56]; b[56]=Piece{};
                }
            }
        }

        auto clearIfTouches = [&](int sq, u8 mask){
            if(int(m.from)==sq || int(m.to)==sq) castling &= ~mask;
        };
        clearIfTouches(4,  0b0011);
        clearIfTouches(0,  0b0010);
        clearIfTouches(7,  0b0001);
        clearIfTouches(60, 0b1100);
        clearIfTouches(56, 0b1000);
        clearIfTouches(63, 0b0100);

        if(moving.t==PieceType::Pawn){
            int fr = int(m.from)/8;
            int tr = int(m.to)/8;
            if(std::abs(tr - fr) == 2){
                epSquare = (int(m.from) + int(m.to))/2;
            }
        }

        stm = other(stm);

        if(inCheck(other(stm))){
            undoMove(u);
            return false;
        }

        if(z){
            int newEpF = (epSquare>=0) ? (epSquare%8) : 8;
            hash ^= z->epFile[newEpF];
            hash ^= z->castling[castling & 0xF];
            if(stm==Color::Black) hash ^= z->sideToMove;
        }

        return true;
    }

    void undoMove(const Undo& u){
        const Move& m = u.m;

        stm = other(stm);

        epSquare = u.epSquare;
        castling = u.castling;
        halfmoveClock = u.halfmoveClock;
        hash = u.hash;

        Piece moved = b[m.to];

        if(m.isCastle){
            if(moved.c==Color::White){
                if(m.to==6){ b[7]=b[5]; b[5]=Piece{}; }
                else if(m.to==2){ b[0]=b[3]; b[3]=Piece{}; }
            } else {
                if(m.to==62){ b[63]=b[61]; b[61]=Piece{}; }
                else if(m.to==58){ b[56]=b[59]; b[59]=Piece{}; }
            }
        }

        b[m.from] = b[m.to];
        b[m.to] = Piece{};

        if(m.promo != PieceType::None){
            b[m.from].t = PieceType::Pawn;
        }

        if(m.isEnPassant){
            int dir = (b[m.from].c==Color::White) ? -8 : 8;
            int capSq = int(m.to) + dir;
            b[capSq] = u.captured;
        } else if(m.isCapture){
            b[m.to] = u.captured;
        }
    }

    void genLegalMoves(std::vector<Move>& legal){
        std::vector<Move> pseudo;
        genPseudoMoves(pseudo);
        legal.clear();
        legal.reserve(pseudo.size());

        for(const auto& m : pseudo){
            Undo u{};
            if(makeMove(m,u)){
                legal.push_back(m);
                undoMove(u);
            }
        }
    }

    void genLegalMovesFrom(int from, std::vector<Move>& out){
        std::vector<Move> legal;
        genLegalMoves(legal);
        out.clear();
        for(const auto& m: legal) if(m.from==from) out.push_back(m);
    }

    bool insufficientMaterial() const {
        int wMinor=0,bMinor=0;
        int wB=0,wN=0,bB=0,bN=0;
        int wOther=0,bOther=0;
        for(auto& p: b){
            if(isNone(p) || p.t==PieceType::King) continue;
            if(p.t==PieceType::Pawn || p.t==PieceType::Rook || p.t==PieceType::Queen){
                if(p.c==Color::White) wOther++;
                else bOther++;
            } else {
                if(p.c==Color::White){ wMinor++; if(p.t==PieceType::Bishop) wB++; if(p.t==PieceType::Knight) wN++; }
                else { bMinor++; if(p.t==PieceType::Bishop) bB++; if(p.t==PieceType::Knight) bN++; }
            }
        }
        if(wOther>0 || bOther>0) return false;
        if(wMinor==0 && bMinor==0) return true;
        if(wMinor==1 && bMinor==0 && (wB==1 || wN==1)) return true;
        if(bMinor==1 && wMinor==0 && (bB==1 || bN==1)) return true;
        if(wMinor==1 && bMinor==1 && wB==1 && bB==1) return true;
        return false;
    }
};

static std::string moveToSAN(const Board& position, const Move& m){
    Piece moving = position.at(m.from);
    if(isNone(moving)) return moveToUCI(m);

    const Square fromSq = indexToSq(m.from);
    const Square toSq   = indexToSq(m.to);

    std::string san;
    if(m.isCastle){
        san = (toSq.file > fromSq.file) ? "O-O" : "O-O-O";
    } else {
        const bool isPawn = (moving.t == PieceType::Pawn);
        const bool isCapture = m.isCapture || m.isEnPassant;

        if(!isPawn){
            san.push_back(sanPieceChar(moving.t));

            Board probe = position;
            std::vector<Move> legal;
            probe.genLegalMoves(legal);

            bool ambiguous = false;
            bool sameFile = false;
            bool sameRank = false;
            for(const auto& cand : legal){
                if(cand.from == m.from || cand.to != m.to) continue;
                Piece cp = probe.at(cand.from);
                if(cp.t != moving.t || cp.c != moving.c) continue;
                ambiguous = true;
                if((cand.from % 8) == (m.from % 8)) sameFile = true;
                if((cand.from / 8) == (m.from / 8)) sameRank = true;
            }

            if(ambiguous){
                if(!sameFile){
                    san.push_back(char('a' + fromSq.file));
                } else if(!sameRank){
                    san.push_back(char('1' + fromSq.rank));
                } else {
                    san.push_back(char('a' + fromSq.file));
                    san.push_back(char('1' + fromSq.rank));
                }
            }
        }

        if(isPawn && isCapture){
            san.push_back(char('a' + fromSq.file));
        }
        if(isCapture){
            san.push_back('x');
        }

        san += sqName(toSq);

        if(m.promo != PieceType::None){
            san.push_back('=');
            san.push_back(sanPieceChar(m.promo));
        }
    }

    Board after = position;
    Undo u{};
    if(after.makeMove(m, u)){
        std::vector<Move> replies;
        after.genLegalMoves(replies);
        if(after.inCheck(after.stm)){
            san.push_back(replies.empty() ? '#' : '+');
        }
    }

    return san;
}

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

    int repCount=0;
    for(u64 h : ctx.repetition){
        if(h==bd.hash) repCount++;
    }
    if(repCount>=3) return 0;

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

    for(int s=0; s<2; s++){
        for(int from=0; from<64; from++){
            for(int to=0; to<64; to++){
                ctx.history[s][from][to] = (ctx.history[s][from][to] * 7) / 8;
            }
        }
    }

    ctx.repetition.clear();
    ctx.repetition.push_back(bd.hash);

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

// ======================== Assets ========================
struct PieceAtlas {
    std::map<std::string, sf::Texture> tex;

    bool loadAll(const std::string& dir){
        const std::vector<std::string> colors = {"white_","black_"};
        const std::vector<std::string> names  = {"king","queen","rook","bishop","knight","pawn"};

        for(const auto& c : colors){
            for(const auto& n : names){
                std::string key = c+n;
                sf::Texture t;
                if(!t.loadFromFile(dir + "/" + key + ".png")) return false;
                t.setSmooth(false);
                tex.emplace(key, std::move(t));
            }
        }
        return true;
    }

    const sf::Texture* get(const Piece& p) const {
        auto k = pieceKey(p);
        auto it = tex.find(k);
        if(it==tex.end()) return nullptr;
        return &it->second;
    }
};

// ======================== App modes ========================
enum class GameMode { Menu, PvP, PvAI, AIvAI };
static std::string modeStr(GameMode m){
    switch(m){
        case GameMode::PvP: return "PvP";
        case GameMode::PvAI: return "PvAI";
        case GameMode::AIvAI: return "AIvAI";
        default: return "Menu";
    }
}

// ======================== Main ========================
int main(){
    constexpr unsigned windowW=1320, windowH=880;
    sf::ContextSettings ctx;
    ctx.antialiasingLevel = 0;
    sf::RenderWindow window(
        sf::VideoMode(windowW, windowH),
        "Chess Engine (SFML 2.6) - NEA Build",
        sf::Style::Titlebar | sf::Style::Close,
        ctx
    );
    window.setVerticalSyncEnabled(true);
    window.setFramerateLimit(60);

    const float tile = 96.f;
    const sf::Vector2f boardOrigin(40.f, 40.f);

    // SFML2: don't use FloatRect.position/size – use explicit vectors.
    const sf::Vector2f panelPos(boardOrigin.x + 8.f*tile + 30.f, boardOrigin.y);
    const sf::Vector2f panelSize(440.f, 8.f*tile);
    const float gameCardY = panelPos.y + 74.f;
    const float engineCardY = panelPos.y + 236.f;
    const float statusCardY = panelPos.y + 494.f;
    const float moveLogCardY = panelPos.y + 618.f;

    // Font: include Linux candidates (and keep your mac ones harmless)
    sf::Font font;
    bool hasFont=false;
    {
        std::vector<std::string> candidates = {
          "assets/fonts/Inter-Regular.ttf",
          // Fedora / Linux (DejaVu)
          "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf",
          "/usr/share/fonts/dejavu-sans-mono-fonts/DejaVuSansMono.ttf",
          // macOS (harmless on Linux)
          "/System/Library/Fonts/Supplemental/Verdana.ttf",
          "/System/Library/Fonts/Supplemental/Arial.ttf",
          "/System/Library/Fonts/Supplemental/Trebuchet MS.ttf",
          "/System/Library/Fonts/SFNS.ttf",
          "/System/Library/Fonts/Helvetica.ttc"
        };
        for(const auto& p : candidates){
            if(std::filesystem::exists(p)){
                hasFont = font.loadFromFile(p);
                if(hasFont){
                    font.setSmooth(true);
                    break;
                }
            }
        }
    }

    PieceAtlas atlas;
    bool hasIcons = atlas.loadAll("assets/pieces_png");

    Zobrist zob;
    Board board;
    board.setZobrist(&zob);
    board.reset();

    // UI thread never calls search now; search runs in a worker thread.
    const int ttSizeMB = 256;
    SearchContext aiSearchCtx;
    aiSearchCtx.tt.resizeMB(ttSizeMB);

    std::vector<Undo> undoStack;
    std::vector<std::string> moveListUCI;
    std::vector<std::string> moveListSAN;

    auto pushMove = [&](const Move& m)->bool{
        const std::string san = moveToSAN(board, m);
        Undo u{};
        if(board.makeMove(m, u)){
            undoStack.push_back(u);
            moveListUCI.push_back(moveToUCI(m));
            moveListSAN.push_back(san);
            return true;
        }
        return false;
    };
    auto popUndo = [&](){
        if(undoStack.empty()) return;
        Undo u = undoStack.back();
        undoStack.pop_back();
        board.undoMove(u);
        if(!moveListUCI.empty()) moveListUCI.pop_back();
        if(!moveListSAN.empty()) moveListSAN.pop_back();
    };
    auto getBookMove = [&]()->std::optional<Move>{
        if(moveListUCI.size() >= 18) return std::nullopt;

        static const std::vector<std::vector<std::string>> openings = {
            {"e2e4","c7c5","g1f3","d7d6","d2d4","c5d4","f3d4","g8f6","b1c3","a7a6"},
            {"e2e4","e7e5","g1f3","b8c6","f1b5","a7a6","b5a4","g8f6","e1g1"},
            {"e2e4","e7e5","g1f3","b8c6","f1c4","g8f6","d2d3","f8c5","c2c3"},
            {"d2d4","d7d5","c2c4","e7e6","b1c3","g8f6","c1g5","f8e7"},
            {"d2d4","g8f6","c2c4","e7e6","g1f3","d7d5","b1c3","f8e7"},
            {"d2d4","g8f6","c2c4","g7g6","b1c3","f8g7","e2e4","d7d6"},
            {"c2c4","e7e5","b1c3","g8f6","g2g3","d7d5","c4d5","f6d5"},
            {"g1f3","d7d5","g2g3","g8f6","f1g2","c7c6","e1g1"},
            {"e2e4","c7c6","d2d4","d7d5","b1c3","d5e4","c3e4","c8f5"},
            {"e2e4","e7e6","d2d4","d7d5","b1c3","g8f6","c1g5","f8e7"}
        };

        std::vector<Move> legal;
        board.genLegalMoves(legal);
        if(legal.empty()) return std::nullopt;

        std::vector<std::string> candidates;
        for(const auto& line : openings){
            if(moveListUCI.size() >= line.size()) continue;
            bool prefix = true;
            for(size_t i=0; i<moveListUCI.size(); i++){
                if(moveListUCI[i] != line[i]){
                    prefix = false;
                    break;
                }
            }
            if(prefix){
                candidates.push_back(line[moveListUCI.size()]);
            }
        }

        if(candidates.empty()) return std::nullopt;
        for(const auto& uci : candidates){
            auto it = std::find_if(legal.begin(), legal.end(), [&](const Move& m){
                return moveToUCI(m) == uci;
            });
            if(it != legal.end()){
                return *it;
            }
        }
        return std::nullopt;
    };
    auto writeMovesFile = [&](const std::filesystem::path& outPath)->bool{
        Board replay;
        replay.setZobrist(board.z);
        replay.reset();

        std::vector<std::string> sanMoves;
        sanMoves.reserve(undoStack.size());
        for(const auto& u : undoStack){
            const Move& m = u.m;
            sanMoves.push_back(moveToSAN(replay, m));
            Undo replayUndo{};
            if(!replay.makeMove(m, replayUndo)){
                sanMoves.back() = moveToUCI(m);
                break;
            }
        }

        std::string result = "*";
        std::string termination = "Game in progress";
        {
            std::vector<Move> legal;
            replay.genLegalMoves(legal);
            if(legal.empty()){
                if(replay.inCheck(replay.stm)){
                    result = (replay.stm == Color::White) ? "0-1" : "1-0";
                    termination = "Checkmate";
                } else {
                    result = "1/2-1/2";
                    termination = "Stalemate";
                }
            } else if(replay.insufficientMaterial()){
                result = "1/2-1/2";
                termination = "Insufficient material";
            } else if(replay.halfmoveClock >= 100){
                result = "1/2-1/2";
                termination = "50-move rule";
            }
        }

        std::ofstream out(outPath);
        if(!out) return false;

        out << "[Event \"Local Game\"]\n";
        out << "[Result \"" << result << "\"]\n";
        out << "[Termination \"" << termination << "\"]\n\n";

        if(sanMoves.empty()){
            out << "{No moves played}\n";
            return true;
        }

        for(size_t i=0; i<sanMoves.size(); i+=2){
            out << (i/2 + 1) << ". " << sanMoves[i];
            if(i + 1 < sanMoves.size()) out << " " << sanMoves[i + 1];
            out << "\n";
        }
        return true;
    };
    auto askOpenMovesFile = [&]()->bool{
#ifdef __APPLE__
        const std::string cmd =
            "osascript -e \"button returned of (display dialog \\\"Moves saved to moves.txt. Open now?\\\" "
            "buttons {\\\"Don't Open\\\",\\\"Open\\\"} default button \\\"Open\\\")\"";
        FILE* p = popen(cmd.c_str(), "r");
        if(p){
            char buf[256];
            std::string out;
            while(fgets(buf, sizeof(buf), p)) out += buf;
            int rc = pclose(p);
            if(rc == 0){
                return trim(out) == "Open";
            }
        }
#endif
        std::cout << "Moves saved to moves.txt. Open now? [y/N]: " << std::flush;
        std::string ans;
        std::getline(std::cin, ans);
        if(ans.empty()) return false;
        char c = ans[0];
        return c=='y' || c=='Y';
    };
    auto openMovesFile = [&](const std::filesystem::path& outPath){
#ifdef __APPLE__
        std::string cmd = "open " + shellQuote(outPath.string());
        std::system(cmd.c_str());
#elif defined(__linux__)
        std::string cmd = "xdg-open " + shellQuote(outPath.string()) + " >/dev/null 2>&1 &";
        std::system(cmd.c_str());
#elif defined(_WIN32)
        std::string cmd = "start \"\" " + shellQuote(outPath.string());
        std::system(cmd.c_str());
#endif
    };

    GameMode mode = GameMode::Menu;
    GameMode pending = GameMode::PvP;
    Color humanColor = Color::White;   

    std::string status = hasIcons ? "Ready." : "Missing icons: assets/pieces_png/*.png";

    std::optional<int> selectedSq;
    std::vector<Move> selectedMoves;
    std::optional<Move> lastMove;

    bool dragging=false;
    std::optional<int> dragFrom;
    sf::Vector2f dragPos(0,0);

    int aiMaxDepth = 20;
    int aiTimeMs = 10000;
    int aiDelayMs = 35;
    sf::Clock aiClock;

    bool flipBoard=false;

    auto setMenuSelection = [&](int idx){
        if(idx==0){
            pending = GameMode::PvP;
            humanColor = Color::White;
        } else if(idx==1){
            pending = GameMode::PvAI;
            humanColor = Color::White;
        } else if(idx==2){
            pending = GameMode::PvAI;
            humanColor = Color::Black;
        } else {
            pending = GameMode::AIvAI;
            humanColor = Color::White;
        }
    };
    auto getMenuSelection = [&]()->int{
        if(pending==GameMode::PvP) return 0;
        if(pending==GameMode::PvAI && humanColor==Color::White) return 1;
        if(pending==GameMode::PvAI && humanColor==Color::Black) return 2;
        return 3;
    };
    auto moveMenuSelection = [&](int delta){
        int idx = (getMenuSelection() + delta) % 4;
        if(idx < 0) idx += 4;
        setMenuSelection(idx);
    };
    auto getMenuCardRects = [&]()->std::array<sf::FloatRect,4>{
        return {
            sf::FloatRect(96.f, 234.f, 760.f, 102.f),
            sf::FloatRect(96.f, 350.f, 760.f, 102.f),
            sf::FloatRect(96.f, 466.f, 760.f, 102.f),
            sf::FloatRect(96.f, 582.f, 760.f, 102.f)
        };
    };
    auto getMenuStartRect = [&]()->sf::FloatRect{
        return sf::FloatRect(908.f, 586.f, 204.f, 58.f);
    };
    auto getAiPauseRect = [&]()->sf::FloatRect{
        return sf::FloatRect(panelPos.x + panelSize.x - 168.f, engineCardY + 18.f, 146.f, 30.f);
    };

    auto isHumanSide = [&](Color c)->bool{
        if(mode==GameMode::PvP) return true;
        if(mode==GameMode::PvAI) return (c==humanColor);
      return false;
    };

    auto refreshSelection = [&](){
        selectedMoves.clear();
        if(!selectedSq) return;
        board.genLegalMovesFrom(*selectedSq, selectedMoves);
    };

    auto resetGame = [&](){
        board.reset();
        undoStack.clear();
        moveListUCI.clear();
        selectedSq.reset();
        selectedMoves.clear();
        lastMove.reset();
        moveListSAN.clear();
        dragging=false;
        dragFrom.reset();
        status = "Reset.";
    };
    auto startPendingGame = [&](){
        mode = pending;
        resetGame();

        if(mode == GameMode::PvAI && humanColor == Color::Black){
            flipBoard = true;
        } else if(mode == GameMode::PvAI && humanColor == Color::White){
            flipBoard = false;
        }

        status = "Game started: " + modeStr(mode);
    };

    auto tryMoveFromTo = [&](int from, int to)->bool{
        std::vector<Move> moves;
        board.genLegalMovesFrom(from, moves);
        auto it = std::find_if(moves.begin(), moves.end(), [&](const Move& m){
            return m.to==to;
        });
        if(it==moves.end()) return false;

        Move chosen = *it;
        if(chosen.promo!=PieceType::None && chosen.promo!=PieceType::Queen){
            auto itQ = std::find_if(moves.begin(), moves.end(), [&](const Move& m){
                return m.to==to && m.promo==PieceType::Queen;
            });
            if(itQ!=moves.end()) chosen = *itQ;
        }

        if(pushMove(chosen)){
            lastMove = chosen;
            selectedSq.reset();
            selectedMoves.clear();
            status = "Played " + sqName(indexToSq(chosen.from)) + "->" + sqName(indexToSq(chosen.to));
            return true;
        }
        return false;
    };

    // ---------------- AI threading (prevents UI freezing / Fedora "not responding") ----------------
    std::atomic<bool> aiThinking(false);
    std::atomic<bool> aiMoveReady(false);
    std::atomic<bool> aiPaused(false);
    std::atomic<bool> aiAbortSearch(false);
    Move aiChosenMove{};
    SearchStats lastSearchStats{};
    std::string lastPV;
    std::mutex aiMutex;
    sf::Clock thinkClock;
    std::thread aiThread;

    struct EngineStepperRects {
        sf::FloatRect depthMinus;
        sf::FloatRect depthPlus;
        sf::FloatRect timeMinus;
        sf::FloatRect timePlus;
    };
    auto getEngineStepperRects = [&]()->EngineStepperRects{
        const float btnW = 32.f;
        const float btnH = 24.f;
        const float plusX  = panelPos.x + panelSize.x - 56.f;
        const float minusX = plusX - 38.f;
        const float depthY = engineCardY + 74.f;
        const float timeY  = engineCardY + 106.f;
        return EngineStepperRects{
            sf::FloatRect(minusX, depthY, btnW, btnH),
            sf::FloatRect(plusX,  depthY, btnW, btnH),
            sf::FloatRect(minusX, timeY,  btnW, btnH),
            sf::FloatRect(plusX,  timeY,  btnW, btnH)
        };
    };
    auto pointInRect = [](sf::Vector2f p, const sf::FloatRect& r)->bool{
        return p.x >= r.left && p.x <= (r.left + r.width) &&
               p.y >= r.top  && p.y <= (r.top + r.height);
    };
    auto adjustDepth = [&](int delta){
        aiMaxDepth = std::clamp(aiMaxDepth + delta, 1, 150);
        status = "AI max depth = " + std::to_string(aiMaxDepth);
    };
    auto adjustTime = [&](int deltaMs){
        aiTimeMs = std::clamp(aiTimeMs + deltaMs, 100, 180000);
        status = "AI time = " + std::to_string(aiTimeMs) + "ms";
    };

    auto stopAiThread = [&](){
        aiAbortSearch.store(true);
        if(aiThread.joinable()) aiThread.join();
        aiThinking.store(false);
        aiMoveReady.store(false);
        aiAbortSearch.store(false);
    };

    auto startAiThink = [&](){
        if(aiThinking.load() || aiPaused.load()) return;

        // don't search if game is over
        std::vector<Move> legal;
        board.genLegalMoves(legal);
        if(legal.empty()) return;

        if(auto bm = getBookMove()){
            {
                std::lock_guard<std::mutex> lock(aiMutex);
                aiChosenMove = *bm;
                lastSearchStats = SearchStats{};
                lastSearchStats.timeMs = 0;
                lastSearchStats.depthReached = 0;
                lastSearchStats.bestScore = 0;
                lastPV = "book";
            }
            aiMoveReady.store(true);
            aiThinking.store(false);
            return;
        }

        // join previous finished thread if needed
        if(aiThread.joinable()) aiThread.join();

        aiAbortSearch.store(false);
        aiThinking.store(true);
        aiMoveReady.store(false);
        thinkClock.restart();

        // snapshot board/settings so thread is safe
        Board searchBoard = board;
        int threadMaxDepth = aiMaxDepth;
        int threadTimeMs   = aiTimeMs;

        aiThread = std::thread([&, searchBoard, threadMaxDepth, threadTimeMs]() mutable {
            aiSearchCtx.abortFlag = &aiAbortSearch;
            Move m = searchBestMove(searchBoard, aiSearchCtx, threadMaxDepth, threadTimeMs);
            std::string pv = extractPVFromTT(searchBoard, aiSearchCtx, 12);

            if(aiAbortSearch.load() || aiPaused.load()){
                aiMoveReady.store(false);
                aiThinking.store(false);
                return;
            }

            {
                std::lock_guard<std::mutex> lock(aiMutex);
                aiChosenMove = m;
                lastSearchStats = aiSearchCtx.stats;
                lastPV = pv;
            }

            aiMoveReady.store(true);
            aiThinking.store(false);
        });
    };
    auto toggleAiPause = [&](){
        bool pausedNow = !aiPaused.load();
        aiPaused.store(pausedNow);
        if(pausedNow){
            stopAiThread();
            status = "AI paused.";
        } else {
            status = "AI resumed.";
        }
    };
    auto runUndo = [&](){
        stopAiThread();
        popUndo();
        selectedSq.reset();
        selectedMoves.clear();
        dragging=false;
        dragFrom.reset();
        if(undoStack.empty()) lastMove.reset();
        else lastMove = undoStack.back().m;
        status = "Undo.";
    };

    while(window.isOpen()){
        sf::Event e;
        while(window.pollEvent(e)){
            if(e.type == sf::Event::Closed) window.close();

            if(e.type == sf::Event::KeyPressed){
                auto code = e.key.code;

                if(code == sf::Keyboard::Escape) window.close();

                if(mode==GameMode::Menu){
                    if(code == sf::Keyboard::Num1){
                        setMenuSelection(0);
                    }

                    // NEW: choose side for PvAI
                    if(code == sf::Keyboard::Num2){
                        setMenuSelection(1);
                    }
                    if(code == sf::Keyboard::Num3){
                        setMenuSelection(2);
                    }

                    // AI vs AI moved to 4
                    if(code == sf::Keyboard::Num4){
                        setMenuSelection(3);
                    }

                    if(code == sf::Keyboard::Up || code == sf::Keyboard::Left){
                        moveMenuSelection(-1);
                    }
                    if(code == sf::Keyboard::Down || code == sf::Keyboard::Right){
                        moveMenuSelection(+1);
                    }

                    if(code == sf::Keyboard::Enter){
                        startPendingGame();
                    }
                } else {
                    if(code == sf::Keyboard::R){
                        stopAiThread();
                        resetGame();
                    }
                    if(code == sf::Keyboard::U){
                        runUndo();
                    }
                    if(code == sf::Keyboard::P){
                        toggleAiPause();
                    }

                    if(code == sf::Keyboard::F){
                        flipBoard = !flipBoard;
                        status = std::string("Flip: ") + (flipBoard ? "ON" : "OFF");
                    }

                    // depth
                    if(code == sf::Keyboard::Equal || code == sf::Keyboard::Add){
                        adjustDepth(+1);
                    }
                    if(code == sf::Keyboard::Hyphen || code == sf::Keyboard::Subtract){
                        adjustDepth(-1);
                    }

                    // time per move (let it go big if you want)
                    if(code == sf::Keyboard::T){
                        adjustTime(+250);
                    }
                    if(code == sf::Keyboard::Y){
                        adjustTime(-250);
                    }
                }
            }

            if(mode==GameMode::Menu){
                if(e.type == sf::Event::MouseButtonPressed && e.mouseButton.button == sf::Mouse::Left){
                    sf::Vector2f mp(float(e.mouseButton.x), float(e.mouseButton.y));
                    if(getMenuStartRect().contains(mp)){
                        startPendingGame();
                        continue;
                    }
                    auto menuRects = getMenuCardRects();
                    for(int i=0;i<4;i++){
                        if(menuRects[size_t(i)].contains(mp)){
                            setMenuSelection(i);
                            break;
                        }
                    }
                }
            }

            if(mode!=GameMode::Menu){
                if(e.type == sf::Event::MouseButtonPressed && e.mouseButton.button == sf::Mouse::Left){
                    sf::Vector2f mp(float(e.mouseButton.x), float(e.mouseButton.y));
                    if(getAiPauseRect().contains(mp)){ toggleAiPause(); continue; }
                    const EngineStepperRects step = getEngineStepperRects();
                    if(pointInRect(mp, step.depthMinus)){ adjustDepth(-1); continue; }
                    if(pointInRect(mp, step.depthPlus)){  adjustDepth(+1); continue; }
                    if(pointInRect(mp, step.timeMinus)){  adjustTime(-250); continue; }
                    if(pointInRect(mp, step.timePlus)){   adjustTime(+250); continue; }
                }

                // Only allow human input if it's human side AND we aren't mid-AI-search (prevents weirdness in PvAI)
                if(isHumanSide(board.stm) && !aiThinking.load()){
                    if(e.type == sf::Event::MouseButtonPressed){
                        if(e.mouseButton.button == sf::Mouse::Left){
                            sf::Vector2f mp(float(e.mouseButton.x), float(e.mouseButton.y));
                            auto sq = pixelToSquare(mp, tile, boardOrigin, flipBoard);
                            if(!sq) continue;
                            int idx = sqToIndex(*sq);

                            // Click-to-move: if a piece is already selected, a second click
                            // on a legal destination plays that move immediately.
                            if(selectedSq){
                                int from = *selectedSq;
                                Piece sel = board.at(from);
                                if(!isNone(sel) && sel.c==board.stm && idx != from){
                                    if(tryMoveFromTo(from, idx)){
                                        dragging=false;
                                        dragFrom.reset();
                                        continue;
                                    }
                                }
                            }

                            selectedSq = idx;
                            refreshSelection();

                            Piece p = board.at(idx);
                            if(!isNone(p) && p.c==board.stm){
                                dragging=true;
                                dragFrom=idx;
                                dragPos=mp;
                            }
                        }
                    }

                    if(e.type == sf::Event::MouseMoved){
                        if(dragging) dragPos = sf::Vector2f(float(e.mouseMove.x), float(e.mouseMove.y));
                    }

                    if(e.type == sf::Event::MouseButtonReleased){
                        if(e.mouseButton.button == sf::Mouse::Left){
                            if(dragging && dragFrom){
                                sf::Vector2f mp(float(e.mouseButton.x), float(e.mouseButton.y));
                                auto sq = pixelToSquare(mp, tile, boardOrigin, flipBoard);
                                if(sq){
                                    int to = sqToIndex(*sq);
                                    if(to != *dragFrom){
                                        bool ok = tryMoveFromTo(*dragFrom, to);
                                        if(!ok) status = "Illegal move.";
                                    }
                                }
                            }
                            dragging=false;
                            dragFrom.reset();
                        }
                    }
                }
            }
        }

        // AI turn (NON-BLOCKING)
        if(mode!=GameMode::Menu && !isHumanSide(board.stm) && !aiPaused.load()){
            bool shouldMove = true;
            if(mode==GameMode::AIvAI){
                shouldMove = (aiClock.getElapsedTime().asMilliseconds() >= aiDelayMs);
            }

            if(shouldMove){
                if(!aiThinking.load() && !aiMoveReady.load()){
                    startAiThink();
                }

                if(aiMoveReady.load()){
                    Move m;
                    {
                        std::lock_guard<std::mutex> lock(aiMutex);
                        m = aiChosenMove;
                    }

                    if(pushMove(m)){
                        lastMove = m;
                        aiClock.restart();
                        status = "AI: " + moveToUCI(m);
                    } else {
                        status = "AI produced illegal move (should not happen).";
                    }

                    aiMoveReady.store(false);
                }
            }
        }

        window.clear(sf::Color(10,12,20));

        // -------- Menu --------
        if(mode==GameMode::Menu){
            if(hasFont){
                auto drawText = [&](float x, float y, unsigned size, sf::Color col, const std::string& str, sf::Uint32 style = sf::Text::Regular){
                    sf::Text t;
                    t.setFont(font);
                    t.setCharacterSize(size);
                    t.setFillColor(col);
                    t.setStyle(style);
                    t.setString(str);
                    setCrispTextPosition(t, sf::Vector2f(x, y));
                    window.draw(t);
                };

                auto isModeSelected = [&](int id)->bool{
                    if(id==1) return pending==GameMode::PvP;
                    if(id==2) return pending==GameMode::PvAI && humanColor==Color::White;
                    if(id==3) return pending==GameMode::PvAI && humanColor==Color::Black;
                    return pending==GameMode::AIvAI;
                };

                auto selectionLabel = [&]()->std::string{
                    if(pending==GameMode::PvP) return "Player vs Player";
                    if(pending==GameMode::AIvAI) return "Watch AI vs AI";
                    return (humanColor==Color::White)
                        ? "Player vs AI (Play as White)"
                        : "Player vs AI (Play as Black)";
                };

                sf::RectangleShape shell(sf::Vector2f(windowW - 120.f, windowH - 120.f));
                shell.setPosition(snap(sf::Vector2f(60.f, 60.f)));
                shell.setFillColor(sf::Color(12,14,24));
                shell.setOutlineThickness(2.f);
                shell.setOutlineColor(sf::Color(52,60,88));
                window.draw(shell);

                sf::RectangleShape accent(sf::Vector2f(shell.getSize().x, 8.f));
                accent.setPosition(shell.getPosition());
                accent.setFillColor(sf::Color(58,132,255));
                window.draw(accent);

                drawText(96.f, 95.f, 22, sf::Color(130,160,220), "CHESS ENGINE", sf::Text::Bold);
                drawText(96.f, 134.f, 48, sf::Color(240,244,255), "Choose Your Match");
                drawText(96.f, 184.f, 18, sf::Color(160,170,200), "Use arrows, click a card, or press 1-4, then Enter to start.");

                sf::Vector2i mousePixel = sf::Mouse::getPosition(window);
                sf::Vector2f mousePos(float(mousePixel.x), float(mousePixel.y));

                auto drawModeCard = [&](float y, int key, const std::string& title, const std::string& subtitle){
                    const bool selected = isModeSelected(key);
                    const sf::FloatRect cardRect(96.f, y, 760.f, 102.f);
                    const bool hover = cardRect.contains(mousePos);

                    sf::RectangleShape card(sf::Vector2f(760.f, 102.f));
                    card.setPosition(snap(sf::Vector2f(96.f, y)));
                    card.setFillColor(selected ? sf::Color(30,74,150) : (hover ? sf::Color(28,35,56) : sf::Color(20,24,36)));
                    card.setOutlineThickness(2.f);
                    card.setOutlineColor(selected ? sf::Color(120,190,255) : (hover ? sf::Color(92,126,188) : sf::Color(50,58,82)));
                    window.draw(card);

                    sf::RectangleShape keyChip(sf::Vector2f(46.f, 46.f));
                    keyChip.setPosition(snap(sf::Vector2f(114.f, y + 28.f)));
                    keyChip.setFillColor(selected ? sf::Color(150,210,255) : sf::Color(36,42,62));
                    keyChip.setOutlineThickness(1.f);
                    keyChip.setOutlineColor(selected ? sf::Color(215,240,255) : sf::Color(70,80,110));
                    window.draw(keyChip);

                    drawText(130.f, y + 38.f, 24, selected ? sf::Color(16,26,40) : sf::Color(220,228,245), std::to_string(key), sf::Text::Bold);
                    drawText(178.f, y + 26.f, 30, sf::Color(238,242,255), title);
                    drawText(178.f, y + 62.f, 18, selected ? sf::Color(210,228,255) : sf::Color(160,174,206), subtitle);

                    if(selected){
                        sf::RectangleShape tag(sf::Vector2f(106.f, 34.f));
                        tag.setPosition(snap(sf::Vector2f(730.f, y + 34.f)));
                        tag.setFillColor(sf::Color(215,240,255));
                        tag.setOutlineThickness(1.f);
                        tag.setOutlineColor(sf::Color(140,190,230));
                        window.draw(tag);
                        drawText(744.f, y + 41.f, 16, sf::Color(16,36,56), "Selected", sf::Text::Bold);
                    }
                };

                drawModeCard(234.f, 1, "Player vs Player", "Two humans on one board.");
                drawModeCard(350.f, 2, "Player vs AI (White)", "You move first, AI responds.");
                drawModeCard(466.f, 3, "Player vs AI (Black)", "AI opens, you defend and counter.");
                drawModeCard(582.f, 4, "Watch AI vs AI", "Let both engines play automatically.");

                sf::RectangleShape side(sf::Vector2f(244.f, 450.f));
                side.setPosition(snap(sf::Vector2f(888.f, 234.f)));
                side.setFillColor(sf::Color(18,22,34));
                side.setOutlineThickness(2.f);
                side.setOutlineColor(sf::Color(45,54,80));
                window.draw(side);

                drawText(908.f, 262.f, 24, sf::Color(232,238,252), "Session");
                drawText(908.f, 300.f, 18, sf::Color(165,178,210), "Current:");
                drawText(908.f, 326.f, 19, sf::Color(220,230,250), selectionLabel());
                drawText(908.f, 376.f, 18, sf::Color(165,178,210), "Controls:");
                drawText(908.f, 404.f, 18, sf::Color(220,230,250), "Arrows / click mode");
                drawText(908.f, 430.f, 18, sf::Color(220,230,250), "Enter start game");
                drawText(908.f, 456.f, 18, sf::Color(220,230,250), "Esc   quit");
                drawText(908.f, 506.f, 18, sf::Color(165,178,210), "Assets:");
                drawText(908.f, 534.f, 18, hasIcons ? sf::Color(145,220,170) : sf::Color(255,180,180),
                         hasIcons ? "Piece icons loaded" : "Piece icons missing");

                const sf::FloatRect startRect = getMenuStartRect();
                const bool startHover = startRect.contains(mousePos);
                sf::RectangleShape startBtn(sf::Vector2f(startRect.width, startRect.height));
                startBtn.setPosition(snap(sf::Vector2f(startRect.left, startRect.top)));
                startBtn.setFillColor(startHover ? sf::Color(62,134,255) : sf::Color(46,108,214));
                startBtn.setOutlineThickness(2.f);
                startBtn.setOutlineColor(startHover ? sf::Color(184,220,255) : sf::Color(112,170,240));
                window.draw(startBtn);
                drawText(startRect.left + 36.f, startRect.top + 10.f, 23, sf::Color(236,246,255), "Start Game", sf::Text::Bold);
                drawText(startRect.left + 77.f, startRect.top + 35.f, 14, sf::Color(210,230,255), "Enter");

                drawText(96.f, 752.f, 18, sf::Color(138,152,188), "Tip: press 2 or 3 to play against AI as White or Black.");
            }
            window.display();
            continue;
        }

        // -------- Draw board --------
        const float boardPadL = 20.f;
        const float boardPadR = 14.f;
        const float boardPadT = 20.f;
        const float boardPadB = 30.f;
        sf::RectangleShape boardShell(sf::Vector2f(8.f*tile + boardPadL + boardPadR, 8.f*tile + boardPadT + boardPadB));
        boardShell.setPosition(snap(sf::Vector2f(boardOrigin.x - boardPadL, boardOrigin.y - boardPadT)));
        boardShell.setFillColor(sf::Color(16,20,32));
        boardShell.setOutlineThickness(2.f);
        boardShell.setOutlineColor(sf::Color(52,60,88));
        window.draw(boardShell);

        sf::RectangleShape boardAccent(sf::Vector2f(boardShell.getSize().x, 6.f));
        boardAccent.setPosition(boardShell.getPosition());
        boardAccent.setFillColor(sf::Color(58,132,255));
        window.draw(boardAccent);

        for(int r=0;r<8;r++){
            for(int f=0;f<8;f++){
                Square s{f,r};
                int idx = sqToIndex(s);

                sf::RectangleShape rect(sf::Vector2f(tile,tile));
                rect.setPosition(snap(squareToPixel(s, tile, boardOrigin, flipBoard)));

                bool dark = ((f+r)%2)==1;
                sf::Color base = dark ? sf::Color(78,83,108) : sf::Color(214,219,233);

                if(lastMove && idx==lastMove->from){
                    base = dark ? sf::Color(205,110,35) : sf::Color(245,150,70);
                }
                if(lastMove && idx==lastMove->to){
                    base = dark ? sf::Color(50,145,225) : sf::Color(120,195,250);
                }
                if(selectedSq && idx==*selectedSq) base = lighten(base, 55);

                rect.setFillColor(base);
                window.draw(rect);
            }
        }

        for(const auto& m : selectedMoves){
            sf::RectangleShape hl(sf::Vector2f(tile,tile));
            hl.setPosition(snap(squareToPixel(indexToSq(m.to), tile, boardOrigin, flipBoard)));
            hl.setFillColor(sf::Color(88,208,164,116));
            window.draw(hl);
        }

        for(Color c : {Color::White, Color::Black}){
            if(board.inCheck(c)){
                int k = board.findKing(c);
                if(k>=0){
                    sf::RectangleShape red(sf::Vector2f(tile,tile));
                    red.setPosition(snap(squareToPixel(indexToSq(k), tile, boardOrigin, flipBoard)));
                    red.setFillColor(sf::Color(228,76,76,124));
                    window.draw(red);
                }
            }
        }

        if(hasFont){
            const float fileLabelY = flipBoard ? (boardOrigin.y - 7.f) : (boardOrigin.y + 8.f*tile + 10.f);
            const float rankLabelX = boardOrigin.x - 12.f;
            for(int f=0; f<8; f++){
                sf::Text t;
                t.setFont(font);
                t.setCharacterSize(14);
                t.setFillColor(sf::Color(126,138,170));
                t.setString(std::string(1, char('a'+f)));
                setCrispTextPosition(t, sf::Vector2f(boardOrigin.x + (flipBoard?(7-f):f)*tile + 6.f, fileLabelY));
                window.draw(t);
            }
            for(int r=0; r<8; r++){
                sf::Text t;
                t.setFont(font);
                t.setCharacterSize(14);
                t.setFillColor(sf::Color(126,138,170));
                t.setString(std::to_string(r+1));
                int rr = flipBoard ? (7-r) : r;
                auto pos = squareToPixel(Square{0,rr}, tile, boardOrigin, flipBoard);
                setCrispTextPosition(t, sf::Vector2f(rankLabelX, pos.y + 6.f));
                window.draw(t);
            }
        }

        auto drawPiece = [&](const Piece& p, sf::Vector2f pos){
            if(!hasIcons) return;
            const sf::Texture* tex = atlas.get(p);
            if(!tex) return;
            sf::Sprite spr(*tex);
            auto sz = tex->getSize();
            spr.setScale(sf::Vector2f(tile/float(sz.x), tile/float(sz.y)));
            spr.setPosition(snap(pos));
            window.draw(spr);
        };

        for(int i=0;i<64;i++){
            if(dragging && dragFrom && i==*dragFrom) continue;
            Piece p = board.at(i);
            if(isNone(p)) continue;
            drawPiece(p, squareToPixel(indexToSq(i), tile, boardOrigin, flipBoard));
        }

        if(dragging && dragFrom){
            Piece p = board.at(*dragFrom);
            if(!isNone(p)){
                drawPiece(p, snap(sf::Vector2f(dragPos.x - tile/2.f, dragPos.y - tile/2.f)));
            }
        }

        // panel
        sf::RectangleShape panelBg(panelSize);
        panelBg.setPosition(panelPos);
        panelBg.setFillColor(sf::Color(14,18,30));
        panelBg.setOutlineThickness(2.f);
        panelBg.setOutlineColor(sf::Color(52,60,88));
        window.draw(panelBg);

        sf::RectangleShape panelAccent(sf::Vector2f(panelSize.x, 6.f));
        panelAccent.setPosition(panelPos);
        panelAccent.setFillColor(sf::Color(58,132,255));
        window.draw(panelAccent);

        if(hasFont){
            auto drawText = [&](float x, float y, unsigned size, sf::Color col, const std::string& str, sf::Uint32 style = sf::Text::Regular){
                sf::Text t;
                t.setFont(font);
                t.setCharacterSize(size);
                t.setFillColor(col);
                t.setStyle(style);
                t.setString(str);
                setCrispTextPosition(t, sf::Vector2f(x, y));
                window.draw(t);
            };

            auto WRAPAT = [&](float x, float y, float w, const std::string& txt, int size=14, sf::Color col=sf::Color(220,228,245)){
                return drawWrappedText(window, font, txt, (unsigned)size, sf::Vector2f(x, y), w, col);
            };

            auto drawCard = [&](float y, float h, const sf::Color& fill = sf::Color(20,24,36)){
                sf::RectangleShape card(sf::Vector2f(panelSize.x - 22.f, h));
                card.setPosition(snap(sf::Vector2f(panelPos.x + 11.f, y)));
                card.setFillColor(fill);
                card.setOutlineThickness(1.f);
                card.setOutlineColor(sf::Color(50,58,82));
                window.draw(card);
            };

            std::vector<Move> legalMoves;
            board.genLegalMoves(legalMoves);

            std::string stateLabel = "State: NORMAL";
            sf::Color stateColor(170,184,218);
            if(legalMoves.empty()){
                if(board.inCheck(board.stm)){
                    stateLabel = "State: CHECKMATE";
                    stateColor = sf::Color(255,175,175);
                } else {
                    stateLabel = "State: STALEMATE";
                    stateColor = sf::Color(255,220,170);
                }
            } else if(board.inCheck(board.stm)){
                stateLabel = "State: CHECK";
                stateColor = sf::Color(255,205,155);
            }

            SearchStats s;
            std::string pv;
            {
                std::lock_guard<std::mutex> lock(aiMutex);
                s = lastSearchStats;
                pv = lastPV;
            }

            double nps = (s.timeMs > 0) ? (double(s.nodes) * 1000.0 / double(s.timeMs)) : 0.0;
            double qPct = (s.nodes > 0) ? (100.0 * double(s.qnodes) / double(s.nodes)) : 0.0;
            double pawns = double(s.bestScore) / 100.0;

            drawText(panelPos.x + 16.f, panelPos.y + 18.f, 27, sf::Color(236,242,255), "Match Dashboard");
            drawText(panelPos.x + 16.f, panelPos.y + 48.f, 16, sf::Color(142,156,190), "Live mode, engine and game-state telemetry.");

            const float cardX = panelPos.x + 24.f;
            const float cardW = panelSize.x - 48.f;
            const float cardTextX = cardX + 12.f;
            const float gameCardH = 150.f;
            const float engineCardH = 246.f;
            const float statusCardH = 112.f;
            const float moveLogCardH = panelSize.y - (moveLogCardY - panelPos.y) - 12.f;

            drawCard(gameCardY, gameCardH);
            drawText(cardTextX, gameCardY + 20.f, 18, sf::Color(190,204,236), "Game");
            drawText(cardTextX, gameCardY + 48.f, 16, sf::Color(224,232,249), "Mode: " + modeStr(mode));
            drawText(cardTextX, gameCardY + 72.f, 16, sf::Color(224,232,249),
                     std::string("Turn: ") + (board.stm==Color::White ? "White" : "Black"));
            drawText(cardTextX, gameCardY + 96.f, 16, stateColor, stateLabel, sf::Text::Bold);
            drawText(cardTextX, gameCardY + 120.f, 14, sf::Color(168,182,215), "R reset  U undo  F flip  P pause  Esc quit");

            drawCard(engineCardY, engineCardH);
            drawText(cardTextX, engineCardY + 20.f, 18, sf::Color(190,204,236), "Engine");

            std::ostringstream evalText;
            if(std::abs(s.bestScore) > MATE/2){
                int matePly = std::max(1, MATE - std::abs(s.bestScore));
                int mateMoves = std::max(1, (matePly + 1) / 2);
                evalText << "Eval: " << (s.bestScore >= 0 ? "M" : "-M") << mateMoves;
            } else {
                evalText << "Eval: " << std::showpos << std::fixed << std::setprecision(2) << pawns;
            }
            drawText(cardTextX, engineCardY + 44.f, 13, sf::Color(174,194,236), evalText.str());
            drawText(cardTextX + 162.f, engineCardY + 44.f, 13, sf::Color(124,146,192),
                     "TT " + std::to_string(ttSizeMB) + "MB");
            {
                float evalNorm = 0.5f;
                if(std::abs(s.bestScore) > MATE/2){
                    evalNorm = (s.bestScore >= 0) ? 1.f : 0.f;
                } else {
                    double cp = std::clamp(double(s.bestScore), -1200.0, 1200.0);
                    evalNorm = float(0.5 + 0.5 * std::tanh(cp / 300.0));
                }
                evalNorm = std::clamp(evalNorm, 0.f, 1.f);

                const float barX = cardTextX;
                const float barY = engineCardY + 60.f;
                const float barW = cardW - 24.f;
                const float barH = 10.f;

                sf::RectangleShape barBg(sf::Vector2f(barW, barH));
                barBg.setPosition(snap(sf::Vector2f(barX, barY)));
                barBg.setFillColor(sf::Color(32,40,62));
                barBg.setOutlineThickness(1.f);
                barBg.setOutlineColor(sf::Color(62,84,124));
                window.draw(barBg);

                sf::RectangleShape barFill(sf::Vector2f(std::max(2.f, barW * evalNorm), barH));
                barFill.setPosition(snap(sf::Vector2f(barX, barY)));
                barFill.setFillColor(sf::Color(74,174,255));
                window.draw(barFill);

                sf::RectangleShape mid(sf::Vector2f(1.f, barH + 2.f));
                mid.setPosition(snap(sf::Vector2f(barX + barW * 0.5f, barY - 1.f)));
                mid.setFillColor(sf::Color(212,224,250,180));
                window.draw(mid);
            }

            const EngineStepperRects step = getEngineStepperRects();
            sf::Vector2i mousePixel = sf::Mouse::getPosition(window);
            sf::Vector2f mousePos(float(mousePixel.x), float(mousePixel.y));
            const sf::FloatRect pauseRect = getAiPauseRect();
            auto drawStepperButton = [&](const sf::FloatRect& r, const std::string& label){
                const bool hover = pointInRect(mousePos, r);
                sf::RectangleShape btn(sf::Vector2f(r.width, r.height));
                btn.setPosition(snap(sf::Vector2f(r.left, r.top)));
                btn.setFillColor(hover ? sf::Color(56,80,130) : sf::Color(34,44,70));
                btn.setOutlineThickness(1.f);
                btn.setOutlineColor(hover ? sf::Color(150,200,255) : sf::Color(96,128,192));
                window.draw(btn);
                drawText(r.left + 9.f, r.top + 2.f, 18, sf::Color(228,236,255), label, sf::Text::Bold);
            };
            {
                const bool pauseHover = pauseRect.contains(mousePos);
                sf::RectangleShape pauseBtn(sf::Vector2f(pauseRect.width, pauseRect.height));
                pauseBtn.setPosition(snap(sf::Vector2f(pauseRect.left, pauseRect.top)));
                pauseBtn.setFillColor(aiPaused.load()
                    ? (pauseHover ? sf::Color(92,132,84) : sf::Color(68,110,62))
                    : (pauseHover ? sf::Color(126,96,72) : sf::Color(102,76,58)));
                pauseBtn.setOutlineThickness(1.f);
                pauseBtn.setOutlineColor(aiPaused.load()
                    ? sf::Color(176,228,160)
                    : sf::Color(235,196,164));
                window.draw(pauseBtn);
                drawText(pauseRect.left + 10.f, pauseRect.top + 7.f, 14, sf::Color(236,244,255),
                         aiPaused.load() ? "Resume AI (P)" : "Pause AI (P)", sf::Text::Bold);
            }

            {
                drawText(cardTextX, engineCardY + 74.f, 16, sf::Color(220,228,245),
                         "Depth: " + std::to_string(aiMaxDepth));
                drawText(cardTextX, engineCardY + 106.f, 16, sf::Color(220,228,245),
                         "Time budget: " + std::to_string(aiTimeMs) + "ms");
                drawStepperButton(step.depthMinus, "-");
                drawStepperButton(step.depthPlus, "+");
                drawStepperButton(step.timeMinus, "-");
                drawStepperButton(step.timePlus, "+");
                drawText(cardTextX, engineCardY + 134.f, 13, sf::Color(158,176,215),
                         "Click +/- or use (+/-), (T/Y) | Book + TT on");
            }

            float statsY = engineCardY + 158.f;
            if(aiThinking.load()){
                int ms = thinkClock.getElapsedTime().asMilliseconds();
                std::ostringstream oss;
                oss << "Thinking... " << ms << "ms / " << aiTimeMs << "ms";
                drawText(cardTextX, statsY, 15, sf::Color(255,210,170), oss.str(), sf::Text::Bold);
            } else if(aiPaused.load()){
                drawText(cardTextX, statsY, 15, sf::Color(192,232,180), "Paused", sf::Text::Bold);
            } else {
                drawText(cardTextX, statsY, 15, sf::Color(150,220,168), "Idle");
            }
            statsY += 24.f;
            {
                std::ostringstream oss;
                oss << "Depth " << s.depthReached
                    << " | Score " << std::fixed << std::setprecision(2) << pawns
                    << " | Time " << s.timeMs << "ms";
                statsY += WRAPAT(cardTextX, statsY, cardW - 24.f, oss.str(), 14, sf::Color(200,214,242));
            }
            {
                std::ostringstream oss;
                oss << "Nodes " << s.nodes
                    << " | Q " << s.qnodes
                    << " (" << std::fixed << std::setprecision(1) << qPct << "%)"
                    << " | NPS " << (long long)nps;
                statsY += WRAPAT(cardTextX, statsY, cardW - 24.f, oss.str(), 14, sf::Color(180,196,232));
            }
            {
                std::ostringstream meta;
                meta << "Pos: halfmove " << board.halfmoveClock
                     << " | ep " << (board.epSquare>=0 ? sqName(indexToSq(board.epSquare)) : "-")
                     << " | castling " << int(board.castling);
                statsY += WRAPAT(cardTextX, statsY, cardW - 24.f, meta.str(), 14, sf::Color(160,178,210));
            }
            if(!pv.empty()){
                std::string pvCompact = "PV: " + pv;
                if(pvCompact.size() > 64) pvCompact = pvCompact.substr(0, 61) + "...";
                if(statsY + font.getLineSpacing(14) <= (engineCardY + engineCardH - 12.f)){
                    drawText(cardTextX, statsY, 14, sf::Color(168,214,255), pvCompact);
                }
            }

            drawCard(statusCardY, statusCardH);
            drawText(cardTextX, statusCardY + 20.f, 18, sf::Color(190,204,236), "Status");
            float statusY = statusCardY + 50.f;
            statusY += WRAPAT(cardTextX, statusY, cardW - 24.f, status, 14, sf::Color(225,232,248));
            if(selectedSq){
                if(statusY + font.getLineSpacing(14) <= statusCardY + statusCardH - 10.f){
                    drawText(cardTextX, statusY, 14, sf::Color(178,196,232),
                             "Selected " + sqName(indexToSq(*selectedSq)) + " | legal " + std::to_string((int)selectedMoves.size()));
                    statusY += font.getLineSpacing(14);
                }
            }
            if(board.insufficientMaterial()){
                if(statusY + font.getLineSpacing(14) <= statusCardY + statusCardH - 10.f){
                    drawText(cardTextX, statusY, 14, sf::Color(220,212,170), "Likely draw: insufficient material");
                }
            }

            drawCard(moveLogCardY, moveLogCardH);
            drawText(cardTextX, moveLogCardY + 20.f, 18, sf::Color(190,204,236), "Move Log");
            float listY = moveLogCardY + 46.f;
            int start = std::max(0, (int)moveListSAN.size()-16);
            for(int i=start; i<(int)moveListSAN.size(); i++){
                std::string prefix = (i%2==0) ? (std::to_string(i/2 + 1) + ". ") : "   ";
                float used = WRAPAT(cardTextX, listY, cardW - 24.f, prefix + moveListSAN[i], 14, sf::Color(208,220,246));
                listY += used;
                if(listY > (moveLogCardY + moveLogCardH - 18.f)) break;
            }
        }

        window.display();
    }

    stopAiThread();

    const std::filesystem::path movesPath = std::filesystem::current_path() / "moves.txt";
    if(writeMovesFile(movesPath)){
        if(askOpenMovesFile()){
            openMovesFile(movesPath);
        }
    } else {
        std::cerr << "Failed to write moves file: " << movesPath << "\n";
    }

    return 0;
}
