<!--
SPDX-FileCopyrightText: 2026 OpenShot Studios, LLC
SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Experimental Windows ARM64 builds

The `arm64-support` branch collects build support before integration into
`develop`. Existing Linux, Intel Mac, and Windows x86/x64 jobs remain enabled.

Set `ENABLE_WINDOWS_ARM64=1` as a GitLab CI variable to enable the optional
job on a native Windows runner tagged `windows-arm64`, with MSYS2 CLANGARM64
installed at `C:\msys64`. GitHub uses the repository Actions variable of the
same name to enable its supplemental `windows-11-arm` job. Neither job
publishes a release.

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

The library GitHub job uses `OpenShot/libopenshot-audio` by default. Set the
Actions variables `OPENSHOT_AUDIO_REPOSITORY` and `OPENSHOT_AUDIO_REF` to
select another source explicitly. Integration runs targeting `arm64-support`
use that audio branch; other runs default to `develop`. GitLab requires the
matching branch's audio ARM64 artifact and never substitutes `develop`.

The optional ARM64 job still needs the newer-toolchain compatibility work
tracked in libopenshot PR #1096. Its addition alone does not establish that
the current rolling FFmpeg/OpenCV toolchain builds successfully.
