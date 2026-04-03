import type { BookData } from "@/types/ws";

export interface OrderBookViewProps {
  book: BookData | null;
  /** Highlight these resting quote order IDs in cyan (from `strategy` channel). */
  strategyBidOrderId?: number | null;
  strategyAskOrderId?: number | null;
  /** Max rows per side (default 18). */
  maxLevels?: number;
}

function sumQty(orders: { order_id: number; quantity: number }[]): number {
  return orders.reduce((s, o) => s + o.quantity, 0);
}

function qtyForOrderId(
  orders: { order_id: number; quantity: number }[],
  id: number | null | undefined,
): number {
  if (id === null || id === undefined) return 0;
  return orders.filter((o) => o.order_id === id).reduce((s, o) => s + o.quantity, 0);
}

/**
 * Horizontal depth: bids extend left (green), asks extend right (red).
 * Strategy quote size at each level is cyan (inner segment, adjacent to the mid spine).
 */
export function OrderBookView({
  book,
  strategyBidOrderId,
  strategyAskOrderId,
  maxLevels = 18,
}: OrderBookViewProps) {
  if (!book || (book.bids.length === 0 && book.asks.length === 0)) {
    return (
      <div className="flex h-[320px] items-center justify-center font-mono text-sm text-terminal-muted">
        No book data
      </div>
    );
  }

  const bidRows = book.bids.slice(0, maxLevels);
  const askRows = book.asks.slice(0, maxLevels);
  const rows = Math.max(bidRows.length, askRows.length, 1);

  const bestBid = book.bids[0]?.price;
  const bestAsk = book.asks[0]?.price;
  const spread =
    bestBid !== undefined && bestAsk !== undefined && Number.isFinite(bestAsk - bestBid)
      ? bestAsk - bestBid
      : null;
  const mid =
    bestBid !== undefined && bestAsk !== undefined ? (bestBid + bestAsk) / 2 : null;

  let maxQty = 1;
  for (const lvl of bidRows) {
    maxQty = Math.max(maxQty, sumQty(lvl.orders));
  }
  for (const lvl of askRows) {
    maxQty = Math.max(maxQty, sumQty(lvl.orders));
  }

  const padRows: number[] = [];
  for (let i = 0; i < rows; i++) padRows.push(i);

  return (
    <div className="flex min-h-[320px] flex-col gap-2">
      <div className="grid grid-cols-[1fr_minmax(11rem,auto)_1fr] gap-2 border-b border-terminal-border pb-2 font-mono text-[11px]">
        <div className="text-right text-[10px] uppercase tracking-wider text-profit">
          Bids (L)
        </div>
        <div className="flex flex-col items-center justify-center gap-0.5 border-x border-terminal-border px-2 text-center">
          <div className="text-[10px] uppercase tracking-wider text-terminal-muted">BBO / spread</div>
          <div className="flex flex-wrap items-center justify-center gap-x-2 gap-y-1 text-xs">
            <span className="text-profit">
              bid <span className="tabular-nums text-terminal-fg">{bestBid ?? "—"}</span>
            </span>
            <span className="text-terminal-muted">|</span>
            <span className="text-loss">
              ask <span className="tabular-nums text-terminal-fg">{bestAsk ?? "—"}</span>
            </span>
          </div>
          <div className="text-sm tabular-nums text-terminal-accent">
            spread{" "}
            {spread !== null ? (
              <span className="text-terminal-fg">{spread}</span>
            ) : (
              <span className="text-terminal-muted">—</span>
            )}
            {mid !== null ? (
              <span className="ml-2 text-[10px] text-terminal-muted">mid {mid.toFixed(2)}</span>
            ) : null}
          </div>
        </div>
        <div className="text-left text-[10px] uppercase tracking-wider text-loss">
          Asks (R)
        </div>
      </div>

      <div className="grid min-h-0 flex-1 grid-cols-[1fr_auto_1fr] gap-0 text-[11px]">
        {/* Bids: bars grow left */}
        <div className="flex flex-col justify-center gap-0.5 pr-1">
          {padRows.map((i) => {
            const lvl = bidRows[i];
            if (!lvl) {
              return <div key={`bpad-${i}`} className="h-5" />;
            }
            const total = sumQty(lvl.orders);
            const ours = qtyForOrderId(lvl.orders, strategyBidOrderId);
            const other = Math.max(0, total - ours);
            const w = maxQty > 0 ? (total / maxQty) * 100 : 0;
            return (
              <div key={`bid-${lvl.price}-${i}`} className="flex h-5 items-center gap-1">
                <div className="flex min-w-0 flex-1 justify-end">
                  <DepthBarBid
                    widthPct={w}
                    oursQty={ours}
                    otherQty={other}
                    totalQty={total}
                  />
                </div>
                <span className="w-14 shrink-0 text-right tabular-nums text-profit/95">{lvl.price}</span>
                <span className="w-8 shrink-0 text-right tabular-nums text-terminal-muted">{total}</span>
              </div>
            );
          })}
        </div>

        {/* Mid spine */}
        <div className="w-px shrink-0 bg-terminal-border" aria-hidden />

        {/* Asks: bars grow right */}
        <div className="flex flex-col justify-center gap-0.5 pl-1">
          {padRows.map((i) => {
            const lvl = askRows[i];
            if (!lvl) {
              return <div key={`apad-${i}`} className="h-5" />;
            }
            const total = sumQty(lvl.orders);
            const ours = qtyForOrderId(lvl.orders, strategyAskOrderId);
            const other = Math.max(0, total - ours);
            const w = maxQty > 0 ? (total / maxQty) * 100 : 0;
            return (
              <div key={`ask-${lvl.price}-${i}`} className="flex h-5 items-center gap-1">
                <span className="w-8 shrink-0 tabular-nums text-terminal-muted">{total}</span>
                <span className="w-14 shrink-0 tabular-nums text-loss/95">{lvl.price}</span>
                <div className="flex min-w-0 flex-1 justify-start">
                  <DepthBarAsk
                    widthPct={w}
                    oursQty={ours}
                    otherQty={other}
                    totalQty={total}
                  />
                </div>
              </div>
            );
          })}
        </div>
      </div>

      <p className="mt-2 font-mono text-[10px] text-terminal-muted">
        <span className="text-profit">■</span> bid depth · <span className="text-loss">■</span> ask depth ·{" "}
        <span className="text-cyan-400">■</span> strategy quote
      </p>
    </div>
  );
}

