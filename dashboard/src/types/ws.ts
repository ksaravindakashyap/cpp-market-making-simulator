/** Aligns with mmsim_ws_server JSON payloads (`channel` + `data`). */

export type WsChannel =
  | "book"
  | "trades"
  | "pnl"
  | "strategy"
  | "analytics"
  | "volatility"
  | "comparison"
  | "control";

export interface WsEnvelope<T = unknown> {
  channel: WsChannel | string;
  data: T;
}

export interface BookLevel {
  price: number;
  orders: { order_id: number; quantity: number }[];
}

export interface BookData {
  bids: BookLevel[];
  asks: BookLevel[];
}

/** One row in the trade log (`trades` WebSocket channel). */
export interface TradeLogEntry {
  order_id: number;
  price: number;
  quantity: number;
  fee: number;
  side: "BUY" | "SELL";
  /** Simulation clock (nanoseconds), same as tick CSV. */
  timestamp_ns: number;
  /** Net inventory after this fill. */
  inventory_after: number;
}

export interface PnlData {
  position: number;
  realized_pnl: number;
  unrealized_pnl: number;
  equity: number;
  max_drawdown: number;
  /** Risk limit: max |inventory|; `0` means no limit (UI may infer a scale). */
  max_position: number;
}

export interface StrategyData {
  bid: number;
  ask: number;
  inventory: number;
  sigma: number;
  gamma: number;
  kappa: number;
  /** Resting quote order IDs from the simulator (for depth highlight). */
  bid_order_id: number | null;
  ask_order_id: number | null;
}

export interface AnalyticsData {
  position: number;
  realized_pnl: number;
  unrealized_pnl: number;
  equity: number;
  max_drawdown: number;
  fill_rate: number;
  average_spread_captured: number;
  mean_inventory: number;
  variance_inventory: number;
  max_abs_inventory: number;
  orders_submitted: number;
  fills_recorded: number;
  sharpe_ratio: number | null;
  sortino_ratio: number | null;
}

/** Rolling vol estimators (per-bar units); `history` is recent samples for sparklines. */
export interface VolatilityData {
  close_to_close: number | null;
  parkinson: number | null;
  yang_zhang: number | null;
  history: {
    close_to_close: number[];
    parkinson: number[];
    yang_zhang: number[];
  };
}

/** Cumulative PnL samples (from `pnl` channel, ~100ms). */
export interface PnlHistoryPoint {
  t: number;
  realized: number;
  /** Realized + unrealized (equity). */
  total: number;
}

/** One side of the offline A/B strategy comparison (`comparison` channel). */
export interface ComparisonRunMetrics {
  ticks: number;
  equity: number;
  realized_pnl: number;
  unrealized_pnl: number;
  max_drawdown: number;
  sharpe_ratio: number | null;
  sortino_ratio: number | null;
  mean_inventory: number;
  variance_inventory: number;
  max_abs_inventory: number;
  final_position: number;
}

export interface ComparisonData {
  error?: string;
  markdown?: string;
  avellaneda_stoikov?: ComparisonRunMetrics;
  fixed_spread?: ComparisonRunMetrics;
  ticks?: number;
  fixed_half_spread?: number;
}

export interface DashboardState {
  connection: "idle" | "connecting" | "open" | "closed" | "error";
  lastError: string | null;
  reconnectAttempt: number;
  book: BookData | null;
  pnl: PnlData | null;
  strategy: StrategyData | null;
  analytics: AnalyticsData | null;
  trades: TradeLogEntry[];
  pnlHistory: PnlHistoryPoint[];
  volatility: VolatilityData | null;
  /** Latest offline Avellaneda–Stoikov vs fixed-spread report. */
  comparison: ComparisonData | null;
  rawMessageCount: number;
  lastMessageAt: number | null;
}

export type DashboardAction =
  | { type: "WS_STATUS"; status: DashboardState["connection"]; error?: string | null }
  | { type: "WS_MESSAGE"; channel: string; data: unknown }
  | { type: "WS_RECONNECT"; attempt: number };
