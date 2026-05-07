import { createContext, useContext, useEffect, useState, type ReactNode } from "react";
import { auth, tokens } from "@/api/client";

interface AuthState {
  userId: number | null;
  loading: boolean;
  signup: (email: string, password: string, displayName: string) => Promise<void>;
  login: (email: string, password: string) => Promise<void>;
  logout: () => Promise<void>;
}

const Ctx = createContext<AuthState | null>(null);

export function AuthProvider({ children }: { children: ReactNode }) {
  const [userId, setUserId] = useState<number | null>(tokens.userId);
  const [loading, setLoading] = useState(false);

  useEffect(() => {
    setUserId(tokens.userId);
  }, []);

  const signup = async (email: string, password: string, displayName: string) => {
    setLoading(true);
    try {
      const pair = await auth.signup(email, password, displayName);
      tokens.set(pair);
      setUserId(pair.user_id);
    } finally { setLoading(false); }
  };

  const login = async (email: string, password: string) => {
    setLoading(true);
    try {
      const pair = await auth.login(email, password);
      tokens.set(pair);
      setUserId(pair.user_id);
    } finally { setLoading(false); }
  };

  const logout = async () => {
    await auth.logout();
    setUserId(null);
  };

  return (
    <Ctx.Provider value={{ userId, loading, signup, login, logout }}>
      {children}
    </Ctx.Provider>
  );
}

export function useAuth() {
  const ctx = useContext(Ctx);
  if (!ctx) throw new Error("useAuth must be used within AuthProvider");
  return ctx;
}
