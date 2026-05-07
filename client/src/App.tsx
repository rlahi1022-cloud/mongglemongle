import { BrowserRouter, Navigate, Route, Routes } from "react-router-dom";
import { AuthProvider } from "@/auth/AuthContext";
import { ProtectedRoute } from "@/auth/ProtectedRoute";
import { ThemeProvider } from "@/theme/ThemeContext";
import { Layout } from "@/components/Layout";
import { LoginPage } from "@/pages/Login";
import { SignupPage } from "@/pages/Signup";
import { FeedPage } from "@/pages/Feed";
import { MyTimelinePage } from "@/pages/MyTimeline";
import { SnapshotPage } from "@/pages/Snapshot";
import { SearchPage } from "@/pages/Search";
import { ProfilePage } from "@/pages/Profile";

export default function App() {
  return (
    <ThemeProvider>
      <AuthProvider>
        <BrowserRouter>
          <Routes>
            <Route path="/login" element={<LoginPage />} />
            <Route path="/signup" element={<SignupPage />} />
            <Route element={<ProtectedRoute />}>
              <Route element={<Layout />}>
                <Route path="/" element={<Navigate to="/feed" replace />} />
                <Route path="/feed" element={<FeedPage />} />
                <Route path="/me/timeline" element={<MyTimelinePage />} />
                <Route path="/snapshot" element={<SnapshotPage />} />
                <Route path="/search" element={<SearchPage />} />
                <Route path="/profile" element={<ProfilePage />} />
              </Route>
            </Route>
            <Route path="*" element={<Navigate to="/feed" replace />} />
          </Routes>
        </BrowserRouter>
      </AuthProvider>
    </ThemeProvider>
  );
}
