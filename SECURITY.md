# Security Policy

LockCracker is a hobby firmware project for a toy aimed at kids. It runs
locally on an M5Stack Core 2 with no network connectivity and no remote
attack surface, so security issues are unlikely — but if you find one,
please report it responsibly.

## Supported versions

Only the `main` branch is supported. There are no tagged releases yet;
fixes will be applied directly to `main`.

| Version | Supported          |
|---------|--------------------|
| main    | :white_check_mark: |
| other   | :x:                |

## Reporting a vulnerability

**Please do not open a public issue for security reports.**

Report vulnerabilities privately via one of these channels:

- Email: **marcel.duetscher@gmail.com** with subject prefix
  `[LockCracker security]`
- GitHub's private vulnerability reporting:
  https://github.com/marceld23/LockCracker/security/advisories/new

Please include:

- A description of the issue and the affected component
- Steps to reproduce, ideally with sample code or a serial log
- Hardware and software versions (M5Stack Core 2 revision, PlatformIO
  Core version, M5Unified version)
- Any suggested mitigation if you have one

## Response expectations

This is a hobby project maintained in spare time. I aim to:

- Acknowledge the report within **7 days**
- Provide a status update within **30 days**
- Publish a fix and credit the reporter (if they wish) once the issue
  is resolved

## Scope

In scope:
- The firmware in this repository (`src/`)
- Build configuration that could lead to insecure binaries

Out of scope:
- Vulnerabilities in the M5Stack Core 2 hardware itself
- Vulnerabilities in upstream libraries (`M5Unified`, ESP-IDF, Arduino
  core) — please report those to their respective maintainers
- Physical access attacks (the device is a toy held in the user's hand;
  someone with physical access can already reflash it)
