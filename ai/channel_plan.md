# HeyGen Channel Forwarding - Analysis and Solutions Plan

## Implementation Status

**Current State (as of 2025-11-05):**
- ✅ Signal handlers removed from heygen_avatar_python (commit 2c5d15f59 on `feat/deepgram-v2`)
- ⚠️ Hardcoded channels updated from `agora_ant2w9` → `agora_g3qhjr` (temporary, still hardcoded)
- ⏳ Dynamic channel forwarding pending (requires new branch)

**Branch Strategy:**
- `feat/deepgram-v2`: Contains heygen signal handler removal (already pushed)
- `feat/heygen-dynamic-channel`: Will contain server config changes + this document
  - Changes shared `server/internal/config.go` (affects all deployments)
  - Requires separate review/testing before merge

## Problem Statement

The HeyGen avatar extension currently uses a hardcoded channel name (now `agora_g3qhjr`, previously `agora_ant2w9`) instead of receiving the dynamic channel name sent by the playground client. This causes all HeyGen sessions to attempt joining the same Agora channel, preventing multiple concurrent sessions from working correctly.

When a user connects via the playground client with a randomly generated channel (e.g., `agora_xk42j9`), the dynamic channel is correctly forwarded to `agora_rtc` and `agora_rtm` extensions, but NOT to the `heygen_avatar_python` extension.

## Root Cause

The Go server's `startPropMap` configuration in `server/internal/config.go` is **missing the heygen avatar extension**.

Current mapping (lines 33-36):
```go
startPropMap = map[string][]Prop{
    "ChannelName": {
        {ExtensionName: extensionNameAgoraRTC, Property: "channel"},  // "agora_rtc"
        {ExtensionName: extensionNameAgoraRTM, Property: "channel"},  // "agora_rtm"
        // MISSING: heygen avatar mapping!
    },
}
```

The `processProperty()` function only injects properties for extensions listed in `startPropMap`. Since the heygen avatar extension (node name: `"avatar"`) is not in the map, it continues using the hardcoded value from `property.json`.

## Complete Data Flow

```
┌─────────────────────────────────────────────────────────────────┐
│ PLAYGROUND CLIENT                                                │
│ - Generates: "agora_xyz123"                                      │
│ - Stores in: Redux state.global.options.channel                  │
└─────────────────────┬───────────────────────────────────────────┘
                      │ POST /api/agents/start
                      │ { channel_name: "agora_xyz123" }
                      ↓
┌─────────────────────────────────────────────────────────────────┐
│ NEXT.JS API ROUTE (playground/src/app/api/agents/start)         │
│ - Forwards to Go server                                          │
└─────────────────────┬───────────────────────────────────────────┘
                      │ POST /start
                      ↓
┌─────────────────────────────────────────────────────────────────┐
│ GO SERVER: handlerStart() (server/internal/http_server.go)      │
│ - Reads: StartReq.ChannelName = "agora_xyz123"                   │
│ - Validates channel                                              │
└─────────────────────┬───────────────────────────────────────────┘
                      │
                      ↓
┌─────────────────────────────────────────────────────────────────┐
│ GO SERVER: processProperty()                                     │
│ 1. Reads: property.json from tenapp                              │
│ 2. Filters to selected graph                                     │
│ 3. Uses startPropMap to inject channel:                          │
│    ✅ agora_rtc.channel = "agora_xyz123"                         │
│    ✅ agora_rtm.channel = "agora_xyz123"                         │
│    ❌ avatar.channel = "agora_g3qhjr" (STILL HARDCODED!)        │
│ 4. Creates temporary property.json                               │
└─────────────────────┬───────────────────────────────────────────┘
                      │ Launches TEN app
                      ↓
┌─────────────────────────────────────────────────────────────────┐
│ TEN FRAMEWORK                                                    │
│ Extensions load config from modified property.json:              │
│ - agora_rtc: channel = "agora_xyz123" ✅                         │
│ - agora_rtm: channel = "agora_xyz123" ✅                         │
│ - heygen_avatar_python: channel = "agora_g3qhjr" ❌             │
└─────────────────────┬───────────────────────────────────────────┘
                      │
                      ↓
┌─────────────────────────────────────────────────────────────────┐
│ HEYGEN EXTENSION: on_start()                                     │
│ - Creates AgoraHeygenRecorder with channel="agora_g3qhjr"        │
│ - Sends to HeyGen API                                            │
└─────────────────────┬───────────────────────────────────────────┘
                      │
                      ↓
┌─────────────────────────────────────────────────────────────────┐
│ HEYGEN API                                                       │
│ - Creates avatar session                                         │
│ - Avatar joins WRONG channel: "agora_g3qhjr"                     │
└─────────────────────────────────────────────────────────────────┘

RESULT: User on channel "agora_xyz123" cannot see avatar!
        Avatar is in different channel "agora_g3qhjr"
```

