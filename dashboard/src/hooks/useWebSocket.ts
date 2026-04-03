import { useCallback, useEffect, useMemo, useReducer, useRef } from "react";

import type {
  AnalyticsData,
  BookData,
  ComparisonData,
  ComparisonRunMetrics,
  DashboardAction,
  DashboardState,
  PnlData,
  StrategyData,
  TradeLogEntry,
  VolatilityData,
} from "@/types/ws";

const DEFAULT_URL = "ws://localhost:8080";
const MAX_TRADES = 100;
const MAX_PNL_POINTS = 480;

function isRecord(x: unknown): x is Record<string, unknown> {
  return typeof x === "object" && x !== null && !Array.isArray(x);
}

function parseBook(data: unknown): BookData | null {
  if (!isRecord(data)) return null;
  const bids = data.bids;
  const asks = data.asks;
  if (!Array.isArray(bids) || !Array.isArray(asks)) return null;
  return { bids: bids as BookData["bids"], asks: asks as BookData["asks"] };
}

function parsePnl(data: unknown): PnlData | null {
  if (!isRecord(data)) return null;
  return {
    position: Number(data.position),
    realized_pnl: Number(data.realized_pnl),
    unrealized_pnl: Number(data.unrealized_pnl),
    equity: Number(data.equity),
    max_drawdown: Number(data.max_drawdown),
    max_position: data.max_position === undefined ? 0 : Number(data.max_position),
  };
}

function parseStrategy(data: unknown): StrategyData | null {
  if (!isRecord(data)) return null;
  return {
    bid: Number(data.bid),
    ask: Number(data.ask),
    inventory: Number(data.inventory),
    sigma: Number(data.sigma),
    gamma: Number(data.gamma),
    kappa: Number(data.kappa),
    bid_order_id:
      data.bid_order_id === null || data.bid_order_id === undefined ? null : Number(data.bid_order_id),
    ask_order_id:
      data.ask_order_id === null || data.ask_order_id === undefined ? null : Number(data.ask_order_id),
  };
}

function parseAnalytics(data: unknown): AnalyticsData | null {
  if (!isRecord(data)) return null;
  return {
    position: Number(data.position),
    realized_pnl: Number(data.realized_pnl),
    unrealized_pnl: Number(data.unrealized_pnl),
    equity: Number(data.equity),
    max_drawdown: Number(data.max_drawdown),
    fill_rate: Number(data.fill_rate),
    average_spread_captured: Number(data.average_spread_captured),
    mean_inventory: Number(data.mean_inventory),
    variance_inventory: Number(data.variance_inventory),
    max_abs_inventory: Number(data.max_abs_inventory),
    orders_submitted: Number(data.orders_submitted),
    fills_recorded: Number(data.fills_recorded),
    sharpe_ratio:
      data.sharpe_ratio === null || data.sharpe_ratio === undefined
        ? null
        : Number(data.sharpe_ratio),
    sortino_ratio:
      data.sortino_ratio === null || data.sortino_ratio === undefined
        ? null
        : Number(data.sortino_ratio),
  };
}

function parseTradeLogEntry(data: unknown): TradeLogEntry | null {
  if (!isRecord(data)) return null;
  const sideRaw = data.side;
  const side: "BUY" | "SELL" = sideRaw === "SELL" ? "SELL" : "BUY";
  return {
    order_id: Number(data.order_id),
    price: Number(data.price),
    quantity: Number(data.quantity),
    fee: Number(data.fee),
    side,
    timestamp_ns:
      data.timestamp_ns === undefined || data.timestamp_ns === null
        ? Math.round(Date.now() * 1e6)
        : Number(data.timestamp_ns),
    inventory_after:
      data.inventory_after === undefined || data.inventory_after === null
        ? 0
        : Number(data.inventory_after),
  };
}

function numOrNull(x: unknown): number | null {
  if (x === null || x === undefined) return null;
  const n = Number(x);
  return Number.isFinite(n) ? n : null;
}

function parseVolatility(data: unknown): VolatilityData | null {
  if (!isRecord(data)) return null;
  const hist = isRecord(data.history) ? data.history : {};
  const arr = (k: string): number[] => {
    const v = hist[k];
    if (!Array.isArray(v)) return [];
    return v.map((x) => Number(x)).filter((x) => Number.isFinite(x));
  };
  return {
    close_to_close: numOrNull(data.close_to_close),
    parkinson: numOrNull(data.parkinson),
    yang_zhang: numOrNull(data.yang_zhang),
    history: {
      close_to_close: arr("close_to_close"),
      parkinson: arr("parkinson"),
      yang_zhang: arr("yang_zhang"),
    },
  };
}

