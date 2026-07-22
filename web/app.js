"use strict";

const elements = {
  board: document.querySelector("#board"),
  connection: document.querySelector("#connection"),
  connectionLabel: document.querySelector("#connection-label"),
  thinking: document.querySelector("#thinking-overlay"),
  status: document.querySelector("#position-status"),
  statusPulse: document.querySelector("#status-pulse"),
  moves: document.querySelector("#moves-list"),
  engineName: document.querySelector("#engine-name"),
  engineState: document.querySelector("#engine-state"),
  evaluationFill: document.querySelector("#evaluation-fill"),
  evaluationScore: document.querySelector("#evaluation-score"),
  analysisScore: document.querySelector("#analysis-score"),
  analysisDepth: document.querySelector("#analysis-depth"),
  analysisPv: document.querySelector("#analysis-pv"),
  metricDepth: document.querySelector("#metric-depth"),
  metricNodes: document.querySelector("#metric-nodes"),
  metricNps: document.querySelector("#metric-nps"),
  metricTime: document.querySelector("#metric-time"),
  topName: document.querySelector("#top-name"),
  topDetail: document.querySelector("#top-detail"),
  topMonogram: document.querySelector("#top-monogram"),
  topTurn: document.querySelector("#top-turn"),
  bottomName: document.querySelector("#bottom-name"),
  bottomDetail: document.querySelector("#bottom-detail"),
  bottomMonogram: document.querySelector("#bottom-monogram"),
  bottomTurn: document.querySelector("#bottom-turn"),
  newGameModal: document.querySelector("#new-game-modal"),
  newGameForm: document.querySelector("#new-game-form"),
  strengthField: document.querySelector("#strength-field"),
  promotionModal: document.querySelector("#promotion-modal"),
  promotionOptions: document.querySelector("#promotion-options"),
  toast: document.querySelector("#toast"),
};

let state = null;
let orientation = "white";
let selectedSquare = null;
let draggedSquare = null;
let busy = false;
let pendingPromotion = [];
let analysisSequence = 0;
let toastTimer = null;

const pieceLetters = {
  pawn: "P", knight: "N", bishop: "B", rook: "R", queen: "Q", king: "K",
};

async function request(path, options = {}) {
  const response = await fetch(path, {
    headers: { "Content-Type": "application/json" },
    ...options,
  });
  let payload;
  try {
    payload = await response.json();
  } catch {
    throw new Error(`The local service returned ${response.status}`);
  }
  if (!response.ok) throw new Error(payload.error || `Request failed (${response.status})`);
  return payload;
}

function showToast(message) {
  elements.toast.textContent = message;
  elements.toast.classList.add("visible");
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => elements.toast.classList.remove("visible"), 2400);
}

function formatNumber(value) {
  if (value === null || value === undefined) return "—";
  if (value >= 1_000_000) return `${(value / 1_000_000).toFixed(1)}m`;
  if (value >= 1_000) return `${(value / 1_000).toFixed(1)}k`;
  return String(value);
}

function formatScore(information) {
  if (!information) return "0.00";
  if (information.mate !== null && information.mate !== undefined) {
    return `${information.mate >= 0 ? "+" : "−"}M${Math.abs(information.mate)}`;
  }
  if (information.scoreCp === null || information.scoreCp === undefined) return "0.00";
  const value = information.scoreCp / 100;
  return `${value >= 0 ? "+" : ""}${value.toFixed(2)}`;
}

function boardOrder() {
  const squares = [];
  for (let row = 0; row < 8; row += 1) {
    for (let column = 0; column < 8; column += 1) {
      const fileIndex = orientation === "white" ? column : 7 - column;
      const rank = orientation === "white" ? 8 - row : row + 1;
      squares.push(`${String.fromCharCode(97 + fileIndex)}${rank}`);
    }
  }
  return squares;
}

function legalFrom(square) {
  return state?.legalMoves?.filter((move) => move.from === square) || [];
}

