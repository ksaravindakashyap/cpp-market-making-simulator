/** @type {import('tailwindcss').Config} */
export default {
  content: ["./index.html", "./src/**/*.{js,ts,jsx,tsx}"],
  theme: {
    extend: {
      fontFamily: {
        sans: ['"IBM Plex Sans"', "system-ui", "sans-serif"],
        mono: ['"IBM Plex Mono"', "ui-monospace", "monospace"],
      },
      colors: {
        terminal: {
          bg: "#0a0e0d",
          panel: "#0f1513",
          border: "#1c2824",
          fg: "#c8e6d8",
          muted: "#5a7a6e",
          accent: "#00c853",
          warn: "#ffb300",
          grid: "#14221c",
        },
        profit: {
          DEFAULT: "#00e676",
          dim: "#004d2a",
        },
        loss: {
          DEFAULT: "#ff5252",
          dim: "#4a1515",
        },
      },
      boxShadow: {
        panel: "inset 0 1px 0 0 rgba(0, 255, 136, 0.06)",
      },
    },
  },
  plugins: [],
};
