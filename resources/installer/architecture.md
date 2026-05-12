# Sen installer: architecture and internals

## Layout of what gets installed

```
~/.sen/                                  # $SEN_INSTALL_HOME (default)
  <build-id>/                            # e.g. 0.5.2-x86_64-linux-gcc-12.4.0
    bin/  lib/  include/  share/
    activate                             # POSIX (bash / zsh / dash)
    activate.fish                        # fish-native
    .sen-manifest.sha256                 # generated at install time
  current -> <build-id>/                 # symlink, retargeted on every install
  cache/
    sen-<...>-release.tar.gz             # downloaded archives
  install.sh                             # self-copied installer
```

The `current` symlink lets users put a single line in their shell rc (`. ~/.sen/current/activate`) and have it
follow whichever build was last installed. `do_install` retargets it at the end of every successful install and
also on the already-installed short-circuit, so `sh install.sh <old-version>` is a cheap "switch back" path.

The `cache/` directory keeps downloaded tarballs across installations. A re-install reuses a cached archive if its
checksum still matches; the verify step deletes a corrupt cache and forces a re-download. The cache is safe to
prune at any time.

## Install pipeline

`do_install` orchestrates the phases. Each prints a progress marker; soft skips and warnings buffer into a
notes queue and flush as a single block at the end.

1. `resolve_url`: query the GitHub Releases API for the requested version, filter assets by host arch/os, and
   either auto-pick (single candidate), pick by `--compiler` (explicit), refuse non-interactively, or open the menu.
2. `fetch_archive`: download to `$SEN_INSTALL_HOME/cache/`. Cache hit returns immediately and silently.
3. `verify_checksum`: best-effort SHA256SUMS check. Four outcomes: verified (prints `step_done`), no SHA256SUMS
   published (queues a note), no entry for archive in SHA256SUMS (queues a warn), no `sha256sum`/`shasum` tool
   on PATH (queues a warn). Mismatch prints `err`, removes the cache, returns non-zero.
4. `unpack`: extract into a per-PID staging dir under `cache/`, promote a single top-level dir if present. Failure
   paths clean up the staging dir.
5. `generate_manifest`: write `<prefix>/.sen-manifest.sha256` covering every file in the build tree except the
   manifest itself and the activate scripts (those are generated post-extraction; including them would fail
   `sha256sum -c`). Order matters: completions are cached *before* the manifest so they get integrity-checked
   alongside the rest.
6. `write_activate_scripts`: generate `<prefix>/activate` and `<prefix>/activate.fish` with `$SEN_PREFIX` baked in.
7. `self_copy_installer`: refetch `install.sh` into `$SEN_INSTALL_HOME/install.sh`. Re-installs skip the bootstrap URL.
8. **`current` symlink update**: `ln -sfn <build-id> $SEN_INSTALL_HOME/current`. The `-n` keeps the symlink itself as
   the target rather than following it into the previous build's directory; `-f` replaces an existing symlink in
   one step. Also runs on the already-installed short-circuit so `sh install.sh <old-version>` flips back to a
   prior build without re-installing.

Rollback: `do_install` arms `trap "rm -rf '$prefix'" EXIT` after `verify_checksum` succeeds, and disarms with
`trap - EXIT` only after the last write succeeds.

## Multi-compiler / multi-arch handling

We can have multiple builds for the same Sen version. `extract_assets` filters by `(arch, os)` derived from
`uname` (or the `SEN_HOST_ARCH` / `SEN_HOST_OS` test overrides). If exactly one candidate remains, the installer
auto-picks it. If several remain, it falls back to the interactive menu (or refuses with `--yes`).

`--compiler` accepts `gcc-<ver>`, `clang-<ver>`, `msvc-<ver>`, or a bare `<ver>`.

The menu's per-line marker compares each candidate against the matching compiler on the host. `detect_compiler`
recognises gcc and clang via `-dumpfullversion`/`-dumpversion`. Toolchains the script doesn't know how to detect
get a "(no `<name>` on your system)" marker.

## Security model

The installer puts Sen builds into the user's home directory. It does not modify system files, does not run as
root by default, and does not install anything outside `$SEN_INSTALL_HOME`.

**Trusted**: TLS to GitHub for all downloads (we rely on the system cert store; no certificate pinning), the
Sen binaries themselves (anything under `<prefix>/bin` ends up on `PATH`), and the activate scripts.

**Verified**: if the release publishes `SHA256SUMS`, the downloaded archive is checked against it; otherwise
the install proceeds with a warning. At install time `<prefix>/.sen-manifest.sha256` is written for the
entire build tree (except for the manifest and activate scripts, which are generated post-extraction).

**Not protected against**: a compromised release pipeline, a tampered local CA store, or local writes to
`$SEN_INSTALL_HOME`

## Environment variables

| Variable              | Default                                                  | Purpose                                            |
|-----------------------| -------------------------------------------------------- | -------------------------------------------------- |
| `SEN_INSTALL_HOME`    | `$HOME/.sen`                                             | Install root                                       |
| `SEN_BASE_URL`        | `https://github.com/airbus/sen`                          | Base for release downloads                         |
| `SEN_API_URL`         | `https://api.github.com/repos/airbus/sen`                | Base for the GitHub Releases API                   |
| `SEN_RAW_URL`         | `https://raw.githubusercontent.com/airbus/sen/main`      | Self-copy refetch URL                              |
| `SEN_HOST_ARCH`       | `uname -m`                                               | Override for the host arch slot (test fixtures)    |
| `SEN_HOST_OS`         | `uname -s`                                               | Override for the host OS slot (test fixtures)      |
| `SEN_ALLOW_ROOT`      | unset                                                    | Set to `1` to allow running as root (containers)   |
| `SEN_INSTALLER_NORUN` | unset                                                    | Source `install.sh` without running `main` (tests) |
| `NO_COLOR`            | unset                                                    | Disable color output                               |
| `SEN_DEV_STRICT`      | unset                                                    | Test runner: fail if optional tooling is missing   |

## Tests

Tests live in `test/`. `run.sh` is the entry point:

```sh
sh test/run.sh
```

`SEN_DEV_STRICT=1` makes a missing `bats` a hard error (the CI default). On a workstation, missing optional
tooling skips the affected steps with a warning.
