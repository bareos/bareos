import { defineConfig } from 'vite'
import { fileURLToPath } from 'url'
import { resolve, dirname } from 'path'
import vue from '@vitejs/plugin-vue'
import { quasar, transformAssetUrls } from '@quasar/vite-plugin'

const __dirname = dirname(fileURLToPath(import.meta.url))

export default defineConfig({
  base: '/',
  plugins: [
    vue({ template: { transformAssetUrls } }),
    quasar({
      sassVariables: resolve(__dirname, 'src/css/quasar.variables.scss')
    })
  ],
  build: {
    // Inline small assets so fewer files need embedding
    assetsInlineLimit: 4096,
  }
})
