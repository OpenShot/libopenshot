<!--
SPDX-FileCopyrightText: 2026 OpenShot Studios, LLC
SPDX-License-Identifier: LGPL-3.0-or-later
-->

# ARM64 build checks

The `arm64-support` branch collects build support before integration into
`develop`. Existing Linux, Intel Mac, and Windows x86/x64 jobs remain enabled.

GitHub automatically runs **Linux ARM64 build and tests** on
`ubuntu-24.04-arm` and **Windows ARM64 build and tests** on `windows-11-arm`
for pushes and pull requests. No repository enable variables are needed, and
failures are reported normally. These checks do not publish releases.

The production GitLab configuration is unchanged: its optional Windows ARM64
job still requires `ENABLE_WINDOWS_ARM64=1`, a runner tagged `windows-arm64`,
and MSYS2 CLANGARM64 installed at `C:\msys64`.

`windows-arm64-packages.lock` is a reference package list. GitHub installs
currently available packages by name, and the validator records actual
versions and reports deviations using `--package-baseline`. GitLab expects
the packages to be provisioned on the runner. Reproducible builds require a
separately archived package snapshot; `--package-lock` retains strict version
checking for such environments. Archive checksums are not verified.

Native validation requires both the Windows host API result and an ARM64 PE
header on the running Python interpreter. Every produced executable, DLL,
and Python extension is also checked independently. A Linux validator test
pass is not evidence of native Windows execution.

All library GitHub builds use `OpenShot/libopenshot-audio`. They select the
PR source branch (or pushed branch) when it exists in the audio repository,
otherwise `develop`. A connection error fails the job instead of silently
falling back. GitLab continues to require matching branch ARM64 artifacts.

Windows ARM64 tests may expose newer-toolchain compatibility issues tracked
in #1096. Enabling the checks does not itself establish that every optional
feature works with rolling MSYS2 packages.
