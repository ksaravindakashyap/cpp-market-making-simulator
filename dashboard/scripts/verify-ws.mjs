#!/usr/bin/env node
/**
 * Connects to mmsim_ws_server and checks main WebSocket channels.
 *
 * Start server first, e.g.:
 *   mmsim_ws_server --data data/demo_ticks.csv --port 8080 --speed 10x
 *
 * From dashboard/: npm run verify:ws
 */
import WebSocket from "ws";

const url = process.env.WS_URL ?? "ws://localhost:8080";
const TIMEOUT_MS = Number(process.env.WS_VERIFY_TIMEOUT_MS ?? 12000);

const counts = Object.create(null);
let firstPnl = null;
let tradeRows = 0;
let reported = false;

function note(ch) {
  counts[ch] = (counts[ch] ?? 0) + 1;
}

function runChecks() {
  console.log("\n--- verify-ws summary ---");
  console.log("channels:", counts);
  console.log("trade messages:", tradeRows);
  if (firstPnl) console.log("sample pnl:", JSON.stringify(firstPnl));

  const errors = [];

  if ((counts.book ?? 0) < 1) errors.push('expected at least 1 "book" message');
  if ((counts.pnl ?? 0) < 1) errors.push('expected at least 1 "pnl" message');
  if ((counts.strategy ?? 0) < 1) errors.push('expected at least 1 "strategy" message');
  if ((counts.volatility ?? 0) < 1) errors.push('expected at least 1 "volatility" message');
  if ((counts.analytics ?? 0) < 1) {
    errors.push('expected at least 1 "analytics" message (first ~1s after connect)');
  }
  if (tradeRows < 1) {
    errors.push(
      'expected at least 1 "trades" message (use demo_ticks.csv; keep server running so fills + replay exist)',
    );
  }

  if (firstPnl && typeof firstPnl.equity !== "number") {
    errors.push("pnl.data.equity missing or not a number");
  }

  if (errors.length) {
    console.error("\nFAIL:");
    for (const e of errors) console.error(" -", e);
    console.error("\nHints:");
    console.error(" - Start: mmsim_ws_server --data data/demo_ticks.csv --port 8080 --speed 10x");
    console.error(" - From repo root; increase WS_VERIFY_TIMEOUT_MS if needed.");
    return 1;
  }

  console.log("\nPASS: WebSocket channels look healthy.\n");
  return 0;
}

function finish() {
  if (reported) return;
  reported = true;
  const code = runChecks();
  process.exit(code);
}

const ws = new WebSocket(url);

ws.on("open", () => {
  console.log(`verify-ws: connected to ${url}`);
});

ws.on("message", (buf) => {
  let j;
  try {
    j = JSON.parse(buf.toString());
  } catch {
    return;
  }
  const ch = j.channel;
  if (typeof ch !== "string") return;
  note(ch);
  if (ch === "pnl" && j.data && firstPnl === null) firstPnl = j.data;
  if (ch === "trades") tradeRows += 1;
});

ws.on("error", (e) => {
  console.error("verify-ws: WebSocket error:", e.message);
  if (!reported) {
    reported = true;
    console.error("\nFAIL: could not connect. Is mmsim_ws_server running on", url, "?");
    process.exit(1);
  }
});

setTimeout(() => {
  ws.close();
}, TIMEOUT_MS);

ws.on("close", () => {
  finish();
});
