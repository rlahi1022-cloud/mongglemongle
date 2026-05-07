// 백엔드 API 클라이언트.
// - access token 자동 첨부
// - 401 발생 시 refresh 1회 재시도
// - 응답: 200/201/204 → 본문(json|null), 그 외는 throw ApiError

const API_BASE = (import.meta.env.VITE_API_BASE as string) || "http://127.0.0.1:8080";

const ACCESS_KEY = "monggle_access";
const REFRESH_KEY = "monggle_refresh";
const USER_KEY = "monggle_user_id";

export class ApiError extends Error {
  constructor(
    public status: number,
    public title: string,
    message: string,
    public payload?: unknown
  ) {
    super(message);
  }
}

export type TokenPair = {
  user_id: number;
  access_token: string;
  refresh_token: string;
  access_expires_at: number;
  refresh_expires_at: number;
};

export const tokens = {
  get access() { return localStorage.getItem(ACCESS_KEY); },
  get refresh() { return localStorage.getItem(REFRESH_KEY); },
  get userId(): number | null {
    const v = localStorage.getItem(USER_KEY);
    return v ? Number(v) : null;
  },
  set(pair: TokenPair) {
    localStorage.setItem(ACCESS_KEY, pair.access_token);
    localStorage.setItem(REFRESH_KEY, pair.refresh_token);
    localStorage.setItem(USER_KEY, String(pair.user_id));
  },
  clear() {
    localStorage.removeItem(ACCESS_KEY);
    localStorage.removeItem(REFRESH_KEY);
    localStorage.removeItem(USER_KEY);
  },
};

async function rawFetch(path: string, init: RequestInit, useAuth: boolean): Promise<Response> {
  const headers = new Headers(init.headers);
  if (useAuth && tokens.access) {
    headers.set("Authorization", `Bearer ${tokens.access}`);
  }
  if (init.body && !(init.body instanceof FormData) && !headers.has("Content-Type")) {
    headers.set("Content-Type", "application/json");
  }
  return fetch(`${API_BASE}${path}`, { ...init, headers });
}

async function tryRefresh(): Promise<boolean> {
  if (!tokens.refresh) return false;
  const resp = await rawFetch(
    "/auth/refresh",
    { method: "POST", body: JSON.stringify({ refresh_token: tokens.refresh }) },
    false
  );
  if (!resp.ok) {
    tokens.clear();
    return false;
  }
  const pair: TokenPair = await resp.json();
  tokens.set(pair);
  return true;
}

async function parseError(resp: Response): Promise<never> {
  let payload: any = null;
  try { payload = await resp.json(); } catch { /* */ }
  const title = payload?.title || "request_failed";
  const detail = payload?.detail || resp.statusText || "";
  throw new ApiError(resp.status, title, detail, payload);
}

export async function api<T = any>(
  path: string,
  init: RequestInit & { useAuth?: boolean } = {}
): Promise<T> {
  const useAuth = init.useAuth !== false;
  let resp = await rawFetch(path, init, useAuth);

  if (resp.status === 401 && useAuth && tokens.refresh) {
    const ok = await tryRefresh();
    if (ok) resp = await rawFetch(path, init, useAuth);
  }

  if (!resp.ok) await parseError(resp);
  if (resp.status === 204) return undefined as T;
  const ct = resp.headers.get("content-type") || "";
  if (ct.includes("application/json")) return resp.json() as Promise<T>;
  return undefined as T;
}

// 미디어 업로드용 — multipart/form-data
export async function uploadMedia(postId: number, file: File): Promise<MediaAsset> {
  const fd = new FormData();
  fd.append("file", file);
  return api<MediaAsset>(`/posts/${postId}/media`, { method: "POST", body: fd });
}

// 미디어 보기 URL (auth 헤더 못 붙는 <img>용 — public/own 정도만 동작.
// friends/private은 추후 서명 URL 또는 cookie 인증으로 보완)
export function mediaUrl(mediaId: number, kind: "view" | "thumb" | "download" = "thumb") {
  return `${API_BASE}/media/${mediaId}/${kind}`;
}

// 도메인 타입 ===

export type Visibility = "public" | "friends" | "private";
export type DownloadPolicy = "owner_only" | "followers" | "public_allowed";

export interface Post {
  id: number;
  user_id: number;
  title: string;
  body: string;
  visibility: Visibility;
  download_policy: DownloadPolicy;
  created_at: string;
  updated_at: string;
}

export interface FeedItem extends Post {
  author_name: string;
}

export interface FeedPage {
  items: FeedItem[];
  next_cursor: number | null;
}

export interface Page<T> {
  items: T[];
  next_cursor: number | null;
}

export interface UserBrief {
  id: number;
  email: string;
  display_name: string;
}

export interface MediaAsset {
  id: number;
  post_id: number;
  user_id: number;
  kind: "photo" | "video";
  mime_type: string;
  size_bytes: number;
  width_px: number;
  height_px: number;
  status: "pending" | "ready" | "failed";
  created_at: string;
  view_url: string;
  download_url: string;
  has_thumb: boolean;
  has_poster: boolean;
}

export interface SnapshotPost {
  id: number;
  title: string;
  body: string;
  visibility: Visibility;
  deleted: boolean;
  last_event_id: number;
}

export interface SnapshotResult {
  user_id: number;
  target_time: string;
  posts: SnapshotPost[];
}

// 도메인 helpers ===

export const auth = {
  signup: (email: string, password: string, display_name: string) =>
    api<TokenPair>("/auth/signup", {
      method: "POST",
      body: JSON.stringify({ email, password, display_name }),
      useAuth: false,
    }),
  login: (email: string, password: string) =>
    api<TokenPair>("/auth/login", {
      method: "POST",
      body: JSON.stringify({ email, password }),
      useAuth: false,
    }),
  logout: async () => {
    if (tokens.refresh) {
      try { await api("/auth/logout", {
        method: "POST",
        body: JSON.stringify({ refresh_token: tokens.refresh }),
        useAuth: false,
      }); } catch { /* ignore */ }
    }
    tokens.clear();
  },
};

