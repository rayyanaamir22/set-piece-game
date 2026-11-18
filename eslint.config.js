import js from '@eslint/js';
import tseslint from 'typescript-eslint';

export default tseslint.config(
  js.configs.recommended,
  ...tseslint.configs.recommended,
  {
    // The simulation must stay portable to the native (C/OpenGL) build.
    files: ['src/sim/**/*.ts'],
    rules: {
      'no-restricted-imports': [
        'error',
        {
          patterns: [
            {
              group: ['three', 'three/*', 'lil-gui', '**/render/*', '**/hud/*', '**/input/*', '**/debug/*'],
              message: 'src/sim must not depend on rendering, DOM or input code.',
            },
          ],
        },
      ],
    },
  },
);