function renderBoard() {
  if (!state) return;
  const pieces = new Map(state.pieces.map((piece) => [piece.square, piece]));
  const legalTargets = new Map(legalFrom(selectedSquare).map((move) => [move.to, move]));
  const last = state.lastMove ? [state.lastMove.slice(0, 2), state.lastMove.slice(2, 4)] : [];
  elements.board.replaceChildren();

  boardOrder().forEach((squareName, index) => {
    const file = squareName.charCodeAt(0) - 97;
    const rank = Number(squareName[1]);
    const square = document.createElement("button");
    square.type = "button";
    square.className = `square ${(file + rank) % 2 ? "dark" : "light"}`;
    square.dataset.square = squareName;
    square.setAttribute("role", "gridcell");
    square.setAttribute("aria-label", squareName);
    if (last.includes(squareName)) square.classList.add("last");
    if (squareName === selectedSquare) square.classList.add("selected");
    if (squareName === state.checkSquare) square.classList.add("check");
    if (legalTargets.has(squareName)) {
      square.classList.add("legal");
      if (legalTargets.get(squareName).capture) square.classList.add("capture");
    }

    const piece = pieces.get(squareName);
    if (piece) {
      const image = document.createElement("img");
      image.className = "piece";
      image.src = `/assets/pieces/${piece.color}_${piece.type}.svg`;
      image.alt = `${piece.color} ${piece.type}`;
      image.draggable = state.canMove && piece.color === state.turn;
      image.addEventListener("dragstart", (event) => {
        draggedSquare = squareName;
        event.dataTransfer.effectAllowed = "move";
        event.dataTransfer.setData("text/plain", squareName);
      });
      image.addEventListener("dragend", () => { draggedSquare = null; });
      square.append(image);
    }

    const row = Math.floor(index / 8);
    const column = index % 8;
    if (column === 0) {
      const rankLabel = document.createElement("span");
      rankLabel.className = "coordinate rank";
      rankLabel.textContent = squareName[1];
      square.append(rankLabel);
    }
    if (row === 7) {
      const fileLabel = document.createElement("span");
      fileLabel.className = "coordinate file";
      fileLabel.textContent = squareName[0];
      square.append(fileLabel);
    }
    square.addEventListener("click", () => handleSquare(squareName));
    square.addEventListener("dragover", (event) => event.preventDefault());
    square.addEventListener("drop", (event) => {
      event.preventDefault();
      const from = draggedSquare || event.dataTransfer.getData("text/plain");
      attemptMove(from, squareName);
      draggedSquare = null;
    });
    elements.board.append(square);
  });
}

function handleSquare(squareName) {
  if (busy || !state?.canMove) return;
  if (selectedSquare) {
    const candidates = legalFrom(selectedSquare).filter((move) => move.to === squareName);
    if (candidates.length) {
      chooseMove(candidates);
      return;
    }
  }
  selectedSquare = legalFrom(squareName).length ? squareName : null;
  renderBoard();
}

function attemptMove(from, to) {
  if (busy || !state?.canMove) return;
  const candidates = legalFrom(from).filter((move) => move.to === to);
  if (candidates.length) chooseMove(candidates);
}

function chooseMove(candidates) {
  const promotionMoves = candidates.filter((move) => move.promotion);
  if (promotionMoves.length > 1) {
    pendingPromotion = promotionMoves;
    elements.promotionOptions.replaceChildren();
    for (const move of promotionMoves) {
      const button = document.createElement("button");
      button.type = "button";
      button.setAttribute("aria-label", `Promote to ${move.promotion}`);
      const image = document.createElement("img");
      image.src = `/assets/pieces/${state.turn}_${move.promotion}.svg`;
      image.alt = move.promotion;
      button.append(image);
      button.addEventListener("click", () => {
        elements.promotionModal.close();
        submitMove(move.uci);
      });
      elements.promotionOptions.append(button);
    }
    elements.promotionModal.showModal();
    return;
  }
  submitMove(candidates[0].uci);
}

async function submitMove(uci) {
  selectedSquare = null;
  setBusy(true, "Applying your move");
  try {
    state = await request("/api/move", { method: "POST", body: JSON.stringify({ uci }) });
    render();
    if (state.needsEngineMove) await askEngineToMove();
    else scheduleAnalysis();
  } catch (error) {
    showToast(error.message);
  } finally {
    setBusy(false);
  }
}

