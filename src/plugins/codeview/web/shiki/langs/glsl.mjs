// Licence stub (feature 070): the upstream grammar for this language could
// not be shipped -- see web/licence-manifest.json. Shipped grammars (cpp,
// cpp-macro, elm, nim) declare it in embeddedLangs, and shiki REFUSES to
// load a grammar whose embedded languages are not registered ("Missing
// languages `glsl`, required by `cpp-macro`, `cpp`") -- an empty module
// would take C++ highlighting down with it. So the stub registers a minimal
// no-op grammar under the right name: embedded GLSL blocks simply get no
// tokens of their own.
export default [
  {
    name: 'glsl',
    scopeName: 'source.glsl',
    patterns: [],
    repository: {},
  },
]
