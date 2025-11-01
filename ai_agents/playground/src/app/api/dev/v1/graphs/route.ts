import { type NextRequest, NextResponse } from 'next/server';

export async function POST(request: NextRequest) {
  try {
    const body = await request.json();
    const agentServerUrl = body.url || process.env.AGENT_SERVER_URL || 'http://localhost:8080';
    
    const response = await fetch(`${agentServerUrl}/graphs`);
    const data = await response.json();
    
    return NextResponse.json(data, { status: response.status });
  } catch (error) {
    console.error('Error fetching graphs:', error);
    return NextResponse.json(
      { code: '1', data: [], msg: 'Failed to fetch graphs' },
      { status: 500 }
    );
  }
}
