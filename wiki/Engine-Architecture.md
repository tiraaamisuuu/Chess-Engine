# Engine Architecture

## System overview

```text
Web / SFML / UCI / developer tools
                 |
          legal board state
                 |
      iterative alpha-beta search
        /        |          \
       TT   time management  move ordering
                 |
       classical or NNUE evaluation
                 |
          best move and PV
```

The chess core is header-based C++17 shared by the UCI engine, developer tools,
tests, and optional SFML application. `src/uci.cpp` supplies the protocol layer;
`web/server.py` drives any compatible UCI profile rather than duplicating chess
search in JavaScript.

## Board representation

`Board` stores a 64-entry array of compact `Piece` values using `a1 == 0`, plus:

- side to move
- castling rights
- en-passant target square
- halfmove and fullmove counters
- cached white and black king squares
- incremental Zobrist hash
- a pointer to the deterministic Zobrist key set

The representation favours clarity and predictable state transitions. Search
move lists use a bounded 320-entry stack container; the known legal-move maximum
is 218, so recursive search avoids heap allocation without constraining legal
positions.

## Moves and undo state

`Move` records source, destination, promotion and explicit capture,
en-passant/castling/double-pawn flags. `Undo` stores exactly the reversible board
state needed by `undoMove`, including the move, captured piece, rule counters,
king caches and previous hash.

`makeMove` updates pieces, special-move effects, rights, clocks, king locations
and hash incrementally. `undoMove` restores the earlier position without parsing
FEN or recomputing the board. Random legal playout/unmake tests require the
position, counters and hash to return exactly to their starting values.

Null moves use a smaller `NullUndo`. They clear en passant, flip the side,
advance counters and update the relevant Zobrist components without copying the
board or rebuilding the hash.

## Move generation

Pseudo-legal generation emits piece moves, promotions, en passant and castling.
Legal generation makes each candidate and rejects moves that leave the moving
side's king attacked. Cached king squares avoid repeated board scans.

Attack detection is explicit for pawns, knights, kings and sliding rays. The
same legality path is exercised by perft, SAN generation, gameplay, UCI input,
search and randomized invariants.

## Hashing and repetition

Zobrist keys cover pieces, side to move, castling rights and only an en-passant
file that can actually be captured. This makes repetition identity match chess
rules instead of distinguishing irrelevant FEN en-passant fields.

Search combines the actual-game hash history with the current line. Draw
handling includes threefold repetition, the fifty-move rule, stalemate and
insufficient material. Mate scores stored in the TT are normalized by ply and
denormalized on retrieval.

## Transposition table

The shared transposition table uses four-entry clusters. Concurrent root workers
access two atomic 64-bit words per slot: a packed data word and a key checksum.
Readers accept only matching complete snapshots; a collision or racing writer
can cause a safe miss, not a partially mixed hit.

Entries retain score, depth, generation, bound type and best move. Replacement
favours useful depth/age while protecting the search from stale or shallow
entries. UCI `Hash` resizes the table and `Clear Hash` resets it.

## Evaluation boundary

`PositionEvaluator` selects classical evaluation or a loaded NNUE network. The
search calls a common evaluation interface. In NNUE mode it maintains an
accumulator stack aligned with search plies; the reference full-rebuild path is
retained for correctness and performance comparison.

See [Evaluation and NNUE](Evaluation-and-NNUE) for the feature and inference
details.

## Time management

Clock searches calculate soft and hard limits from remaining time, increment,
moves-to-go, legal-move count and a simple position-complexity scale. A
configurable move-overhead reserve protects transport and scheduling time.

Iterative deepening may extend or shorten its soft budget based on root best-move
stability. Hard-limit checks are sampled periodically rather than at every node
to reduce steady-clock overhead. Very short budgets fall back to one worker to
avoid thread coordination costing more than the search.

## Interfaces

- The UCI target is the authoritative tournament and analysis interface.
- The Python local service manages profiles and maps HTTP actions to UCI
  sessions; python-chess owns the displayed game state and export notation.
- The browser renders the board, legal targets, analysis telemetry and setup
  dialogs but does not implement engine search.
- The SFML application remains available as a legacy desktop shell.
- Developer tools call the same core for perft, evaluation and benchmark runs.
