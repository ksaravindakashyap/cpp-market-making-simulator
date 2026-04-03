#!/usr/bin/env node
/**
 * Writes data/demo_ticks.csv — ~250 ticks, 50ms apart, oscillating price and alternating
 * sides so the simulator has time to quote, fill, and move PnL while you connect the dashboard.
 *
 * Usage (from repo root): node scripts/generate-demo-ticks.mjs
 */
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const outPath = path.join(__dirname, "..", "data", "demo_ticks.csv");

const ROWS = 250;
const STEP_NS = 50_000_000n; // 50ms between ticks (visible at 1x replay)
let t = 1_000_000_000_000_000n; // start ns (arbitrary, monotonic)

const lines = ["timestamp_ns,price,quantity,side"];

for (let i = 0; i < ROWS; i++) {
  t += STEP_NS;
  const wave = Math.sin(i * 0.15) * 400;
  const price = Math.round(10000 + wave + (i % 7) * 12);
  const qty = 4 + (i % 5);
  const side = i % 2 === 0 ? "BUY" : "SELL";
  lines.push(`${t},${price},${qty},${side}`);
}

fs.mkdirSync(path.dirname(outPath), { recursive: true });
fs.writeFileSync(outPath, lines.join("\n") + "\n", "utf8");
console.log(`Wrote ${ROWS} ticks to ${outPath}`);
