# Contributing to TelegramP4

Thanks for your interest in contributing! This project is built as an incremental
learning platform, so contributions should respect that structure.

## Ground rules

1. **Follow the phase structure.** Don't submit a PR that jumps ahead of the current
   phase in [CLAUDE.md](CLAUDE.md)'s checklist or bundles multiple phases together —
   review is much easier when each PR maps to one phase or one clear fix.
2. **Every PR must build.** `idf.py build` must succeed. If you can, flash and test
   on real hardware and say so in the PR description; if you can't, say that too —
   never claim a hardware test you didn't run.
3. **No secrets.** Never commit a real Wi-Fi password, Telegram bot token, or chat
   ID. Check `git diff` before pushing.
4. **Match the module boundaries.** New features belong in the right
   `components/telegramp4_*` module, not bolted onto `main/app_main.cpp`.
5. **Update docs alongside code.** If you add a Kconfig option, update
   `docs/configuration.md`. If you finish a phase, update the lesson in
   `docs/lessons/` and the checklist in `CLAUDE.md`.
6. **Respect licenses.** If you bring in third-party code, check its license,
   preserve its copyright notice, and document the dependency (see
   [LICENSE](LICENSE)).

## Getting set up

See [docs/getting-started.md](docs/getting-started.md).

## Reporting bugs / requesting features

Open a GitHub issue with:
- What phase/component is affected
- Steps to reproduce (for bugs)
- Whether you've tested on real hardware

## Code style

- C/C++ following ESP-IDF conventions (see existing components once Phase 0 lands).
- Comments explain *why*, not *what* — the code should be readable on its own for
  the "beginner friendly" audience this project targets (see spec §2.2), but avoid
  redundant narration.

## Code of Conduct

See [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md).
