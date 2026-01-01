import type { Metadata, Viewport } from "next";
import { StoreProvider } from "@/store";
import "./global.css";
import { Roboto } from "next/font/google";
import Script from "next/script";
import { Toaster } from "@/components/ui/sonner";
import { cn } from "@/lib/utils";

const roboto = Roboto({
  subsets: ["latin"],
  weight: ["400", "700"],
  variable: "--font-roboto",
  display: "swap",
});

export const metadata: Metadata = {
  title: "TEN Agent",
  description: "Multi-Purpose Voice Assistant Agent Example Powered by TEN",
  appleWebApp: {
    capable: true,
    statusBarStyle: "black",
  },
};

export const viewport: Viewport = {
  width: "device-width",
  initialScale: 1,
  minimumScale: 1,
  maximumScale: 1,
  userScalable: false,
  viewportFit: "cover",
};

export default function RootLayout({
  children,
}: Readonly<{
  children: React.ReactNode;
}>) {
  return (
    <html lang="en">
      <body className={cn("dark", "antialiased", roboto.variable)}>
        {/* <ConfigProvider
          theme={{
            components: {
              Select: {
                selectorBg: "#181A1D",
              },
            },
          }}
        > */}
        <StoreProvider>{children}</StoreProvider>
        {/* </ConfigProvider> */}
        <Toaster richColors closeButton theme="dark" />
        <Script id="scarf-analytics" strategy="afterInteractive">
          {`
          (function() {
            const pixelID = '1295f95b-eaaa-4f5c-8861-825c0e350978';
            let lastHref = null;

            function sendScarfPing() {
              const currentHref = window.location.href;
              if (currentHref === lastHref) return;
              lastHref = currentHref;

              const url = \`https://static.scarf.sh/a.png?x-pxid=\${pixelID}\`;
              const img = new Image();
              img.referrerPolicy = 'no-referrer-when-downgrade';
              img.src = url;
            }

            ['pushState', 'replaceState'].forEach(fn => {
              const original = history[fn];
              history[fn] = function() {
                original.apply(this, arguments);
                window.dispatchEvent(new Event('scarf:locationchange'));
              };
            });

            window.addEventListener('hashchange', sendScarfPing);
            window.addEventListener('popstate', sendScarfPing);
            window.addEventListener('scarf:locationchange', sendScarfPing);

            sendScarfPing();
          })();
          `}
        </Script>
      </body>
    </html>
  );
}
