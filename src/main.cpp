#include "ui.hpp"
#include "time_management.hpp"
#include "uci.hpp"

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
        "Forklift | Chess Engine",
        sf::Style::Titlebar | sf::Style::Close,
        ctx
    );
    window.setVerticalSyncEnabled(true);
    window.setFramerateLimit(60);

    const float tile = 96.f;
    const sf::Vector2f boardOrigin(40.f, 72.f);

    // SFML2: don't use FloatRect.position/size – use explicit vectors.
    const sf::Vector2f panelPos(boardOrigin.x + 8.f*tile + 30.f, boardOrigin.y);
    const sf::Vector2f panelSize(440.f, 8.f*tile);
    const float engineCardY = panelPos.y + 194.f;
    const float statusCardY = panelPos.y + 506.f;
    const float moveLogCardY = panelPos.y + 608.f;

    std::filesystem::path executableDir;
    if(argc > 0 && argv[0] && argv[0][0] != '\0'){
        std::error_code pathError;
        const std::filesystem::path executablePath = std::filesystem::absolute(argv[0], pathError);
        if(!pathError) executableDir = executablePath.parent_path();
    }

    // Font: include Linux candidates (and keep your mac ones harmless)
    sf::Font font;
    bool hasFont=false;
    {
        std::vector<std::string> candidates = {
          (executableDir / "assets/fonts/Inter-Regular.ttf").string(),
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
    bool hasIcons = false;
    const std::array<std::filesystem::path, 2> pieceDirectories = {
        std::filesystem::path("assets/pieces_png"),
        executableDir / "assets/pieces_png"
    };
    for(const auto& pieceDirectory : pieceDirectories){
        if(!pieceDirectory.empty() && atlas.loadAll(pieceDirectory.string())){
            hasIcons = true;
            break;
        }
    }

    // UI thread never calls search now; search runs in a worker thread.
    const int ttSizeMB = 256;
    SearchContext aiSearchCtx;
    aiSearchCtx.tt.resizeMB(ttSizeMB);

    std::vector<Undo> undoStack;
    std::vector<std::string> moveListUCI;
    std::vector<std::string> moveListSAN;
    std::vector<u64> positionHistory{board.hash};
    GameStatus gameStatus{};
    aiSearchCtx.gameHistory = positionHistory;

    auto pushMove = [&](const Move& m)->bool{
        const std::string san = moveToSAN(board, m);
        Undo u{};
        if(board.makeMove(m, u)){
            undoStack.push_back(u);
            moveListUCI.push_back(moveToUCI(m));
            moveListSAN.push_back(san);
            positionHistory.push_back(board.hash);
            gameStatus = assessGameStatus(board, positionHistory);
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
        gameStatus = assessGameStatus(board, positionHistory);
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

        const GameStatus finalStatus = assessGameStatus(replay, positionHistory);
        std::string result = "*";
        if(finalStatus.termination == GameTermination::WhiteCheckmated) result = "0-1";
        else if(finalStatus.termination == GameTermination::BlackCheckmated) result = "1-0";
        else if(finalStatus.draw()) result = "1/2-1/2";
        const std::string termination = gameTerminationName(finalStatus.termination);

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

    std::string status = hasIcons ? "ready" : "piece assets unavailable";

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
            sf::FloatRect(72.f, 246.f, 780.f, 80.f),
            sf::FloatRect(72.f, 338.f, 780.f, 80.f),
            sf::FloatRect(72.f, 430.f, 780.f, 80.f),
            sf::FloatRect(72.f, 522.f, 780.f, 80.f)
        };
    };
    auto getMenuStartRect = [&]()->sf::FloatRect{
        return sf::FloatRect(900.f, 530.f, 348.f, 72.f);
    };

    struct GameActionRects {
        sf::FloatRect reset;
        sf::FloatRect undo;
        sf::FloatRect flip;
        sf::FloatRect pause;
    };
    auto getGameActionRects = [&]()->GameActionRects{
        const float x = panelPos.x + 20.f;
        const float y = panelPos.y + 118.f;
        const float w = 88.f;
        const float h = 32.f;
        const float gap = 8.f;
        return GameActionRects{
            sf::FloatRect(x, y, w, h),
            sf::FloatRect(x + (w + gap), y, w, h),
            sf::FloatRect(x + 2.f*(w + gap), y, w, h),
            sf::FloatRect(x + 3.f*(w + gap), y, w, h)
        };
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
        gameStatus = GameStatus{};
        selectedSq.reset();
        selectedMoves.clear();
        lastMove.reset();
        moveListSAN.clear();
        dragging=false;
        dragFrom.reset();
        status = "game reset";
    };
    auto startPendingGame = [&](){
        mode = pending;
        resetGame();

        if(mode == GameMode::PvAI && humanColor == Color::Black){
            flipBoard = true;
        } else if(mode == GameMode::PvAI && humanColor == Color::White){
            flipBoard = false;
        }

        status = "game started  /  " + modeStr(mode);
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
            status = "played  /  " + sqName(indexToSq(chosen.from)) + "-" + sqName(indexToSq(chosen.to));
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
        const float btnW = 30.f;
        const float btnH = 26.f;
        const float plusX  = panelPos.x + panelSize.x - 50.f;
        const float minusX = plusX - 38.f;
        const float depthY = engineCardY + 92.f;
        const float timeY  = engineCardY + 126.f;
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
        status = "depth  /  " + std::to_string(aiMaxDepth);
    };
    auto adjustTime = [&](int deltaMs){
        aiTimeMs = std::clamp(aiTimeMs + deltaMs, 100, 180000);
        status = "move time  /  " + std::to_string(aiTimeMs) + " ms";
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

        if(gameStatus.finished()) return;

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
            const TimeBudget budget = pickGuiTimeBudget(searchBoard, threadTimeMs);
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
            status = "Forklift paused";
        } else {
            status = "Forklift resumed";
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
        status = "move undone";
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
                        status = std::string("board flipped  /  ") + (flipBoard ? "black" : "white");
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
                    const GameActionRects actions = getGameActionRects();
                    if(actions.reset.contains(mp)){
                        stopAiThread();
                        resetGame();
                        continue;
                    }
                    if(actions.undo.contains(mp)){ runUndo(); continue; }
                    if(actions.flip.contains(mp)){
                        flipBoard = !flipBoard;
                        status = std::string("board flipped  /  ") + (flipBoard ? "black" : "white");
                        continue;
                    }
                    if(actions.pause.contains(mp)){ toggleAiPause(); continue; }
                    const EngineStepperRects step = getEngineStepperRects();
                    if(pointInRect(mp, step.depthMinus)){ adjustDepth(-1); continue; }
                    if(pointInRect(mp, step.depthPlus)){  adjustDepth(+1); continue; }
                    if(pointInRect(mp, step.timeMinus)){  adjustTime(-250); continue; }
                    if(pointInRect(mp, step.timePlus)){   adjustTime(+250); continue; }
                }

                // Only allow human input if it's human side AND we aren't mid-AI-search (prevents weirdness in PvAI)
                if(isHumanSide(board.stm) && !aiThinking.load() && !gameStatus.finished()){
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
                                        if(!ok) status = "illegal move";
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
        if(mode!=GameMode::Menu && !isHumanSide(board.stm) && !aiPaused.load() && !gameStatus.finished()){
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
                        status = "Forklift  /  " + moveToUCI(m);
                    } else {
                        status = "unexpected illegal engine move";
                    }

                    aiMoveReady.store(false);
                }
            }
        }

        using namespace ForkliftTheme;
        window.clear(canvas);

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

                auto drawCenteredText = [&](const sf::FloatRect& rect,
                                            unsigned size,
                                            sf::Color col,
                                            const std::string& str){
                    sf::Text t;
                    t.setFont(font);
                    t.setCharacterSize(size);
                    t.setFillColor(col);
                    t.setString(str);
                    const sf::FloatRect bounds = t.getLocalBounds();
                    const float x = rect.left + (rect.width - bounds.width) * 0.5f;
                    const float y = rect.top + (rect.height - font.getLineSpacing(size)) * 0.5f + 1.f;
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
                    if(pending==GameMode::PvP) return "Player vs player";
                    if(pending==GameMode::AIvAI) return "Watch Forklift play";
                    return (humanColor==Color::White) ? "Play as white" : "Play as black";
                };

                drawText(72.f, 34.f, 18, text, "forklift");
                drawText(127.f, 34.f, 18, accent, "/");
                drawText(1142.f, 37.f, 13, textMuted, "v1.0  /  local");

                sf::RectangleShape topRule(sf::Vector2f(1176.f, 1.f));
                topRule.setPosition(snap(sf::Vector2f(72.f, 80.f)));
                topRule.setFillColor(border);
                window.draw(topRule);

                drawText(72.f, 120.f, 43, text, "choose a game");
                drawText(72.f, 178.f, 15, textMuted,
                         "select a mode, then start when you are ready.");

                sf::Vector2i mousePixel = sf::Mouse::getPosition(window);
                sf::Vector2f mousePos(float(mousePixel.x), float(mousePixel.y));
                const auto menuRects = getMenuCardRects();

                auto drawModeCard = [&](size_t index,
                                        int key,
                                        const std::string& title,
                                        const std::string& subtitle){
                    const bool selected = isModeSelected(key);
                    const sf::FloatRect cardRect = menuRects[index];
                    const bool hover = cardRect.contains(mousePos);

                    sf::RectangleShape card(sf::Vector2f(cardRect.width, cardRect.height));
                    card.setPosition(snap(sf::Vector2f(cardRect.left, cardRect.top)));
                    card.setFillColor(selected ? surfaceRaised : (hover ? surface : canvas));
                    card.setOutlineThickness(1.f);
                    card.setOutlineColor(selected ? borderStrong : border);
                    window.draw(card);

                    if(selected){
                        sf::RectangleShape selectionRule(sf::Vector2f(3.f, cardRect.height));
                        selectionRule.setPosition(snap(sf::Vector2f(cardRect.left, cardRect.top)));
                        selectionRule.setFillColor(accent);
                        window.draw(selectionRule);
                    }

                    std::ostringstream keyLabel;
                    keyLabel << '0' << key;
                    drawText(cardRect.left + 20.f, cardRect.top + 27.f, 13,
                             selected ? accent : textMuted, keyLabel.str());
                    drawText(cardRect.left + 72.f, cardRect.top + 14.f, 22, text, title);
                    drawText(cardRect.left + 72.f, cardRect.top + 47.f, 14,
                             selected ? textSoft : textMuted, subtitle);

                    if(selected){
                        sf::CircleShape marker(4.f);
                        marker.setPosition(snap(sf::Vector2f(cardRect.left + cardRect.width - 28.f,
                                                            cardRect.top + 36.f)));
                        marker.setFillColor(accent);
                        window.draw(marker);
                    }
                };

                drawModeCard(0, 1, "player vs player", "two people, one board.");
                drawModeCard(1, 2, "play Forklift as white", "you move first.");
                drawModeCard(2, 3, "play Forklift as black", "Forklift opens.");
                drawModeCard(3, 4, "Forklift vs Forklift", "watch the engine play itself.");

                const sf::FloatRect sideRect(900.f, 246.f, 348.f, 356.f);
                sf::RectangleShape side(sf::Vector2f(sideRect.width, sideRect.height));
                side.setPosition(snap(sf::Vector2f(sideRect.left, sideRect.top)));
                side.setFillColor(surface);
                side.setOutlineThickness(1.f);
                side.setOutlineColor(border);
                window.draw(side);

                drawText(924.f, 270.f, 13, textMuted, "session");
                drawText(924.f, 302.f, 24, text, selectionLabel());

                sf::RectangleShape sideRule(sf::Vector2f(300.f, 1.f));
                sideRule.setPosition(snap(sf::Vector2f(924.f, 354.f)));
                sideRule.setFillColor(border);
                window.draw(sideRule);

                sf::CircleShape readyDot(3.5f);
                readyDot.setPosition(snap(sf::Vector2f(924.f, 386.f)));
                readyDot.setFillColor(hasIcons ? accent : danger);
                window.draw(readyDot);
                drawText(940.f, 379.f, 14, hasIcons ? textSoft : danger,
                         hasIcons ? "ready" : "piece assets unavailable");
                drawText(924.f, 420.f, 13, textMuted, "controls");
                drawText(924.f, 448.f, 14, textSoft, "arrow keys / click to select");
                drawText(924.f, 474.f, 14, textSoft, "enter to start");
                drawText(924.f, 500.f, 14, textSoft, "esc to quit");

                const sf::FloatRect startRect = getMenuStartRect();
                const bool startHover = startRect.contains(mousePos);
                sf::RectangleShape startBtn(sf::Vector2f(startRect.width, startRect.height));
                startBtn.setPosition(snap(sf::Vector2f(startRect.left, startRect.top)));
                startBtn.setFillColor(startHover ? accent : text);
                startBtn.setOutlineThickness(1.f);
                startBtn.setOutlineColor(startHover ? accent : text);
                window.draw(startBtn);
                drawCenteredText(startRect, 18, canvas, "start game");

                sf::RectangleShape bottomRule(sf::Vector2f(1176.f, 1.f));
                bottomRule.setPosition(snap(sf::Vector2f(72.f, 790.f)));
                bottomRule.setFillColor(border);
                window.draw(bottomRule);
                drawText(72.f, 812.f, 13, textMuted,
                         "1-4 select   arrows navigate   enter start");
                drawText(1176.f, 812.f, 13, textMuted, "esc quit");
            }
            window.display();
            continue;
        }

        // -------- Draw board --------
        const float boardPadL = 20.f;
        const float boardPadR = 14.f;
        const float boardPadT = 20.f;
        const float boardPadB = 30.f;

        if(hasFont){
            sf::Text brand;
            brand.setFont(font);
            brand.setCharacterSize(17);
            brand.setFillColor(text);
            brand.setString("forklift/");
            setCrispTextPosition(brand, sf::Vector2f(20.f, 20.f));
            window.draw(brand);

            sf::Text context;
            context.setFont(font);
            context.setCharacterSize(13);
            context.setFillColor(textMuted);
            context.setString("local game  /  esc quit");
            setCrispTextPosition(context, sf::Vector2f(1122.f, 23.f));
            window.draw(context);
        }

        sf::RectangleShape boardShell(sf::Vector2f(8.f*tile + boardPadL + boardPadR, 8.f*tile + boardPadT + boardPadB));
        boardShell.setPosition(snap(sf::Vector2f(boardOrigin.x - boardPadL, boardOrigin.y - boardPadT)));
        boardShell.setFillColor(surface);
        boardShell.setOutlineThickness(1.f);
        boardShell.setOutlineColor(border);
        window.draw(boardShell);

        for(int r=0;r<8;r++){
            for(int f=0;f<8;f++){
                Square s{f,r};
                int idx = sqToIndex(s);

                sf::RectangleShape rect(sf::Vector2f(tile,tile));
                rect.setPosition(snap(squareToPixel(s, tile, boardOrigin, flipBoard)));

                bool dark = ((f+r)%2)==1;
                sf::Color base = dark ? boardDark : boardLight;

                if(lastMove && idx==lastMove->from){
                    base = dark ? lastMoveDark : lastMoveLight;
                }
                if(lastMove && idx==lastMove->to){
                    base = dark ? selectedDark : selectedLight;
                }
                if(selectedSq && idx==*selectedSq){
                    base = dark ? selectedDark : selectedLight;
                }

                rect.setFillColor(base);
                window.draw(rect);
            }
        }

        for(const auto& m : selectedMoves){
            const sf::Vector2f target = squareToPixel(indexToSq(m.to), tile, boardOrigin, flipBoard);
            if(isNone(board.at(m.to))){
                sf::CircleShape dot(8.f);
                dot.setPosition(snap(sf::Vector2f(target.x + tile*0.5f - 8.f,
                                                  target.y + tile*0.5f - 8.f)));
                dot.setFillColor(sf::Color(accent.r, accent.g, accent.b, 180));
                window.draw(dot);
            } else {
                sf::CircleShape ring(tile*0.5f - 9.f);
                ring.setPosition(snap(sf::Vector2f(target.x + 9.f, target.y + 9.f)));
                ring.setFillColor(sf::Color::Transparent);
                ring.setOutlineThickness(3.f);
                ring.setOutlineColor(sf::Color(accent.r, accent.g, accent.b, 190));
                window.draw(ring);
            }
        }

        for(Color c : {Color::White, Color::Black}){
            if(board.inCheck(c)){
                int k = board.findKing(c);
                if(k>=0){
                    sf::RectangleShape red(sf::Vector2f(tile,tile));
                    red.setPosition(snap(squareToPixel(indexToSq(k), tile, boardOrigin, flipBoard)));
                    red.setFillColor(sf::Color(danger.r, danger.g, danger.b, 112));
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
                t.setCharacterSize(12);
                t.setFillColor(textMuted);
                t.setString(std::string(1, char('a'+f)));
                setCrispTextPosition(t, sf::Vector2f(boardOrigin.x + (flipBoard?(7-f):f)*tile + 6.f, fileLabelY));
                window.draw(t);
            }
            for(int r=0; r<8; r++){
                sf::Text t;
                t.setFont(font);
                t.setCharacterSize(12);
                t.setFillColor(textMuted);
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
        panelBg.setFillColor(surface);
        panelBg.setOutlineThickness(1.f);
        panelBg.setOutlineColor(border);
        window.draw(panelBg);

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

            auto WRAPAT = [&](float x, float y, float w, const std::string& txt, int size=14, sf::Color col=textSoft){
                return drawWrappedText(window, font, txt, (unsigned)size, sf::Vector2f(x, y), w, col);
            };

            auto drawDivider = [&](float y){
                sf::RectangleShape rule(sf::Vector2f(panelSize.x - 40.f, 1.f));
                rule.setPosition(snap(sf::Vector2f(panelPos.x + 20.f, y)));
                rule.setFillColor(border);
                window.draw(rule);
            };

            std::vector<Move> legalMoves;
            board.genLegalMoves(legalMoves);

            std::string stateLabel = "normal";
            sf::Color stateColor = textMuted;
            if(legalMoves.empty()){
                if(board.inCheck(board.stm)){
                    stateLabel = "checkmate";
                    stateColor = danger;
                } else {
                    stateLabel = "stalemate";
                    stateColor = warning;
                }
            } else if(board.inCheck(board.stm)){
                stateLabel = "check";
                stateColor = warning;
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

            const float cardTextX = panelPos.x + 20.f;
            const float cardW = panelSize.x - 40.f;
            const float moveLogCardH = panelSize.y - (moveLogCardY - panelPos.y);

            std::string modeLabel = "player vs player";
            if(mode == GameMode::PvAI){
                modeLabel = (humanColor == Color::White)
                    ? "player vs Forklift  /  white"
                    : "player vs Forklift  /  black";
            } else if(mode == GameMode::AIvAI){
                modeLabel = "Forklift vs Forklift";
            }

            drawText(cardTextX, panelPos.y + 18.f, 13, textMuted, "game");
            drawText(cardTextX, panelPos.y + 47.f, 30, text,
                     std::string(board.stm==Color::White ? "white" : "black") + " to move");
            drawText(cardTextX, panelPos.y + 87.f, 14, stateColor,
                     modeLabel + "  /  " + stateLabel);

            drawDivider(panelPos.y + 174.f);
            drawText(cardTextX, engineCardY, 13, textMuted, "engine");
            const TimeBudget liveBudget = pickGuiTimeBudget(board, aiTimeMs);

            std::ostringstream evalText;
            if(std::abs(s.bestScore) > MATE/2){
                int matePly = std::max(1, MATE - std::abs(s.bestScore));
                int mateMoves = std::max(1, (matePly + 1) / 2);
                evalText << (s.bestScore >= 0 ? "M" : "-M") << mateMoves;
            } else {
                evalText << std::showpos << std::fixed << std::setprecision(2) << pawns;
            }

            std::string engineState = "idle";
            sf::Color engineStateColor = textMuted;
            if(aiThinking.load()){
                const int elapsedMs = thinkClock.getElapsedTime().asMilliseconds();
                const int remainingMs = std::max(0, liveBudget.softMs - elapsedMs);
                std::ostringstream remaining;
                remaining << "thinking  " << std::fixed << std::setprecision(1)
                          << (double(remainingMs) / 1000.0) << "s";
                engineState = remaining.str();
                engineStateColor = warning;
            } else if(aiPaused.load()){
                engineState = "paused";
                engineStateColor = accent;
            }

            drawText(cardTextX, engineCardY + 27.f, 31, text, evalText.str());
            sf::CircleShape engineDot(3.5f);
            engineDot.setPosition(snap(sf::Vector2f(panelPos.x + panelSize.x - 145.f,
                                                    engineCardY + 39.f)));
            engineDot.setFillColor(engineStateColor);
            window.draw(engineDot);
            drawText(panelPos.x + panelSize.x - 129.f, engineCardY + 31.f, 13,
                     engineStateColor, engineState);
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
                const float barY = engineCardY + 72.f;
                const float barW = cardW;
                const float barH = 4.f;

                sf::RectangleShape barBg(sf::Vector2f(barW, barH));
                barBg.setPosition(snap(sf::Vector2f(barX, barY)));
                barBg.setFillColor(border);
                window.draw(barBg);

                sf::RectangleShape barFill(sf::Vector2f(std::max(1.f, barW * evalNorm), barH));
                barFill.setPosition(snap(sf::Vector2f(barX, barY)));
                barFill.setFillColor(accent);
                window.draw(barFill);

                sf::RectangleShape mid(sf::Vector2f(1.f, barH + 4.f));
                mid.setPosition(snap(sf::Vector2f(barX + barW * 0.5f, barY - 2.f)));
                mid.setFillColor(textSoft);
                window.draw(mid);
            }

            const EngineStepperRects step = getEngineStepperRects();
            sf::Vector2i mousePixel = sf::Mouse::getPosition(window);
            sf::Vector2f mousePos(float(mousePixel.x), float(mousePixel.y));
            auto drawStepperButton = [&](const sf::FloatRect& r, const std::string& label){
                const bool hover = pointInRect(mousePos, r);
                sf::RectangleShape btn(sf::Vector2f(r.width, r.height));
                btn.setPosition(snap(sf::Vector2f(r.left, r.top)));
                btn.setFillColor(hover ? surfaceRaised : surface);
                btn.setOutlineThickness(1.f);
                btn.setOutlineColor(hover ? borderStrong : border);
                window.draw(btn);
                drawText(r.left + 9.f, r.top + 3.f, 17, hover ? text : textSoft, label);
            };

            auto drawActionButton = [&](const sf::FloatRect& r,
                                        const std::string& label,
                                        bool active = false){
                const bool hover = pointInRect(mousePos, r);
                sf::RectangleShape btn(sf::Vector2f(r.width, r.height));
                btn.setPosition(snap(sf::Vector2f(r.left, r.top)));
                btn.setFillColor(hover ? surfaceRaised : surface);
                btn.setOutlineThickness(1.f);
                btn.setOutlineColor(active ? accent : (hover ? borderStrong : border));
                window.draw(btn);

                sf::Text labelText;
                labelText.setFont(font);
                labelText.setCharacterSize(13);
                labelText.setFillColor(active ? accent : (hover ? text : textSoft));
                labelText.setString(label);
                const sf::FloatRect labelBounds = labelText.getLocalBounds();
                setCrispTextPosition(labelText, sf::Vector2f(
                    r.left + (r.width - labelBounds.width) * 0.5f,
                    r.top + 8.f));
                window.draw(labelText);
            };

            const GameActionRects actions = getGameActionRects();
            drawActionButton(actions.reset, "reset");
            drawActionButton(actions.undo, "undo");
            drawActionButton(actions.flip, "flip");
            drawActionButton(actions.pause, aiPaused.load() ? "resume" : "pause", aiPaused.load());

            {
                std::ostringstream timeLabel;
                timeLabel << std::fixed << std::setprecision(1)
                          << (double(aiTimeMs) / 1000.0) << " s";
                drawText(cardTextX, engineCardY + 96.f, 14, textMuted, "depth");
                drawText(cardTextX + 82.f, engineCardY + 96.f, 14, text,
                         std::to_string(aiMaxDepth));
                drawText(cardTextX, engineCardY + 130.f, 14, textMuted, "move time");
                drawText(cardTextX + 82.f, engineCardY + 130.f, 14, text,
                         timeLabel.str());
                drawStepperButton(step.depthMinus, "-");
                drawStepperButton(step.depthPlus, "+");
                drawStepperButton(step.timeMinus, "-");
                drawStepperButton(step.timePlus, "+");
                drawText(cardTextX, engineCardY + 162.f, 12, textMuted,
                         "book on  /  tt " + std::to_string(ttSizeMB) + " mb  /  threads " +
                         std::to_string(aiThreads));
            }

            float statsY = engineCardY + 190.f;
            drawText(cardTextX, statsY, 12, textMuted, "search");
            statsY += 23.f;
            {
                std::ostringstream oss;
                oss << "depth " << s.depthReached
                    << "  /  score " << std::showpos << std::fixed << std::setprecision(2) << pawns
                    << "  /  " << s.timeMs << " ms";
                statsY += WRAPAT(cardTextX, statsY, cardW, oss.str(), 13, textSoft);
            }
            {
                std::ostringstream oss;
                oss << compactCount(s.nodes) << " nodes"
                    << "  /  q " << compactCount(s.qnodes)
                    << " (" << std::fixed << std::setprecision(1) << qPct << "%)"
                    << "  /  " << compactCount(static_cast<u64>(std::max(0.0, nps))) << " nps";
                statsY += WRAPAT(cardTextX, statsY, cardW, oss.str(), 13, textSoft);
            }
            {
                const double rssMb = double(runtimeResources.rssBytes) / (1024.0 * 1024.0);
                std::ostringstream oss;
                oss << "cpu " << std::fixed << std::setprecision(1) << runtimeResources.cpuPercent
                    << "% (~" << std::setprecision(1) << cpuCoresUsed << "c)"
                    << "  /  ram " << std::setprecision(1) << rssMb << " mb"
                    << "  /  workers " << s.workersUsed;
                statsY += WRAPAT(cardTextX, statsY, cardW, oss.str(), 13, textMuted);
            }
            if(!pv.empty()){
                std::string pvCompact = "pv  " + pv;
                if(pvCompact.size() > 54) pvCompact = pvCompact.substr(0, 51) + "...";
                drawText(cardTextX, statsY, 13, accent, pvCompact);
            }

            drawDivider(statusCardY);
            drawText(cardTextX, statusCardY + 18.f, 12, textMuted, "activity");
            float statusY = statusCardY + 43.f;
            statusY += WRAPAT(cardTextX, statusY, cardW, status, 13, textSoft);
            if(selectedSq){
                if(statusY + font.getLineSpacing(13) <= moveLogCardY - 10.f){
                    drawText(cardTextX, statusY, 13, textMuted,
                             "selected " + sqName(indexToSq(*selectedSq)) + "  /  " +
                             std::to_string((int)selectedMoves.size()) + " legal moves");
                    statusY += font.getLineSpacing(13);
                }
            }
            if(gameStatus.finished()){
                if(statusY + font.getLineSpacing(13) <= moveLogCardY - 10.f){
                    drawText(cardTextX, statusY, 13, warning,
                             std::string("game over  /  ") + gameTerminationName(gameStatus.termination));
                }
            }

            drawDivider(moveLogCardY);
            drawText(cardTextX, moveLogCardY + 18.f, 12, textMuted, "moves");
            float listY = moveLogCardY + 46.f;
            if(moveListSAN.empty()){
                drawText(cardTextX, listY, 13, textMuted, "no moves yet");
            }
            int start = std::max(0, (int)moveListSAN.size()-10);
            start -= start % 2;
            for(int i=start; i<(int)moveListSAN.size(); i+=2){
                std::string line = std::to_string(i/2 + 1) + ".  " + moveListSAN[i];
                if(i + 1 < (int)moveListSAN.size()) line += "    " + moveListSAN[i + 1];
                drawText(cardTextX, listY, 13, textSoft, line);
                listY += 22.f;
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
