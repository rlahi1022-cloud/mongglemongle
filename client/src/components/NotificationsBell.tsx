import { useEffect, useState } from "react";
import { notifications as notifApi, type Notification } from "@/api/client";
import { Button } from "@/components/ui/button";

export function NotificationsBell() {
  const [open, setOpen] = useState(false);
  const [items, setItems] = useState<Notification[]>([]);
  const [unread, setUnread] = useState(0);
  const [loading, setLoading] = useState(false);

  const refresh = async () => {
    try {
      const r = await notifApi.recent();
      setItems(r.items);
      setUnread(r.unread);
    } catch { /* */ }
  };

  useEffect(() => {
    refresh();
    const id = window.setInterval(refresh, 30_000);
    return () => window.clearInterval(id);
  }, []);

  const onOpen = async () => {
    setOpen((v) => !v);
    if (!open) {
      setLoading(true);
      await refresh();
      setLoading(false);
    }
  };

  const onMarkAll = async () => {
    try {
      await notifApi.markAllRead();
      setUnread(0);
      setItems((prev) => prev.map((n) => ({ ...n, is_read: true })));
    } catch { /* */ }
  };

  return (
    <div className="relative">
      <Button
        variant="ghost"
        size="sm"
        className="rounded-2xl relative"
        onClick={onOpen}
        title="알림"
      >
        🔔
        {unread > 0 && (
          <span className="absolute -top-1 -right-1 h-5 min-w-[1.25rem] rounded-full bg-destructive text-destructive-foreground text-[10px] font-bold grid place-items-center px-1">
            {unread > 99 ? "99+" : unread}
          </span>
        )}
      </Button>
      {open && (
        <div
          className="absolute right-0 mt-2 w-80 max-h-96 overflow-y-auto cloud-card p-2 z-30"
          onMouseLeave={() => setOpen(false)}
        >
          <div className="flex items-center justify-between px-2 py-1">
            <div className="text-sm font-semibold">알림</div>
            <button onClick={onMarkAll} className="text-xs text-primary hover:underline">
              모두 읽음
            </button>
          </div>
          {loading && <div className="text-xs text-muted-foreground p-3">불러오는 중...</div>}
          {!loading && items.length === 0 && (
            <div className="text-xs text-muted-foreground p-3 text-center">아직 알림이 없어요.</div>
          )}
          <div className="space-y-1">
            {items.map((n) => (
              <div
                key={n.id}
                className={`px-3 py-2 rounded-xl text-sm ${n.is_read ? "" : "bg-primary/5"}`}
              >
                <div className="text-xs text-muted-foreground">
                  {n.kind === "follow" ? "👤 새 팔로워" : n.kind === "comment" ? "💬 새 댓글" : n.kind}
                  · {n.created_at.split(" ")[0]}
                </div>
                <div className="truncate">{n.body || (n.actor_name ? `${n.actor_name} 님` : "")}</div>
              </div>
            ))}
          </div>
        </div>
      )}
    </div>
  );
}
