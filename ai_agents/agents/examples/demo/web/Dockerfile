FROM oven/bun:alpine AS base

# 1. Install dependencies only when needed
FROM base AS deps
# Check https://github.com/nodejs/docker-node/tree/b4117f9333da4138b03a546ec926ef50a31506c3#nodealpine to understand why libc6-compat might be needed.
RUN apk add --no-cache libc6-compat

WORKDIR /app

# Install dependencies based on the preferred package manager
COPY package.json bun.lock ./
RUN \
  if [ -f yarn.lock ]; then corepack enable yarn && yarn --immutable; \
  elif [ -f package-lock.json ]; then npm ci; \
  elif [ -f pnpm-lock.yaml ]; then npm install -g pnpm@10.2.0 && pnpm i; \
  elif [ -f bun.lockb ]; then bun i --frozen-lockfile; \
  elif [ -f bun.lock ]; then bun i --frozen-lockfile; \
  else echo "Lockfile not found." && exit 1; \
  fi

# 2. Rebuild the source code only when needed
FROM base AS builder
WORKDIR /app
COPY --from=deps /app/node_modules ./node_modules
COPY . .

# Ensure '@/lib/utils' exists and exports cn/useIsMobileScreen in CI builds.
# If missing, synthesize it with the expected helpers instead of copying unrelated files.
RUN if [ ! -f src/lib/utils.ts ]; then \
      mkdir -p src/lib && \
      printf '%s\n' \
        'import * as React from "react"' \
        'import { clsx, type ClassValue } from "clsx"' \
        'import { twMerge } from "tailwind-merge"' \
        '' \
        'export function cn(...inputs: ClassValue[]) {' \
        '  return twMerge(clsx(inputs))' \
        '}' \
        '' \
        'export function useIsMobileScreen(breakpoint?: string) {' \
        '  const [isMobileScreen, setIsMobileScreen] = React.useState(false)' \
        '' \
        '  React.useEffect(() => {' \
        '    const mql = window.matchMedia(`(max-width: ${breakpoint ?? "768px"})`)' \
        '    setIsMobileScreen(mql.matches)' \
        '    const listener = () => setIsMobileScreen(mql.matches)' \
        '    mql.addEventListener("change", listener)' \
        '' \
        '    return () => mql.removeEventListener("change", listener)' \
        '  }, [breakpoint])' \
        '' \
        '  return isMobileScreen' \
        '}' \
        '' \
        'export function formatNumber(num: number, decimals: number = 1): string {' \
        '  if (num === 0) return "0"' \
        '  const k = 1000' \
        '  const sizes = ["", "K", "M", "B", "T"]' \
        '  const i = Math.floor(Math.log(Math.abs(num)) / Math.log(k))' \
        '  if (i === 0) return num.toString()' \
        '  const scaled = num / Math.pow(k, i)' \
        '  return `${scaled.toFixed(decimals)}${sizes[i]}`' \
        '}' \
        > src/lib/utils.ts ; \
    fi

# Next.js collects completely anonymous telemetry data about general usage.
# Learn more here: https://nextjs.org/telemetry
# Uncomment the following line in case you want to disable telemetry during the build.
ENV NEXT_TELEMETRY_DISABLED=1

RUN \
  if [ -f yarn.lock ]; then corepack enable yarn && yarn run build; \
  elif [ -f package-lock.json ]; then npm run build; \
  elif [ -f pnpm-lock.yaml ]; then npm install -g pnpm@10.2.0 && pnpm run build; \
  elif [ -f bun.lockb ]; then bun run build; \
  elif [ -f bun.lock ]; then bun run build; \
  else echo "Lockfile not found." && exit 1; \
  fi

# 3. Production image, copy all the files and run next
FROM base AS runner
WORKDIR /app

# Uncomment the following line in case you want to disable telemetry during runtime.
ENV NEXT_TELEMETRY_DISABLED=1

ENV NODE_ENV=production

RUN addgroup --system --gid 1001 nodejs
RUN adduser --system --uid 1001 nextjs

# COPY --from=builder /app/public ./public

# Set the correct permission for prerender cache
RUN mkdir .next
RUN chown nextjs:nodejs .next

# Automatically leverage output traces to reduce image size
# https://nextjs.org/docs/advanced-features/output-file-tracing
COPY --from=builder --chown=nextjs:nodejs /app/.next/standalone ./
COPY --from=builder --chown=nextjs:nodejs /app/.next/static ./.next/static

USER nextjs

EXPOSE 3000

ENV PORT=3000

# server.js is created by next build from the standalone output
# https://nextjs.org/docs/pages/api-reference/next-config-js/output
CMD HOSTNAME="0.0.0.0" node server.js
