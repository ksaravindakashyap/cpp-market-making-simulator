import type { ComparisonData } from "@/types/ws";
import { MonospaceNum } from "@/components/MonospaceNum";

function fmtSharpe(x: number | null): string {
  if (x === null || !Number.isFinite(x)) return "n/a";
  return x.toFixed(4);
}

export interface ComparisonReportProps {
  data: ComparisonData | null;
}

export function ComparisonReport({ data }: ComparisonReportProps) {
  if (!data) {
    return (
      <p className="font-mono text-sm text-terminal-muted">
        Run an offline A/B comparison from Control — same tick CSV, Avellaneda–Stoikov vs fixed spread.
      </p>
    );
  }

  if (data.error) {
    return (
      <p className="rounded border border-loss/40 bg-loss-dim/20 px-3 py-2 font-mono text-sm text-loss" role="alert">
        {data.error}
      </p>
    );
  }

  const as = data.avellaneda_stoikov;
  const nv = data.fixed_spread;

  return (
    <div className="space-y-4">
      {data.fixed_half_spread !== undefined ? (
        <p className="font-mono text-[10px] text-terminal-muted">
          Fixed-spread half width: <span className="text-terminal-accent">{data.fixed_half_spread}</span> raw units
          {data.ticks !== undefined ? (
            <>
              {" "}
              · ticks: {data.ticks}
            </>
          ) : null}
        </p>
      ) : null}

      {as && nv ? (
        <div className="overflow-x-auto rounded border border-terminal-border">
          <table className="w-full min-w-[720px] border-collapse text-left text-[11px]">
            <thead>
              <tr className="border-b border-terminal-border bg-terminal-bg/80 font-sans uppercase tracking-wider text-terminal-muted">
                <th className="sticky left-0 z-[1] bg-terminal-bg/95 px-2 py-2">Metric</th>
                <th className="px-2 py-2 text-right">Avellaneda–Stoikov</th>
                <th className="px-2 py-2 text-right">Fixed spread</th>
              </tr>
            </thead>
            <tbody className="font-mono text-terminal-fg">
              <tr className="border-b border-terminal-border/60">
                <td className="sticky left-0 bg-terminal-panel px-2 py-1.5 text-terminal-muted">Sharpe (ann.)</td>
                <td className="px-2 py-1.5 text-right">{fmtSharpe(as.sharpe_ratio)}</td>
                <td className="px-2 py-1.5 text-right">{fmtSharpe(nv.sharpe_ratio)}</td>
              </tr>
              <tr className="border-b border-terminal-border/60">
                <td className="sticky left-0 bg-terminal-panel px-2 py-1.5 text-terminal-muted">Sortino (ann.)</td>
                <td className="px-2 py-1.5 text-right">{fmtSharpe(as.sortino_ratio)}</td>
                <td className="px-2 py-1.5 text-right">{fmtSharpe(nv.sortino_ratio)}</td>
              </tr>
              <tr className="border-b border-terminal-border/60">
                <td className="sticky left-0 bg-terminal-panel px-2 py-1.5 text-terminal-muted">Max drawdown</td>
                <td className="px-2 py-1.5 text-right text-loss">{as.max_drawdown.toFixed(4)}</td>
                <td className="px-2 py-1.5 text-right text-loss">{nv.max_drawdown.toFixed(4)}</td>
              </tr>
              <tr className="border-b border-terminal-border/60">
                <td className="sticky left-0 bg-terminal-panel px-2 py-1.5 text-terminal-muted">Equity</td>
                <td className="px-2 py-1.5 text-right">
                  <MonospaceNum value={as.equity} signedColor={false} fractionDigits={4} />
                </td>
                <td className="px-2 py-1.5 text-right">
                  <MonospaceNum value={nv.equity} signedColor={false} fractionDigits={4} />
                </td>
              </tr>
              <tr className="border-b border-terminal-border/60">
                <td className="sticky left-0 bg-terminal-panel px-2 py-1.5 text-terminal-muted">Realized PnL</td>
                <td className="px-2 py-1.5 text-right">
                  <MonospaceNum value={as.realized_pnl} fractionDigits={4} />
                </td>
                <td className="px-2 py-1.5 text-right">
                  <MonospaceNum value={nv.realized_pnl} fractionDigits={4} />
                </td>
              </tr>
              <tr className="border-b border-terminal-border/60">
                <td className="sticky left-0 bg-terminal-panel px-2 py-1.5 text-terminal-muted">Unrealized PnL</td>
                <td className="px-2 py-1.5 text-right">
                  <MonospaceNum value={as.unrealized_pnl} fractionDigits={4} />
                </td>
                <td className="px-2 py-1.5 text-right">
                  <MonospaceNum value={nv.unrealized_pnl} fractionDigits={4} />
                </td>
              </tr>
              <tr className="border-b border-terminal-border/60">
                <td className="sticky left-0 bg-terminal-panel px-2 py-1.5 text-terminal-muted">Mean inventory</td>
                <td className="px-2 py-1.5 text-right tabular-nums">{as.mean_inventory.toFixed(4)}</td>
                <td className="px-2 py-1.5 text-right tabular-nums">{nv.mean_inventory.toFixed(4)}</td>
              </tr>
              <tr className="border-b border-terminal-border/60">
                <td className="sticky left-0 bg-terminal-panel px-2 py-1.5 text-terminal-muted">Inventory variance</td>
                <td className="px-2 py-1.5 text-right tabular-nums">{as.variance_inventory.toFixed(4)}</td>
                <td className="px-2 py-1.5 text-right tabular-nums">{nv.variance_inventory.toFixed(4)}</td>
              </tr>
              <tr className="border-b border-terminal-border/60">
                <td className="sticky left-0 bg-terminal-panel px-2 py-1.5 text-terminal-muted">Max |inventory|</td>
                <td className="px-2 py-1.5 text-right">{as.max_abs_inventory}</td>
                <td className="px-2 py-1.5 text-right">{nv.max_abs_inventory}</td>
              </tr>
              <tr className="border-b border-terminal-border/60">
                <td className="sticky left-0 bg-terminal-panel px-2 py-1.5 text-terminal-muted">Final position</td>
                <td className="px-2 py-1.5 text-right">{as.final_position}</td>
                <td className="px-2 py-1.5 text-right">{nv.final_position}</td>
              </tr>
              <tr>
                <td className="sticky left-0 bg-terminal-panel px-2 py-1.5 text-terminal-muted">Ticks</td>
                <td className="px-2 py-1.5 text-right text-terminal-muted">{as.ticks}</td>
                <td className="px-2 py-1.5 text-right text-terminal-muted">{nv.ticks}</td>
              </tr>
            </tbody>
          </table>
        </div>
      ) : (
        <p className="font-mono text-sm text-terminal-warn">Incomplete comparison payload.</p>
      )}

      {data.markdown ? (
        <details className="group">
          <summary className="cursor-pointer font-sans text-[10px] font-semibold uppercase tracking-widest text-terminal-accent">
            Markdown report
          </summary>
          <pre className="mt-2 max-h-64 overflow-auto whitespace-pre-wrap rounded border border-terminal-border bg-terminal-bg p-3 font-mono text-[10px] leading-relaxed text-terminal-muted scroll-terminal">
            {data.markdown}
          </pre>
        </details>
      ) : null}
    </div>
  );
}
