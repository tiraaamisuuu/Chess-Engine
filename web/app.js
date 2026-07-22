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
  runAnalysis: document.querySelector("#run-analysis"),
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
  pvcOptions: document.querySelector("#pvc-options"),
  cvcOptions: document.querySelector("#cvc-options"),
  strengthField: document.querySelector("#strength-field"),
  modeHelp: document.querySelector("#mode-help"),
  pvcProfile: document.querySelector("#pvc-profile"),
  whiteProfile: document.querySelector("#white-profile"),
  blackProfile: document.querySelector("#black-profile"),
  pvcProfileSummary: document.querySelector("#pvc-profile-summary"),
  whiteProfileSummary: document.querySelector("#white-profile-summary"),
  blackProfileSummary: document.querySelector("#black-profile-summary"),
  autoPlay: document.querySelector("#auto-play"),
  promotionModal: document.querySelector("#promotion-modal"),
  promotionOptions: document.querySelector("#promotion-options"),
  exportModal: document.querySelector("#export-modal"),
  exportForm: document.querySelector("#export-form"),
  exportMetadata: document.querySelector("#export-metadata"),
  exportHelp: document.querySelector("#export-help"),
  exportPreview: document.querySelector("#export-preview"),
  exportWhite: document.querySelector("#export-white"),
  exportBlack: document.querySelector("#export-black"),
  downloadExport: document.querySelector("#download-export"),
  toast: document.querySelector("#toast"),
};

let state = null;
let orientation = "white";
let orientationPreference = "auto";
let selectedSquare = null;
let dragState = null;
let suppressClickUntil = 0;
let busy = false;
let pendingPromotion = [];
let analysisSequence = 0;
let engineLoopToken = 0;
let engineTimer = null;
let autoPlay = true;
let profileOptionsKey = "";
let toastTimer = null;
let analysisBusy = false;
let analysisError = "";
let exportPayload = null;
let exportSequence = 0;

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
      image.draggable = false;
      const movable = state.canMove && piece.color === state.turn && legalFrom(squareName).length > 0;
      image.classList.toggle("movable", movable);
      if (movable) {
        image.addEventListener("pointerdown", (event) => beginPieceDrag(event, squareName, image));
      }
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
    elements.board.append(square);
  });
}

function beginPieceDrag(event, squareName, image) {
  if (busy || !state?.canMove || (event.pointerType === "mouse" && event.button !== 0)) return;
  event.preventDefault();
  selectedSquare = squareName;
  const bounds = image.getBoundingClientRect();
  renderBoard();
  const source = elements.board.querySelector(`[data-square="${squareName}"] .piece`);
  if (source) source.classList.add("drag-source");

  const floating = image.cloneNode(true);
  floating.className = "dragging-piece";
  floating.style.width = `${bounds.width}px`;
  floating.style.height = `${bounds.height}px`;
  document.body.append(floating);
  dragState = {
    from: squareName,
    floating,
    pointerId: event.pointerId,
    startX: event.clientX,
    startY: event.clientY,
  };
  moveFloatingPiece(event.clientX, event.clientY);
}

function moveFloatingPiece(clientX, clientY) {
  if (!dragState) return;
  dragState.floating.style.left = `${clientX}px`;
  dragState.floating.style.top = `${clientY}px`;
}

function finishPieceDrag(event, cancelled = false) {
  if (!dragState || event.pointerId !== dragState.pointerId) return;
  event.preventDefault();
  const currentDrag = dragState;
  const target = cancelled ? null : document.elementFromPoint(event.clientX, event.clientY)?.closest(".square");
  const targetSquare = target?.dataset.square;
  const travelled = Math.hypot(
    event.clientX - currentDrag.startX,
    event.clientY - currentDrag.startY,
  );
  currentDrag.floating.remove();
  dragState = null;
  suppressClickUntil = performance.now() + 300;

  const candidates = legalFrom(currentDrag.from).filter((move) => move.to === targetSquare);
  if (!cancelled && travelled > 3 && candidates.length) {
    chooseMove(candidates);
  } else {
    renderBoard();
  }
}

