#include "chess_core.hpp"

#include <functional>
#include <random>

namespace {

int failures = 0;

void expect(bool condition, const std::string& message){
    if(condition) return;
    std::cerr << "[FAIL] " << message << '\n';
    failures++;
}

bool samePosition(const Board& lhs, const Board& rhs){
    if(lhs.stm != rhs.stm || lhs.epSquare != rhs.epSquare || lhs.castling != rhs.castling ||
       lhs.halfmoveClock != rhs.halfmoveClock || lhs.fullmoveNumber != rhs.fullmoveNumber ||
       lhs.kingSquare != rhs.kingSquare || lhs.hash != rhs.hash){
        return false;
    }
    for(size_t index = 0; index < lhs.b.size(); index++){
        if(lhs.b[index].t != rhs.b[index].t || lhs.b[index].c != rhs.b[index].c) return false;
    }
    return true;
}

Move findMove(Board& board, const std::string& uci){
    std::vector<Move> legal;
    board.genLegalMoves(legal);
    const auto found = std::find_if(legal.begin(), legal.end(), [&](const Move& move){
        return moveToUCI(move) == uci;
    });
    if(found == legal.end()) return invalidMove();
    return *found;
}

void testPerft(const Zobrist& zobrist){
    struct Case { const char* fen; int depth; u64 expected; };
    const std::vector<Case> cases = {
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 4, 197281ULL},
        {"r3k2r/p1ppqpb1/bn2pnp1/2pP4/1p2P3/2N2N2/PPQBBPPP/R3K2R w KQkq - 0 1", 3, 85877ULL},
        {"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 3, 2812ULL},
        {"r3k2r/Pppp1ppp/1b3nbN/nP6/B1P1P3/5N2/Pp1P1PPP/R2Q1RK1 w kq - 0 1", 3, 35941ULL},
        {"rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 3, 62379ULL},
        {"r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/2NP1N2/PPP1QPPP/R4RK1 w - - 0 10", 3, 83034ULL}
    };

    for(const Case& test : cases){
        Board board;
        board.setZobrist(&zobrist);
        expect(board.loadFEN(test.fen), "perft FEN should load");
        expect(perft(board, test.depth) == test.expected, "perft mismatch at depth " + std::to_string(test.depth));
    }
}

void testMakeUnmakeAndHash(const Zobrist& zobrist){
    Board board;
    board.setZobrist(&zobrist);
    board.reset();
    const Board initial = board;

    std::vector<Move> legal;
    board.genLegalMoves(legal);
    for(const Move& move : legal){
        Undo undo{};
        expect(board.makeMove(move, undo), "generated legal move should be makeable");
        const u64 incrementalHash = board.hash;
        board.recomputeHash();
        expect(board.hash == incrementalHash, "incremental hash should match recomputed hash");
        board.undoMove(undo);
        expect(samePosition(board, initial), "make/unmake should restore the exact root position");
    }

    std::mt19937 random(0x54495241U);
    std::vector<Undo> undoStack;
    for(int ply = 0; ply < 160; ply++){
        board.genLegalMoves(legal);
        if(legal.empty()) break;
        const Move move = legal[static_cast<size_t>(random() % legal.size())];
        Undo undo{};
        expect(board.makeMove(move, undo), "random legal move should be makeable");
        undoStack.push_back(undo);
        const u64 incrementalHash = board.hash;
        board.recomputeHash();
        expect(board.hash == incrementalHash, "random playout hash should remain incremental");
    }
    while(!undoStack.empty()){
        board.undoMove(undoStack.back());
        undoStack.pop_back();
    }
    expect(samePosition(board, initial), "random playout should unwind exactly to the start position");
}

void testEvaluationOrientation(const Zobrist& zobrist){
    Board board;
    board.setZobrist(&zobrist);
    board.reset();

    const int e2 = sqToIndex(Square{4, 1});
    const int e4 = sqToIndex(Square{4, 3});
    expect(PST_PAWN[mirrorIndex(e4)] > PST_PAWN[mirrorIndex(e2)],
           "white pawn PST should reward e4 over its initial e2 square");

    const Move e2e4 = findMove(board, "e2e4");
    expect(e2e4.from < 64, "e2e4 should be legal from the start position");
}

void testEnPassantHashing(const Zobrist& zobrist){
    Board unavailableEp;
    unavailableEp.setZobrist(&zobrist);
    expect(unavailableEp.loadFEN("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1"),
           "unavailable en-passant FEN should load");
    Board noEp;
    noEp.setZobrist(&zobrist);
    expect(noEp.loadFEN("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1"),
           "matching no-en-passant FEN should load");
    expect(unavailableEp.hash == noEp.hash,
           "unavailable en-passant target must not alter repetition hash");

    Board availableEp;
    availableEp.setZobrist(&zobrist);
    expect(availableEp.loadFEN("4k3/8/8/8/3pP3/8/8/4K3 b - e3 0 1"),
           "available en-passant FEN should load");
    Board availableNoEp;
    availableNoEp.setZobrist(&zobrist);
    expect(availableNoEp.loadFEN("4k3/8/8/8/3pP3/8/8/4K3 b - - 0 1"),
           "available position without en-passant target should load");
    expect(availableEp.hash != availableNoEp.hash,
           "available en-passant capture must alter repetition hash");
}

void testDrawRules(const Zobrist& zobrist){
    Board sameColorBishops;
    sameColorBishops.setZobrist(&zobrist);
    expect(sameColorBishops.loadFEN("5b1k/8/8/8/8/8/8/K1B5 w - - 0 1"), "same-colour bishop FEN should load");
    expect(sameColorBishops.insufficientMaterial(), "same-colour K+B vs K+B should be dead");

    Board oppositeColorBishops;
    oppositeColorBishops.setZobrist(&zobrist);
    expect(oppositeColorBishops.loadFEN("6bk/8/8/8/8/8/8/K1B5 w - - 0 1"), "opposite-colour bishop FEN should load");
    expect(!oppositeColorBishops.insufficientMaterial(), "opposite-colour K+B vs K+B is not automatically dead");

    Board repeated;
    repeated.setZobrist(&zobrist);
    repeated.reset();
    const std::vector<u64> history{repeated.hash, repeated.hash, repeated.hash};
    expect(assessGameStatus(repeated, history).termination == GameTermination::ThreefoldRepetition,
           "third occurrence should adjudicate a repetition draw");

    Board checkmateAtHundred;
    checkmateAtHundred.setZobrist(&zobrist);
    expect(checkmateAtHundred.loadFEN("7k/6Q1/6K1/8/8/8/8/8 b - - 100 75"), "checkmate FEN should load");
    expect(assessGameStatus(checkmateAtHundred, {checkmateAtHundred.hash}).termination == GameTermination::BlackCheckmated,
           "checkmate must take precedence over a rule-draw claim");
}

void testNotation(const Zobrist& zobrist){
    Board board;
    board.setZobrist(&zobrist);
    board.reset();
    Move move = findMove(board, "e2e4");
    expect(moveToSAN(board, move) == "e4", "e2e4 SAN should be e4");

    Undo undo{};
    expect(board.makeMove(move, undo), "e2e4 should be makeable for SAN continuation");
    move = findMove(board, "e7e5");
    expect(moveToSAN(board, move) == "e5", "e7e5 SAN should be e5");
}

void testFENValidation(const Zobrist& zobrist){
    Board board;
    board.setZobrist(&zobrist);
    expect(!board.loadFEN("8/8/8/8/8/8/8 w - - 0 1"), "FEN with seven ranks must fail");
    expect(!board.loadFEN("8/8/8/8/8/8/8/9 w - - 0 1"), "FEN digit 9 must fail");
    expect(!board.loadFEN("8/8/8/8/8/8/8/8 x - - 0 1"), "invalid side to move must fail");

    const std::string fen = "r3k2r/ppp2ppp/2n5/3qp3/8/2N2N2/PPP2PPP/R2Q1RK1 b kq - 7 14";
    expect(board.loadFEN(fen), "round-trip FEN should load");
    expect(board.toFEN() == fen, "FEN serialization should preserve all six fields");
}

} // namespace

int main(){
    const Zobrist zobrist;
    testPerft(zobrist);
    testMakeUnmakeAndHash(zobrist);
    testEvaluationOrientation(zobrist);
    testEnPassantHashing(zobrist);
    testDrawRules(zobrist);
    testNotation(zobrist);
    testFENValidation(zobrist);

    if(failures != 0){
        std::cerr << failures << " test assertion(s) failed\n";
        return 1;
    }
    std::cout << "Core tests: PASS\n";
    return 0;
}
