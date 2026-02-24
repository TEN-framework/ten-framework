import { Bot, Brain } from "lucide-react";
import * as React from "react";
import type { Components } from "react-markdown";
import ReactMarkdown from "react-markdown";
import rehypeSanitize from "rehype-sanitize";
import { useAppSelector, useAutoScroll } from "@/common";
import { Avatar, AvatarFallback } from "@/components/ui/avatar";
import {
  Dialog,
  DialogContent,
  DialogDescription,
  DialogHeader,
  DialogTitle,
} from "@/components/ui/dialog";
import { cn } from "@/lib/utils";
import { EMessageDataType, EMessageType, type IChatItem } from "@/types";

export default function MessageList(props: { className?: string }) {
  const { className } = props;

  const chatItems = useAppSelector((state) => state.global.chatItems);

  const containerRef = React.useRef<HTMLDivElement>(null);

  useAutoScroll(containerRef);

  return (
    <div
      ref={containerRef}
      className={cn("grow space-y-2 overflow-y-auto p-4", className)}
    >
      {chatItems.map((item, _index) => {
        return <MessageItem data={item} key={item.time} />;
      })}
    </div>
  );
}

export function MessageItem(props: { data: IChatItem }) {
  const { data } = props;

  if (data.data_type === EMessageDataType.OPENCLAW) {
    return <OpenclawMessageCard data={data} />;
  }

  return (
    <div
      className={cn("flex items-start gap-2", {
        "flex-row-reverse": data.type === EMessageType.USER,
      })}
    >
      {data.type === EMessageType.AGENT ? (
        data.data_type === EMessageDataType.REASON ? (
          <Avatar>
            <AvatarFallback>
              <Brain size={20} />
            </AvatarFallback>
          </Avatar>
        ) : (
          <Avatar>
            <AvatarFallback>
              <Bot />
            </AvatarFallback>
          </Avatar>
        )
      ) : null}
      <div className="max-w-[80%] rounded-lg bg-secondary p-2 text-secondary-foreground">
        {data.data_type === EMessageDataType.IMAGE ? (
          <img src={data.text} alt="chat" className="w-full" />
        ) : (
          <p
            className={
              data.data_type === EMessageDataType.REASON
                ? cn("text-xs", "text-zinc-500")
                : ""
            }
          >
            {data.text}
          </p>
        )}
      </div>
    </div>
  );
}

function OpenclawMessageCard(props: { data: IChatItem }) {
  const { data } = props;
  const [open, setOpen] = React.useState(false);
  const { summary, hasMore } = getOpenclawSummary(data.text);

  return (
    <Dialog open={open} onOpenChange={setOpen}>
      <div className="flex items-start gap-2">
        <Avatar>
          <AvatarFallback>
            <Bot />
          </AvatarFallback>
        </Avatar>
        <button
          type="button"
          onClick={() => setOpen(true)}
          className="w-full max-w-[80%] rounded-lg border border-[#2C5A73] bg-[#0E1F2A] p-2 text-left text-[#CDEBFF] transition-colors hover:border-[#4A8EB4]"
        >
          <div className="text-xs uppercase tracking-wide text-[#8AC6E8]">
            OpenClaw
          </div>
          <p className="mt-1 line-clamp-3 overflow-hidden text-ellipsis text-sm leading-relaxed">
            {summary}
          </p>
          {hasMore ? (
            <p className="mt-1 text-xs text-[#8AC6E8]/90">... 点击查看全文</p>
          ) : null}
        </button>
      </div>
      <DialogContent className="max-h-[80vh] max-w-3xl overflow-y-auto">
        <DialogHeader>
          <DialogTitle>OpenClaw</DialogTitle>
          <DialogDescription>
            Full delegated task result
          </DialogDescription>
        </DialogHeader>
        <div className="rounded-md border border-border/60 bg-muted/20 p-4">
          <ReactMarkdown
            rehypePlugins={[rehypeSanitize]}
            components={openclawMarkdownComponents}
          >
            {data.text?.trim() || "(empty response)"}
          </ReactMarkdown>
        </div>
      </DialogContent>
    </Dialog>
  );
}

function getOpenclawSummary(text: string): { summary: string; hasMore: boolean } {
  const normalized = (text || "").trim();
  if (!normalized) {
    return { summary: "(empty response)", hasMore: false };
  }

  const lines = normalized
    .split(/\r?\n/)
    .map((line) => line.trim())
    .filter((line) => line.length > 0);
  if (lines.length === 0) {
    return { summary: "(empty response)", hasMore: false };
  }

  const previewLines = lines.slice(0, 3);
  const summary = previewLines.join("\n");
  const hasMore = lines.length > 3 || normalized.length > summary.length;
  return { summary, hasMore };
}

const openclawMarkdownComponents: Components = {
  h1: ({ children }) => (
    <h1 className="mt-4 mb-2 border-border/60 border-b pb-2 font-semibold text-xl first:mt-0">
      {children}
    </h1>
  ),
  h2: ({ children }) => (
    <h2 className="mt-4 mb-2 font-semibold text-lg first:mt-0">{children}</h2>
  ),
  h3: ({ children }) => (
    <h3 className="mt-3 mb-1 font-semibold text-base">{children}</h3>
  ),
  p: ({ children }) => (
    <p className="mb-3 text-[14px] leading-7 last:mb-0">{children}</p>
  ),
  ul: ({ children }) => (
    <ul className="mb-3 list-disc space-y-1 pl-5 text-[14px] leading-7">
      {children}
    </ul>
  ),
  ol: ({ children }) => (
    <ol className="mb-3 list-decimal space-y-1 pl-5 text-[14px] leading-7">
      {children}
    </ol>
  ),
  li: ({ children }) => <li className="pl-1">{children}</li>,
  blockquote: ({ children }) => (
    <blockquote className="mb-3 border-l-4 border-[#2C5A73] bg-[#0E1F2A]/40 py-2 pr-3 pl-4 text-[13px] text-muted-foreground italic">
      {children}
    </blockquote>
  ),
  a: ({ href, children }) => (
    <a
      href={href}
      target="_blank"
      rel="noreferrer"
      className="underline decoration-[#4A8EB4] underline-offset-2 hover:text-[#4A8EB4]"
    >
      {children}
    </a>
  ),
  code: ({ className, children }) => {
    const isBlock = Boolean(className);
    if (isBlock) {
      return (
        <code className="block overflow-x-auto rounded-md bg-[#0B1620] p-3 text-[12px] leading-6 text-[#CDEBFF]">
          {children}
        </code>
      );
    }
    return (
      <code className="rounded bg-[#0B1620] px-1.5 py-0.5 text-[12px] text-[#CDEBFF]">
        {children}
      </code>
    );
  },
  pre: ({ children }) => <pre className="mb-3">{children}</pre>,
  table: ({ children }) => (
    <div className="mb-3 overflow-x-auto">
      <table className="min-w-full border-collapse border border-border/60 text-[13px]">
        {children}
      </table>
    </div>
  ),
  th: ({ children }) => (
    <th className="border border-border/60 bg-muted/50 px-2 py-1.5 text-left font-semibold">
      {children}
    </th>
  ),
  td: ({ children }) => (
    <td className="border border-border/60 px-2 py-1.5 align-top">{children}</td>
  ),
  hr: () => <hr className="my-4 border-border/60" />,
};
