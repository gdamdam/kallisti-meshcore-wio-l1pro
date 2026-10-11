# Releasing Firmware

## Tag scheme

Release tags are `kallisti-v<major>.<minor>.<patch>`, for example `kallisti-v0.1.0`.

- The version is KALLISTI's own and is what the splash screen shows. Do not use dashes in it, because the splash cuts the version at the first dash.
- The MeshCore release it is based on is recorded in `UPSTREAM_BASE` and appears in the release title.
- Bump the patch for fixes, the minor for new features or a new MeshCore base.
- Never push upstream-style tags (`companion-*`, `repeater-*`, `room-server-*`) to this repo.

## Cutting a release

```sh
git tag kallisti-v0.1.0
git push origin kallisti-v0.1.0
```

The `Release Wio Firmware` workflow builds only `WioTrackerL1_companion_radio_ble`, writes `SHA256SUMS.txt`, and creates a **draft** GitHub Release using `.github/release-notes.md`.
Fill in "What changed", then publish the draft.

## Following upstream MeshCore

The `Check Upstream MeshCore Release` workflow runs weekly and opens an issue when a newer stable `companion-v*` tag exists upstream.
To move to it, replay our commits onto the new tag:

```sh
git fetch upstream --tags
git switch -c rebase/companion-v1.18.0 main
git rebase --onto companion-v1.18.0 "$(cat UPSTREAM_BASE)"
echo companion-v1.18.0 > UPSTREAM_BASE
# resolve conflicts, build, run tests, test on hardware
```

When it works, update `main` to the rebased branch, commit the new `UPSTREAM_BASE`, and tag a new minor version, for example `kallisti-v0.2.0`.
