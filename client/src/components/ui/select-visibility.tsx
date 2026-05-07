import * as React from "react";
import { cn } from "@/lib/utils";

interface VisibilitySelectProps {
  value: "public" | "friends" | "private";
  onChange: (v: "public" | "friends" | "private") => void;
  className?: string;
}

export function VisibilitySelect({ value, onChange, className }: VisibilitySelectProps) {
  return (
    <select
      value={value}
      onChange={(e) => onChange(e.target.value as VisibilitySelectProps["value"])}
      className={cn(
        "h-9 rounded-2xl border border-input bg-background px-3 text-sm",
        className
      )}
    >
      <option value="public">전체 공개</option>
      <option value="friends">친구만</option>
      <option value="private">나만</option>
    </select>
  );
}