## Solution: Add Avatar to startPropMap

**Files to change:**

### `server/internal/config.go`

Add the avatar extension constant and mapping (2 lines total):

```go
const (
	// Extension name
	extensionNameAgoraRTC   = "agora_rtc"
	extensionNameAgoraRTM   = "agora_rtm"
	extensionNameHttpServer = "http_server"
	extensionNameAvatar     = "avatar"        // ADD THIS LINE
)

var (
	logTag = slog.String("service", "HTTP_SERVER")

	// Retrieve parameters from the request and map them to the property.json file
	startPropMap = map[string][]Prop{
		"ChannelName": {
			{ExtensionName: extensionNameAgoraRTC, Property: "channel"},
			{ExtensionName: extensionNameAgoraRTM, Property: "channel"},
			{ExtensionName: extensionNameAvatar, Property: "channel"},  // ADD THIS LINE
		},
		// ... rest of mappings
	}
)
```

**How it works:**
1. Server receives dynamic channel from client API call
2. `processProperty()` now injects channel into `avatar` extension (in addition to agora_rtc and agora_rtm)
3. HeyGen extension loads dynamic channel from modified property.json
4. Avatar joins correct channel matching the user

**Why this is the proper solution:**
- ✅ Clean, maintainable
- ✅ Follows existing pattern for `agora_rtc` and `agora_rtm`
- ✅ Works for all graphs with HeyGen avatar
- ✅ No code changes needed in Python extensions
- ✅ Centralized configuration in Go server
- ✅ Consistent with framework design

## Alternative Solutions (Not Recommended)

### Option 2: TEN Framework Message Passing
**Complexity**: Medium-High | **Risk**: Medium

Send channel from `main_python` extension to heygen via TEN commands. Issues: timing, initialization order, requires Python changes.

### Option 3: Environment Variable
**Complexity**: Medium | **Risk**: Medium-High

Pass channel via env var when launching app. Issues: environment pollution, not discoverable, inconsistent with framework patterns.

### Option 4: Properties Override in API Call
**Complexity**: Low | **Risk**: Low

Client sends channel in both `channel_name` AND `properties.avatar.channel`. Issues: duplicates data, requires frontend change for every client.

See full analysis below for details on these alternatives.

## Recommended Implementation Plan

### ✅ Phase 0: Remove Signal Handlers (COMPLETED)
**Status**: Completed and pushed to `feat/deepgram-v2` (commit 2c5d15f59)
- Removed signal handlers from heygen_avatar_python extension
- Fixes ValueError: signal only works in main thread
- Extension now initializes cleanly

### Phase 1: Proper Solution - NEW BRANCH REQUIRED
**Branch**: `feat/heygen-dynamic-channel` (create from `main`)
**Time**: 15 minutes + testing + review
**Why**: Clean, maintainable, follows framework patterns

**Files to change:**
1. `server/internal/config.go` (2 lines added - see above)
2. `ai/channel_plan.md` (this file)

