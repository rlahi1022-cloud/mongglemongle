import { createContext, useContext, useEffect, useState, type ReactNode } from "react";
import { auth, me as meApi, tokens } from "@/api/client";

interface AuthState {
  userId: number | null;
  displayName: string | null;
  email: string | null;
  loading: boolean;
  signup: (email: string, password: string, displayName: string) => Promise<void>;
  login: (email: string, password: string) => Promise<void>;
  logout: () => Promise<void>;
  refreshProfile: () => Promise<void>;
}

const Ctx = createContext<AuthState | null>(null);

const NAME_KEY = "monggle_display_name";
const EMAIL_KEY = "monggle_email";

export function AuthProvider({ children }: { children: ReactNode }) {
  const [userId, setUserId] = useState<number | null>(tokens.userId);
  const [displayName, setDisplayName] = useState<string | null>(
    localStorage.getItem(NAME_KEY)
  );
  const [email, setEmail] = useState<string | null>(localStorage.getItem(EMAIL_KEY));
  const [loading, setLoading] = useState(false);

  // 프로필 동기화 — 토큰만 있으면 매번 한 번 갱신
  const refreshProfile = async () => {
    if (!tokens.access) return;
    try {
      const p = await meApi.whoami();
      setUserId(p.user_id);
      setDisplayName(p.display_name);
      setEmail(p.email);
      localStorage.setItem(NAME_KEY, p.display_name);
      localStorage.setItem(EMAIL_KEY, p.email);
    } catch {
      // 401이면 api 클라이언트가 refresh 시도. 그래도 실패면 무시.
    }
  };

  useEffect(() => {
    setUserId(tokens.userId);
    if (tokens.access) refreshProfile();
  }, []);

  const signup = async (email: string, password: string, displayName: string) => {
    setLoading(true);
    try {
      const pair = await auth.signup(email, password, displayName);
      tokens.set(pair);
      setUserId(pair.user_id);
      await refreshProfile();
    } finally { setLoading(false); }
  };

  const login = async (email: string, password: string) => {
    setLoading(true);
    try {
      const pair = await auth.login(email, password);
      tokens.set(pair);
      setUserId(pair.user_id);
      await refreshProfile();
    } finally { setLoading(false); }
  };

  const logout = async () => {
    await auth.logout();
    setUserId(null);
    setDisplayName(null);
    setEmail(null);
    localStorage.removeItem(NAME_KEY);
    localStorage.removeItem(EMAIL_KEY);
  };

  return (
    <Ctx.Provider value={{ userId, displayName, email, loading, signup, login, logout, refreshProfile }}>
      {children}
    </Ctx.Provider>
  );
}

export function useAuth() {
  const ctx = useContext(Ctx);
  if (!ctx) throw new Error("useAuth must be used within AuthProvider");
  return ctx;
}
