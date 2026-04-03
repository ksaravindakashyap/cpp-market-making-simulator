import { useMemo } from "react";

import type { VolatilityData } from "@/types/ws";

function MiniSparkline({ values, accent }: { values: number[]; accent: string }) {
  if (values.length < 2) {
    return <div className="h-7 w-24 shrink-0 rounded bg-terminal-grid/60" title="Need more samples" />;
  }
  const min = Math.min(...values);
  const max = Math.max(...values);
  const range = max - min || 1e-12;
  const w = 100;
  const h = 100;
  const pad = 4;
  const pts = values
    .map((v, i) => {
      const x = pad + (i / (values.length - 1)) * (w - 2 * pad);
      const y = pad + (1 - (v - min) / range) * (h - 2 * pad);
      return `${x},${y}`;
    })
    .join(" ");

  return (
    <svg className="h-7 w-24 shrink-0" viewBox={`0 0 ${w} ${h}`} preserveAspectRatio="none" aria-hidden>
      <polyline
        fill="none"
        stroke={accent}
        strokeWidth="2.5"
        strokeLinejoin="round"
        strokeLinecap="round"
        points={pts}
        vectorEffect="non-scaling-stroke"
      />
    </svg>
  );
}

function fmt(v: number | null): string {
  if (v === null || !Number.isFinite(v)) return "—";
  if (Math.abs(v) < 1e-8) return "0";
  if (Math.abs(v) >= 0.01) return v.toFixed(4);
  return v.toExponential(3);
}

export interface VolatilityPanelProps {
  data: VolatilityData | null;
}

const ACCENTS = {
  ctc: "#00e676",
  park: "#ffb300",
  yz: "#22d3ee",
} as const;

/**
 * Three comparable horizontal bars (common scale) + numeric value + sparkline history per estimator.
 */
export function VolatilityPanel({ data }: VolatilityPanelProps) {
  const maxBar = useMemo(() => {
    if (!data) return 1e-12;
    const vals = [data.close_to_close, data.parkinson, data.yang_zhang].filter(
      (x): x is number => x !== null && Number.isFinite(x),
    );
    if (vals.length === 0) return 1e-12;
    return Math.max(...vals, 1e-12);
  }, [data]);

  if (!data) {
    return (
      <p className="font-mono text-sm text-terminal-muted">Waiting for volatility stream…</p>
    );
  }

  const rows: {
    key: keyof typeof ACCENTS;
    label: string;
    short: string;
    value: number | null;
    hist: number[];
  }[] = [
    {
      key: "ctc",
      label: "Close-to-close",
      short: "CtC",
      value: data.close_to_close,
      hist: data.history.close_to_close,
    },
    {
      key: "park",
      label: "Parkinson",
      short: "PK",
      value: data.parkinson,
      hist: data.history.parkinson,
    },
    {
      key: "yz",
      label: "Yang–Zhang",
      short: "YZ",
      value: data.yang_zhang,
      hist: data.history.yang_zhang,
    },
  ];

  return (
    <div className="space-y-3">
      <p className="font-mono text-[10px] text-terminal-muted">
        Per-bar σ (not annualized) · rolling window · bars aligned to max of the three estimates
      </p>
      <div className="space-y-3">
        {rows.map((row) => {
          const v = row.value;
          const pct = v !== null && Number.isFinite(v) ? Math.min(100, (Math.abs(v) / maxBar) * 100) : 0;
          return (
            <div
              key={row.key}
              className="flex flex-col gap-2 border-b border-terminal-border/40 pb-3 last:border-0 last:pb-0 sm:flex-row sm:items-center"
            >
              <div className="w-full shrink-0 font-sans text-[11px] font-medium uppercase tracking-wide text-terminal-muted sm:w-36">
                <span className="hidden sm:inline">{row.label}</span>
                <span className="sm:hidden">{row.short}</span>
              </div>
              <div className="flex min-w-0 flex-1 flex-wrap items-center gap-2">
                <div className="h-2.5 min-w-[120px] flex-1 overflow-hidden rounded-full bg-terminal-grid/90">
                  <div
                    className="h-full rounded-full transition-[width] duration-150"
                    style={{
                      width: `${pct}%`,
                      backgroundColor: ACCENTS[row.key],
                      boxShadow: `0 0 8px ${ACCENTS[row.key]}55`,
                    }}
                    title={`${row.label}: ${fmt(v)}`}
                  />
                </div>
                <span className="w-[7.5rem] shrink-0 text-right font-mono text-xs tabular-nums text-terminal-fg">
                  {fmt(v)}
                </span>
                <MiniSparkline values={row.hist} accent={ACCENTS[row.key]} />
              </div>
            </div>
          );
        })}
      </div>
    </div>
  );
}
