import { defineConfig } from 'vite'
import { fileURLToPath } from 'url'
import { resolve, dirname } from 'path'
import { createRequire } from 'module'
import vue from '@vitejs/plugin-vue'
import { quasar, transformAssetUrls } from '@quasar/vite-plugin'

const __dirname = dirname(fileURLToPath(import.meta.url))
const require = createRequire(import.meta.url)
const pkg = require('./package.json')

// BAREOS_VERSION may be injected by the CMake build; fall back to package.json.
const bareosVersion = process.env.BAREOS_VERSION || pkg.version

export default defineConfig({
  plugins: [
    vue({ template: { transformAssetUrls } }),
    quasar({
      sassVariables: resolve(__dirname, 'src/css/quasar.variables.scss')
    })
  ],
  define: {
    __BAREOS_VERSION__: JSON.stringify(bareosVersion),
  },
})