document.addEventListener("pointermove", (event) => {
  if (!dragState || event.pointerId !== dragState.pointerId) return;
  event.preventDefault();
  moveFloatingPiece(event.clientX, event.clientY);
}, { passive: false });

document.addEventListener("pointerup", (event) => finishPieceDrag(event));
document.addEventListener("pointercancel", (event) => finishPieceDrag(event, true));

function handleSquare(squareName) {
  if (performance.now() < suppressClickUntil || busy || !state?.canMove) return;
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

function cancelScheduledEnginePlay() {
  engineLoopToken += 1;
  clearTimeout(engineTimer);
  engineTimer = null;
}

async function submitMove(uci) {
  selectedSquare = null;
  cancelScheduledEnginePlay();
  analysisSequence += 1;
  analysisError = "";
  setBusy(true, "Applying your move");
  try {
    state = await request("/api/move", { method: "POST", body: JSON.stringify({ uci }) });
    render();
    maybeContinueGame();
  } catch (error) {
    showToast(error.message);
  } finally {
    setBusy(false);
  }
}

function maybeContinueGame(delay = 0) {
  if (!state || state.gameOver || !state.needsEngineMove) {
    scheduleAnalysis();
    return;
  }
  if (state.mode === "cvc" && !autoPlay) {
    scheduleAnalysis();
    return;
  }
  const token = engineLoopToken;
  clearTimeout(engineTimer);
  engineTimer = window.setTimeout(() => performEngineMove(token), delay);
}

async function performEngineMove(token) {
  if (token !== engineLoopToken || !state?.needsEngineMove) return;
  const controller = state.controllers[state.turn];
  setBusy(true, `${controller.name} is calculating`);
  try {
    const nextState = await request("/api/engine-move", { method: "POST", body: "{}" });
    if (token !== engineLoopToken) return;
    state = nextState;
    render();
  } catch (error) {
    if (token !== engineLoopToken) return;
    autoPlay = false;
    showToast(error.message);
    renderAutoPlay();
  } finally {
    if (token === engineLoopToken) setBusy(false);
  }
  if (token !== engineLoopToken) return;
  if (state.mode === "cvc" && autoPlay && state.needsEngineMove) {
    maybeContinueGame(180);
  } else {
    scheduleAnalysis();
  }
}

function scheduleAnalysis() {
  if (!state || state.gameOver || !state.engine.connected || (state.mode === "cvc" && autoPlay)) return;
  const sequence = ++analysisSequence;
  window.setTimeout(() => runAnalysis(false, sequence), 80);
}

async function runAnalysis(showFailure = true, sequence = ++analysisSequence) {
  if (!state || state.gameOver || !state.engine.connected || analysisBusy) return;
  analysisBusy = true;
  analysisError = "";
  renderAnalysis();
  try {
    const analysis = await request("/api/analyse", { method: "POST", body: "{}" });
    if (sequence !== analysisSequence || !state) return;
    state.engine.analysis = analysis;
  } catch (error) {
    if (sequence !== analysisSequence) return;
    analysisError = error.message;
    if (showFailure) showToast(`Analysis failed: ${error.message}`);
  } finally {
    const stale = sequence !== analysisSequence;
    analysisBusy = false;
    if (state) renderAnalysis();
    if (stale) scheduleAnalysis();
  }
}

function setBusy(value, detail = "Searching the position") {
  busy = value;
  const obscureBoard = value && state?.mode !== "cvc";
  elements.thinking.classList.toggle("visible", obscureBoard);
  elements.statusPulse.classList.toggle("thinking", value);
  document.querySelector("#thinking-detail").textContent = detail;
  if (state) {
    elements.status.textContent = state.gameOver
      ? `${state.result} · ${state.resultReason}`
      : value ? detail : state.status;
  }
}

function initials(name, isEngine) {
  const words = name.match(/[A-Za-z0-9]+/g) || [];
  return words.slice(0, 2).map((word) => word[0]).join("").toUpperCase() || "—";
}

function renderPlayer(position, color) {
  const controller = state.controllers[color];
  const isEngine = controller.type === "engine";
  const nameElement = position === "top" ? elements.topName : elements.bottomName;
  const detailElement = position === "top" ? elements.topDetail : elements.bottomDetail;
  const monogramElement = position === "top" ? elements.topMonogram : elements.bottomMonogram;
  const turnElement = position === "top" ? elements.topTurn : elements.bottomTurn;
  const stripElement = document.querySelector(`#${position}-player`);
  nameElement.textContent = controller.name;
  detailElement.textContent = isEngine
    ? `${controller.badge} · ${controller.detail} · ${state.engine.moveTimeMs} ms`
    : `${color[0].toUpperCase()}${color.slice(1)} pieces`;
  stripElement.dataset.role = isEngine ? controller.role : "human";
  monogramElement.textContent = initials(controller.name, isEngine);
  monogramElement.closest(".avatar").classList.toggle("human", !isEngine);
  const active = !state.gameOver && state.turn === color;
  turnElement.textContent = active ? "TO MOVE" : "WAITING";
  turnElement.classList.toggle("active", active);
}

function renderPlayers() {
  const topColor = orientation === "white" ? "black" : "white";
  const bottomColor = topColor === "white" ? "black" : "white";
  renderPlayer("top", topColor);
  renderPlayer("bottom", bottomColor);
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
  if (analysisBusy) {
    elements.analysisDepth.textContent = `Analysing with ${state.engine.name}…`;
    elements.analysisPv.textContent = "Searching candidate moves and evaluating the position.";
  } else if (analysisError) {
    elements.analysisDepth.textContent = "Analysis unavailable";
    elements.analysisPv.textContent = analysisError;
  } else {
    elements.analysisDepth.textContent = analysis.depth
      ? `Depth ${analysis.depth} · ${analysis.profileName || state.engine.name} · White perspective`
      : "Ready to analyse this position";
    elements.analysisPv.textContent = analysis.pv?.length
      ? analysis.pv.join("  ")
      : "Press Analyse position to calculate a principal variation.";
  }
  elements.metricDepth.textContent = analysis.depth ?? "—";
  elements.metricNodes.textContent = formatNumber(analysis.nodes);
  elements.metricNps.textContent = formatNumber(analysis.nps);
  elements.metricTime.textContent = analysis.timeMs ? `${analysis.timeMs} ms` : "—";
  elements.runAnalysis.disabled = analysisBusy || state.gameOver || !state.engine.connected;
  elements.runAnalysis.textContent = analysisBusy ? "Analysing…" : "Analyse position";
}

function renderAutoPlay() {
  const visible = state?.mode === "cvc";
  elements.autoPlay.hidden = !visible;
  elements.autoPlay.innerHTML = autoPlay ? "<span>Ⅱ</span>Pause" : "<span>▶</span>Resume";
  elements.autoPlay.title = autoPlay ? "Pause computer play" : "Resume computer play";
  elements.autoPlay.setAttribute("aria-label", elements.autoPlay.title);
  document.querySelector(".board-actions").classList.toggle("has-auto-play", visible);
}

function render() {
  if (!state) return;
  elements.connection.classList.toggle("offline", !state.engine.connected);
  elements.connectionLabel.textContent = state.engine.connected
    ? `${state.engine.profileCount} engine profile${state.engine.profileCount === 1 ? "" : "s"} ready`
    : "Engine unavailable";
  elements.engineName.textContent = state.engine.name;
  const compactBadges = {
    development: "NEWEST",
    candidate: "CANDIDATE",
    legacy: "LEGACY",
    baseline: "BASELINE",
    nnue: "NNUE",
    revision: "REVISION",
  };
  elements.engineState.textContent = state.engine.connected
    ? compactBadges[state.engine.activeProfileRole] || "READY"
    : "OFFLINE";
  elements.engineState.dataset.role = state.engine.activeProfileRole || "revision";
  elements.status.textContent = state.gameOver
    ? `${state.result} · ${state.resultReason}`
    : busy ? "Engine thinking" : state.status;
  elements.statusPulse.style.background = state.gameOver ? "var(--muted)" : "var(--accent)";
  renderPlayers();
  renderBoard();
  renderMoves();
  renderAnalysis();
  renderAutoPlay();
  populateProfileOptions();
}

function populateProfileOptions() {
  if (!state?.profiles?.length) return;
  const key = state.profiles.map((profile) => `${profile.id}:${profile.badge}:${profile.detail}`).join("|");
  if (key !== profileOptionsKey) {
    profileOptionsKey = key;
    const revision = state.profiles.find((profile) => profile.role === "legacy")
      || state.profiles.find((profile) => profile.role === "baseline")
      || state.profiles.find((profile) => profile.kind === "revision");
    const selectors = [elements.pvcProfile, elements.whiteProfile, elements.blackProfile];
    selectors.forEach((select, index) => {
      const previous = select.value;
      select.replaceChildren();
      state.profiles.forEach((profile) => {
        const option = document.createElement("option");
        option.value = profile.id;
        option.textContent = `[${profile.badge}] ${profile.name} — ${profile.detail}`;
        option.disabled = !profile.available;
        select.append(option);
      });
      if (previous && state.profiles.some((profile) => profile.id === previous)) {
        select.value = previous;
      } else if (index === 2 && revision) {
        select.value = revision.id;
      } else {
        select.value = state.profiles[0].id;
      }
    });
  }
  renderProfileSummaries();
}

function renderProfileSummary(select, summary) {
  const profile = state?.profiles?.find((candidate) => candidate.id === select.value);
  if (!profile) return;
  summary.dataset.role = profile.role;
  summary.querySelector(".profile-badge").textContent = profile.badge;
  summary.querySelector("strong").textContent = profile.name;
  summary.querySelector("small").textContent = profile.detail;
}

function renderProfileSummaries() {
  renderProfileSummary(elements.pvcProfile, elements.pvcProfileSummary);
  renderProfileSummary(elements.whiteProfile, elements.whiteProfileSummary);
  renderProfileSummary(elements.blackProfile, elements.blackProfileSummary);
}

function resolveOrientation(preference) {
  if (preference === "white" || preference === "black") return preference;
  return state.mode === "pvc" ? state.playerColor || "white" : "white";
}

async function newGameFromForm() {
  const form = new FormData(elements.newGameForm);
  const mode = form.get("mode");
  const payload = {
    mode,
    side: form.get("side"),
    engineProfile: form.get("pvc-profile"),
    whiteProfile: form.get("white-profile"),
    blackProfile: form.get("black-profile"),
    engineTimeMs: Number(form.get("engine-time")),
    analysisTimeMs: 500,
  };
  orientationPreference = form.get("orientation");
  autoPlay = mode === "cvc";
  cancelScheduledEnginePlay();
  analysisSequence += 1;
  analysisError = "";
  setBusy(true, "Preparing a new board");
  try {
    state = await request("/api/new", { method: "POST", body: JSON.stringify(payload) });
    orientation = resolveOrientation(orientationPreference);
    selectedSquare = null;
    render();
    maybeContinueGame();
  } catch (error) {
    showToast(error.message);
  } finally {
    setBusy(false);
  }
}

function updateSetupVisibility() {
  const mode = new FormData(elements.newGameForm).get("mode");
  elements.pvcOptions.hidden = mode !== "pvc";
  elements.cvcOptions.hidden = mode !== "cvc";
  elements.strengthField.hidden = mode === "pvp";
  const help = {
    pvp: "Two people share this board; no engine moves are made.",
    pvc: "Play against a selected local engine or model.",
    cvc: "Assign an engine or model to each colour and watch them play.",
  };
  elements.modeHelp.textContent = help[mode];
}

for (const button of document.querySelectorAll("#new-game, #new-game-top")) {
  button.addEventListener("click", () => {
    if (state?.mode === "cvc" && autoPlay) {
      autoPlay = false;
      renderAutoPlay();
    }
    populateProfileOptions();
    updateSetupVisibility();
    elements.newGameModal.showModal();
  });
}

document.querySelector("#close-new-game").addEventListener("click", () => elements.newGameModal.close());

elements.newGameForm.addEventListener("submit", (event) => {
  event.preventDefault();
  elements.newGameModal.close();
  newGameFromForm();
});

elements.newGameForm.addEventListener("change", updateSetupVisibility);

for (const select of [elements.pvcProfile, elements.whiteProfile, elements.blackProfile]) {
  select.addEventListener("change", renderProfileSummaries);
}

document.querySelector("#undo").addEventListener("click", async () => {
  if (busy) return;
  cancelScheduledEnginePlay();
  if (state?.mode === "cvc") autoPlay = false;
  analysisSequence += 1;
  analysisError = "";
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
  orientationPreference = orientation;
  selectedSquare = null;
  render();
});

elements.autoPlay.addEventListener("click", () => {
  if (state?.mode !== "cvc" || state.gameOver) return;
  autoPlay = !autoPlay;
  renderAutoPlay();
  if (autoPlay) maybeContinueGame();
  else scheduleAnalysis();
});

elements.runAnalysis.addEventListener("click", () => runAnalysis(true));

function exportRequestBody() {
  const form = new FormData(elements.exportForm);
  return {
    format: form.get("export-format"),
    event: form.get("event"),
    white: form.get("white"),
    black: form.get("black"),
  };
}

async function refreshExport() {
  const sequence = ++exportSequence;
  const format = exportRequestBody().format;
  elements.exportMetadata.hidden = format !== "pgn";
  elements.exportHelp.textContent = {
    pgn: "Standard PGN for Chessigma and other game-analysis tools.",
    fen: "The current position only; move history is not included.",
    json: "A structured log with moves, engine profiles, and every resulting FEN.",
  }[format];
  elements.exportPreview.value = "Preparing export…";
  elements.downloadExport.disabled = true;
  try {
    const payload = await request("/api/export", {
      method: "POST",
      body: JSON.stringify(exportRequestBody()),
    });
    if (sequence !== exportSequence) return;
    exportPayload = payload;
    elements.exportPreview.value = payload.content;
    elements.downloadExport.textContent = `Download .${payload.format}`;
    elements.downloadExport.disabled = false;
  } catch (error) {
    if (sequence !== exportSequence) return;
    exportPayload = null;
    elements.exportPreview.value = `Export failed: ${error.message}`;
    showToast(error.message);
  }
}

document.querySelector("#export-game").addEventListener("click", () => {
  if (!state) return;
  elements.exportWhite.value = state.controllers.white.name;
  elements.exportBlack.value = state.controllers.black.name;
  elements.exportModal.showModal();
  refreshExport();
});

document.querySelector("#close-export").addEventListener("click", () => elements.exportModal.close());
elements.exportForm.addEventListener("submit", (event) => event.preventDefault());
elements.exportForm.addEventListener("change", refreshExport);
for (const input of elements.exportForm.querySelectorAll('input[type="text"], input:not([type])')) {
  input.addEventListener("input", refreshExport);
}

document.querySelector("#copy-export").addEventListener("click", async () => {
  if (!exportPayload) return;
  try {
    await navigator.clipboard.writeText(exportPayload.content);
    showToast(`${exportPayload.format.toUpperCase()} copied`);
  } catch {
    showToast("Could not access the clipboard");
  }
});

elements.downloadExport.addEventListener("click", () => {
  if (!exportPayload) return;
  const blob = new Blob([exportPayload.content], { type: exportPayload.mimeType });
  const url = URL.createObjectURL(blob);
  const anchor = document.createElement("a");
  anchor.href = url;
  anchor.download = exportPayload.filename;
  document.body.append(anchor);
  anchor.click();
  anchor.remove();
  URL.revokeObjectURL(url);
  showToast(`${exportPayload.filename} downloaded`);
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
    orientation = resolveOrientation("auto");
    autoPlay = state.mode === "cvc";
    render();
    updateSetupVisibility();
    maybeContinueGame();
  } catch (error) {
    elements.connection.classList.add("offline");
    elements.connectionLabel.textContent = "Service unavailable";
    showToast(error.message);
  }
}

initialise();