export const posts = {
  create: (title: string, body: string, visibility: Visibility, download_policy: DownloadPolicy = "owner_only") =>
    api<Post>("/posts", {
      method: "POST",
      body: JSON.stringify({ title, body, visibility, download_policy }),
    }),
  get: (id: number) => api<Post>(`/posts/${id}`),
  update: (id: number, patch: Partial<Pick<Post, "title" | "body" | "visibility" | "download_policy">>) =>
    api<Post>(`/posts/${id}`, { method: "PATCH", body: JSON.stringify(patch) }),
  remove: (id: number) => api<void>(`/posts/${id}`, { method: "DELETE" }),
  restore: (id: number) => api<Post>(`/posts/${id}/restore`, { method: "POST" }),
  myTimeline: (cursor?: number, limit = 20) => {
    const q = new URLSearchParams();
    if (cursor) q.set("cursor", String(cursor));
    q.set("limit", String(limit));
    return api<Page<Post>>(`/me/timeline?${q}`);
  },
  search: (q: string) =>
    api<{ q: string; count: number; items: Post[] }>(`/me/search?q=${encodeURIComponent(q)}`),
  snapshot: (atIso: string) =>
    api<SnapshotResult>(`/me/snapshot?at=${encodeURIComponent(atIso)}`),
};

export const social = {
  follow: (userId: number) => api<void>(`/users/${userId}/follow`, { method: "POST" }),
  unfollow: (userId: number) => api<void>(`/users/${userId}/follow`, { method: "DELETE" }),
  followers: () => api<{ items: UserBrief[] }>("/me/followers"),
  following: () => api<{ items: UserBrief[] }>("/me/following"),
  feed: (cursor?: number, limit = 30) => {
    const q = new URLSearchParams();
    if (cursor) q.set("cursor", String(cursor));
    q.set("limit", String(limit));
    return api<FeedPage>(`/me/feed?${q}`);
  },
};

export interface MeProfile {
  user_id: number;
  email: string;
  display_name: string;
  has_avatar: boolean;
}

export const me = {
  whoami: () => api<MeProfile>("/me"),
};

export const profile = {
  uploadAvatar: async (file: File) => {
    const fd = new FormData();
    fd.append("file", file);
    // Drogon 1.8.7 MultiPartParser가 PUT method body를 잘 못 읽어서 POST로.
    // 백엔드는 {Put, Post} 둘 다 받음.
    return api<{ avatar_path: string }>("/me/avatar", { method: "POST", body: fd });
  },
  updateDisplayName: (display_name: string) =>
    api<{ display_name: string }>("/me", {
      method: "PATCH",
      body: JSON.stringify({ display_name }),
    }),
  changePassword: (oldPw: string, newPw: string) =>
    api<void>("/me/password", {
      method: "PATCH",
      body: JSON.stringify({ old_password: oldPw, new_password: newPw }),
    }),
  verifyPassword: (password: string) =>
    api<{ ok: boolean }>("/me/verify-password", {
      method: "POST",
      body: JSON.stringify({ password }),
    }),
  avatarUrl: (userId: number, bust?: number) =>
    `${API_BASE}/users/${userId}/avatar${bust ? `?v=${bust}` : ""}`,
};

export interface CommentItem {
  id: number;
  post_id: number;
  user_id: number;
  body: string;
  author_name: string;
  created_at: string;
}

export const comments = {
  list: (postId: number) => api<{ items: CommentItem[] }>(`/posts/${postId}/comments`),
  create: (postId: number, body: string) =>
    api<CommentItem>(`/posts/${postId}/comments`, {
      method: "POST",
      body: JSON.stringify({ body }),
    }),
  remove: (commentId: number) =>
    api<void>(`/comments/${commentId}`, { method: "DELETE" }),
};

export interface Notification {
  id: number;
  kind: "follow" | "comment" | string;
  actor_id: number | null;
  target_id: number | null;
  body: string;
  is_read: boolean;
  created_at: string;
  actor_name: string;
}

export const notifications = {
  recent: () => api<{ items: Notification[]; unread: number }>("/me/notifications"),
  markAllRead: () => api<void>("/me/notifications/read", { method: "POST" }),
};

export interface BlockedUser {
  id: number;
  email: string;
  display_name: string;
  blocked_at: string;
}

export const blocks = {
  list: () => api<{ items: BlockedUser[] }>("/me/blocks"),
  add: (userId: number) => api<void>(`/users/${userId}/block`, { method: "POST" }),
  remove: (userId: number) => api<void>(`/users/${userId}/block`, { method: "DELETE" }),
};

export interface PostMedia {
  id: number;
  post_id: number;
  kind: "photo" | "video";
  mime_type: string;
  width_px: number;
  height_px: number;
  has_thumb: boolean;
  has_poster: boolean;
}

export const media = {
  listForPost: (postId: number) =>
    api<{ items: PostMedia[] }>(`/posts/${postId}/media`),
  // <img>용 단순 URL. friends/private 글은 anon 접근시 403이지만 우리는
  // 보통 본인이 본 피드 안에서만 호출하니 OK. (서명 URL은 후속.)
  viewUrl: (mediaId: number) => `${API_BASE}/media/${mediaId}/view`,
  thumbUrl: (mediaId: number) => `${API_BASE}/media/${mediaId}/thumb`,
};

// 글 본문 글자수 제한 (백엔드도 동일 적용)
export const POST_BODY_MAX = 1000;
