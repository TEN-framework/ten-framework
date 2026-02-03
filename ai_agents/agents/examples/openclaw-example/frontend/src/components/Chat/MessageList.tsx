import { Bot, Brain, ChevronDown, ChevronUp } from "lucide-react";
import * as React from "react";
import ReactMarkdown from "react-markdown";
import rehypeSanitize from "rehype-sanitize";
import { useAppSelector, useAutoScroll } from "@/common";
import { Avatar, AvatarFallback } from "@/components/ui/avatar";
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
  const [expanded, setExpanded] = React.useState(false);

  return (
    <div className="flex items-start gap-2">
      <Avatar>
        <AvatarFallback>
          <Bot />
        </AvatarFallback>
      </Avatar>
      <div className="w-full max-w-[80%] rounded-lg border border-[#2C5A73] bg-[#0E1F2A] p-2 text-[#CDEBFF]">
        <button
          type="button"
          onClick={() => setExpanded((prev) => !prev)}
          className="flex w-full items-center justify-between text-left"
        >
          <div className="text-xs uppercase tracking-wide text-[#8AC6E8]">
            OpenClaw
          </div>
          {expanded ? (
            <ChevronUp className="h-4 w-4 text-[#8AC6E8]" />
          ) : (
            <ChevronDown className="h-4 w-4 text-[#8AC6E8]" />
          )}
        </button>
        <div
          className={cn("mt-2 text-sm leading-relaxed", {
            "line-clamp-6": !expanded,
          })}
        >
          <ReactMarkdown rehypePlugins={[rehypeSanitize]}>
            {data.text}
          </ReactMarkdown>
        </div>
      </div>
    </div>
  );
}
