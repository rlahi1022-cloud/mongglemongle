import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";

const visBadge: Record<string, string> = {
  public: "bg-emerald-100 text-emerald-700",
  friends: "bg-amber-100 text-amber-800",
  private: "bg-slate-200 text-slate-600",
};
const visLabel: Record<string, string> = {
  public: "전체 공개",
  friends: "친구만",
  private: "나만",
};

interface Props {
  authorName: string;
  body: string;
  visibility: string;
  createdAt: string;
  rightSlot?: React.ReactNode;
}

export function PostCard({ authorName, body, visibility, createdAt, rightSlot }: Props) {
  return (
    <Card>
      <CardHeader className="flex-row items-center justify-between space-y-0 pb-3">
        <div className="flex items-center gap-3">
          <div className="h-8 w-8 rounded-full bg-primary text-primary-foreground grid place-items-center text-xs font-bold">
            {authorName?.[0] ?? "?"}
          </div>
          <div>
            <CardTitle className="text-base">{authorName}</CardTitle>
            <div className="text-xs text-muted-foreground">{createdAt}</div>
          </div>
        </div>
        <div className="flex items-center gap-2">
          <span className={`text-xs px-2 py-0.5 rounded-full ${visBadge[visibility] ?? "bg-slate-100"}`}>
            {visLabel[visibility] ?? visibility}
          </span>
          {rightSlot}
        </div>
      </CardHeader>
      <CardContent>
        <div className="whitespace-pre-wrap leading-relaxed">{body}</div>
      </CardContent>
    </Card>
  );
}
