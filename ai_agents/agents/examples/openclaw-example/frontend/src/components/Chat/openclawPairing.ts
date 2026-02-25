export function isPairingRequiredMessage(text: string): boolean {
  return text.toLowerCase().includes("pairing is required");
}

export function extractApproveCommand(text: string): string {
  const match = text.match(/^openclaw devices approve.*$/m);
  return match ? match[0].trim() : "";
}

export function extractListCommand(text: string): string {
  const match = text.match(/^openclaw devices list.*$/m);
  return match ? match[0].trim() : "";
}
