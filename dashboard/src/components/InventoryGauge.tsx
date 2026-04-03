import { useCallback, useMemo, useRef, useState } from "react";

export interface InventoryGaugeProps {
  /** Net position (same scale as simulator `Quantity`). */
  current: number | null;
  /** Time-averaged inventory from analytics. */
  meanInventory: number | null;
  /** Max |inventory| observed (session). */
  maxAbsInventory: number | null;
  /** Risk cap: max |position|; `0` means no limit — scale is inferred. */
  maxPositionLimit: number;
}

function pctFromClientX(clientX: number, rect: DOMRect): number {
  const x = clientX - rect.left;
  const p = (x / rect.width) * 100;
  return Math.min(100, Math.max(0, p));
}

function pctToInventory(pct: number, halfRange: number): number {
  return ((pct / 100) * 2 * halfRange) - halfRange;
}

/**
 * Horizontal gauge from `-range` … `+range` with a marker at `current`.
 * Drag the track to scrub a **preview** position (does not change the simulator).
 * Track gradient: red at limits → yellow at ~half span → green at center (zero).
 */
export function InventoryGauge({
  current,
  meanInventory,
  maxAbsInventory,
  maxPositionLimit,
}: InventoryGaugeProps) {
  const trackRef = useRef<HTMLDivElement>(null);
  /** Dragging / manual scrub position as percent 0–100; null = follow live. */
  const [manualPct, setManualPct] = useState<number | null>(null);
  const draggingRef = useRef(false);

  const halfRange = useMemo(() => {
    if (maxPositionLimit > 0) {
      return maxPositionLimit;
    }
    const c = Math.abs(current ?? 0);
    const obs = maxAbsInventory ?? 0;
    const inferred = Math.max(c, obs, 1);
    return Math.max(1, Math.ceil(inferred * 1.15));
  }, [maxPositionLimit, current, maxAbsInventory]);

  const liveMarkerPct = useMemo(() => {
    if (current === null || !Number.isFinite(current)) return 50;
    const x = (current + halfRange) / (2 * halfRange);
    return Math.min(100, Math.max(0, x * 100));
  }, [current, halfRange]);

  const displayMarkerPct = manualPct ?? liveMarkerPct;
  const previewValue =
    manualPct !== null ? pctToInventory(manualPct, halfRange) : null;

  const updateFromEvent = useCallback(
    (clientX: number) => {
      const el = trackRef.current;
      if (!el) return;
      const rect = el.getBoundingClientRect();
      setManualPct(pctFromClientX(clientX, rect));
    },
    [],
  );

  const onPointerDown = (e: React.PointerEvent) => {
    e.preventDefault();
    (e.currentTarget as HTMLElement).setPointerCapture(e.pointerId);
    draggingRef.current = true;
    updateFromEvent(e.clientX);
  };

  const onPointerMove = (e: React.PointerEvent) => {
    if (!draggingRef.current) return;
    updateFromEvent(e.clientX);
  };

  const onPointerUp = (e: React.PointerEvent) => {
    draggingRef.current = false;
    try {
      (e.currentTarget as HTMLElement).releasePointerCapture(e.pointerId);
    } catch {
      /* ignore */
    }
  };

  const limitLabel =
    maxPositionLimit > 0 ? `±${maxPositionLimit}` : `±${halfRange} (inferred)`;

  const showLiveGhost =
    manualPct !== null &&
    current !== null &&
    Number.isFinite(current) &&
    Math.abs(manualPct - liveMarkerPct) > 0.5;

  return (
    <div className="space-y-3 font-mono">
      <div className="grid grid-cols-3 gap-2 text-center text-[11px]">
        <div>
          <div className="text-[9px] uppercase tracking-wider text-terminal-muted">Current</div>
          <div
            className={`mt-0.5 text-lg tabular-nums ${
              current === null
                ? "text-terminal-muted"
                : current > 0
                  ? "text-profit"
                  : current < 0
                    ? "text-loss"
                    : "text-terminal-fg"
            }`}
          >
            {current === null ? "—" : current}
          </div>
        </div>
        <div>
          <div className="text-[9px] uppercase tracking-wider text-terminal-muted">Avg</div>
          <div className="mt-0.5 text-lg tabular-nums text-terminal-warn">
            {meanInventory === null || !Number.isFinite(meanInventory) ? "—" : meanInventory.toFixed(2)}
          </div>
        </div>
        <div>
          <div className="text-[9px] uppercase tracking-wider text-terminal-muted">Max |inv|</div>
          <div className="mt-0.5 text-lg tabular-nums text-loss">
            {maxAbsInventory === null || !Number.isFinite(maxAbsInventory) ? "—" : maxAbsInventory}
          </div>
        </div>
      </div>

      <div className="relative pt-1">
        <div className="mb-1 flex justify-between text-[9px] text-terminal-muted">
          <span>−{halfRange}</span>
          <span>0</span>
          <span>+{halfRange}</span>
        </div>
        <div
          ref={trackRef}
          role="slider"
          tabIndex={0}
          aria-label={`Inventory scale ${limitLabel}. Drag to preview; does not change simulation.`}
          aria-valuemin={-halfRange}
          aria-valuemax={halfRange}
          aria-valuenow={
            previewValue !== null && Number.isFinite(previewValue)
              ? Math.round(previewValue)
              : current ?? 0
          }
          className="relative h-3 w-full cursor-grab touch-none overflow-visible rounded-full active:cursor-grabbing"
          style={{
            background:
              "linear-gradient(90deg, #ff5252 0%, #ffb300 25%, #00e676 50%, #ffb300 75%, #ff5252 100%)",
            boxShadow: "inset 0 0 0 1px rgba(0,0,0,0.35)",
          }}
          onPointerDown={onPointerDown}
          onPointerMove={onPointerMove}
          onPointerUp={onPointerUp}
          onPointerCancel={onPointerUp}
        >
          {/* Live position (ghost) when scrubbing away from live */}
          {showLiveGhost ? (
            <div
              className="pointer-events-none absolute -top-1 z-[5] flex w-0 flex-col items-center opacity-50"
              style={{ left: `${liveMarkerPct}%`, transform: "translateX(-50%)" }}
              aria-hidden
            >
              <div className="h-0 w-0 border-x-[4px] border-b-[5px] border-x-transparent border-b-terminal-muted" />
              <div className="h-4 w-px bg-terminal-muted" />
            </div>
          ) : null}

          <div
            className="absolute -top-1 z-10 flex w-0 flex-col items-center"
            style={{ left: `${displayMarkerPct}%`, transform: "translateX(-50%)" }}
          >
            <div className="h-0 w-0 border-x-[5px] border-b-[6px] border-x-transparent border-b-terminal-fg" />
            <div className="h-5 w-px bg-terminal-fg" />
          </div>
        </div>

        <div className="mt-0.5 flex min-h-[1.25rem] flex-wrap items-center justify-between gap-2 text-[9px]">
          <span className="text-terminal-muted">
            {manualPct !== null ? (
              <>
                Preview:{" "}
                <span className="tabular-nums text-terminal-accent">
                  {previewValue !== null ? previewValue.toFixed(2) : "—"}
                </span>{" "}
                <span className="text-terminal-muted/80">(not live)</span>
              </>
            ) : (
              <span>Drag the bar to scrub the scale.</span>
            )}
          </span>
          {manualPct !== null ? (
            <button
              type="button"
              className="rounded border border-terminal-border/60 px-2 py-0.5 font-mono text-[9px] text-terminal-muted hover:border-terminal-accent hover:text-terminal-fg"
              onClick={() => setManualPct(null)}
            >
              Follow live
            </button>
          ) : null}
        </div>
      </div>
    </div>
  );
}
