export {};

declare global {
  interface Window {
    openclawGateway?: {
      connect: () => void;
      disconnect: () => void;
      send: (message: string) => Promise<void>;
      isConnected: () => boolean;
    };
  }
}
