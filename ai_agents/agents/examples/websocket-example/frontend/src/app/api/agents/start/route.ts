import axios from "axios";
import { type NextRequest, NextResponse } from "next/server";

export async function POST(request: NextRequest) {
  try {
    const body = await request.json();

    const agentServerUrl =
      process.env.AGENT_SERVER_URL || "http://localhost:8080";

    const response = await axios.post(`${agentServerUrl}/start`, body);

    return NextResponse.json(response.data);
  } catch (error) {
    console.error("Failed to start agent:", error);

    if (axios.isAxiosError(error)) {
      return NextResponse.json(
        { error: error.response?.data || error.message },
        { status: error.response?.status || 500 },
      );
    }

    return NextResponse.json(
      { error: "Failed to start agent" },
      { status: 500 },
    );
  }
}