async function askEngineToMove() {
  setBusy(true, "Searching the position");
  try {
    state = await request("/api/engine-move", { method: "POST", body: "{}" });
    render();
    scheduleAnalysis();
  } catch (error) {
    showToast(error.message);
  } finally {
    setBusy(false);
  }
}

function scheduleAnalysis() {
  if (!state || state.gameOver || !state.engine.connected) return;
  const sequence = ++analysisSequence;
  window.setTimeout(async () => {
    try {
      const analysis = await request("/api/analyse", { method: "POST", body: "{}" });
      if (sequence !== analysisSequence || !state) return;
      state.engine.analysis = analysis;
      renderAnalysis();
    } catch (error) {
      console.warn(error);
    }
  }, 80);
}

function setBusy(value, detail = "Searching the position") {
  busy = value;
  elements.thinking.classList.toggle("visible", value);
  elements.statusPulse.classList.toggle("thinking", value);
  document.querySelector("#thinking-detail").textContent = detail;
  if (state) {
    elements.status.textContent = state.gameOver
      ? `${state.result} · ${state.resultReason}`
      : value ? "Engine thinking" : state.status;
  }
}

function renderPlayers() {
  const engineColor = state.mode === "local" ? null : state.playerColor === "white" ? "black" : "white";
  const topColor = orientation === "white" ? "black" : "white";
  const topIsEngine = engineColor === topColor;
  const topName = state.mode === "local" ? `${topColor[0].toUpperCase()}${topColor.slice(1)}` : topIsEngine ? state.engine.name : "You";
  const bottomColor = topColor === "white" ? "black" : "white";
  const bottomIsEngine = engineColor === bottomColor;
  const bottomName = state.mode === "local" ? `${bottomColor[0].toUpperCase()}${bottomColor.slice(1)}` : bottomIsEngine ? state.engine.name : "You";

  elements.topName.textContent = topName;
  elements.topDetail.textContent = topIsEngine ? `Engine · ${state.engine.moveTimeMs} ms` : `${topColor[0].toUpperCase()}${topColor.slice(1)} pieces`;
  elements.topMonogram.textContent = topIsEngine ? "TC" : topColor === "white" ? "W" : "B";
  elements.bottomName.textContent = bottomName;
  elements.bottomDetail.textContent = bottomIsEngine ? `Engine · ${state.engine.moveTimeMs} ms` : `${bottomColor[0].toUpperCase()}${bottomColor.slice(1)} pieces`;
  elements.bottomMonogram.textContent = bottomIsEngine ? "TC" : bottomColor === "white" ? "W" : "B";

  const topActive = !state.gameOver && state.turn === topColor;
  const bottomActive = !state.gameOver && state.turn === bottomColor;
  elements.topTurn.textContent = topActive ? "TO MOVE" : "WAITING";
  elements.bottomTurn.textContent = bottomActive ? "TO MOVE" : "WAITING";
  elements.topTurn.classList.toggle("active", topActive);
  elements.bottomTurn.classList.toggle("active", bottomActive);
}

function renderMoves() {
  elements.moves.replaceChildren();
  if (!state.moves.length) {
    elements.moves.innerHTML = '<div class="empty-state"><span>01</span><p>Your game will appear here.</p></div>';
    return;
  }
  state.moves.forEach((row, index) => {
    const element = document.createElement("div");
    element.className = `move-row${index === state.moves.length - 1 ? " latest" : ""}`;
    element.innerHTML = `<span class="number">${String(row.number).padStart(2, "0")}</span><span class="move"></span><span class="move"></span>`;
    element.children[1].textContent = row.white;
    element.children[2].textContent = row.black;
    elements.moves.append(element);
  });
  elements.moves.scrollTop = elements.moves.scrollHeight;
}