function parseComparisonRunMetrics(x: unknown): ComparisonRunMetrics | null {
  if (!isRecord(x)) return null;
  return {
    ticks: Number(x.ticks),
    equity: Number(x.equity),
    realized_pnl: Number(x.realized_pnl),
    unrealized_pnl: Number(x.unrealized_pnl),
    max_drawdown: Number(x.max_drawdown),
    sharpe_ratio: x.sharpe_ratio === null || x.sharpe_ratio === undefined ? null : Number(x.sharpe_ratio),
    sortino_ratio: x.sortino_ratio === null || x.sortino_ratio === undefined ? null : Number(x.sortino_ratio),
    mean_inventory: Number(x.mean_inventory),
    variance_inventory: Number(x.variance_inventory),
    max_abs_inventory: Number(x.max_abs_inventory),
    final_position: Number(x.final_position),
  };
}

function parseComparison(data: unknown): ComparisonData | null {
  if (!isRecord(data)) return null;
  if (typeof data.error === "string") {
    return { error: data.error };
  }
  const as = parseComparisonRunMetrics(data.avellaneda_stoikov);
  const nv = parseComparisonRunMetrics(data.fixed_spread);
  const out: ComparisonData = {};
  if (typeof data.markdown === "string") out.markdown = data.markdown;
  if (typeof data.ticks === "number") out.ticks = data.ticks;
  if (typeof data.fixed_half_spread === "number") out.fixed_half_spread = data.fixed_half_spread;
  if (as) out.avellaneda_stoikov = as;
  if (nv) out.fixed_spread = nv;
  if (!out.markdown && !out.error && !as && !nv) return null;
  return out;
}

const initialState: DashboardState = {
  connection: "idle",
  lastError: null,
  reconnectAttempt: 0,
  book: null,
  pnl: null,
  strategy: null,
  analytics: null,
  trades: [],
  pnlHistory: [],
  volatility: null,
  comparison: null,
  rawMessageCount: 0,
  lastMessageAt: null,
};

function pushPnlSample(
  history: DashboardState["pnlHistory"],
  realized: number,
  total: number,
): DashboardState["pnlHistory"] {
  const t = Date.now();
  const next = [...history, { t, realized, total }];
  if (next.length > MAX_PNL_POINTS) {
    return next.slice(next.length - MAX_PNL_POINTS);
  }
  return next;
}

function dashboardReducer(state: DashboardState, action: DashboardAction): DashboardState {
  switch (action.type) {
    case "WS_STATUS":
      return {
        ...state,
        connection: action.status,
        lastError: action.error ?? null,
      };
    case "WS_RECONNECT":
      return { ...state, reconnectAttempt: action.attempt };
    case "WS_MESSAGE": {
      const now = Date.now();
      const ch = action.channel;
      const data = action.data;
      const base = {
        ...state,
        rawMessageCount: state.rawMessageCount + 1,
        lastMessageAt: now,
      };

      switch (ch) {
        case "book": {
          const book = parseBook(data);
          return book ? { ...base, book } : base;
        }
        case "pnl": {
          const pnl = parsePnl(data);
          if (!pnl) return base;
          return {
            ...base,
            pnl,
            pnlHistory: pushPnlSample(base.pnlHistory, pnl.realized_pnl, pnl.equity),
          };
        }
        case "strategy": {
          const strategy = parseStrategy(data);
          return strategy ? { ...base, strategy } : base;
        }
        case "analytics": {
          const analytics = parseAnalytics(data);
          if (!analytics) return base;
          return {
            ...base,
            analytics,
          };
        }
        case "volatility": {
          const vol = parseVolatility(data);
          return vol ? { ...base, volatility: vol } : base;
        }
        case "trades": {
          const row = parseTradeLogEntry(data);
          if (!row) return base;
          const trades = [...base.trades, row];
          return {
            ...base,
            trades: trades.length > MAX_TRADES ? trades.slice(trades.length - MAX_TRADES) : trades,
          };
        }
        case "comparison": {
          const cmp = parseComparison(data);
          return cmp ? { ...base, comparison: cmp } : base;
        }
        default:
          return base;
      }
    }
    default:
      return state;
  }
}

export interface UseWebSocketOptions {
  url?: string;
  connect?: boolean;
  reconnectBaseDelayMs?: number;
  reconnectMaxDelayMs?: number;
}

