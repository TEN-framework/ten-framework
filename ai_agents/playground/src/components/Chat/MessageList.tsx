import { Bot, Brain, GitBranch } from "lucide-react";
import * as React from "react";
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
      {chatItems.map((item, index) => {
        return <MessageItem data={item} key={`${item.time}-${index}`} />;
      })}
    </div>
  );
}

export function MessageItem(props: { data: IChatItem }) {
  const { data } = props;

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
        ) : data.data_type === EMessageDataType.ROUTE ? (
          <Avatar>
            <AvatarFallback>
              <GitBranch size={20} />
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
      <div
        className={cn(
          "max-w-[80%] rounded-lg bg-secondary p-2 text-secondary-foreground",
          data.data_type === EMessageDataType.REASON &&
            "border border-violet-700/40 bg-violet-950/20",
          data.data_type === EMessageDataType.ROUTE &&
            "border border-sky-700/50 bg-sky-950/30"
        )}
      >
        {data.data_type === EMessageDataType.IMAGE ? (
          <img src={data.text} alt="chat" className="w-full" />
        ) : (
          <>
            {data.data_type === EMessageDataType.REASON && (
              <p className="mb-1 text-xs font-medium text-violet-300">
                DeepSeek 思考过程
              </p>
            )}
            <p
              className={cn(
                "whitespace-pre-wrap break-words",
                (data.data_type === EMessageDataType.REASON ||
                  data.data_type === EMessageDataType.ROUTE) &&
                  "text-xs text-zinc-400"
              )}
            >
              {data.text}
            </p>
          </>
        )}
      </div>
    </div>
  );
}
