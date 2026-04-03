import type { ReactNode } from "react";
import type { AnalyticsData, PnlData, StrategyData } from "@/types/ws";
import { MonospaceNum } from "@/components/MonospaceNum";

function ConnectionBadge({ status }: { status: string }) {
  const colors: Record<string, string> = {
    idle: "bg-terminal-muted",
    connecting: "bg-terminal-warn",
    open: "bg-profit",
    closed: "bg-terminal-muted",
    error: "bg-loss",
  };
  return (
    <span className="flex items-center gap-2 font-mono text-[10px] text-terminal-muted sm:text-xs">
      <span className={`h-2 w-2 shrink-0 rounded-full ${colors[status] ?? "bg-terminal-muted"}`} aria-hidden />
      <span className="uppercase tracking-wider">{status}</span>
    </span>
  );
}

function Metric({ label, children, className = "" }: { label: string; children: ReactNode; className?: string }) {
  return (
    <div className={`min-w-[5.5rem] ${className}`}>
      <p className="font-sans text-[9px] font-medium uppercase tracking-widest text-terminal-muted sm:text-[10px]">{label}</p>
      <div className="mt-0.5 font-mono text-sm tabular-nums text-terminal-fg sm:text-base">{children}</div>
    </div>
  );
}

export interface MetricsBarProps {
  pnl: PnlData | null;
  strategy: StrategyData | null;
  analytics: AnalyticsData | null;
  connection: string;
  reconnectAttempt: number;
  onReconnect: () => void;
}

export function MetricsBar({
  pnl,
  strategy,
  analytics,
  connection,
  reconnectAttempt,
  onReconnect,
}: MetricsBarProps) {
  return (
    <header className="sticky top-0 z-20 border-b border-terminal-border bg-terminal-panel/90 backdrop-blur-md">
      <div className="mx-auto flex max-w-[1800px] flex-col gap-3 px-3 py-3 sm:px-4 lg:flex-row lg:items-center lg:justify-between lg:gap-4">
        <div className="flex shrink-0 items-center gap-3">
          <div
            className="flex h-9 w-9 shrink-0 items-center justify-center rounded border border-terminal-accent/40 bg-terminal-bg font-mono text-xs font-bold text-terminal-accent shadow-[0_0_12px_rgba(74,222,128,0.12)]"
            aria-hidden
          >
            MS
          </div>
          <div>
            <h1 className="font-sans text-base font-semibold tracking-tight text-terminal-fg sm:text-lg">
              MM<span className="text-terminal-accent">SIM</span>
            </h1>
            <p className="font-mono text-[10px] text-terminal-muted sm:text-[11px]">
              Market making simulator · live metrics
            </p>
          </div>
        </div>

        <div className="flex min-w-0 flex-1 flex-wrap items-end justify-start gap-x-5 gap-y-3 lg:justify-center xl:gap-x-6">
          <Metric label="Equity">
            {pnl ? <MonospaceNum value={pnl.equity} signedColor={false} fractionDigits={4} /> : "—"}
          </Metric>
          <Metric label="Realized">
            {pnl ? <MonospaceNum value={pnl.realized_pnl} fractionDigits={4} /> : "—"}
          </Metric>
          <Metric label="Unrealized">
            {pnl ? <MonospaceNum value={pnl.unrealized_pnl} fractionDigits={4} /> : "—"}
          </Metric>
          <Metric label="Max DD">
            {pnl ? (
              <span className="font-mono text-loss tabular-nums">
                {pnl.max_drawdown.toLocaleString(undefined, { maximumFractionDigits: 4 })}
              </span>
            ) : (
              "—"
            )}
          </Metric>
          <Metric label="Position">
            {pnl ? <MonospaceNum value={pnl.position} signedColor fractionDigits={2} /> : "—"}
          </Metric>
          <div className="hidden min-w-[4.5rem] sm:block">
            <p className="font-sans text-[9px] font-medium uppercase tracking-widest text-terminal-muted sm:text-[10px]">Bid / Ask</p>
            <p className="mt-0.5 font-mono text-sm tabular-nums text-terminal-fg sm:text-base">
              {strategy ? (
                <>
                  {strategy.bid.toFixed(2)} <span className="text-terminal-muted">/</span> {strategy.ask.toFixed(2)}
                </>
              ) : (
                "—"
              )}
            </p>
          </div>
          <Metric label="σ" className="hidden sm:block">
            {strategy ? strategy.sigma.toFixed(4) : "—"}
          </Metric>
          <div className="hidden md:block">
            <p className="font-sans text-[9px] font-medium uppercase tracking-widest text-terminal-muted sm:text-[10px]">γ / κ</p>
            <p className="mt-0.5 font-mono text-sm tabular-nums text-terminal-fg sm:text-base">
              {strategy ? `${strategy.gamma.toFixed(3)} / ${strategy.kappa.toFixed(2)}` : "—"}
            </p>
          </div>
          <Metric label="Sharpe" className="hidden lg:block">
            {analytics?.sharpe_ratio === null || analytics === null ? "n/a" : analytics.sharpe_ratio.toFixed(3)}
          </Metric>
          <Metric label="Sortino" className="hidden xl:block">
            {analytics?.sortino_ratio === null || analytics === null ? "n/a" : analytics.sortino_ratio.toFixed(3)}
          </Metric>
          <Metric label="Fill %" className="hidden xl:block">
            {analytics ? `${(analytics.fill_rate * 100).toFixed(1)}%` : "—"}
          </Metric>
        </div>

        <div className="flex shrink-0 flex-wrap items-center gap-3 border-t border-terminal-border pt-3 lg:border-t-0 lg:pt-0">
          <ConnectionBadge status={connection} />
          {reconnectAttempt > 0 ? (
            <span className="font-mono text-[10px] text-terminal-warn">#{reconnectAttempt}</span>
          ) : null}
          <button
            type="button"
            onClick={onReconnect}
            className="rounded border border-terminal-border bg-terminal-bg px-2.5 py-1 font-mono text-[10px] text-terminal-fg hover:border-terminal-accent hover:text-terminal-accent sm:text-xs"
          >
            Reconnect
          </button>
        </div>
      </div>
    </header>
  );
}
