#include "chess_core.hpp"
#include "time_management.hpp"

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

void testPerft(const Zobrist& zobrist, bool quick){
    struct Case { const char* fen; int depth; u64 expected; };
    const std::vector<Case> cases = {
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
         quick ? 3 : 4, quick ? 8902ULL : 197281ULL},
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

void testPseudoMobilityCounter(const Zobrist& zobrist){
    auto compare = [&](const Board& position, const std::string& label){
        for(const Color color : {Color::White, Color::Black}){
            Board generated = position;
            generated.stm = color;
            std::vector<Move> moves;
            generated.genPseudoMoves(moves);
            expect(pseudoMobility(position, color) == static_cast<int>(moves.size()),
                   label + " mobility counter should match generated pseudo moves");
        }
    };

    Board board;
    board.setZobrist(&zobrist);
    board.reset();
    compare(board, "start position");

    const std::vector<std::string> fixtures = {
        "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1",
        "4k3/P6p/8/3pP3/8/8/8/4K3 w - d6 0 1",
        "4k3/2q5/3r4/2B1N3/3Q4/8/4R3/4K3 b - - 0 1",
    };
    for(const std::string& fen : fixtures){
        expect(board.loadFEN(fen), "mobility fixture should load");
        compare(board, "fixture");
    }

    board.reset();
    std::mt19937 random(0x4D4F4249U);
    for(int ply = 0; ply < 100; ply++){
        compare(board, "random playout");
        std::vector<Move> legal;
        board.genLegalMoves(legal);
        if(legal.empty()) break;
        Undo undo{};
        expect(board.makeMove(legal[static_cast<size_t>(random() % legal.size())], undo),
               "random mobility move should be makeable");
    }
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
    expect(!board.loadFEN("8/8/8/8/8/8/8/8 w - - 0 1"), "kingless FEN must fail");

    const std::string fen = "r3k2r/ppp2ppp/2n5/3qp3/8/2N2N2/PPP2PPP/R2Q1RK1 b kq - 7 14";
    expect(board.loadFEN(fen), "round-trip FEN should load");
    expect(board.toFEN() == fen, "FEN serialization should preserve all six fields");
}

void testTranspositionClusters(){
    TranspositionTable table;
    table.resizeMB(1);
    const u64 stride = static_cast<u64>(table.mask + 1);
    const Move move{};
    for(u64 collision = 0; collision < TranspositionTable::ClusterSize; collision++){
        const u64 key = 17 + collision * stride;
        table.store(key, static_cast<int>(collision + 1), static_cast<int>(collision * 10), TTFlag::Exact, move);
    }
    for(u64 collision = 0; collision < TranspositionTable::ClusterSize; collision++){
        const u64 key = 17 + collision * stride;
        const auto entry = table.probe(key);
        expect(entry && entry->key == key, "TT cluster should retain colliding positions");
    }

    Move packedMove{};
    packedMove.from = 12;
    packedMove.to = 34;
    packedMove.promo = PieceType::Queen;
    table.store(0x123456789ABCDEF0ULL, 63, -12345, TTFlag::Upper, packedMove);
    const auto packedEntry = table.probe(0x123456789ABCDEF0ULL);
    expect(packedEntry && packedEntry->depth == 63 && packedEntry->score == -12345 &&
           packedEntry->flag == TTFlag::Upper &&
           packedEntry->best.from == 12 && packedEntry->best.to == 34 &&
           packedEntry->best.promo == PieceType::Queen,
           "packed TT entry should preserve score, depth, flag, and best move");
}

void testConcurrentTranspositionTable(){
    TranspositionTable table;
    table.resizeMB(1);
    const u64 stride = static_cast<u64>(table.mask + 1);
    constexpr int WorkerCount = 8;
    constexpr int StoresPerWorker = 2000;
    std::atomic<bool> valid{true};
    std::vector<std::thread> workers;
    workers.reserve(WorkerCount);

    for(int worker = 0; worker < WorkerCount; worker++){
        workers.emplace_back([&, worker](){
            for(int index = 1; index <= StoresPerWorker; index++){
                const u64 serial = static_cast<u64>(worker * StoresPerWorker + index);
                const u64 key = 29 + serial * stride;
                const int depth = 1 + int(serial % 63);
                const int score = int(serial % 60001) - 30000;
                Move move{};
                move.from = static_cast<u8>(serial % 64);
                move.to = static_cast<u8>((serial / 3) % 64);
                move.promo = static_cast<PieceType>(serial % 7);
                const TTFlag flag = static_cast<TTFlag>(serial % 3);
                table.store(key, depth, score, flag, move);

                const auto entry = table.probe(key);
                if(entry && (entry->depth != depth || entry->score != score ||
                   entry->flag != flag || entry->best.from != move.from ||
                   entry->best.to != move.to || entry->best.promo != move.promo)){
                    valid.store(false, std::memory_order_relaxed);
                    return;
                }
            }
        });
    }

    for(std::thread& worker : workers) worker.join();
    expect(valid.load(std::memory_order_relaxed),
           "concurrent TT collisions must produce complete entries or clean misses");
}

void testNnueFormat(const Zobrist& zobrist){
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "chess-engine-nnue-format-test.nnue";
    {
        std::ofstream output(path, std::ios::binary);
        const std::array<char, 8> magic{{'T','N','N','U','E','1','\0','\0'}};
        const u32 version = NnueNetwork::FormatVersion;
        const u32 features = NnueNetwork::FeatureCount;
        const u32 hidden = 1;
        const int32_t hiddenScale = 1;
        const int32_t outputScale = 1;
        const int32_t outputBias = 42;
        const int32_t hiddenBias = 0;
        const std::vector<int16_t> inputWeights(features, 0);
        const std::array<int16_t, 2> outputWeights{{0, 0}};
        output.write(magic.data(), static_cast<std::streamsize>(magic.size()));
        output.write(reinterpret_cast<const char*>(&version), sizeof(version));
        output.write(reinterpret_cast<const char*>(&features), sizeof(features));
        output.write(reinterpret_cast<const char*>(&hidden), sizeof(hidden));
        output.write(reinterpret_cast<const char*>(&hiddenScale), sizeof(hiddenScale));
        output.write(reinterpret_cast<const char*>(&outputScale), sizeof(outputScale));
        output.write(reinterpret_cast<const char*>(&outputBias), sizeof(outputBias));
        output.write(reinterpret_cast<const char*>(&hiddenBias), sizeof(hiddenBias));
        output.write(reinterpret_cast<const char*>(inputWeights.data()),
                     static_cast<std::streamsize>(inputWeights.size() * sizeof(int16_t)));
        output.write(reinterpret_cast<const char*>(outputWeights.data()), sizeof(outputWeights));
    }

    PositionEvaluator evaluator;
    std::string error;
    expect(evaluator.loadNnue(path, &error), "valid NNUE v1 fixture should load: " + error);
    expect(evaluator.setUseNnue(true), "loaded NNUE should be selectable");
    Board board;
    board.setZobrist(&zobrist);
    board.reset();
    expect(evaluator.evaluate(board) == 42, "NNUE quantized inference should match fixture output");
    std::error_code removeError;
    std::filesystem::remove(path, removeError);
}

void testStaticExchange(const Zobrist& zobrist){
    Board board;
    board.setZobrist(&zobrist);
    expect(board.loadFEN("4k3/8/4p3/3p4/8/8/8/3QK3 w - - 0 1"), "SEE fixture should load");
    const Move queenTakesPawn = findMove(board, "d1d5");
    expect(queenTakesPawn.from < 64, "SEE fixture capture should be legal");
    expect(staticExchangeEvaluation(board, queenTakesPawn) <= -700,
           "SEE should identify a queen taking a defended pawn as losing");

    expect(board.loadFEN("4k3/8/8/3q4/8/8/3R4/4K3 w - - 0 1"), "winning SEE fixture should load");
    const Move rookTakesQueen = findMove(board, "d2d5");
    expect(rookTakesQueen.from < 64, "winning SEE capture should be legal");
    expect(staticExchangeEvaluation(board, rookTakesQueen) == 900,
           "SEE should value an undefended queen capture");
}

void testClockTimeManagement(const Zobrist& zobrist){
    Board board;
    board.setZobrist(&zobrist);
    board.reset();

    const TimeBudget protectedClock = pickClockTimeBudget(board, 20, 20, -1, 25);
    expect(protectedClock.softMs == 1 && protectedClock.hardMs == 1,
           "move overhead should preserve a one-millisecond emergency clock budget");

    const TimeBudget smallerOverhead = pickClockTimeBudget(board, 20, 20, -1, 10);
    expect(smallerOverhead.softMs >= 1 && smallerOverhead.hardMs >= smallerOverhead.softMs,
           "low-clock budget should remain positive and ordered");
    expect(smallerOverhead.hardMs <= 10,
           "low-clock hard limit should not consume the configured reserve");

    const TimeBudget normalClock = pickClockTimeBudget(board, 60'000, 500, 30, 25);
    expect(normalClock.softMs > 0 && normalClock.hardMs >= normalClock.softMs,
           "normal clock budget should remain positive and ordered");
    expect(normalClock.hardMs <= 59'975,
           "normal clock hard limit should retain the configured move overhead");
}

void testParallelSearchSafety(const Zobrist& zobrist){
    Board singleBoard;
    singleBoard.setZobrist(&zobrist);
    singleBoard.reset();
    Board parallelBoard = singleBoard;

    SearchContext single;
    single.tt.resizeMB(16);
    single.gameHistory = {singleBoard.hash};
    const Move singleBest = searchBestMove(singleBoard, single, 2, 2000, 2000, 1);

    SearchContext parallel;
    parallel.tt.resizeMB(16);
    parallel.gameHistory = {parallelBoard.hash};
    const Move parallelBest = searchBestMove(parallelBoard, parallel, 2, 2000, 2000, 4);

    expect(sameMove(singleBest, parallelBest),
           "parallel root tie must preserve the fully searched principal move");
    expect(single.stats.bestScore == parallel.stats.bestScore,
           "parallel root tie must preserve the principal score");

    SearchContext shortSearch;
    shortSearch.tt.resizeMB(16);
    shortSearch.gameHistory = {parallelBoard.hash};
    const Move shortBest = searchBestMove(parallelBoard, shortSearch, 64, 10, 20, 4);
    expect(shortBest.from < 64, "short parallel request should still return a legal move");
    expect(shortSearch.stats.configuredThreads == 4,
           "short search should report the requested thread count");
    expect(shortSearch.stats.workersUsed == 1,
           "short search should avoid thread-startup overhead");
}

} // namespace

int main(){
    const Zobrist zobrist;
    const bool quick = std::getenv("CHESS_TEST_QUICK") != nullptr;
    testPerft(zobrist, quick);
    testMakeUnmakeAndHash(zobrist);
    testEvaluationOrientation(zobrist);
    testPseudoMobilityCounter(zobrist);
    testEnPassantHashing(zobrist);
    testDrawRules(zobrist);
    testNotation(zobrist);
    testFENValidation(zobrist);
    testTranspositionClusters();
    testConcurrentTranspositionTable();
    testNnueFormat(zobrist);
    testStaticExchange(zobrist);
    testClockTimeManagement(zobrist);
    testParallelSearchSafety(zobrist);

    if(failures != 0){
        std::cerr << failures << " test assertion(s) failed\n";
        return 1;
    }
    std::cout << "Core tests: PASS\n";
    return 0;
}
