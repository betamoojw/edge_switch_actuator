# svelte-focus-trap packaging compatibility

Vendored from the MIT-licensed npm package `svelte-focus-trap@1.2.0`:
https://github.com/Duder-onomy/svelte-focus-trap

The JavaScript, type declaration and license are copied unchanged. Only package
metadata is corrected: explicit ESM mode and conditional exports point to the
shipped source and types instead of nonexistent distribution files. This fixes
Vite's missing Svelte exports warning without changing modal keyboard behavior.

The application uses `file:vendor/svelte-focus-trap`, recorded in the npm lockfile,
so the correction survives `npm ci`. Remove this local package when an upstream
release supplies equivalent exports and has been validated with the application.
