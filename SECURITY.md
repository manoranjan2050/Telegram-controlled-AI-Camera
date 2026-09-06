# Security Policy

TelegramP4 controls a physical camera and GPIOs over the internet via Telegram.
Please report security issues responsibly.

## Reporting a vulnerability

Please **do not** open a public GitHub issue for security vulnerabilities. Instead,
open a private GitHub Security Advisory on this repository, or contact the
maintainer directly. Include:

- A description of the vulnerability and its impact
- Steps to reproduce
- Affected phase/component/version

We'll acknowledge reports and work on a fix before public disclosure.

## Scope

Security-relevant areas of this project (see [docs/security.md](docs/security.md)
for the full model):

- Telegram chat-ID authorization bypass
- Bot token exposure (logs, docs, commits)
- TLS/certificate validation bypass
- Path traversal in file operations (`/delete`, received files, gallery callbacks)
- Unbounded resource consumption (storage, memory, GPIO exposure)
- OTA update integrity/rollback issues

## Supported versions

This project is pre-1.0 and under active incremental development. Security fixes
land on the current development line — there is no long-term-support branch yet.
