# Test fixtures

These JSON files are captured (or hand-crafted) responses from the GitHub
Releases API. They are the input that `_senv_extract_assets`, the resolve
pipeline, and the toolchain parser test against.

The `browser_download_url` fields hard-code `https://github.com/airbus/sen/...`
because that's the real URL shape today. If Sen ever migrates off GitHub,
every fixture needs updating in lockstep with the code that generates the
URLs (`_senv_extract_assets` and friends).

## Files

- `release-0.5.2.json`: captured live response for the 0.5.2 release tag.
  Single Linux artifact + Windows artifact. Exercises the legacy `gnu`
  filename slot.
- `release-multi-build.json`: synthesized. Multiple gcc versions for one
  Sen version on x86_64. Exercises the multi-candidate path with a single
  compiler family.
- `release-multi-compiler.json`: synthesized. Mix of gcc, clang, msvc,
  and aarch64. Exercises the multi-toolchain selection logic.
- `release-rc.json`: synthesized. An rc-tagged version, to exercise the
  hyphen-in-version-string parser edge case.
