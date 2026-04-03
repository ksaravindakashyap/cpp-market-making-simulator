import { useMemo } from "react";
import {
  CartesianGrid,
  ComposedChart,
  Legend,
  Line,
  ReferenceArea,
  ReferenceLine,
  ResponsiveContainer,
  Tooltip,
  XAxis,
  YAxis,
} from "recharts";

import type { AnalyticsData, PnlData, PnlHistoryPoint } from "@/types/ws";

function strokeForSeries(values: number[]): string {
  const v = values[values.length - 1];
  if (!Number.isFinite(v)) return "#5a7a6e";
  return v >= 0 ? "#00e676" : "#ff5252";
}

/** Gross upside / gross downside from consecutive sample deltas on `total` (equity). */
function sampledProfitFactor(points: PnlHistoryPoint[]): number | null {
  if (points.length < 2) return null;
  let grossUp = 0;
  let grossDown = 0;
  for (let i = 1; i < points.length; i++) {
    const d = points[i].total - points[i - 1].total;
    if (d > 0) grossUp += d;
    else if (d < 0) grossDown += -d;
  }
  if (grossDown < 1e-12) {
    return grossUp > 1e-12 ? Number.POSITIVE_INFINITY : null;
  }
  return grossUp / grossDown;
}

export interface PnLChartProps {
  history: PnlHistoryPoint[];
  pnl: PnlData | null;
  analytics: AnalyticsData | null;
}

export function PnLChart({ history, pnl, analytics }: PnLChartProps) {
  const chartData = useMemo(
    () =>
      history.map((p) => ({
        t: new Date(p.t).toLocaleTimeString(undefined, { hour12: false }),
        realized: p.realized,
        total: p.total,
      })),
    [history],
  );

  const realizedStroke = useMemo(
    () => strokeForSeries(history.map((h) => h.realized)),
    [history],
  );
  const totalStroke = useMemo(() => strokeForSeries(history.map((h) => h.total)), [history]);

  const pf = useMemo(() => sampledProfitFactor(history), [history]);
  const sharpe = analytics?.sharpe_ratio ?? null;
  const maxDd = pnl?.max_drawdown ?? analytics?.max_drawdown ?? null;

  const yDomain = useMemo(() => {
    if (history.length === 0) return [0, 1] as [number, number];
    let lo = 0;
    let hi = 0;
    for (const p of history) {
      lo = Math.min(lo, p.realized, p.total);
      hi = Math.max(hi, p.realized, p.total);
    }
    const pad = Math.max(Math.abs(hi - lo) * 0.06, 1e-6);
    return [lo - pad, hi + pad] as [number, number];
  }, [history]);

  if (chartData.length === 0) {
    return (
      <div className="flex h-full min-h-[200px] items-center justify-center font-mono text-sm text-terminal-muted">
        Awaiting PnL samples…
      </div>
    );
  }

  return (
    <div className="relative h-full min-h-[220px] w-full">
      <div className="pointer-events-none absolute left-2 top-2 z-10 max-w-[min(100%,22rem)] rounded border border-terminal-border/80 bg-terminal-bg/90 px-2 py-1.5 font-mono text-[10px] leading-relaxed text-terminal-fg shadow-panel backdrop-blur-sm">
        <div className="mb-0.5 text-[9px] uppercase tracking-wider text-terminal-muted">Metrics</div>
        <div className="grid grid-cols-[auto_1fr] gap-x-3 gap-y-0.5">
          <span className="text-terminal-muted">Sharpe</span>
          <span className="text-right tabular-nums text-terminal-accent">
            {sharpe === null || sharpe === undefined ? "n/a" : sharpe.toFixed(3)}
          </span>
          <span className="text-terminal-muted">Max DD</span>
          <span className="text-right tabular-nums text-loss">
            {maxDd === null || maxDd === undefined ? "—" : maxDd.toFixed(4)}
          </span>
          <span className="text-terminal-muted">PF</span>
          <span
            className="text-right tabular-nums text-terminal-fg"
            title="Profit factor on sampled equity: sum(Δ+) / sum(Δ−) between PnL ticks"
          >
            {pf === null ? "—" : Number.isFinite(pf) ? pf.toFixed(3) : "∞"}
          </span>
        </div>
      </div>

      <ResponsiveContainer width="100%" height="100%" minHeight={220}>
        <ComposedChart data={chartData} margin={{ top: 36, right: 12, left: 4, bottom: 4 }}>
          <CartesianGrid strokeDasharray="3 6" stroke="#14221c" vertical={false} />
          <XAxis
            dataKey="t"
            tick={{ fill: "#5a7a6e", fontSize: 10, fontFamily: "IBM Plex Mono" }}
            tickLine={false}
            axisLine={{ stroke: "#1c2824" }}
            minTickGap={28}
          />
          <YAxis
            tick={{ fill: "#5a7a6e", fontSize: 10, fontFamily: "IBM Plex Mono" }}
            tickLine={false}
            axisLine={{ stroke: "#1c2824" }}
            width={52}
            domain={yDomain}
          />
          {yDomain[0] < 0 ? (
            <ReferenceArea y1={yDomain[0]} y2={0} fill="#ff5252" fillOpacity={0.06} stroke="none" />
          ) : null}
          {yDomain[1] > 0 ? (
            <ReferenceArea y1={0} y2={yDomain[1]} fill="#00e676" fillOpacity={0.06} stroke="none" />
          ) : null}
          <ReferenceLine y={0} stroke="#1c2824" strokeDasharray="4 4" />
          <Tooltip
            contentStyle={{
              background: "#0f1513",
              border: "1px solid #1c2824",
              borderRadius: 4,
              fontFamily: "IBM Plex Mono, monospace",
              fontSize: 12,
              color: "#c8e6d8",
            }}
            labelStyle={{ color: "#5a7a6e" }}
            formatter={(value: number, name: string) => [value.toFixed(4), name]}
          />
          <Legend
            wrapperStyle={{ fontFamily: "IBM Plex Mono, monospace", fontSize: 11, paddingTop: 8 }}
            formatter={(value) => <span className="text-terminal-muted">{value}</span>}
          />
          <Line
            type="monotone"
            dataKey="realized"
            name="Realized"
            stroke={realizedStroke}
            strokeWidth={2}
            dot={false}
            isAnimationActive={false}
            connectNulls
          />
          <Line
            type="monotone"
            dataKey="total"
            name="Total (r+u)"
            stroke={totalStroke}
            strokeWidth={1.75}
            strokeDasharray="6 5"
            dot={false}
            isAnimationActive={false}
            connectNulls
          />
        </ComposedChart>
      </ResponsiveContainer>
    </div>
  );
}
