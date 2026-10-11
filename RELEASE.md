# Releasing Firmware

## Tag scheme

Release tags are `kallisti-<upstream version>-r<revision>`, for example `kallisti-v1.17.1-r1`.

- The upstream part is the MeshCore companion release this build is based on (see `UPSTREAM_BASE`).
- The revision counts our releases on that base. It restarts at `r1` after rebasing onto a new upstream release.
- Never push upstream-style tags (`companion-*`, `repeater-*`, `room-server-*`) to this repo.

## Cutting a release

```sh
git tag kallisti-v1.17.1-r1
git push origin kallisti-v1.17.1-r1
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

When it works, update `main` to the rebased branch, commit the new `UPSTREAM_BASE`, and tag `kallisti-v1.18.0-r1`.
