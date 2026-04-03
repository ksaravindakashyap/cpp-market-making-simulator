import { useMemo } from "react";

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

/**
 * Horizontal gauge from `-range` … `+range` with a marker at `current`.
 * Track gradient: red at limits → yellow at ~half span → green at center (zero).
 */
export function InventoryGauge({
  current,
  meanInventory,
  maxAbsInventory,
  maxPositionLimit,
}: InventoryGaugeProps) {
  const halfRange = useMemo(() => {
    if (maxPositionLimit > 0) {
      return maxPositionLimit;
    }
    const c = Math.abs(current ?? 0);
    const obs = maxAbsInventory ?? 0;
    const inferred = Math.max(c, obs, 1);
    return Math.max(1, Math.ceil(inferred * 1.15));
  }, [maxPositionLimit, current, maxAbsInventory]);

  const markerPct = useMemo(() => {
    if (current === null || !Number.isFinite(current)) return 50;
    const x = (current + halfRange) / (2 * halfRange);
    return Math.min(100, Math.max(0, x * 100));
  }, [current, halfRange]);

  const limitLabel =
    maxPositionLimit > 0 ? `±${maxPositionLimit}` : `±${halfRange} (inferred)`;

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
          className="relative h-3 w-full overflow-visible rounded-full"
          style={{
            background:
              "linear-gradient(90deg, #ff5252 0%, #ffb300 25%, #00e676 50%, #ffb300 75%, #ff5252 100%)",
            boxShadow: "inset 0 0 0 1px rgba(0,0,0,0.35)",
          }}
          role="img"
          aria-label={`Inventory ${current ?? "unknown"} on scale ${limitLabel}`}
        >
          <div
            className="absolute -top-1 z-10 flex w-0 flex-col items-center"
            style={{ left: `${markerPct}%`, transform: "translateX(-50%)" }}
          >
            <div className="h-0 w-0 border-x-[5px] border-b-[6px] border-x-transparent border-b-terminal-fg" />
            <div className="h-5 w-px bg-terminal-fg" />
          </div>
        </div>
      </div>
    </div>
  );
}
