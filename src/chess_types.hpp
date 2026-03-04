// Chess types and shared utilities (SFML 2.6.x)
#pragma once

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
static std::string windowsCmdQuote(const std::string& s){
    std::string out = "\"";
    for(char c : s){
        if(c=='"') out += "\"\"";
        else out.push_back(c);
    }
    out.push_back('"');
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
    int32_t score=0;
    int8_t depth=0;
    u8 generation=0;
    TTFlag flag=TTFlag::Exact;
    Move best{};
};

struct TranspositionTable {
    std::vector<TTEntry> table;
    size_t mask=0;
    u8 generation=0;

    void resizeMB(size_t mb){
        size_t bytes = mb*1024ull*1024ull;
        size_t n = std::max<size_t>(1, bytes / sizeof(TTEntry));
        size_t p=1;
        while(p < n) p<<=1;
        table.assign(p, TTEntry{});
        mask = p-1;
        generation = 0;
    }

    TTEntry* probe(u64 key){
        if(table.empty()) return nullptr;
        return &table[size_t(key) & mask];
    }

    void newSearch(){
        generation = static_cast<u8>(generation + 1);
    }

    static int ageOf(u8 currentGen, u8 entryGen){
        return int(static_cast<u8>(currentGen - entryGen));
    }

    void store(u64 key, int depth, int score, TTFlag flag, const Move& best){
        if(table.empty()) return;
        TTEntry& e = table[size_t(key) & mask];
        bool replace = false;
        if(e.key==0 || e.key==key){
            replace = true;
        } else {
            // Prefer deeper entries, but age out stale data from previous searches.
            const int existingQuality = int(e.depth) - 2 * ageOf(generation, e.generation);
            const int incomingQuality = depth;
            replace = incomingQuality >= existingQuality;
        }

        if(replace){
            e.key=key;
            e.depth=(int8_t)std::clamp(depth, 0, 127);
            e.score=score;
            e.generation=generation;
            e.flag=flag;
            e.best=best;
        }
    }
};

// ======================== Board ========================
