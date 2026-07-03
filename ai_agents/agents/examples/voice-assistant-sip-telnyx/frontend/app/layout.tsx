import type { Metadata } from 'next';
import './globals.css';

export const metadata: Metadata = {
  title: 'Telnyx Voice Assistant',
  description: 'Real-time voice assistant powered by TEN Framework and Telnyx',
};

export default function RootLayout({
  children,
}: {
  children: React.ReactNode;
}) {
  return (
    <html lang="en">
      <body className="bg-gray-100">{children}</body>
    </html>
  );
}