export function useWebSocket(opts: UseWebSocketOptions = {}) {
  const {
    url = DEFAULT_URL,
    connect: shouldConnect = true,
    reconnectBaseDelayMs = 800,
    reconnectMaxDelayMs = 30_000,
  } = opts;

  const [state, dispatch] = useReducer(dashboardReducer, initialState);
  const wsRef = useRef<WebSocket | null>(null);
  const reconnectTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  const attemptRef = useRef(0);
  const closedCleanlyRef = useRef(false);
  const optsRef = useRef({ reconnectBaseDelayMs, reconnectMaxDelayMs, shouldConnect });
  optsRef.current = { reconnectBaseDelayMs, reconnectMaxDelayMs, shouldConnect };

  const clearReconnectTimer = useCallback(() => {
    if (reconnectTimerRef.current !== null) {
      clearTimeout(reconnectTimerRef.current);
      reconnectTimerRef.current = null;
    }
  }, []);

  const scheduleReconnect = useCallback(() => {
    clearReconnectTimer();
    const { reconnectBaseDelayMs: base, reconnectMaxDelayMs: max, shouldConnect: sc } = optsRef.current;
    if (!sc) return;
    attemptRef.current += 1;
    const exp = Math.min(max, base * 2 ** Math.min(attemptRef.current - 1, 10));
    const jitter = exp * (0.85 + Math.random() * 0.15);
    dispatch({ type: "WS_RECONNECT", attempt: attemptRef.current });
    reconnectTimerRef.current = setTimeout(() => {
      openSocketRef.current?.();
    }, jitter);
  }, [clearReconnectTimer]);

  const openSocketRef = useRef<() => void>(() => {});

  const openSocket = useCallback(() => {
    clearReconnectTimer();
    const targetUrl = url;
    closedCleanlyRef.current = false;
    if (wsRef.current?.readyState === WebSocket.OPEN) {
      return;
    }
    dispatch({ type: "WS_STATUS", status: "connecting" });
    try {
      const ws = new WebSocket(targetUrl);
      wsRef.current = ws;

      ws.onopen = () => {
        attemptRef.current = 0;
        dispatch({ type: "WS_STATUS", status: "open", error: null });
      };

      ws.onmessage = (ev) => {
        try {
          const parsed: unknown = JSON.parse(ev.data as string);
          if (!isRecord(parsed)) return;
          const channel = parsed.channel;
          if (typeof channel !== "string") return;
          if (channel === "control") return;
          dispatch({
            type: "WS_MESSAGE",
            channel,
            data: parsed.data,
          });
        } catch {
          dispatch({
            type: "WS_STATUS",
            status: "error",
            error: "Invalid JSON from server",
          });
        }
      };

      ws.onerror = () => {
        dispatch({ type: "WS_STATUS", status: "error", error: "WebSocket error" });
      };

      ws.onclose = () => {
        wsRef.current = null;
        dispatch({
          type: "WS_STATUS",
          status: "closed",
          error: closedCleanlyRef.current ? null : "Disconnected",
        });
        if (!closedCleanlyRef.current && optsRef.current.shouldConnect) {
          scheduleReconnect();
        }
      };
    } catch (e) {
      dispatch({
        type: "WS_STATUS",
        status: "error",
        error: e instanceof Error ? e.message : "Failed to connect",
      });
      if (optsRef.current.shouldConnect) {
        scheduleReconnect();
      }
    }
  }, [url, clearReconnectTimer, scheduleReconnect]);

  openSocketRef.current = openSocket;

  const disconnect = useCallback(() => {
    closedCleanlyRef.current = true;
    clearReconnectTimer();
    const w = wsRef.current;
    wsRef.current = null;
    if (w && (w.readyState === WebSocket.OPEN || w.readyState === WebSocket.CONNECTING)) {
      w.close();
    }
  }, [clearReconnectTimer]);

  const sendJson = useCallback((payload: unknown) => {
    const w = wsRef.current;
    if (!w || w.readyState !== WebSocket.OPEN) {
      return false;
    }
    try {
      w.send(JSON.stringify(payload));
      return true;
    } catch {
      return false;
    }
  }, []);

  useEffect(() => {
    if (!shouldConnect) {
      return;
    }
    closedCleanlyRef.current = false;
    openSocket();
    return () => {
      closedCleanlyRef.current = true;
      clearReconnectTimer();
      const w = wsRef.current;
      wsRef.current = null;
      if (w && (w.readyState === WebSocket.OPEN || w.readyState === WebSocket.CONNECTING)) {
        w.close();
      }
    };
  }, [shouldConnect, url, openSocket, clearReconnectTimer]);

  const api = useMemo(
    () => ({
      state,
      sendJson,
      reconnect: () => {
        closedCleanlyRef.current = false;
        disconnect();
        attemptRef.current = 0;
        setTimeout(() => openSocket(), 0);
      },
      disconnect,
    }),
    [state, sendJson, disconnect, openSocket],
  );

  return api;
}
