#include "ui.hpp"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

struct RuntimeResources {
    double cpuPercent = 0.0;
    size_t rssBytes = 0;
};

static size_t readProcessRSSBytes(){
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS_EX pmc{};
    if(GetProcessMemoryInfo(GetCurrentProcess(),
                            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
                            sizeof(pmc))){
        return static_cast<size_t>(pmc.WorkingSetSize);
    }
    return 0;
#elif defined(__APPLE__)
    mach_task_basic_info info{};
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if(task_info(mach_task_self(),
                 MACH_TASK_BASIC_INFO,
                 reinterpret_cast<task_info_t>(&info),
                 &count) == KERN_SUCCESS){
        return static_cast<size_t>(info.resident_size);
    }
    return 0;
#elif defined(__linux__)
    std::ifstream in("/proc/self/statm");
    long totalPages = 0;
    long rssPages = 0;
    if(!(in >> totalPages >> rssPages)) return 0;
    const long pageSize = sysconf(_SC_PAGESIZE);
    if(pageSize <= 0 || rssPages <= 0) return 0;
    return static_cast<size_t>(rssPages) * static_cast<size_t>(pageSize);
#else
    return 0;
#endif
}

class RuntimeResourceTracker {
public:
    RuntimeResourceTracker()
    : lastWall(std::chrono::steady_clock::now()), lastCpu(std::clock()){}

    void tick(RuntimeResources& out){
        const auto now = std::chrono::steady_clock::now();
        const double wallSec = std::chrono::duration<double>(now - lastWall).count();
        if(wallSec < 0.25) return;

        const std::clock_t cpuNow = std::clock();
        const double cpuSec = double(cpuNow - lastCpu) / double(CLOCKS_PER_SEC);
        if(wallSec > 0.0 && cpuSec >= 0.0){
            out.cpuPercent = std::max(0.0, (cpuSec / wallSec) * 100.0);
        }
        out.rssBytes = readProcessRSSBytes();

        lastWall = now;
        lastCpu = cpuNow;
    }

private:
    std::chrono::steady_clock::time_point lastWall;
    std::clock_t lastCpu{};
};

