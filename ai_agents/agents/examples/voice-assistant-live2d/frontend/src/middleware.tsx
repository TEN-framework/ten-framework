// middleware.js
import { type NextRequest, NextResponse } from "next/server";

/**
 * Environment Variables Required:
 * - AGENT_SERVER_URL: The URL of your agent server (typically http://localhost:8080)
 *
 * Example .env.local:
 * AGENT_SERVER_URL=http://localhost:8080
 */

const { AGENT_SERVER_URL } = process.env;

// Check if environment variables are available
if (!AGENT_SERVER_URL) {
  throw "Environment variables AGENT_SERVER_URL are not available";
}

export async function middleware(req: NextRequest) {
  const { pathname } = req.nextUrl;
  const url = req.nextUrl.clone();

  console.log("Middleware triggered for:", pathname);

  if (pathname.startsWith(`/api/agents/`)) {
    // Proxy agents API requests to the agent server (port 8080)
    url.href = `${AGENT_SERVER_URL}${pathname.replace("/api/agents/", "/")}`;

    try {
      const body = await req.json();
      console.log("Agents request to", pathname, "with body:", body);
    } catch (e) {
      console.log("Agents request to", pathname, "(no body):", e);
    }

    console.log("Rewriting agents request from", pathname, "to", url.href);
    return NextResponse.rewrite(url);
  } else if (pathname.startsWith(`/api/token/`)) {
    // Proxy token requests to the agent server (port 8080)
    url.href = `${AGENT_SERVER_URL}${pathname.replace("/api/token/", "/token/")}`;

    try {
      const body = await req.json();
      console.log("Token request to", pathname, "with body:", body);
    } catch (e) {
      console.log("Token request to", pathname, "(no body):", e);
    }

    console.log("Rewriting token request from", pathname, "to", url.href);
    return NextResponse.rewrite(url);
  } else {
    console.log("No rewrite needed for:", pathname);
    return NextResponse.next();
  }
}

export const config = {
  matcher: ["/api/agents/:path*", "/api/token/:path*"],
};
