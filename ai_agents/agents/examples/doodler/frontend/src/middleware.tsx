// middleware.tsx
import { type NextRequest, NextResponse } from "next/server";

const AGENT_SERVER_URL = process.env.AGENT_SERVER_URL || "http://localhost:8080";
const TEN_DEV_SERVER_URL = process.env.TEN_DEV_SERVER_URL || "http://localhost:49483";

export async function middleware(req: NextRequest) {
  const { pathname } = req.nextUrl;
  const url = req.nextUrl.clone();

  if (pathname.startsWith(`/api/agents/`)) {
    // Proxy all other agents API requests
    url.href = `${AGENT_SERVER_URL}${pathname.replace("/api/agents/", "/")}`;
    return NextResponse.rewrite(url);
  } else if (pathname.startsWith(`/api/vector/`)) {
    // Proxy all other documents requests
    url.href = `${AGENT_SERVER_URL}${pathname.replace("/api/vector/", "/vector/")}`;

    return NextResponse.rewrite(url);
  } else if (pathname.startsWith(`/api/token/`)) {
    // Proxy all other documents requests
    url.href = `${AGENT_SERVER_URL}${pathname.replace("/api/token/", "/token/")}`;

    return NextResponse.rewrite(url);
  } else if (pathname.startsWith("/api/dev/")) {
    if (pathname.startsWith("/api/dev/v1/addons/default-properties")) {
      url.href = `${AGENT_SERVER_URL}/dev-tmp/addons/default-properties`;
      return NextResponse.rewrite(url);
    }

    url.href = `${TEN_DEV_SERVER_URL}${pathname.replace("/api/dev/", "/api/designer/")}`;

    return NextResponse.rewrite(url);
  }

  return NextResponse.next();
}
