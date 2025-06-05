//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import * as React from "react";
import { Bot, Brain } from "lucide-react";
import { EMessageDataType, EMessageType, type IChatItem } from "@/types/rtc";
import { cn } from "@/lib/utils";
import { useAutoScroll } from "@/hooks/use-auto-scroll";
import { Avatar, AvatarFallback } from "@/components/ui/ChatProfile";
import { t } from "i18next";

export default function MessageList(props: {
  chatItems: IChatItem[];
  className?: string;
}) {
  const { className, chatItems = [] } = props;

  const containerRef = React.useRef<HTMLDivElement>(null);

  useAutoScroll(containerRef);

  return (
    <div
      ref={containerRef}
      className={cn("flex-grow overflow-y-auto h-full w-full px-2", className)}
    >
      {chatItems.length === 0 ? (
        <div
          className="
            flex
            items-center
            justify-center
            h-full
          "
        >
          <p>{t("components.messageList.noMessages")}</p>
        </div>
      ) : (
        chatItems.map((item) => <MessageItem data={item} key={item.time} />)
      )}
    </div>
  );
}

export function MessageItem(props: { data: IChatItem }) {
  const { data } = props;

  return (
    <>
      <div
        className={cn("flex items-start mt-2 gap-2", {
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
          className="
            max-w-[80%]
            rounded-lg
            bg-secondary
            p-2
            text-secondary-foreground
          "
        >
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
    </>
  );
}
