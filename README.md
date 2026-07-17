# Izan Wallet — core

Native desktop wallet core: vault, HD derivation, signing authority, and
transaction engine. Single static binary, no Electron, no browser.

- **Keys never leave the signer process.** The UI, scripts, and AI agents can
  only *propose* transactions; signing happens in an isolated process behind a
  human-controlled approval queue.
- **Zero swap fees.** The wallet is free for humans.
- **Reproducible builds.** Every release ships with build instructions that let
  you produce a byte-identical binary.

This repository contains the open, auditable trust path (`core/`, `platform/`,
`keyd/`, `domain/`). The UI shell and application layer live elsewhere.

Status: pre-release, under active development. Do not use with real funds yet.

## Build

Requires CMake ≥ 3.28 and a C++23 toolchain. All third-party dependencies are
pinned and built from source via FetchContent.

```
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build
```

## License

MIT