static bool startsWith(const std::string& s, const std::string& prefix){
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

static std::string toLowerASCII(std::string s){
    for(char& ch : s){
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return s;
}

static bool parseIntStrict(const std::string& s, int& out){
    try{
        size_t pos = 0;
        int v = std::stoi(s, &pos);
        if(pos != s.size()) return false;
        out = v;
        return true;
    } catch(...){
        return false;
    }
}

static bool parseUCIMove(Board& board, const std::string& uci, Move& out){
    std::vector<Move> legal;
    board.genLegalMoves(legal);
    auto it = std::find_if(legal.begin(), legal.end(), [&](const Move& m){
        return moveToUCI(m) == uci;
    });
    if(it == legal.end()) return false;
    out = *it;
    return true;
}

static bool applyUCIPositionCommand(const std::string& line, Board& board, std::vector<u64>& history){
    std::istringstream iss(line);
    std::string token;
    iss >> token; // "position"

    std::vector<std::string> parts;
    while(iss >> token){
        parts.push_back(token);
    }
    if(parts.empty()) return false;

    size_t idx = 0;
    if(parts[idx] == "startpos"){
        board.reset();
        idx++;
    } else if(parts[idx] == "fen"){
        idx++;
        const size_t fenStart = idx;
        while(idx < parts.size() && parts[idx] != "moves"){
            idx++;
        }
        if(idx - fenStart < 6) return false;

        std::string fen;
        for(size_t i = fenStart; i < idx; i++){
            if(!fen.empty()) fen.push_back(' ');
            fen += parts[i];
        }
        if(!board.loadFEN(fen)) return false;
    } else {
        return false;
    }

    history.clear();
    history.push_back(board.hash);

    if(idx >= parts.size()) return true;
    if(parts[idx] != "moves") return false;
    idx++;

    for(; idx < parts.size(); idx++){
        Move m{};
        if(!parseUCIMove(board, parts[idx], m)) return false;
        Undo u{};
        if(!board.makeMove(m, u)) return false;
        history.push_back(board.hash);
    }
    return true;
}

struct UCIGoParams {
    int depth = -1;
    int movetimeMs = -1;
    int wtimeMs = -1;
    int btimeMs = -1;
    int wincMs = 0;
    int bincMs = 0;
    int movesToGo = -1;
    bool infinite = false;
};

struct TimeBudget {
    int softMs = 1000;
    int hardMs = 1000;
};

static UCIGoParams parseUCIGoCommand(const std::string& line){
    UCIGoParams out;
    std::istringstream iss(line);
    std::string token;
    iss >> token; // "go"

    auto readInt = [&](int& dst){
        std::string v;
        if(!(iss >> v)) return;
        int parsed = 0;
        if(parseIntStrict(v, parsed)) dst = parsed;
    };

    while(iss >> token){
        if(token == "depth") readInt(out.depth);
        else if(token == "movetime") readInt(out.movetimeMs);
        else if(token == "wtime") readInt(out.wtimeMs);
        else if(token == "btime") readInt(out.btimeMs);
        else if(token == "winc") readInt(out.wincMs);
        else if(token == "binc") readInt(out.bincMs);
        else if(token == "movestogo") readInt(out.movesToGo);
        else if(token == "infinite") out.infinite = true;
        else if(token == "ponder"){
            // Supported as a token but treated the same as normal search.
        } else if(token == "searchmoves"){
            // Skip explicit move list for now.
            while(iss >> token){
                if(token.size() < 4 || token.size() > 5) break;
            }
            break;
        }
    }
    return out;
}

static int countNonKingPieces(const Board& board){
    int pieces = 0;
    for(const Piece& p : board.b){
        if(isNone(p) || p.t == PieceType::King) continue;
        pieces++;
    }
    return pieces;
}

static TimeBudget pickUCITimeBudget(const Board& board, const UCIGoParams& p){
    if(p.movetimeMs > 0) return TimeBudget{p.movetimeMs, p.movetimeMs};
    if(p.infinite) return TimeBudget{24 * 60 * 60 * 1000, 24 * 60 * 60 * 1000};

    const bool white = (board.stm == Color::White);
    const int sideTime = white ? p.wtimeMs : p.btimeMs;
    const int sideInc = white ? p.wincMs : p.bincMs;
    if(sideTime <= 0) return TimeBudget{1000, 1500};

    int movesToGo = p.movesToGo;
    if(movesToGo <= 0){
        const int pieces = countNonKingPieces(board);
        if(pieces >= 22) movesToGo = 32;
        else if(pieces >= 12) movesToGo = 24;
        else movesToGo = 16;
    }

    const int reserve = std::max(25, std::min(250, sideTime / 50));
    const int safeTime = std::max(20, sideTime - reserve);
    const int baseSlice = safeTime / std::max(1, movesToGo + 3);
    int soft = baseSlice + (sideInc * 3) / 4;
    if(movesToGo <= 8) soft += baseSlice / 3;
    if(sideTime < 2000) soft = std::max(15, baseSlice + sideInc / 2);
    soft = std::clamp(soft, 15, std::max(15, safeTime / 2));

    int hard = std::max(soft + 40, soft + soft / 2);
    hard = std::max(hard, baseSlice * 3 + sideInc);
    if(sideTime < 1000) hard = std::max(soft + 20, soft * 2);
    hard = std::clamp(hard, soft, safeTime);
    return TimeBudget{soft, hard};
}

static TimeBudget pickGuiTimeBudget(int requestedMs){
    const int soft = std::clamp(requestedMs, 100, 180000);
    const int hard = std::clamp(soft + std::max(250, soft / 4), soft, 180000);
    return TimeBudget{soft, hard};
}

static int runUCILoop(int defaultThreads = 1){
    Zobrist zob;
    Board board;
    board.setZobrist(&zob);
    board.reset();

    std::vector<u64> positionHistory{board.hash};

    const int maxThreads = std::max(1, int(std::thread::hardware_concurrency()));
    int searchThreads = std::clamp(defaultThreads, 1, maxThreads);
    int hashMB = 256;
    SearchContext searchCtx;
    searchCtx.tt.resizeMB(static_cast<size_t>(hashMB));

    std::atomic<bool> abortSearch(false);
    std::atomic<bool> thinking(false);
    std::mutex ioMutex;
    std::thread worker;

    auto stopSearch = [&](){
        abortSearch.store(true);
        if(worker.joinable()) worker.join();
        thinking.store(false);
        abortSearch.store(false);
    };

    auto resetSearchState = [&](){
        stopSearch();
        searchCtx = SearchContext{};
        searchCtx.tt.resizeMB(static_cast<size_t>(hashMB));
    };

    auto launchSearch = [&](int depth, TimeBudget budget){
        stopSearch();

        Board root = board;
        std::vector<u64> rootHistory = positionHistory;
        const int threadsThisSearch = searchThreads;
        thinking.store(true);
        abortSearch.store(false);

        worker = std::thread([&, root, rootHistory, depth, budget, threadsThisSearch]() mutable {
            searchCtx.abortFlag = &abortSearch;
            searchCtx.gameHistory = rootHistory;

            Move best = searchBestMove(root, searchCtx, depth, budget.softMs, budget.hardMs, threadsThisSearch);

            std::vector<Move> legal;
            root.genLegalMoves(legal);
            auto legalIt = std::find_if(legal.begin(), legal.end(), [&](const Move& m){
                return sameMove(m, best);
            });
            if(legalIt == legal.end()){
                if(!legal.empty()) best = legal.front();
                else best = Move{};
            }

            const u64 nodes = searchCtx.stats.nodes + searchCtx.stats.qnodes;
            const long long nps = (searchCtx.stats.timeMs > 0)
                ? static_cast<long long>((nodes * 1000ULL) / static_cast<u64>(searchCtx.stats.timeMs))
                : 0LL;

            const std::string pv = extractPVFromTT(root, searchCtx, 16);
            const std::string bestUCI = (legal.empty()) ? "0000" : moveToUCI(best);

            {
                std::lock_guard<std::mutex> lock(ioMutex);
                std::cout << "info depth " << searchCtx.stats.depthReached
                          << " score cp " << searchCtx.stats.bestScore
                          << " nodes " << nodes
                          << " nps " << nps
                          << " time " << searchCtx.stats.timeMs;
                if(!pv.empty()) std::cout << " pv " << pv;
                std::cout << "\n";
                std::cout << "bestmove " << bestUCI << "\n" << std::flush;
            }

            thinking.store(false);
            abortSearch.store(false);
        });
    };

    std::string line;
    while(std::getline(std::cin, line)){
        line = trim(line);
        if(line.empty()) continue;

        const std::string lower = toLowerASCII(line);

        if(lower == "uci"){
            std::lock_guard<std::mutex> lock(ioMutex);
            std::cout << "id name TiramisuChess v0.5.0-dev\n";
            std::cout << "id author Alfie + Codex\n";
            std::cout << "option name Hash type spin default 256 min 1 max 4096\n";
            std::cout << "option name Threads type spin default " << searchThreads
                      << " min 1 max " << maxThreads << "\n";
            std::cout << "uciok\n" << std::flush;
            continue;
        }

        if(lower == "isready"){
            std::lock_guard<std::mutex> lock(ioMutex);
            std::cout << "readyok\n" << std::flush;
            continue;
        }

        if(startsWith(lower, "setoption")){
            if(lower.find("name hash") != std::string::npos){
                const size_t valuePos = lower.find(" value ");
                if(valuePos != std::string::npos){
                    const std::string value = trim(line.substr(valuePos + 7));
                    int mb = 0;
                    if(parseIntStrict(value, mb)){
                        mb = std::clamp(mb, 1, 4096);
                        hashMB = mb;
                        resetSearchState();
                    }
                }
            } else if(lower.find("name threads") != std::string::npos){
                const size_t valuePos = lower.find(" value ");
                if(valuePos != std::string::npos){
                    const std::string value = trim(line.substr(valuePos + 7));
                    int t = 1;
                    if(parseIntStrict(value, t)){
                        searchThreads = std::clamp(t, 1, maxThreads);
                    }
                }
            } else if(lower.find("name clear hash") != std::string::npos){
                resetSearchState();
            }
            continue;
        }

        if(lower == "ucinewgame"){
            board.reset();
            positionHistory = {board.hash};
            resetSearchState();
            continue;
        }

        if(startsWith(lower, "position ")){
            stopSearch();
            if(!applyUCIPositionCommand(line, board, positionHistory)){
                std::lock_guard<std::mutex> lock(ioMutex);
                std::cout << "info string invalid position command\n" << std::flush;
            }
            continue;
        }

        if(startsWith(lower, "go")){
            const UCIGoParams go = parseUCIGoCommand(line);
            const int depth = (go.depth > 0) ? go.depth : 64;
            TimeBudget budget = pickUCITimeBudget(board, go);
            budget.softMs = std::clamp(budget.softMs, 1, 24 * 60 * 60 * 1000);
            budget.hardMs = std::clamp(budget.hardMs, budget.softMs, 24 * 60 * 60 * 1000);
            launchSearch(depth, budget);
            continue;
        }

        if(lower == "stop"){
            stopSearch();
            continue;
        }

        if(lower == "quit"){
            stopSearch();
            return 0;
        }

        if(lower == "ponderhit" || lower == "d" || lower == "debug on" || lower == "debug off"){
            continue;
        }

        std::lock_guard<std::mutex> lock(ioMutex);
        std::cout << "info string unknown command: " << line << "\n" << std::flush;
    }

    stopSearch();
    return 0;
}

int main(int argc, char** argv){
    Zobrist zob;
    Board board;
    board.setZobrist(&zob);
    board.reset();

    auto parseInt = [](const std::string& s, int& out)->bool{
        try{
            size_t pos = 0;
            int v = std::stoi(s, &pos);
            if(pos != s.size()) return false;
            out = v;
            return true;
        } catch(...){
            return false;
        }
    };

    int perftDepth = -1;
    bool perftDivideMode = false;
    bool runPerftTests = false;
    bool runBench = false;
    bool runUci = false;
    int cliThreads = 1;
    int perftSuiteMaxDepth = 4;
    int benchDepth = 8;
    int benchTimeMs = 4000;
    int benchTTMB = 256;
    std::string cliFen;

    for(int i=1; i<argc; i++){
        std::string a = argv[i];
        auto needValue = [&](const std::string& flag)->const char*{
            if(i + 1 >= argc){
                std::cerr << "Missing value for " << flag << "\n";
                return nullptr;
            }
            return argv[++i];
        };

        if(a == "--help" || a == "-h"){
            std::cout
                << "Usage:\n"
                << "  gui                        # launch GUI\n"
                << "  gui --perft <depth> [--fen \"...\"]\n"
                << "  gui --divide <depth> [--fen \"...\"]\n"
                << "  gui --perft-tests [--max-depth <n>]\n"
                << "  gui --uci\n"
                << "  gui --bench [--bench-depth <n>] [--bench-time <ms>] [--bench-tt <mb>] [--threads <n>]\n";
            return 0;
        } else if(a == "--perft"){
            const char* v = needValue("--perft");
            if(!v || !parseInt(v, perftDepth) || perftDepth < 0){
                std::cerr << "Invalid --perft depth\n";
                return 1;
            }
        } else if(a == "--divide"){
            const char* v = needValue("--divide");
            if(!v || !parseInt(v, perftDepth) || perftDepth < 0){
                std::cerr << "Invalid --divide depth\n";
                return 1;
            }
            perftDivideMode = true;
        } else if(a == "--fen"){
            const char* v = needValue("--fen");
            if(!v){
                return 1;
            }
            cliFen = v;
        } else if(a == "--perft-tests"){
            runPerftTests = true;
        } else if(a == "--max-depth"){
            const char* v = needValue("--max-depth");
            if(!v || !parseInt(v, perftSuiteMaxDepth) || perftSuiteMaxDepth < 1){
                std::cerr << "Invalid --max-depth value\n";
                return 1;
            }
        } else if(a == "--bench"){
            runBench = true;
        } else if(a == "--uci"){
            runUci = true;
        } else if(a == "--bench-depth"){
            const char* v = needValue("--bench-depth");
            if(!v || !parseInt(v, benchDepth) || benchDepth < 1){
                std::cerr << "Invalid --bench-depth value\n";
                return 1;
            }
        } else if(a == "--bench-time"){
            const char* v = needValue("--bench-time");
            if(!v || !parseInt(v, benchTimeMs) || benchTimeMs < 50){
                std::cerr << "Invalid --bench-time value\n";
                return 1;
            }
        } else if(a == "--bench-tt"){
            const char* v = needValue("--bench-tt");
            if(!v || !parseInt(v, benchTTMB) || benchTTMB < 1){
                std::cerr << "Invalid --bench-tt value\n";
                return 1;
            }
        } else if(a == "--threads"){
            const char* v = needValue("--threads");
            if(!v || !parseInt(v, cliThreads) || cliThreads < 1 || cliThreads > 64){
                std::cerr << "Invalid --threads value (1..64)\n";
                return 1;
            }
        } else {
            std::cerr << "Unknown argument: " << a << "\n";
            std::cerr << "Use --help for CLI options.\n";
            return 1;
        }
    }

    if(runPerftTests){
        return runPerftSuite(zob, perftSuiteMaxDepth);
    }

    if(perftDepth >= 0){
        if(!cliFen.empty()){
            if(!board.loadFEN(cliFen)){
                std::cerr << "Invalid FEN for --perft/--divide\n";
                return 1;
            }
        }

        const auto t0 = std::chrono::steady_clock::now();
        if(perftDivideMode){
            const auto lines = perftDivide(board, perftDepth);
            u64 total = 0;
            for(const auto& [mv, nodes] : lines){
                total += nodes;
                std::cout << mv << ": " << nodes << "\n";
            }
            const auto t1 = std::chrono::steady_clock::now();
            const int ms = (int)std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
            const double nps = (ms > 0) ? (double(total) * 1000.0 / double(ms)) : 0.0;
            std::cout << "Total: " << total << " nodes in " << ms
                      << " ms (" << static_cast<long long>(nps) << " nps)\n";
        } else {
            const u64 nodes = perft(board, perftDepth);
            const auto t1 = std::chrono::steady_clock::now();
            const int ms = (int)std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
            const double nps = (ms > 0) ? (double(nodes) * 1000.0 / double(ms)) : 0.0;
            std::cout << "Perft(" << perftDepth << ") = " << nodes
                      << " nodes in " << ms << " ms"
                      << " (" << static_cast<long long>(nps) << " nps)\n";
        }
        return 0;
    }

    if(runBench){
        return runSearchBenchmark(zob, benchDepth, benchTimeMs, benchTTMB, cliThreads);
    }

    if(runUci){
        return runUCILoop(cliThreads);
    }

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
          // Windows
          "C:/Windows/Fonts/segoeui.ttf",
          "C:/Windows/Fonts/arial.ttf",
          "C:/Windows/Fonts/calibri.ttf",
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

    // UI thread never calls search now; search runs in a worker thread.
    const int ttSizeMB = 256;
    SearchContext aiSearchCtx;
    aiSearchCtx.tt.resizeMB(ttSizeMB);

    std::vector<Undo> undoStack;
    std::vector<std::string> moveListUCI;
    std::vector<std::string> moveListSAN;
    std::vector<u64> positionHistory{board.hash};
    aiSearchCtx.gameHistory = positionHistory;

    auto pushMove = [&](const Move& m)->bool{
        const std::string san = moveToSAN(board, m);
        Undo u{};
        if(board.makeMove(m, u)){
            undoStack.push_back(u);
            moveListUCI.push_back(moveToUCI(m));
            moveListSAN.push_back(san);
            positionHistory.push_back(board.hash);
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
        if(positionHistory.size() > 1) positionHistory.pop_back();
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
        std::string cmd = "cmd /C start \"\" " + windowsCmdQuote(outPath.string());
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
    int aiThreads = std::clamp(cliThreads, 1, 64);
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
        positionHistory.clear();
        positionHistory.push_back(board.hash);
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
    RuntimeResourceTracker resourceTracker;
    RuntimeResources runtimeResources{};
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
        int threadThreads  = aiThreads;
        std::vector<u64> threadHistory = positionHistory;

        aiThread = std::thread([&, searchBoard, threadMaxDepth, threadTimeMs, threadThreads, threadHistory]() mutable {
            aiSearchCtx.abortFlag = &aiAbortSearch;
            aiSearchCtx.gameHistory = threadHistory;
            const TimeBudget budget = pickGuiTimeBudget(threadTimeMs);
            Move m = searchBestMove(searchBoard, aiSearchCtx, threadMaxDepth, budget.softMs, budget.hardMs, threadThreads);
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
        resourceTracker.tick(runtimeResources);

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

            const u64 totalNodes = s.nodes + s.qnodes;
            double nps = (s.timeMs > 0) ? (double(totalNodes) * 1000.0 / double(s.timeMs)) : 0.0;
            double qPct = (totalNodes > 0) ? (100.0 * double(s.qnodes) / double(totalNodes)) : 0.0;
            double pawns = double(s.bestScore) / 100.0;
            const double cpuCoresUsed = runtimeResources.cpuPercent / 100.0;

            auto compactCount = [](u64 v)->std::string{
                std::ostringstream oss;
                if(v >= 1000000000ULL){
                    oss << std::fixed << std::setprecision(2) << (double(v) / 1000000000.0) << "B";
                } else if(v >= 1000000ULL){
                    oss << std::fixed << std::setprecision(2) << (double(v) / 1000000.0) << "M";
                } else if(v >= 1000ULL){
                    oss << std::fixed << std::setprecision(1) << (double(v) / 1000.0) << "K";
                } else {
                    oss << v;
                }
                return oss.str();
            };

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
                     "TT " + std::to_string(ttSizeMB) + "MB | Thr " +
                     std::to_string(s.workersUsed) + "/" + std::to_string(aiThreads));
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
                oss << "Nodes " << compactCount(s.nodes)
                    << " | Q " << compactCount(s.qnodes)
                    << " (" << std::fixed << std::setprecision(1) << qPct << "%)"
                    << " | NPS " << compactCount(static_cast<u64>(std::max(0.0, nps)));
                statsY += WRAPAT(cardTextX, statsY, cardW - 24.f, oss.str(), 14, sf::Color(180,196,232));
            }
            {
                const double rssMb = double(runtimeResources.rssBytes) / (1024.0 * 1024.0);
                std::ostringstream oss;
                oss << "Res CPU " << std::fixed << std::setprecision(1) << runtimeResources.cpuPercent
                    << "% (~" << std::setprecision(1) << cpuCoresUsed << "c)"
                    << " | RAM " << std::setprecision(1) << rssMb << "MB";
                statsY += WRAPAT(cardTextX, statsY, cardW - 24.f, oss.str(), 14, sf::Color(166,216,236));
            }
            {
                std::ostringstream thr;
                thr << "Threads used " << s.workersUsed
                    << " | set " << aiThreads
                    << " | hw " << s.hardwareThreads;
                statsY += WRAPAT(cardTextX, statsY, cardW - 24.f, thr.str(), 14, sf::Color(164,208,232));
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
