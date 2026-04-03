import { useEffect, useRef } from "react";

import type { TradeLogEntry } from "@/types/ws";

function fmtTime(ns: number): string {
  const ms = Number(ns) / 1e6;
  const d = new Date(ms);
  if (!Number.isFinite(d.getTime())) return "—";
  return d.toLocaleTimeString(undefined, {
    hour12: false,
    hour: "2-digit",
    minute: "2-digit",
    second: "2-digit",
  });
}

export interface TradeLogProps {
  trades: TradeLogEntry[];
}

/**
 * Chronological feed (oldest → newest); newest at bottom. Auto-scrolls to latest.
 */
export function TradeLog({ trades }: TradeLogProps) {
  const containerRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    const el = containerRef.current;
    if (!el) return;
    el.scrollTo({ top: el.scrollHeight, behavior: "smooth" });
  }, [trades]);

  if (trades.length === 0) {
    return (
      <p className="font-mono text-sm text-terminal-muted">No trades yet.</p>
    );
  }

  return (
    <div
      ref={containerRef}
      className="scroll-terminal max-h-[min(420px,50vh)] overflow-y-auto rounded border border-terminal-border/60 bg-terminal-bg/40"
    >
      <table className="w-full border-collapse font-mono text-[11px]">
        <thead className="sticky top-0 z-[1] bg-terminal-panel/95 backdrop-blur-sm">
          <tr className="border-b border-terminal-border text-left text-[9px] uppercase tracking-wider text-terminal-muted">
            <th className="px-2 py-1.5 font-medium">Time</th>
            <th className="px-2 py-1.5 font-medium">Side</th>
            <th className="px-2 py-1.5 text-right font-medium">Qty</th>
            <th className="px-2 py-1.5 text-right font-medium">Price</th>
            <th className="px-2 py-1.5 text-right font-medium">Inv</th>
          </tr>
        </thead>
        <tbody>
          {trades.map((t, i) => (
            <tr
              key={`${t.timestamp_ns}-${t.order_id}-${i}`}
              className="border-b border-terminal-grid/40 text-terminal-fg last:border-0"
            >
              <td className="whitespace-nowrap px-2 py-1 text-terminal-muted">{fmtTime(t.timestamp_ns)}</td>
              <td className="px-2 py-1">
                <span
                  className={
                    t.side === "BUY"
                      ? "font-semibold text-profit"
                      : "font-semibold text-loss"
                  }
                >
                  {t.side}
                </span>
              </td>
              <td className="px-2 py-1 text-right tabular-nums">{t.quantity}</td>
              <td className="px-2 py-1 text-right tabular-nums">{t.price}</td>
              <td className="px-2 py-1 text-right tabular-nums text-terminal-accent">{t.inventory_after}</td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}
