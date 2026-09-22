# Security Policy

## Reporting a vulnerability

Report vulnerabilities privately through GitHub: open the **Security** tab of this repository and choose **Report a vulnerability**. Do not open a public Issue for a vulnerability. Japanese or English is fine.

Include the affected commit, what an attacker could do, and the steps to reproduce it. This is a personal project maintained on a best-effort basis; there is no guaranteed response time.

## Scope

- In scope: the code in this repository — the US-JIS module (`src/`, `include/`, `dts/`), the keymaps and configuration in `config/`, the build and simulation scripts, and the GitHub Actions workflows.
- Out of scope: vulnerabilities in Keychron ZMK, Zephyr, the Nordic 2.4 GHz library, the bootloader or the Keychron Launcher themselves. Report those to their maintainers. If such a vulnerability is reachable only because of how this repository uses the code, it is in scope.

Only the latest commit on `main` is supported. This repository publishes no firmware binaries; you build the firmware yourself from the source ([README](README.md)).
