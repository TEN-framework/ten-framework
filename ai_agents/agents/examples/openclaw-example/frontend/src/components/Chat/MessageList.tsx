import { Bot, Brain } from "lucide-react";
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
      {chatItems.map((item, _index) => {
        return <MessageItem data={item} key={item.time} />;
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
        ) : (
          <Avatar>
            <AvatarFallback>
              <Bot />
            </AvatarFallback>
          </Avatar>
        )
      ) : null}
      <div
        className={cn("max-w-[80%] rounded-lg p-2 text-secondary-foreground", {
          "bg-secondary": data.data_type !== EMessageDataType.OPENCLAW,
          "border border-[#2C5A73] bg-[#0E1F2A] text-[#CDEBFF]":
            data.data_type === EMessageDataType.OPENCLAW,
        })}
      >
        {data.data_type === EMessageDataType.OPENCLAW ? (
          <div className="mb-1 text-xs uppercase tracking-wide text-[#8AC6E8]">
            OpenClaw
          </div>
        ) : null}
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
