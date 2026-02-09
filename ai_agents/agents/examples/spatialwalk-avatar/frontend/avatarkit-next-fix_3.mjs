/**
 * Next.js WASM 临时修复文件
 * 解决 withAvatarkit 找不到 WASM 文件的问题
 *
 * 用法：
 *   import { withAvatarkitFixed } from './avatarkit-next-fix.mjs'
 *   export default withAvatarkitFixed({ ...your next config... })
 *
 * 等 SDK 发布修复版本后，换回：
 *   import { withAvatarkit } from '@spatialwalk/avatarkit/next'
 */

import { createRequire } from 'module'
import { dirname, join } from 'path'
import { readFileSync, readdirSync, writeFileSync, mkdirSync, existsSync } from 'fs'

// 通过 ./next 导出定位 SDK 根目录（避免 package.json 未导出的问题）
const require = createRequire(import.meta.url)
const sdkNextPath = require.resolve('@spatialwalk/avatarkit/next')
const sdkDir = dirname(sdkNextPath)
const sdkDistDir = join(sdkDir, 'dist')

// ── Loader: 修复 Emscripten scriptDirectory ──
// 关键改进：使用更简单的正则，只匹配赋值行而非整个 try/catch
// 并加入详细的诊断日志
const LOADER_CODE = `module.exports = function(source) {
  console.log('[avatarkit-loader] Applied to:', this.resourcePath);

  // 策略1: 只替换赋值语句（不依赖 try/catch 语法）
  var pattern1 = /scriptDirectory\\s*=\\s*new\\s+URL\\(\\s*"\\."\\s*,\\s*_scriptName\\s*\\)\\.href\\s*;/;
  var result = source.replace(pattern1, 'scriptDirectory = "/_next/static/chunks/";');

  if (result !== source) {
    console.log('[avatarkit-loader] SUCCESS: scriptDirectory replaced via pattern1');
    return result;
  }

  // 策略2: 匹配完整 try/catch 块（兼容不同 catch 写法）
  var pattern2 = /try\\s*\\{\\s*scriptDirectory\\s*=\\s*new\\s+URL\\(\\s*"\\."\\s*,\\s*_scriptName\\s*\\)\\.href\\s*;?\\s*\\}\\s*catch\\s*(\\([^)]*\\))?\\s*\\{\\s*\\}/;
  result = source.replace(pattern2, 'scriptDirectory = "/_next/static/chunks/";');

  if (result !== source) {
    console.log('[avatarkit-loader] SUCCESS: scriptDirectory replaced via pattern2');
    return result;
  }

  // 都没匹配到 — 输出诊断信息
  console.warn('[avatarkit-loader] WARNING: No scriptDirectory pattern matched!');
  var idx = source.indexOf('scriptDirectory');
  if (idx >= 0) {
    console.warn('[avatarkit-loader] Found "scriptDirectory" at index', idx);
    console.warn('[avatarkit-loader] Context (200 chars):', source.substring(Math.max(0, idx - 20), idx + 200));
  } else {
    console.warn('[avatarkit-loader] "scriptDirectory" not found in source at all!');
    console.warn('[avatarkit-loader] Source length:', source.length);
    console.warn('[avatarkit-loader] First 300 chars:', source.substring(0, 300));
  }
  return source;
}
`

function ensureLoader() {
  const cacheDir = join(sdkDir, '.cache')
  const loaderPath = join(cacheDir, 'wasm-loader.cjs')
  mkdirSync(cacheDir, { recursive: true })
  writeFileSync(loaderPath, LOADER_CODE)
  return loaderPath
}

// ── Plugin: 复制 WASM 文件 ──
class CopyWasmPlugin {
  apply(compiler) {
    compiler.hooks.thisCompilation.tap('AvatarkitCopyWasm', (compilation) => {
      compilation.hooks.processAssets.tap(
        {
          name: 'AvatarkitCopyWasm',
          stage: compiler.webpack.Compilation.PROCESS_ASSETS_STAGE_ADDITIONAL,
        },
        () => {
          try {
            const files = readdirSync(sdkDistDir)
            let copied = 0
            for (const file of files) {
              if (file.startsWith('avatar_core_wasm') && file.endsWith('.wasm')) {
                const content = readFileSync(join(sdkDistDir, file))
                compilation.emitAsset(
                  `static/chunks/${file}`,
                  new compiler.webpack.sources.RawSource(content)
                )
                console.log(`[avatarkit] Copied WASM: ${file} (${content.length} bytes)`)
                copied++
              }
            }
            if (copied === 0) {
              console.warn('[avatarkit] WARNING: No WASM files found in:', sdkDistDir)
              console.warn('[avatarkit] Available files:', files.join(', '))
            }
          } catch (err) {
            console.warn('[avatarkit] Failed to copy WASM files:', err.message)
            console.warn('[avatarkit] SDK dist dir:', sdkDistDir)
            console.warn('[avatarkit] Dir exists:', existsSync(sdkDistDir))
          }
        }
      )
    })
  }
}

// ── 主函数 ──
export function withAvatarkitFixed(nextConfig = {}) {
  const loaderPath = ensureLoader()

  // 启动时诊断日志
  console.log('[avatarkit] ────────────────────────────────────')
  console.log('[avatarkit] SDK location:', sdkDir)
  console.log('[avatarkit] WASM source:', sdkDistDir)
  console.log('[avatarkit] Dist exists:', existsSync(sdkDistDir))
  if (existsSync(sdkDistDir)) {
    const distFiles = readdirSync(sdkDistDir)
    const wasmFiles = distFiles.filter(f => f.endsWith('.wasm'))
    console.log('[avatarkit] WASM files found:', wasmFiles.length > 0 ? wasmFiles.join(', ') : 'NONE!')
  }
  console.log('[avatarkit] Loader path:', loaderPath)
  console.log('[avatarkit] ────────────────────────────────────')

  return {
    ...nextConfig,

    webpack: (config, context) => {
      console.log('[avatarkit] webpack() called, isServer:', context.isServer)

      // 1. 修复 generator.asset.filename 与 asset/inline 冲突
      if (config.module.generator?.asset?.filename) {
        const filename = config.module.generator.asset.filename
        delete config.module.generator.asset.filename
        config.module.generator['asset/resource'] = {
          ...config.module.generator['asset/resource'],
          filename,
        }
        console.log('[avatarkit] Fixed generator.asset.filename →', filename)
      }

      // 2. 修复 Emscripten scriptDirectory
      config.module.rules.push({
        test: /avatar_core_wasm.*\.js$/,
        enforce: 'pre',
        use: [{ loader: loaderPath }],
      })

      // 3. 复制 WASM 文件 (仅客户端)
      if (!context.isServer) {
        config.plugins.push(new CopyWasmPlugin())
      }

      // 链式调用用户的 webpack 配置
      if (typeof nextConfig.webpack === 'function') {
        return nextConfig.webpack(config, context)
      }
      return config
    },

    async headers() {
      const userHeaders =
        typeof nextConfig.headers === 'function' ? await nextConfig.headers() : []
      return [
        ...userHeaders,
        {
          source: '/_next/static/chunks/:path*.wasm',
          headers: [{ key: 'Content-Type', value: 'application/wasm' }],
        },
      ]
    },
  }
}

export default withAvatarkitFixed
