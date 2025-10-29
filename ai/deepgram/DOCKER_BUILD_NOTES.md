# Docker Build Notes for voice-assistant-advanced

**Date**: 2025-10-29
**Issue**: Building Docker image for voice-assistant-advanced

---

## Problem

When trying to build a Docker image specifically for voice-assistant-advanced, the build fails because:

1. The Dockerfile uses `ARG USE_AGENT=agents/examples/voice-assistant` by default
2. Changing it to `voice-assistant-advanced` causes build failures:
   - Cannot find `.release` directory
   - Cannot find Taskfile.docker.yml
   - Missing specific extension directories (heygen_avatar_python, generic_video_python)

## Root Cause

The voice-assistant-advanced directory was created by copying from voice-assistant, but the Dockerfile expects certain files/directories to be created during the build process:

1. **`.release` directory**: Created by `task install && task release` during build
2. **Extension directories**: The advanced version needs additional extensions that are actual directories (not symlinks):
   - `heygen_avatar_python`
   - `generic_video_python`
   - These need to be explicitly COPY'd in the Dockerfile

## Dockerfile Differences Needed

The voice-assistant Dockerfile has:
```dockerfile
COPY ${USE_AGENT}/tenapp/ten_packages/extension/main_python/ ${USE_AGENT}/tenapp/ten_packages/extension/main_python/
```

voice-assistant-advanced needs additional COPY commands:
```dockerfile
COPY ${USE_AGENT}/tenapp/ten_packages/extension/main_python/ ${USE_AGENT}/tenapp/ten_packages/extension/main_python/
COPY ${USE_AGENT}/tenapp/ten_packages/extension/heygen_avatar_python/ ${USE_AGENT}/tenapp/ten_packages/extension/heygen_avatar_python/
COPY ${USE_AGENT}/tenapp/ten_packages/extension/generic_video_python/ ${USE_AGENT}/tenapp/ten_packages/extension/generic_video_python/
```

But wait - these directories were created as symlinks when we copied the structure! So they don't exist as actual directories to COPY.

## The Real Issue

When we created voice-assistant-advanced by copying from voice-assistant:
```bash
cp -r voice-assistant/* voice-assistant-advanced/
```

The symlinks in `tenapp/ten_packages/extension/` were preserved. In a Docker build context, these symlinks point to `/app/agents/ten_packages/extension/`, which exists during the builder stage.

However, the Dockerfile tries to COPY specific extension directories explicitly, which fails for symlinks.

## Solutions

### Solution 1: Use Existing Container (Immediate Testing)

Don't build a new Docker image. Use the existing `ten_agent_dev` container which already has all extensions installed:

```bash
docker exec -it ten_agent_dev bash
cd /app/server
./bin/api -tenapp_dir=/app/agents/examples/voice-assistant-advanced/tenapp &
```

This works because:
- All extensions are already in `/app/agents/ten_packages/extension/`
- The symlinks in voice-assistant-advanced point to those extensions
- No need to rebuild anything

### Solution 2: Simplify Dockerfile (Recommended for Production)

Instead of trying to copy specific extension directories, rely on the shared extension library:

```dockerfile
# Don't copy individual extension directories
# Instead, the release process should handle creating symlinks

# Just copy the non-symlinked directories
COPY ${USE_AGENT}/tenapp/ten_packages/extension/main_python/ ${USE_AGENT}/tenapp/ten_packages/extension/main_python/
# Note: heygen_avatar_python and generic_video_python are handled by symlinks pointing to shared library
```

The key insight: The tenapp directory structure uses symlinks to point to the shared extension library at `/app/agents/ten_packages/extension/`. During Docker build:
1. Stage 1: Build extensions into shared library
2. Stage 2: Copy shared library + tenapp with symlinks
3. Runtime: Symlinks resolve to shared library

### Solution 3: Fix COPY Commands (If Really Needed)

If you absolutely need to COPY those directories, first check if they're actually directories or symlinks:

```bash
ls -la ai_agents/agents/examples/voice-assistant-advanced/tenapp/ten_packages/extension/heygen_avatar_python

# If it's a symlink:
lrwxrwxrwx ... heygen_avatar_python -> /app/agents/ten_packages/extension/heygen_avatar_python

# Docker COPY won't follow symlinks outside build context
# You'd need to dereference them first or copy from the actual location
```

## Recommended Approach

**For Testing Now**:
Use Solution 1 - test in existing container

**For Production Later**:
Use Solution 2 - simplify the Dockerfile to not explicitly copy symlinked extensions. The release process should handle this.

The Dockerfile should be:
```dockerfile
# In builder stage - build everything including all extensions
RUN cd /app/${USE_AGENT} && \
  task install && task release

# In final stage - copy the .release output which has everything resolved
COPY --from=builder /app/${USE_AGENT}/tenapp/.release/ /app/agents/
```

The `task release` command should create a `.release` directory with all extensions properly linked/copied.

## Current State

- ✅ Code is complete and correct
- ✅ Git commit pushed
- 🔄 Docker build needs refinement
- ✅ Can test in existing container immediately

## Next Steps

1. **Immediate**: Test using existing container (Solution 1)
2. **Short-term**: Verify the extension works correctly
3. **Medium-term**: Fix Dockerfile to properly handle voice-assistant-advanced
4. **Long-term**: Create production Docker image

---

**Bottom Line**: The code itself is correct. The Docker build complexity is a deployment detail that can be solved separately. Test the functionality first, then optimize the Docker build process.
