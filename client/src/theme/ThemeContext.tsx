import { createContext, useContext, useEffect, useState, type ReactNode } from "react";

export type ThemeId = "evening" | "dawn" | "ocean" | "forest";

export interface Theme {
  id: ThemeId;
  label: string;
  emoji: string;
  // body의 그라데이션 (위→아래)
  bgGradient: string;
}

export const THEMES: Theme[] = [
  {
    id: "evening",
    label: "저녁하늘",
    emoji: "🌆",
    bgGradient:
      "linear-gradient(180deg, hsl(232 55% 18%) 0%, hsl(228 55% 32%) 22%, hsl(224 55% 52%) 50%, hsl(220 70% 75%) 78%, hsl(218 80% 90%) 100%)",
  },
  {
    id: "dawn",
    label: "새벽노을",
    emoji: "🌅",
    bgGradient:
      "linear-gradient(180deg, hsl(248 45% 22%) 0%, hsl(290 45% 38%) 25%, hsl(330 65% 60%) 55%, hsl(20 90% 75%) 82%, hsl(45 95% 90%) 100%)",
  },
  {
    id: "ocean",
    label: "심해",
    emoji: "🌊",
    bgGradient:
      "linear-gradient(180deg, hsl(220 60% 12%) 0%, hsl(200 70% 25%) 30%, hsl(190 60% 40%) 60%, hsl(180 55% 65%) 85%, hsl(170 60% 88%) 100%)",
  },
  {
    id: "forest",
    label: "숲속새벽",
    emoji: "🌲",
    bgGradient:
      "linear-gradient(180deg, hsl(200 30% 15%) 0%, hsl(170 35% 25%) 25%, hsl(140 40% 40%) 55%, hsl(110 50% 65%) 80%, hsl(90 60% 88%) 100%)",
  },
];

interface ThemeState {
  theme: Theme;
  setTheme: (id: ThemeId) => void;
}

const Ctx = createContext<ThemeState | null>(null);
const STORAGE_KEY = "monggle_theme";

export function ThemeProvider({ children }: { children: ReactNode }) {
  const [themeId, setThemeId] = useState<ThemeId>(() => {
    const saved = localStorage.getItem(STORAGE_KEY) as ThemeId | null;
    if (saved && THEMES.some((t) => t.id === saved)) return saved;
    return "evening";
  });

  const theme = THEMES.find((t) => t.id === themeId) ?? THEMES[0];

  useEffect(() => {
    document.body.style.backgroundImage = theme.bgGradient;
    document.body.style.backgroundAttachment = "fixed";
    document.body.style.backgroundSize = "100% 100%";
    localStorage.setItem(STORAGE_KEY, themeId);
  }, [theme, themeId]);

  return (
    <Ctx.Provider value={{ theme, setTheme: setThemeId }}>
      {children}
    </Ctx.Provider>
  );
}

export function useTheme() {
  const ctx = useContext(Ctx);
  if (!ctx) throw new Error("useTheme must be used within ThemeProvider");
  return ctx;
}
