// proxy.ts
import { type NextRequest, NextResponse } from "next/server";

const { AGENT_SERVER_URL, TEN_DEV_SERVER_URL } = process.env;

// Check if environment variables are available
if (!AGENT_SERVER_URL) {
  throw "Environment variables AGENT_SERVER_URL are not available";
}

if (!TEN_DEV_SERVER_URL) {
  throw "Environment variables TEN_DEV_SERVER_URL are not available";
}

/**
 * Validate port number is in allowed range (8000-9000)
 * @param port - Port number as string
 * @returns true if valid, false otherwise
 */
function validatePort(port: string): boolean {
  const portNum = parseInt(port, 10);
  return !Number.isNaN(portNum) && portNum >= 8000 && portNum <= 9000;
}

export async function proxy(req: NextRequest) {
  const { pathname } = req.nextUrl;
  const url = req.nextUrl.clone();

  // Handle /proxy/{port_number}/* requests (POST only)
  const proxyMatch = pathname.match(/^\/proxy\/(\d+)(\/.*)?$/);
  if (proxyMatch && req.method === "POST") {
    const portNumber = proxyMatch[1];
    const path = proxyMatch[2] || "/";

    // Validate port range (8000-9000)
    if (!validatePort(portNumber)) {
      console.warn(
        `Rejected proxy request: Invalid port ${portNumber} (must be 8000-9000)`
      );
      return NextResponse.json(
        { error: "Invalid port number. Port must be between 8000 and 9000." },
        { status: 400 }
      );
    }

    // Extract hostname from AGENT_SERVER_URL
    const agentUrl = new URL(AGENT_SERVER_URL!);
    const hostname = agentUrl.hostname;

    // Rewrite to http://{hostname}:{port_number}{path}
    url.href = `http://${hostname}:${portNumber}${path}`;
    console.log(`Proxying POST request to ${url.href}`);
    return NextResponse.rewrite(url);
  }

  if (pathname.startsWith(`/api/agents/`)) {
    // if (!pathname.startsWith('/api/agents/start')) {
    // Proxy all other agents API requests
    url.href = `${AGENT_SERVER_URL}${pathname.replace("/api/agents/", "/")}`;

    try {
      const _body = await req.json();
      console.log(`Request to ${pathname}`);
    } catch (e) {
      console.log(`Request to ${pathname} ${e}`);
    }

    // console.log(`Rewriting request to ${url.href}`);
    return NextResponse.rewrite(url);
    // } else {
    //     return NextResponse.next();
    // }
  } else if (pathname.startsWith(`/api/vector/`)) {
    // Proxy all other documents requests
    url.href = `${AGENT_SERVER_URL}${pathname.replace("/api/vector/", "/vector/")}`;

    // console.log(`Rewriting request to ${url.href}`);
    return NextResponse.rewrite(url);
  } else if (pathname.startsWith(`/api/token/`)) {
    // Proxy all other documents requests
    url.href = `${AGENT_SERVER_URL}${pathname.replace("/api/token/", "/token/")}`;

    // console.log(`Rewriting request to ${url.href}`);
    return NextResponse.rewrite(url);
  } else if (pathname.startsWith("/api/dev/")) {
    if (pathname.startsWith("/api/dev/v1/addons/default-properties")) {
      url.href = `${AGENT_SERVER_URL}/dev-tmp/addons/default-properties`;
      console.log(`Rewriting request to ${url.href}`);
      return NextResponse.rewrite(url);
    }

    url.href = `${TEN_DEV_SERVER_URL}${pathname.replace("/api/dev/", "/api/designer/")}`;

    // console.log(`Rewriting request to ${url.href}`);
    return NextResponse.rewrite(url);
  } else {
    return NextResponse.next();
  }
}

// Configure which routes should be processed by the proxy
export const config = {
  matcher: ["/api/:path*", "/proxy/:path*"],
};
