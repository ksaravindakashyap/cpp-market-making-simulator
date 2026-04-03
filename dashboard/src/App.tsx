import { ComparisonReport } from "@/components/ComparisonReport";
import { ControlPanel } from "@/components/ControlPanel";
import { InventoryGauge } from "@/components/InventoryGauge";
import { MetricsBar } from "@/components/MetricsBar";
import { MonospaceNum } from "@/components/MonospaceNum";
import { OrderBookView } from "@/components/OrderBookView";
import { PnLChart } from "@/components/PnLChart";
import { TradeLog } from "@/components/TradeLog";
import { TerminalPanel } from "@/components/TerminalPanel";
import { VolatilityPanel } from "@/components/VolatilityPanel";
import { useWebSocket } from "@/hooks/useWebSocket";

export default function App() {
  const { state, sendJson, reconnect } = useWebSocket({ url: "ws://localhost:8080" });

  const pnl = state.pnl;
  const strat = state.strategy;
  const book = state.book;
  const analytics = state.analytics;

  return (
    <div className="flex min-h-screen flex-col bg-terminal-bg bg-[radial-gradient(ellipse_at_top,_#0f1814_0%,_#0a0e0d_55%)]">
      <MetricsBar
        pnl={pnl}
        strategy={strat}
        analytics={analytics}
        connection={state.connection}
        reconnectAttempt={state.reconnectAttempt}
        onReconnect={reconnect}
      />

      <main className="mx-auto grid w-full max-w-[1800px] flex-1 grid-cols-1 content-start gap-3 p-3 sm:p-4 lg:grid-cols-2 lg:gap-4 lg:p-5">
        {/* Middle row: order book + PnL (side by side on lg+) */}
        <TerminalPanel
          title="Depth"
          subtitle="book · bid L / ask R · cyan = quote"
          className="flex min-h-[260px] flex-col sm:min-h-[300px] lg:min-h-[360px]"
        >
          <div className="min-h-0 flex-1">
            <OrderBookView
              book={book}
              strategyBidOrderId={strat?.bid_order_id ?? null}
              strategyAskOrderId={strat?.ask_order_id ?? null}
            />
          </div>
        </TerminalPanel>

        <TerminalPanel
          title="Cumulative PnL"
          subtitle="realized (solid) · total (dashed)"
          className="flex min-h-[260px] flex-col sm:min-h-[300px] lg:min-h-[360px]"
        >
          <p className="mb-2 shrink-0 font-mono text-[10px] text-terminal-muted">
            Colors follow sign (last sample). Analytics reference line when available.
          </p>
          <div className="min-h-[200px] flex-1 sm:min-h-[220px]">
            <PnLChart history={state.pnlHistory} pnl={pnl} analytics={analytics} />
          </div>
        </TerminalPanel>

        {/* Lower row: inventory + volatility */}
        <TerminalPanel title="Inventory" subtitle="position vs cap · session stats" className="min-h-[200px] lg:min-h-[240px]">
          <InventoryGauge
            current={pnl?.position ?? strat?.inventory ?? null}
            meanInventory={analytics?.mean_inventory ?? null}
            maxAbsInventory={analytics?.max_abs_inventory ?? null}
            maxPositionLimit={pnl?.max_position ?? 0}
          />
          {analytics ? (
            <dl className="mt-4 grid grid-cols-2 gap-x-3 gap-y-1.5 border-t border-terminal-border pt-3 font-mono text-[10px] text-terminal-muted sm:text-[11px]">
              <dt>Mean inv.</dt>
              <dd className="text-right text-terminal-fg">
                <MonospaceNum value={analytics.mean_inventory} fractionDigits={4} />
              </dd>
              <dt>Max |inv|</dt>
              <dd className="text-right text-terminal-fg">{analytics.max_abs_inventory.toFixed(2)}</dd>
              <dt>Spread capture</dt>
              <dd className="text-right">
                <MonospaceNum value={analytics.average_spread_captured} fractionDigits={4} />
              </dd>
              <dt>Orders / fills</dt>
              <dd className="text-right text-terminal-fg">
                {analytics.orders_submitted} / {analytics.fills_recorded}
              </dd>
            </dl>
          ) : null}
        </TerminalPanel>

        <TerminalPanel title="Volatility" subtitle="CtC · Parkinson · Yang–Zhang · ~100ms" className="min-h-[200px] lg:min-h-[240px]">
          <VolatilityPanel data={state.volatility} />
        </TerminalPanel>

        {/* Bottom row: trade log + controls */}
        <TerminalPanel
          title="Trades"
          subtitle="last 100 · auto-scroll"
          className="min-h-[220px] lg:min-h-[280px] lg:max-h-[min(420px,45vh)]"
        >
          <TradeLog trades={state.trades} />
        </TerminalPanel>

        <TerminalPanel title="Control" subtitle="channel: control · WebSocket" className="min-h-[220px] lg:min-h-[280px]">
          <ControlPanel
            sendJson={sendJson}
            rawMessageCount={state.rawMessageCount}
            lastMessageAt={state.lastMessageAt}
          />
        </TerminalPanel>

        <TerminalPanel
          title="Strategy comparison"
          subtitle="offline replay · same CSV · Avellaneda–Stoikov vs fixed spread"
          className="lg:col-span-2"
        >
          <ComparisonReport data={state.comparison} />
        </TerminalPanel>
      </main>
    </div>
  );
}
