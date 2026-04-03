interface MonospaceNumProps {
  value: number;
  /** When true, positive → profit color, negative → loss (zero = muted). */
  signedColor?: boolean;
  fractionDigits?: number;
  className?: string;
}

export function MonospaceNum({
  value,
  signedColor = true,
  fractionDigits = 2,
  className = "",
}: MonospaceNumProps) {
  const formatted =
    Number.isFinite(value) ? value.toLocaleString(undefined, { maximumFractionDigits: fractionDigits }) : "—";

  let color = "text-terminal-muted";
  if (signedColor && Number.isFinite(value)) {
    if (value > 0) color = "text-profit";
    else if (value < 0) color = "text-loss";
    else color = "text-terminal-muted";
  }

  return (
    <span className={`font-mono tabular-nums tracking-tight ${color} ${className}`}>{formatted}</span>
  );
}
