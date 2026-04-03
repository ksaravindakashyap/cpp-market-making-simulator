import { useCallback, useState } from "react";

export interface ControlPanelProps {
  sendJson: (payload: unknown) => boolean;
  rawMessageCount: number;
  lastMessageAt: number | null;
}

const SPEED_OPTIONS = [
  { label: "1×", value: "1x" },
  { label: "5×", value: "5x" },
  { label: "10×", value: "10x" },
  { label: "50×", value: "50x" },
  { label: "Max", value: "max" },
] as const;

function clamp(n: number, lo: number, hi: number): number {
  return Math.min(hi, Math.max(lo, n));
}

export function ControlPanel({ sendJson, rawMessageCount, lastMessageAt }: ControlPanelProps) {
  const [gamma, setGamma] = useState(0.5);
  const [kappa, setKappa] = useState(1.0);
  const [volWindow, setVolWindow] = useState(32);
  const [maxPosition, setMaxPosition] = useState(100);
  const [fixedHalfSpread, setFixedHalfSpread] = useState(100);
  const [speed, setSpeed] = useState<(typeof SPEED_OPTIONS)[number]["value"]>("max");

  const pushParams = useCallback(
    (next: { gamma?: number; kappa?: number; vol_window?: number; max_position?: number }) => {
      const g = next.gamma ?? gamma;
      const k = next.kappa ?? kappa;
      const vw = next.vol_window ?? volWindow;
      const mp = next.max_position ?? maxPosition;
      sendJson({
        channel: "control",
        command: "set_params",
        gamma: clamp(g, 0.001, 1.0),
        kappa: clamp(k, 0.1, 10.0),
        vol_window: Math.round(clamp(vw, 10, 500)),
        max_position: Math.round(clamp(mp, 10, 200)),
      });
    },
    [gamma, kappa, volWindow, maxPosition, sendJson],
  );

  const onGamma = (v: number) => {
    setGamma(v);
    pushParams({ gamma: v });
  };
  const onKappa = (v: number) => {
    setKappa(v);
    pushParams({ kappa: v });
  };
  const onVolWindow = (v: number) => {
    setVolWindow(v);
    pushParams({ vol_window: v });
  };
  const onMaxPosition = (v: number) => {
    setMaxPosition(v);
    pushParams({ max_position: v });
  };

  const onSpeedChange = (v: (typeof SPEED_OPTIONS)[number]["value"]) => {
    setSpeed(v);
    sendJson({ channel: "control", command: "set_speed", speed: v });
  };

  return (
    <div className="space-y-4">
      <div className="grid gap-4 sm:grid-cols-2 lg:grid-cols-4">
        <label className="block font-mono text-[11px] text-terminal-muted">
          <span className="mb-1 flex justify-between text-terminal-fg">
            <span>γ (gamma)</span>
            <span className="tabular-nums text-terminal-accent">{gamma.toFixed(3)}</span>
          </span>
          <input
            type="range"
            min={0.001}
            max={1}
            step={0.001}
            value={gamma}
            onChange={(e) => onGamma(Number(e.target.value))}
            className="mt-1 w-full accent-terminal-accent"
          />
        </label>
        <label className="block font-mono text-[11px] text-terminal-muted">
          <span className="mb-1 flex justify-between text-terminal-fg">
            <span>κ (kappa)</span>
            <span className="tabular-nums text-terminal-accent">{kappa.toFixed(2)}</span>
          </span>
          <input
            type="range"
            min={0.1}
            max={10}
            step={0.1}
            value={kappa}
            onChange={(e) => onKappa(Number(e.target.value))}
            className="mt-1 w-full accent-terminal-accent"
          />
        </label>
        <label className="block font-mono text-[11px] text-terminal-muted">
          <span className="mb-1 flex justify-between text-terminal-fg">
            <span>Vol window</span>
            <span className="tabular-nums text-terminal-accent">{volWindow}</span>
          </span>
          <input
            type="range"
            min={10}
            max={500}
            step={1}
            value={volWindow}
            onChange={(e) => onVolWindow(Number(e.target.value))}
            className="mt-1 w-full accent-terminal-accent"
          />
        </label>
        <label className="block font-mono text-[11px] text-terminal-muted">
          <span className="mb-1 flex justify-between text-terminal-fg">
            <span>Max inventory</span>
            <span className="tabular-nums text-terminal-accent">{maxPosition}</span>
          </span>
          <input
            type="range"
            min={10}
            max={200}
            step={1}
            value={maxPosition}
            onChange={(e) => onMaxPosition(Number(e.target.value))}
            className="mt-1 w-full accent-terminal-accent"
          />
        </label>
      </div>

      <div className="flex flex-col gap-3 border-t border-terminal-border pt-3 sm:flex-row sm:flex-wrap sm:items-end">
        <label className="font-mono text-[11px] text-terminal-muted">
          <span className="mb-1 block text-terminal-fg">Fixed half-spread (A/B baseline)</span>
          <input
            type="number"
            min={1}
            max={100000}
            value={fixedHalfSpread}
            onChange={(e) => {
              const v = Math.round(Number(e.target.value));
              if (!Number.isFinite(v)) return;
              const c = Math.min(100000, Math.max(1, v));
              setFixedHalfSpread(c);
              sendJson({
                channel: "control",
                command: "set_params",
                fixed_half_spread: c,
              });
            }}
            className="w-full min-w-[6rem] rounded border border-terminal-border bg-terminal-bg px-2 py-1.5 font-mono text-xs text-terminal-fg sm:w-28"
          />
        </label>
        <button
          type="button"
          className="rounded border border-terminal-accent/50 bg-terminal-bg px-3 py-1.5 font-mono text-xs text-terminal-accent hover:bg-terminal-accent/10"
          onClick={() =>
            sendJson({
              channel: "control",
              command: "run_comparison",
              fixed_half_spread: fixedHalfSpread,
            })
          }
        >
          Run A/B comparison
        </button>
      </div>

      <div className="flex flex-wrap items-end gap-3">
        <label className="font-mono text-[11px] text-terminal-muted">
          <span className="mb-1 block text-terminal-fg">Replay speed</span>
          <select
            value={speed}
            onChange={(e) => onSpeedChange(e.target.value as (typeof SPEED_OPTIONS)[number]["value"])}
            className="rounded border border-terminal-border bg-terminal-bg px-2 py-1.5 font-mono text-xs text-terminal-fg hover:border-terminal-accent"
          >
            {SPEED_OPTIONS.map((o) => (
              <option key={o.value} value={o.value}>
                {o.label}
              </option>
            ))}
          </select>
        </label>

        <div className="flex flex-wrap gap-2">
          <button
            type="button"
            className="rounded border border-terminal-border px-3 py-1.5 font-mono text-xs text-profit hover:bg-profit-dim/30"
            onClick={() => sendJson({ channel: "control", command: "start" })}
          >
            Start
          </button>
          <button
            type="button"
            className="rounded border border-terminal-border px-3 py-1.5 font-mono text-xs text-loss hover:bg-loss-dim/30"
            onClick={() => sendJson({ channel: "control", command: "stop" })}
          >
            Stop
          </button>
          <button
            type="button"
            className="rounded border border-terminal-border px-3 py-1.5 font-mono text-xs text-terminal-fg hover:border-terminal-accent"
            onClick={() => sendJson({ channel: "control", command: "reset" })}
          >
            Reset
          </button>
        </div>
      </div>

      <div className="flex flex-wrap items-center justify-between gap-2 border-t border-terminal-border pt-3">
        <p className="font-mono text-[10px] text-terminal-muted">
          msgs in: {rawMessageCount}
          {lastMessageAt ? ` · last ${new Date(lastMessageAt).toLocaleTimeString()}` : ""}
        </p>
        <button
          type="button"
          className="rounded border border-terminal-border/60 px-2 py-1 font-mono text-[10px] text-terminal-muted hover:text-terminal-fg"
          onClick={() => sendJson({ channel: "control", command: "shutdown" })}
        >
          shutdown sim
        </button>
      </div>
    </div>
  );
}
