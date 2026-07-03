//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

import { type UID, useClientEvent } from "agora-rtc-react";
import { useCallback, useRef, useState } from "react";
import { EMessageDataType, EMessageType, type IChatItem } from "@/types/rtc";

interface TextDataChunk {
  message_id: string;
  part_index: number;
  total_parts: number;
  content: string;
}

const TIMEOUT_MS = 5000;

export function useRTCMessageParser(
  client: unknown,
  userId: number | null,
  onMessage: (msg: IChatItem) => void
) {
  const messageCache = useRef<Record<string, TextDataChunk[]>>({});
  // @ts-expect-error-next-line
  // biome-ignore lint/correctness/noUnusedFunctionParameters: <ignore>
  useClientEvent(client, "stream-message", (uid: UID, payload: Uint8Array) => {
    // Handle incoming stream messages
    const ascii = String.fromCharCode(...new Uint8Array(payload));
    handleChunk(ascii);
  });

  const reconstructMessage = (chunks: TextDataChunk[]) => {
    return chunks
      .sort((a, b) => a.part_index - b.part_index)
      .map((chunk) => chunk.content)
      .join("");
  };

  const base64ToUtf8 = (base64: string): string => {
    const binaryString = atob(base64); // Latin-1 形式的二进制字符串
    const bytes = new Uint8Array(binaryString.length);
    for (let i = 0; i < binaryString.length; i++) {
      bytes[i] = binaryString.charCodeAt(i);
    }
    return new TextDecoder("utf-8").decode(bytes);
  };

  // biome-ignore lint/correctness/useExhaustiveDependencies: <ignore>
  const handleChunk = useCallback(
    (formattedChunk: string) => {
      try {
        const [message_id, partIndexStr, totalPartsStr, content] =
          formattedChunk.split("|");

        const part_index = parseInt(partIndexStr, 10);
        const total_parts =
          totalPartsStr === "???" ? -1 : parseInt(totalPartsStr, 10);
        if (total_parts === -1) return;

        const chunkData: TextDataChunk = {
          message_id,
          part_index,
          total_parts,
          content,
        };

        if (!messageCache.current[message_id]) {
          messageCache.current[message_id] = [];
          setTimeout(() => {
            if (messageCache.current[message_id]?.length !== total_parts) {
              delete messageCache.current[message_id];
            }
          }, TIMEOUT_MS);
        }

        messageCache.current[message_id].push(chunkData);

        if (messageCache.current[message_id].length === total_parts) {
          const complete = reconstructMessage(messageCache.current[message_id]);
          const { stream_id, is_final, text, text_ts, data_type } = JSON.parse(
            base64ToUtf8(complete)
          );
          const isAgent = Number(stream_id) !== Number(userId);

          const chatItem: IChatItem = {
            type: isAgent ? EMessageType.AGENT : EMessageType.USER,
            time: text_ts,
            text,
            data_type: EMessageDataType.TEXT,
            userId: stream_id,
            isFinal: is_final,
          };

          if (data_type === "raw") {
            const { data, type } = JSON.parse(text);
            if (type === "image_url") {
              chatItem.data_type = EMessageDataType.IMAGE;
              chatItem.text = data.image_url;
            } else if (type === "reasoning") {
              chatItem.data_type = EMessageDataType.REASON;
              chatItem.text = data.text;
            } else if (type === "action" && data.action === "browse_website") {
              window.open(data.data.url, "_blank");
              delete messageCache.current[message_id];
              return;
            }
          }

          if (chatItem.text.trim().length > 0) {
            onMessage(chatItem);
          }

          delete messageCache.current[message_id];
        }
      } catch (err) {
        console.error("Error handling RTC chunk:", err);
      }
    },
    [onMessage, userId]
  );
}

/**
 * Custom hook to manage chatItems with reducer-style update logic.
 */
export function useChatItemReducer() {
  const [chatItems, setChatItems] = useState<IChatItem[]>([]);

  const addChatItem = useCallback((newMsg: IChatItem) => {
    setChatItems((prev) => {
      const { userId, time, isFinal, text, type } = newMsg;

      const lastFinalIndex = [...prev]
        .reverse()
        .findIndex((el) => el.userId === userId && el.isFinal);
      const lastNonFinalIndex = [...prev]
        .reverse()
        .findIndex((el) => el.userId === userId && !el.isFinal);

      const realLastFinalIndex =
        lastFinalIndex === -1 ? -1 : prev.length - 1 - lastFinalIndex;
      const realLastNonFinalIndex =
        lastNonFinalIndex === -1 ? -1 : prev.length - 1 - lastNonFinalIndex;

      const lastFinalItem = prev[realLastFinalIndex];
      const lastNonFinalItem = prev[realLastNonFinalIndex];

      const updated = [...prev];

      if (lastFinalItem) {
        if (time <= lastFinalItem.time) {
          console.log(
            "[chatReducer] Discard: time <= last final",
            text,
            isFinal,
            type
          );
          return updated;
        } else {
          if (lastNonFinalItem) {
            console.log(
              "[chatReducer] Update last non-final:",
              text,
              isFinal,
              type
            );
            updated[realLastNonFinalIndex] = newMsg;
          } else {
            console.log("[chatReducer] Add new item:", text, isFinal, type);
            updated.push(newMsg);
          }
        }
      } else {
        if (lastNonFinalItem) {
          console.log(
            "[chatReducer] Update last non-final:",
            text,
            isFinal,
            type
          );
          updated[realLastNonFinalIndex] = newMsg;
        } else {
          console.log("[chatReducer] Add new item:", text, isFinal, type);
          updated.push(newMsg);
        }
      }

      return updated.sort((a, b) => a.time - b.time);
    });
  }, []);

  return {
    chatItems,
    addChatItem,
  };
}
