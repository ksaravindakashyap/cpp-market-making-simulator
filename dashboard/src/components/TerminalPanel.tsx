import type { ReactNode } from "react";

interface TerminalPanelProps {
  title: string;
  subtitle?: string;
  children: ReactNode;
  className?: string;
}

export function TerminalPanel({ title, subtitle, children, className = "" }: TerminalPanelProps) {
  return (
    <section
      className={`flex min-h-0 flex-col rounded border border-terminal-border bg-terminal-panel shadow-panel ${className}`}
    >
      <header className="flex shrink-0 items-baseline justify-between border-b border-terminal-border px-3 py-2">
        <h2 className="font-sans text-xs font-semibold uppercase tracking-widest text-terminal-accent">{title}</h2>
        {subtitle ? (
          <span className="font-mono text-[10px] text-terminal-muted">{subtitle}</span>
        ) : null}
      </header>
      <div className="min-h-0 flex-1 overflow-auto p-3 scroll-terminal">{children}</div>
    </section>
  );
}
