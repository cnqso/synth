# cnqsosynth

Native SDL3 synth and sequencer written in C.

## Local build

Install SDL3 and make sure `pkg-config` can find it, then run:

```sh
make
./cnqsosynth
```

`Nintendo-DS-BIOS.ttf` is loaded at runtime and should stay next to the binary unless you are launching the packaged macOS app bundle.

## CI and releases

GitHub Actions now does the following:

- `CI` builds the project on Linux (`ubuntu-24.04`), macOS (`macos-14`, arm64), and Windows (`windows-2022`, x64) for pushes to `main` and all pull requests.
- `Release` runs when you push a tag that matches `v*`, packages platform-specific assets, and publishes them on the GitHub Releases page with generated release notes.

Release assets:

- Linux: `tar.gz` bundle with the binary, font, README, and bundled SDL3 runtime library.
- macOS: zipped `.app` bundle with the font and SDL3 framework embedded.
- Windows: zipped bundle with `cnqsosynth.exe`, `SDL3.dll`, the font, and the README.

## Shipping a release

```sh
git tag v0.1.0
git push origin v0.1.0
```

That tag push triggers the release workflow and creates or updates the matching GitHub Release automatically.
