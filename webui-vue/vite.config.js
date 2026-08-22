import { defineConfig } from 'vite'
import { fileURLToPath } from 'url'
import { resolve, dirname } from 'path'
import vue from '@vitejs/plugin-vue'
import { quasar, transformAssetUrls } from '@quasar/vite-plugin'

const __dirname = dirname(fileURLToPath(import.meta.url))

export default defineConfig({
  plugins: [
    vue({ template: { transformAssetUrls } }),
    quasar({
      sassVariables: resolve(__dirname, 'src/css/quasar.variables.scss')
    })
  ],
  // vue-grid-layout is a CJS/UMD package that internally bundles Vue.
  // vue-i18n and vue-chartjs also have Rolldown (Vite 8) CJS interop issues
  // unless all Vue-ecosystem packages are pre-bundled together so Rolldown
  // sees exactly one copy of @vue/runtime-dom and @vue/shared.
  resolve: {
    dedupe: [
      'vue',
      '@vue/runtime-dom',
      '@vue/runtime-core',
      '@vue/reactivity',
      '@vue/shared',
      'vue-router',
      'pinia',
      'vue-i18n',
    ],
  },
  optimizeDeps: {
    // Only pre-bundle CJS/UMD packages that would otherwise cause Rolldown
    // interop issues. Native ESM packages (vue, vue-i18n, vue-router, pinia)
    // must NOT be listed here — splitting them into separate pre-bundle chunks
    // causes `init_*_esm_bundler` cross-chunk reference errors in Vite 8.
    include: [
      'grid-layout-plus',
      'chart.js',
      'vue-chartjs',
    ],
    // Exclude native ESM vue packages from pre-bundling so Rolldown resolves
    // them inline and their init functions stay in scope.
    exclude: [
      'vue',
      '@vue/runtime-dom',
      '@vue/runtime-core',
      '@vue/reactivity',
      '@vue/shared',
      'vue-i18n',
      'vue-router',
      'pinia',
    ],
  },
  server: {
    proxy: {
      // Forward /api/* HTTP requests (directors list, session management) and
      // /ws WebSocket connections to the bareos-webui-proxy.
      '/api': {
        target: `http://${process.env.VITE_DIRECTOR_HOST ?? '127.0.0.1'}:${process.env.VITE_DIRECTOR_PORT ?? '9101'}`,
        changeOrigin: true,
      },
      '/ws': {
        target: `ws://${process.env.VITE_DIRECTOR_HOST ?? '127.0.0.1'}:${process.env.VITE_DIRECTOR_PORT ?? '9101'}`,
        ws: true,
        changeOrigin: true,
      },
    },
  },
})
