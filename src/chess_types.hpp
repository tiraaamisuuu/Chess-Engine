// Chess types and shared engine utilities.
#pragma once

#include <iomanip>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <cctype>
#include <cstring>
#include <optional>
#include <memory>
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
#include <condition_variable>
#include <functional>
#include <cstdio>
#include <cstdlib>
#include <ctime>

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

inline std::string trim(std::string s){
    while(!s.empty() && (s.back()=='\n' || s.back()=='\r' || s.back()==' ' || s.back()=='\t')) s.pop_back();
    size_t i = 0;
    while(i < s.size() && (s[i]==' ' || s[i]=='\t' || s[i]=='\n' || s[i]=='\r')) ++i;
    return s.substr(i);
}
inline std::string shellQuote(const std::string& s){
    std::string out = "'";
    for(char c : s){
        if(c=='\'') out += "'\\''";
        else out.push_back(c);
    }
    out.push_back('\'');
    return out;
}
inline std::string windowsCmdQuote(const std::string& s){
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
inline bool operator==(const Square& a, const Square& b){ return a.file==b.file && a.rank==b.rank; }
inline bool inBounds(const Square& s){ return s.file>=0 && s.file<8 && s.rank>=0 && s.rank<8; }
inline int sqToIndex(const Square& s){ return s.rank*8 + s.file; }
inline Square indexToSq(int idx){ return Square{idx%8, idx/8}; }
inline std::string sqName(const Square& s){
    return std::string() + char('a'+s.file) + char('1'+s.rank);
}

// ======================== Chess Types ========================
enum class Color : u8 { White=0, Black=1 };
inline Color other(Color c){ return c==Color::White ? Color::Black : Color::White; }

enum class PieceType : u8 { None=0, Pawn, Knight, Bishop, Rook, Queen, King };

struct Piece {
    PieceType t = PieceType::None;
    Color c = Color::White;
};
inline bool isNone(const Piece& p){ return p.t==PieceType::None; }

inline int pieceValue(PieceType t){
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

inline std::string pieceName(PieceType t){
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
inline std::string pieceKey(const Piece& p){
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

template<typename T, size_t Capacity>
class FixedList {
public:
    using iterator = typename std::array<T, Capacity>::iterator;
    using const_iterator = typename std::array<T, Capacity>::const_iterator;

    void clear(){ size_ = 0; }
    void reserve(size_t requested) const {
        if(requested > Capacity) std::abort();
    }
    void push_back(const T& value){
        if(size_ >= Capacity) std::abort();
        storage_[size_++] = value;
    }

    [[nodiscard]] bool empty() const { return size_ == 0; }
    [[nodiscard]] size_t size() const { return size_; }
    [[nodiscard]] constexpr size_t capacity() const { return Capacity; }
    T& operator[](size_t index){ return storage_[index]; }
    const T& operator[](size_t index) const { return storage_[index]; }
    iterator begin(){ return storage_.begin(); }
    iterator end(){ return storage_.begin() + static_cast<std::ptrdiff_t>(size_); }
    const_iterator begin() const { return storage_.begin(); }
    const_iterator end() const { return storage_.begin() + static_cast<std::ptrdiff_t>(size_); }

private:
    std::array<T, Capacity> storage_{};
    size_t size_ = 0;
};

// The maximum number of legal moves in a chess position is 218. Leave generous
// extra room for pseudo-legal generation while keeping hot lists on the stack.
using MoveList = FixedList<Move, 320>;

struct Undo {
    Move m{};
    Piece captured{};
    int epSquare=-1;
    u8 castling=0;
    int halfmoveClock=0;
    int fullmoveNumber=1;
    std::array<int, 2> kingSquare{{-1, -1}};
    u64 hash=0;
};

inline std::string moveToUCI(const Move& m){
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

inline char sanPieceChar(PieceType t){
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

struct TTSlot {
    std::atomic<u64> keyXor{0};
    std::atomic<u64> data{0};
};

struct TranspositionTable {
    static constexpr size_t ClusterSize = 4;
    std::unique_ptr<TTSlot[]> table;
    size_t entryCount=0;
    size_t mask=0;
    u8 generation=0;

    void resizeMB(size_t mb){
        size_t bytes = mb*1024ull*1024ull;
        const size_t requestedClusters = std::max<size_t>(1, bytes / (sizeof(TTSlot) * ClusterSize));
        size_t clusters = 1;
        while((clusters << 1) <= requestedClusters) clusters <<= 1;
        entryCount = clusters * ClusterSize;
        table = std::make_unique<TTSlot[]>(entryCount);
        mask = clusters - 1;
        generation = 0;
    }

    [[nodiscard]] size_t byteSize() const {
        return entryCount * sizeof(TTSlot);
    }

    [[nodiscard]] std::optional<TTEntry> probe(u64 key) const {
        if(!table) return std::nullopt;
        const size_t base = (size_t(key) & mask) * ClusterSize;
        for(size_t slot = 0; slot < ClusterSize; slot++){
            const TTEntry entry = load(base + slot);
            if(entry.key == key) return entry;
        }
        return std::nullopt;
    }

    void newSearch(){
        generation = static_cast<u8>(generation + 1);
    }

    static int ageOf(u8 currentGen, u8 entryGen){
        return int(static_cast<u8>(currentGen - entryGen));
    }

    void store(u64 key, int depth, int score, TTFlag flag, const Move& best){
        if(!table) return;
        const size_t base = (size_t(key) & mask) * ClusterSize;
        size_t replacementIndex = base;
        TTEntry replacementEntry{};
        bool replacementFound = false;
        int replacementQuality = 1000000;
        for(size_t slot = 0; slot < ClusterSize; slot++){
            const size_t index = base + slot;
            const TTEntry candidate = load(index);
            if(candidate.key == key){
                replacementIndex = index;
                replacementEntry = candidate;
                replacementFound = true;
                break;
            }
            if(candidate.key == 0){
                replacementIndex = index;
                replacementEntry = candidate;
                replacementFound = true;
                break;
            }
            const int quality = int(candidate.depth) - 2 * ageOf(generation, candidate.generation);
            if(quality < replacementQuality){
                replacementQuality = quality;
                replacementIndex = index;
                replacementEntry = candidate;
                replacementFound = true;
            }
        }

        if(!replacementFound) return;
        if(replacementEntry.key != 0 && replacementEntry.key != key && depth < replacementQuality) return;

        const u64 packed = pack(depth, score, generation, flag, best);
        TTSlot& replacement = table[replacementIndex];
        replacement.data.store(packed, std::memory_order_relaxed);
        replacement.keyXor.store(key ^ packed, std::memory_order_release);
    }

private:
    static u64 pack(int depth, int score, u8 entryGeneration, TTFlag flag, const Move& best){
        u64 packed = static_cast<u32>(score);
        packed |= u64(std::clamp(depth, 0, 127)) << 32;
        packed |= u64(entryGeneration) << 39;
        packed |= u64(static_cast<u8>(flag) & 0x3U) << 47;
        packed |= u64(best.from & 0x3FU) << 49;
        packed |= u64(best.to & 0x3FU) << 55;
        packed |= u64(static_cast<u8>(best.promo) & 0x7U) << 61;
        return packed;
    }

    static TTEntry unpack(u64 key, u64 packed){
        TTEntry entry{};
        entry.key = key;
        entry.score = static_cast<int32_t>(static_cast<u32>(packed));
        entry.depth = static_cast<int8_t>((packed >> 32) & 0x7FU);
        entry.generation = static_cast<u8>((packed >> 39) & 0xFFU);
        entry.flag = static_cast<TTFlag>((packed >> 47) & 0x3U);
        entry.best.from = static_cast<u8>((packed >> 49) & 0x3FU);
        entry.best.to = static_cast<u8>((packed >> 55) & 0x3FU);
        entry.best.promo = static_cast<PieceType>((packed >> 61) & 0x7U);
        return entry;
    }

    [[nodiscard]] TTEntry load(size_t index) const {
        const TTSlot& slot = table[index];
        const u64 keyXor = slot.keyXor.load(std::memory_order_acquire);
        const u64 packed = slot.data.load(std::memory_order_relaxed);
        return unpack(keyXor ^ packed, packed);
    }
};

// ======================== Board ========================
