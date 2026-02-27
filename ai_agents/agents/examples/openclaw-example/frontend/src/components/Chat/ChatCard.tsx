"use client";

import { Send } from "lucide-react";
import * as React from "react";
import { useAppDispatch, useAppSelector, useAutoScroll } from "@/common";
import MessageList from "@/components/Chat/MessageList";
import {
  extractApproveCommand,
  extractListCommand,
  isPairingRequiredMessage,
} from "@/components/Chat/openclawPairing";
import { Button } from "@/components/ui/button";
import {
  Dialog,
  DialogContent,
  DialogDescription,
  DialogHeader,
  DialogTitle,
} from "@/components/ui/dialog";
import { cn } from "@/lib/utils";
import { rtmManager, type TRtmMessage } from "@/manager/rtm";
import { addChatItem, setAgentPhase } from "@/store/reducers/global";
import {
  EMessageDataType,
  EMessageType,
  ERTMTextType,
} from "@/types";

export default function ChatCard(props: { className?: string }) {
  const { className } = props;
  const [_modal2Open, _setModal2Open] = React.useState(false);
  const [inputValue, setInputValue] = React.useState("");
  const [pairingDialogOpen, setPairingDialogOpen] = React.useState(false);
  const [pairingApproveCmd, setPairingApproveCmd] = React.useState("");
  const [pairingListCmd, setPairingListCmd] = React.useState("");
  const [pairingCopied, setPairingCopied] = React.useState(false);
  const [pairingBannerVisible, setPairingBannerVisible] = React.useState(false);

  const rtmConnected = useAppSelector((state) => state.global.rtmConnected);
  const dispatch = useAppDispatch();
  const graphName = useAppSelector((state) => state.global.selectedGraphId);
  const agentConnected = useAppSelector((state) => state.global.agentConnected);
  const options = useAppSelector((state) => state.global.options);

  const disableInputMemo = React.useMemo(() => {
    return (
      !options.channel ||
      !options.userId ||
      !options.appId ||
      !options.token ||
      !rtmConnected ||
      !agentConnected
    );
  }, [
    options.channel,
    options.userId,
    options.appId,
    options.token,
    rtmConnected,
    agentConnected,
  ]);

  // const chatItems = genRandomChatList(10)
  const chatRef = React.useRef(null);
  const lastPairingDialogKeyRef = React.useRef<string>("");

  useAutoScroll(chatRef);

  const _onTextChanged = (text: TRtmMessage) => {
    console.log("[rtm] onTextChanged", text);
    if ("data_type" in text && text.data_type === "openclaw_result") {
      const openclawText = String(text.text ?? "");
      if (isPairingRequiredMessage(openclawText)) {
        const approveCmd = extractApproveCommand(openclawText);
        const listCmd = extractListCommand(openclawText);
        const dialogKey = `${approveCmd}|${Number(text.ts ?? 0)}`;
        if (lastPairingDialogKeyRef.current !== dialogKey) {
          lastPairingDialogKeyRef.current = dialogKey;
          setPairingApproveCmd(approveCmd);
          setPairingListCmd(listCmd);
          setPairingCopied(false);
          setPairingBannerVisible(true);
          setPairingDialogOpen(true);
        }
        return;
      }
      dispatch(
        addChatItem({
          userId: "openclaw",
          text: openclawText,
          type: EMessageType.AGENT,
          data_type: EMessageDataType.OPENCLAW,
          isFinal: true,
          time: Number(text.ts ?? Date.now()),
        })
      );
      return;
    }
    if ("data_type" in text && text.data_type === "openclaw_phase") {
      dispatch(setAgentPhase(String(text.phase ?? "")));
      return;
    }
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

  React.useEffect(() => {
    if (!rtmConnected) {
      return;
    }
    rtmManager.on("rtmMessage", _onTextChanged);
    return () => {
      rtmManager.off("rtmMessage", _onTextChanged);
    };
  }, [rtmConnected]);

  const handleInputChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    setInputValue(e.target.value);
  };

  const handleInputSubmit = (e: React.FormEvent<HTMLFormElement>) => {
    e.preventDefault();
    if (!inputValue || disableInputMemo) {
      return;
    }
    rtmManager.sendText(inputValue);
    setInputValue("");
  };

  const copyPairingCommand = async () => {
    if (!pairingApproveCmd) {
      return;
    }
    try {
      await navigator.clipboard.writeText(pairingApproveCmd);
      setPairingCopied(true);
    } catch (_error) {
      setPairingCopied(false);
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
          {pairingBannerVisible ? (
            <div className="mb-3 rounded-md border border-amber-300/60 bg-amber-50 px-3 py-2 text-sm text-amber-900">
              <p className="mb-2">
                OpenClaw pairing is pending. Approve on gateway host to continue.
              </p>
              <div className="flex flex-wrap items-center gap-2">
                <Button
                  type="button"
                  size="sm"
                  variant="outline"
                  onClick={() => setPairingDialogOpen(true)}
                >
                  Reopen instructions
                </Button>
                <Button type="button" size="sm" onClick={copyPairingCommand}>
                  {pairingCopied ? "Copied" : "Copy command"}
                </Button>
                <Button
                  type="button"
                  size="sm"
                  variant="ghost"
                  onClick={() => setPairingBannerVisible(false)}
                >
                  Dismiss
                </Button>
              </div>
            </div>
          ) : null}
          <div
            className={cn("border-t pt-4", {
              hidden: !graphName.includes("rtm"), // TODO: TMP use rtm key word
            })}
          >
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
      <Dialog open={pairingDialogOpen} onOpenChange={setPairingDialogOpen}>
        <DialogContent className="max-w-xl">
          <DialogHeader>
            <DialogTitle>Pairing approval required</DialogTitle>
            <DialogDescription>
              Copy and run the command on the gateway host to approve this device.
            </DialogDescription>
          </DialogHeader>
          <div className="space-y-3 text-sm">
            {pairingListCmd ? (
              <div>
                <p className="mb-1 text-muted-foreground">Optional: check pending requests</p>
                <pre className="rounded bg-muted px-3 py-2 text-xs">{pairingListCmd}</pre>
              </div>
            ) : null}
            <div>
              <p className="mb-1 text-muted-foreground">Approve pairing</p>
              <pre className="rounded bg-muted px-3 py-2 text-xs">{pairingApproveCmd || "openclaw devices approve --latest"}</pre>
            </div>
            <div className="flex items-center gap-2">
              <Button type="button" onClick={copyPairingCommand}>
                {pairingCopied ? "Copied" : "Copy command"}
              </Button>
              <Button
                type="button"
                variant="outline"
                onClick={() => setPairingDialogOpen(false)}
              >
                Close
              </Button>
            </div>
          </div>
        </DialogContent>
      </Dialog>
    </>
  );
}
