//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import {
  forwardRef,
  useEffect,
  useImperativeHandle,
  useRef,
  useState,
} from "react";
import { Terminal as XTermTerminal } from "@xterm/xterm";
import { Unicode11Addon } from "@xterm/addon-unicode11";
import { WebLinksAddon } from "@xterm/addon-web-links";
import { FitAddon } from "@xterm/addon-fit";
import { TEN_PATH_WS_TERMINAL } from "@/constants";
import { getWSEndpointFromWindow } from "@/constants/utils";

import type { ITerminalWidgetData } from "@/types/widgets";

import "@xterm/xterm/css/xterm.css";

export interface ITerminalWidgetProps {
  id: string;
  data: ITerminalWidgetData;
  onClose?: () => void;
}

const TerminalWidget = forwardRef<unknown, ITerminalWidgetProps>(
  ({ data, onClose }, ref) => {
    const terminalRef = useRef<HTMLDivElement>(null);
    const xtermRef = useRef<XTermTerminal | null>(null);
    const ws = useRef<WebSocket | null>(null);
    const resizeObserverRef = useRef<ResizeObserver | null>(null);
    const fitAddonRef = useRef<FitAddon | null>(null);
    const [terminalSize, setTerminalSize] = useState<{
      cols: number;
      rows: number;
    }>({ cols: 80, rows: 24 });

    useEffect(() => {
      if (!terminalRef.current) {
        return;
      }

      if (!data.url) {
        return;
      }

      const xterm = new XTermTerminal({
        cursorBlink: true,
        macOptionIsMeta: true,
        convertEol: true,
        allowProposedApi: true,
      });

      xtermRef.current = xterm;

      const fitAddon = new FitAddon();
      fitAddonRef.current = fitAddon;
      xterm.loadAddon(fitAddon);

      xterm.loadAddon(new Unicode11Addon());
      xterm.loadAddon(new WebLinksAddon());

      // Due to the introduction of StrictMode in React 18, and the
      // fact that we enable StrictMode for more comprehensive checks,
      // React components will render twice under StrictMode. This
      // means that `useEffect` will be called once, followed by a
      // cleanup, and then called again. The `fit` method of the
      // `FitAddon` is an asynchronous operation. Therefore, if `fit`
      // is performed directly during the first `useEffect` call, it
      // may lead to issues when the `fit` action actually occurs, as
      // the HTML DOM element for the xterm might have already
      // disappeared. This causes xterm to throw an error about
      // missing dimensions (essentially due to the disappearance of
      // the xterm HTML DOM element).
      //
      // To overcome this issue, the direct `fit` action inside
      // `useEffect` is placed within a timer to asynchronously check
      // if the xterm HTML DOM element still exists. If the element
      // does not exist, the `fit` call is skipped to avoid this
      // problem. This workaround logic works properly in both
      // StrictMode and non-StrictMode environments.
      const timeoutId = setTimeout(() => {
        if (terminalRef.current) {
          xterm.open(terminalRef.current);
          fitAddon.fit(); // Initialize fit size.

          // Set keyboard focus when the Terminal is shown.
          xterm.focus();

          // Change the terminal size.
          setTerminalSize({ cols: xterm.cols, rows: xterm.rows });
        } else {
          console.warn(
            "Terminal container no longer exists during initialization."
          );
        }
      }, 0);

      // Initialize the websocket connection to the backend.
      const wsUrl =
        getWSEndpointFromWindow() +
        TEN_PATH_WS_TERMINAL +
        `?path=${encodeURIComponent(data.url)}`;
      ws.current = new WebSocket(wsUrl);

      ws.current.onopen = () => {
        console.log("WebSocket to the backend is connected!");
        sendResize(xterm.cols, xterm.rows);
      };

      ws.current.onmessage = async (event) => {
        if (!xtermRef.current) {
          return;
        }

        // Handle the data from the backend.
        if (event.data instanceof Blob) {
          try {
            const text = await event.data.text();
            xtermRef.current.write(text);
          } catch (error) {
            console.error(
              "Failed to convert received blob from backend:",
              error
            );
          }
        } else if (typeof event.data === "string") {
          // Try to parse the string data as a JSON.
          try {
            const msg = JSON.parse(event.data);

            // Check if its a `exit` message.
            if (msg.type === "exit") {
              // Display a exit message in the terminal UI.
              xtermRef.current.writeln(
                `\r\nProcess exited with code ${msg.code}\r\n`
              );

              // Close the websocket actively.
              ws.current?.close();

              // Close the terminal popup.
              onClose?.();
              return;
            }
            // eslint-disable-next-line @typescript-eslint/no-unused-vars
          } catch (e) {
            // It's not a JSON message, write it to the terminal UI directly.
          }

          // Output to the terminal UI.
          xtermRef.current.write(event.data);
        } else if (event.data instanceof ArrayBuffer) {
          const uint8Array = new Uint8Array(event.data);
          xtermRef.current.write(uint8Array);
        } else {
          console.warn("Unknown received data type:", typeof event.data);
        }
      };

      ws.current.onerror = (err) => {
        console.error("WebSocket error:", err);
      };

      ws.current.onclose = () => {
        console.log("WebSocket closed!");
        if (xtermRef.current) {
          xtermRef.current.writeln("\r\nConnection closed.");
        }
      };

      xterm.onData(handleInput);

      const handleResize = () => {
        if (fitAddonRef.current && xtermRef.current) {
          // Fit xterm to its DOM container, and enable xterm to calculate the
          // updated cols/rows.
          fitAddonRef.current.fit();

          // Get the updated cols/rows from xterm, and update to the backend.
          const cols = xtermRef.current.cols;
          const rows = xtermRef.current.rows;

          // Change the terminal size.
          setTerminalSize({ cols, rows });

          // Notify the backend that the size of terminal should be changed.
          sendResize(cols, rows);
        }
      };

      resizeObserverRef.current = new ResizeObserver(() => {
        handleResize();
      });

      resizeObserverRef.current.observe(terminalRef.current);

      return () => {
        // Cleanup.

        // Remove the timer.
        clearTimeout(timeoutId);

        // Remove the resize handler.
        resizeObserverRef.current?.disconnect();

        // Close the websocket connection.
        ws.current?.close();

        // Close the xterm.
        xterm.dispose();
      };
      // eslint-disable-next-line react-hooks/exhaustive-deps
    }, [data.url]);

    useEffect(() => {
      console.log("Terminal size updated:", terminalSize);
    }, [terminalSize]);

    const handleInput = (data: string) => {
      if (ws.current && ws.current.readyState === WebSocket.OPEN) {
        ws.current.send(data);
      }
    };

    const sendResize = (cols: number, rows: number) => {
      if (ws.current && ws.current.readyState === WebSocket.OPEN) {
        const resizeMessage = JSON.stringify({
          type: "resize",
          cols,
          rows,
        });
        ws.current.send(resizeMessage);
      }
    };

    useImperativeHandle(ref, () => ({
      handleCollapseToggle(isCollapsed: boolean) {
        if (!isCollapsed && fitAddonRef.current) {
          // Wait for DOM updates before fitting the terminal.
          setTimeout(() => {
            if (terminalRef.current) {
              terminalRef.current.style.display = "block"; // Restore display.
            }
            fitAddonRef.current?.fit();
          }, 0);
        } else if (isCollapsed && terminalRef.current) {
          // Hide the terminal container on collapse.
          terminalRef.current.style.display = "none";
        }
      },
    }));

    return <div ref={terminalRef} className="flex-1 w-full h-full bg-black" />;
  }
);

export default TerminalWidget;
