import { NextRequest, NextResponse } from 'next/server';

export function middleware(req: NextRequest) {
  const { pathname } = req.nextUrl;
  const AGENT_SERVER_URL = process.env.AGENT_SERVER_URL;

  // If env is missing, do not break the app; just pass through.
  if (!AGENT_SERVER_URL) {
    return NextResponse.next();
  }

  if (pathname.startsWith('/api/token/')) {
    const url = req.nextUrl.clone();
    url.href = `${AGENT_SERVER_URL}${pathname.replace('/api/token/', '/token/')}`;
    return NextResponse.rewrite(url);
  }

  if (pathname.startsWith('/api/agents/') && !pathname.startsWith('/api/agents/start')) {
    const url = req.nextUrl.clone();
    url.href = `${AGENT_SERVER_URL}${pathname.replace('/api/agents/', '/')}`;
    return NextResponse.rewrite(url);
  }

  return NextResponse.next();
}

// Only run middleware for API routes
export const config = {
  matcher: ['/api/:path*'],
}
