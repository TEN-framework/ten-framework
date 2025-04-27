import js from '@eslint/js'
import tseslint from 'typescript-eslint'
import prettierPlugin from 'eslint-plugin-prettier'
import prettierConfig from 'eslint-config-prettier'

export default await tseslint.config({
  files: ['**/*.ts', '**/*.tsx'],
  languageOptions: {
    parserOptions: {
      ecmaVersion: 2022,
      sourceType: 'module',
    },
  },
  rules: {
    '@typescript-eslint/no-unused-vars': 'warn',
    'prettier/prettier': 'warn',
  },
  plugins: {
    '@typescript-eslint': tseslint.plugin,
    prettier: prettierPlugin,
  },
  extends: [
    js.configs.recommended,
    ...tseslint.configs.recommended,
    prettierConfig,
  ],
})
