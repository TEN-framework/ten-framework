"use client";

import axios from "axios";
import { Send } from "lucide-react";
import * as React from "react";
import { toast } from "sonner";
import { useAppDispatch, useAppSelector, useAutoScroll } from "@/common";
import MessageList from "@/components/Chat/MessageList";
import { Button } from "@/components/ui/button";
import { cn } from "@/lib/utils";
import { addChatItem } from "@/store/reducers/global";
import {
  EMessageDataType,
  EMessageType,
  ERTMTextType,
  type IRTMTextItem,
} from "@/types";

export default function ChatCard(props: { className?: string }) {
  const { className } = props;
  const [_modal2Open, _setModal2Open] = React.useState(false);
  const [inputValue, setInputValue] = React.useState("");

  const _rtmConnected = useAppSelector((state) => state.global.rtmConnected);
  const dispatch = useAppDispatch();
  const _graphName = useAppSelector((state) => state.global.selectedGraphId);
  const agentConnected = useAppSelector((state) => state.global.agentConnected);
  const options = useAppSelector((state) => state.global.options);
  const httpPortNumber = useAppSelector(
    (state) => state.global.options.http_port_number
  );

  const disableInputMemo = React.useMemo(() => {
    return (
      !options.channel || !options.userId || !httpPortNumber || !agentConnected
    );
  }, [options.channel, options.userId, httpPortNumber, agentConnected]);

  // const chatItems = genRandomChatList(10)
  const chatRef = React.useRef(null);

  useAutoScroll(chatRef);

  const _onTextChanged = (text: IRTMTextItem) => {
    console.log("[rtm] onTextChanged", text);
    if (text.type === ERTMTextType.TRANSCRIBE) {
      // const isAgent = Number(text.uid) != Number(options.userId)
      dispatch(
        addChatItem({
          userId: options.userId,
          text: text.text,
          type: text.stream_id === "0" ? EMessageType.AGENT : EMessageType.USER,
          data_type: EMessageDataType.TEXT,
          isFinal: text.is_final,
          time: text.ts,
        })
      );
    }
    if (text.type === ERTMTextType.INPUT_TEXT) {
      dispatch(
        addChatItem({
          userId: options.userId,
          text: text.text,
          type: EMessageType.USER,
          data_type: EMessageDataType.TEXT,
          isFinal: true,
          time: text.ts,
        })
      );
    }
  };

  const handleInputChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    setInputValue(e.target.value);
  };

  const handleInputSubmit = async (e: React.FormEvent<HTMLFormElement>) => {
    e.preventDefault();
    if (!inputValue || disableInputMemo || !httpPortNumber) {
      return;
    }

    try {
      // Send POST request to /proxy/{http_port_number}/cmd
      await axios.post(`/proxy/${httpPortNumber}/cmd`, {
        name: "message",
        payload: {
          text: inputValue,
        },
      });

      // Clear input on success
      setInputValue("");
    } catch (error) {
      console.error("Failed to send message:", error);
      toast.error("Failed to send message. Please try again.");
    }
  };

  return (
    <>
      {/* Chat Card */}
      <div className={cn("flex h-full min-h-0 overflow-hidden", className)}>
        <div className="flex w-full flex-1 flex-col p-4">
          {/* Scrollable messages container */}
          <div className="flex-1 overflow-y-auto" ref={chatRef}>
            <MessageList />
          </div>
          {/* Input area */}
          <div className="border-t pt-4">
            <form
              onSubmit={handleInputSubmit}
              className="flex items-center space-x-2"
            >
              <input
                type="text"
                disabled={disableInputMemo}
                placeholder="Type a message..."
                value={inputValue}
                onChange={handleInputChange}
                className={cn(
                  "grow rounded-md border bg-background p-1.5 focus:outline-hidden focus:ring-1 focus:ring-ring",
                  {
                    "cursor-not-allowed": disableInputMemo,
                  }
                )}
              />
              <Button
                type="submit"
                disabled={disableInputMemo || inputValue.length === 0}
                size="icon"
                variant="outline"
                className={cn("bg-transparent", {
                  "opacity-50": disableInputMemo || inputValue.length === 0,
                  "cursor-not-allowed": disableInputMemo,
                })}
              >
                <Send className="h-4 w-4" />
                <span className="sr-only">Send message</span>
              </Button>
            </form>
          </div>
        </div>
      </div>
    </>
  );
}
