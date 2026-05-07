import { Navigate, Outlet, useLocation } from "react-router-dom";
import { useAuth } from "./AuthContext";

export function ProtectedRoute() {
  const { userId } = useAuth();
  const loc = useLocation();
  if (!userId) {
    return <Navigate to="/login" state={{ from: loc.pathname }} replace />;
  }
  return <Outlet />;
}
