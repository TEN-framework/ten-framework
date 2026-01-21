// middleware.tsx
import { type NextRequest, NextResponse } from "next/server";

// Use environment variable or default to localhost
const AGENT_SERVER_URL =
  process.env.AGENT_SERVER_URL || "http://localhost:8080";

export async function middleware(req: NextRequest) {
  const { pathname } = req.nextUrl;
  const url = req.nextUrl.clone();

  if (pathname.startsWith(`/api/agents/`)) {
    // Proxy agents API requests to backend
    url.href = `${AGENT_SERVER_URL}${pathname.replace("/api/agents/", "/")}`;
    console.log(`[Middleware] Rewriting ${pathname} to ${url.href}`);
    return NextResponse.rewrite(url);
  } else if (pathname.startsWith(`/api/token/`)) {
    // Proxy token API requests to backend
    url.href = `${AGENT_SERVER_URL}${pathname.replace("/api/token/", "/token/")}`;
    console.log(`[Middleware] Rewriting ${pathname} to ${url.href}`);
    return NextResponse.rewrite(url);
  } else {
    return NextResponse.next();
  }
}