function DepthBarBid({
  widthPct,
  oursQty,
  otherQty,
  totalQty,
}: {
  widthPct: number;
  oursQty: number;
  otherQty: number;
  totalQty: number;
}) {
  if (totalQty <= 0) {
    return <div className="h-3 w-full" />;
  }
  const po = otherQty / totalQty;
  const pm = oursQty / totalQty;
  return (
    <div
      className="h-3 max-w-full rounded-l-sm bg-terminal-grid/80"
      style={{ width: `${Math.min(100, widthPct)}%` }}
      title={`qty ${totalQty}${oursQty > 0 ? ` · ours ${oursQty}` : ""}`}
    >
      <div className="flex h-full w-full flex-row overflow-hidden rounded-l-sm">
        {po > 0 ? (
          <div
            className="h-full bg-profit/85"
            style={{ width: `${po * 100}%` }}
          />
        ) : null}
        {pm > 0 ? (
          <div
            className="h-full bg-cyan-400/90"
            style={{ width: `${pm * 100}%` }}
          />
        ) : null}
      </div>
    </div>
  );
}

function DepthBarAsk({
  widthPct,
  oursQty,
  otherQty,
  totalQty,
}: {
  widthPct: number;
  oursQty: number;
  otherQty: number;
  totalQty: number;
}) {
  if (totalQty <= 0) {
    return <div className="h-3 w-full" />;
  }
  const po = otherQty / totalQty;
  const pm = oursQty / totalQty;
  return (
    <div
      className="h-3 max-w-full rounded-r-sm bg-terminal-grid/80"
      style={{ width: `${Math.min(100, widthPct)}%` }}
      title={`qty ${totalQty}${oursQty > 0 ? ` · ours ${oursQty}` : ""}`}
    >
      <div className="flex h-full w-full overflow-hidden rounded-r-sm">
        {pm > 0 ? (
          <div
            className="h-full bg-cyan-400/90"
            style={{ width: `${pm * 100}%` }}
          />
        ) : null}
        {po > 0 ? (
          <div
            className="h-full bg-loss/85"
            style={{ width: `${po * 100}%` }}
          />
        ) : null}
      </div>
    </div>
  );
}