function renderAnalysis() {
  if (!state) return;
  const analysis = state.engine.analysis || state.engine.lastMove || {};
  const score = formatScore(analysis);
  elements.analysisScore.textContent = score;
  elements.evaluationScore.textContent = score;
  const cp = Math.max(-1200, Math.min(1200, analysis.scoreCp || 0));
  const whitePercent = 50 + 48 * Math.tanh(cp / 420);
  elements.evaluationFill.style.height = `${whitePercent}%`;
  elements.analysisDepth.textContent = analysis.depth ? `Depth ${analysis.depth} · White perspective` : "Waiting for a position";
  elements.analysisPv.textContent = analysis.pv?.length ? analysis.pv.join("  ") : "Analysis begins after the first move.";
  elements.metricDepth.textContent = analysis.depth ?? "—";
  elements.metricNodes.textContent = formatNumber(analysis.nodes);
  elements.metricNps.textContent = formatNumber(analysis.nps);
  elements.metricTime.textContent = analysis.timeMs ? `${analysis.timeMs} ms` : "—";
}

function render() {
  if (!state) return;
  elements.connection.classList.toggle("offline", !state.engine.connected);
  elements.connectionLabel.textContent = state.engine.connected ? "Engine online" : "Local mode only";
  elements.engineName.textContent = state.engine.name;
  elements.engineState.textContent = state.engine.connected ? "UCI" : "OFFLINE";
  elements.status.textContent = state.gameOver ? `${state.result} · ${state.resultReason}` : busy ? "Engine thinking" : state.status;
  elements.statusPulse.style.background = state.gameOver ? "var(--muted)" : "var(--accent)";
  renderPlayers();
  renderBoard();
  renderMoves();
  renderAnalysis();
}

async function newGameFromForm() {
  const form = new FormData(elements.newGameForm);
  const mode = form.get("mode");
  const side = form.get("side");
  const engineTimeMs = Number(form.get("engine-time"));
  setBusy(true, "Preparing a new board");
  analysisSequence += 1;
  try {
    state = await request("/api/new", {
      method: "POST",
      body: JSON.stringify({ mode, side, engineTimeMs, analysisTimeMs: 180 }),
    });
    orientation = state.playerColor || "white";
    selectedSquare = null;
    render();
    if (state.needsEngineMove) await askEngineToMove();
    else scheduleAnalysis();
  } catch (error) {
    showToast(error.message);
  } finally {
    setBusy(false);
  }
}

for (const button of document.querySelectorAll("#new-game, #new-game-top")) {
  button.addEventListener("click", () => elements.newGameModal.showModal());
}

elements.newGameForm.addEventListener("submit", (event) => {
  event.preventDefault();
  elements.newGameModal.close();
  newGameFromForm();
});

elements.newGameForm.addEventListener("change", () => {
  const mode = new FormData(elements.newGameForm).get("mode");
  elements.strengthField.hidden = mode === "local";
});

document.querySelector("#undo").addEventListener("click", async () => {
  if (busy) return;
  analysisSequence += 1;
  try {
    state = await request("/api/undo", { method: "POST", body: "{}" });
    selectedSquare = null;
    render();
    scheduleAnalysis();
  } catch (error) {
    showToast(error.message);
  }
});

document.querySelector("#flip").addEventListener("click", () => {
  orientation = orientation === "white" ? "black" : "white";
  selectedSquare = null;
  render();
});

document.querySelector("#copy-fen").addEventListener("click", async () => {
  if (!state) return;
  try {
    await navigator.clipboard.writeText(state.fen);
    showToast("FEN copied");
  } catch {
    showToast("Could not access the clipboard");
  }
});

for (const tab of document.querySelectorAll(".tab")) {
  tab.addEventListener("click", () => {
    document.querySelectorAll(".tab").forEach((item) => {
      const active = item === tab;
      item.classList.toggle("active", active);
      item.setAttribute("aria-selected", String(active));
    });
    document.querySelectorAll(".panel-view").forEach((view) => view.classList.remove("active"));
    document.querySelector(`#${tab.dataset.tab}-view`).classList.add("active");
  });
}

async function initialise() {
  try {
    state = await request("/api/state");
    orientation = state.playerColor || "white";
    render();
    if (state.needsEngineMove) await askEngineToMove();
    else scheduleAnalysis();
  } catch (error) {
    elements.connection.classList.add("offline");
    elements.connectionLabel.textContent = "Service unavailable";
    showToast(error.message);
  }
}

initialise();