**Testing checklist:**
- [ ] Rebuild Go server: `cd server && go build -o bin/api main.go`
- [ ] Restart server with new binary
- [ ] Test multiple concurrent heygen sessions with different channels
- [ ] Verify channel logged correctly in heygen extension startup
- [ ] Check all 3 heygen graphs work:
  - `voice_assistant_heygen`
  - `flux_thymia_heygen_cartesia`
  - `flux_thymia_heygen_rime`
- [ ] Verify avatars join correct channels (check Agora Console)
- [ ] Test session cleanup (no leaked channels)

**Why separate branch:**
- Changes `server/internal/config.go` which is shared across ALL agent deployments
- Requires thorough testing and review before merging to main
- Allows independent review/merge cycle from `feat/deepgram-v2`

### Phase 2: Alternative Quick Fix (Option 4) - IF NEEDED
**Only if Phase 1 blocked or needs urgent production fix**

Update `playground/src/common/request.ts` to use properties override:
```typescript
const data = {
  request_id: genUUID(),
  channel_name: channel,
  user_uid: userId,
  graph_name: graphName,
  properties: {
    avatar: { channel: channel }  // Add this
  },
  // ...
};
```

**Note**: This is a workaround. Phase 1 is the proper solution.

### Phase 3: Cleanup (After Phase 1 merges)
1. Update `AI_working_with_ten.md` explaining dynamic channel flow
2. Add comment in `server/internal/config.go` explaining avatar mapping
3. Consider validation that all three extensions receive same channel
4. Update heygen graphs to use placeholder channel (will be overridden)

## Testing Verification

After implementing the fix, verify:

- [ ] Multiple concurrent sessions work (different channels)
- [ ] HeyGen avatar appears in playground
- [ ] Avatar joins same channel as user (verify in Agora Console)
- [ ] Check logs show correct channel:
  ```
  Avatar config: avatar_name=Wayne_20240711, channel=agora_xyz123
  ```
- [ ] Works for all heygen graphs
- [ ] No channel conflicts between sessions
- [ ] Session cleanup works (no leaked channels)

## Impact of Not Fixing

Without this fix:
- Only ONE concurrent HeyGen session can work at a time
- All users compete for the same hardcoded channel
- Avatars appear in wrong channels for most users
- Production deployment to public demo server will fail for multiple users
- User experience severely degraded

## Quick Reference for Implementation

```bash
# Create new branch for server changes
cd /home/ubuntu/ten-framework/ai_agents
git checkout main
git pull
git checkout -b feat/heygen-dynamic-channel

# Apply server config changes (see above)
# Edit: server/internal/config.go
# Add this file: ai/channel_plan.md

# Test
cd server
go build -o bin/api main.go
# Kill old server process
# Start new server: ./bin/api -tenapp_dir=...
# Test with playground

# Commit and push
git add server/internal/config.go ../ai/channel_plan.md
git commit -m "feat: add dynamic channel forwarding to heygen avatar extension

- Add avatar extension to server's startPropMap
- Enables dynamic channel forwarding from client to heygen
- Fixes hardcoded channel preventing concurrent sessions
- Allows multiple users to use heygen avatars simultaneously

Includes comprehensive analysis document explaining the issue,
data flow, and implementation approach."

git push origin feat/heygen-dynamic-channel
# Create PR for review
```

## Conclusion

**Current state**:
- ✅ Signal handlers removed (fixes production crash)
- ⚠️ Channel still hardcoded to `agora_g3qhjr`
- ⏳ Dynamic forwarding needs server changes in new branch

**Next steps**:
1. Create `feat/heygen-dynamic-channel` branch
2. Apply 2-line fix to `server/internal/config.go`
3. Test thoroughly with multiple concurrent sessions
4. Submit PR for review/merge
5. Deploy to production after approval

This fix enables the production demo server to support multiple concurrent HeyGen avatar sessions, which is critical for public deployment.
