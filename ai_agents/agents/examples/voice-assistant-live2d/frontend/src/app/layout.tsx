import type { Metadata } from "next";
import { Inter } from "next/font/google";
import "./globals.css";

const inter = Inter({ subsets: ["latin"] });

export const metadata: Metadata = {
  title: "Live2D Voice Assistant",
  description: "Real-time voice assistant with Live2D character",
};

export default function RootLayout({
  children,
}: {
  children: React.ReactNode;
}) {
  return (
    <html lang="en">
      <head>
        <script src="/lib/live2dcubismcore.min.js" async></script>
      </head>
      <body className={inter.className}>{children}</body>
    </html>
  );
}
