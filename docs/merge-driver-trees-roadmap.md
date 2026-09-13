# Roadmap: merge the 3.10.14 and 4.4.94 driver trees

## Goal

Replace the two parallel source trees (`3.10.14/` and `4.4.94/`) with a single set of
driver sources where the per-kernel differences are handled **inside** the sources
via `LINUX_VERSION_CODE` / `KERNEL_VERSION` guards, instead of by duplicating whole
directories.

Drivers that are genuinely a different implementation per kernel stay separate and
are selected in `Kbuild`. The 3.10 motor driver is such a case and is renamed
`motors-pp` (directory `motor-pp`) to distinguish it from the 4.4 `motor` driver.

## Constraints / non-goals

- Do **not** merge platform-only trees that only ever build on one SoC/kernel:
  - A1-only: `aip/`, `fb/`, `ipu/`, `video/` (4.4 only)
  - Legacy SoC ISP/sensors/audio: `t10`, `t20`, `t21`, `t23`, `t30` (3.10 only)
- Keep the external build contract working: `SOC_FAMILY`, `KERNEL_VERSION`,
  `BR2_THINGINO_MOTORS*` and `CONFIG_SOC_*` are still passed by the firmware build.
- One commit per task.

## Inventory (duplication)

- 66 files are byte-identical across both trees.
- ~56 common files differ, split into:
  - cosmetic/whitespace differences (avpu_alloc, avpu_no_dmabuf, ...)
  - small include / platform-gate differences (jz-dtrng, avpu_ip.h, ...)
  - substantial logic/format drift (soc_nna_main 901 lines, tx-isp-debug 678,
    tx-isp-common.h 295, pwm_core 284, sensor sources)

## Strategy

Create a single `common/` source root for the mergeable driver sources. Keep the
`3.10.14/` and `4.4.94/` trees in place for the platform-only content. `Kbuild`
selects either the merged `common/` path or a kernel-specific path.

Rationale: incremental and reviewable. Each task moves one driver at a time from
"duplicated in two trees" to "one source in `common/` behind a version guard",
without a flag-day rename of every file at once.

### In-source kernel selector

Use the existing repo convention of the preprocessor symbols
`CONFIG_KERNEL_3_10`, `CONFIG_KERNEL_4_4_94` (and `CONFIG_KERNEL_6_1` if ever
needed). These are supplied as `EXTRA_CFLAGS` by the firmware build
(`package/ingenic-sdk/ingenic-sdk.mk`) and are already used throughout the
tree. Do not introduce raw `LINUX_VERSION_CODE` checks unless a driver needs a
finer-grained kernel revision than the two supported ones.

## Tasks

| # | Task | Status |
|---|------|--------|
| 0 | Branch `refactor/merge-driver-trees`, write this roadmap | done |
| 1 | Finish the `motors-pp` rename for the 3.10 motor driver and split Kbuild selection | done |
| 2 | Merge `misc/jz-dtrng` into `common/misc/jz-dtrng` | done |
| 3 | Merge `misc/mpsys-driver` Kbuild into a single per-kernel-aware Kbuild | done |
| 4 | Rename the 3.10.14 PWM driver source set to `pwm-pp` (a different
|   implementation from 4.4.94, like `motors-pp`); select per-kernel in Kbuild | done |
| 5 | Merge `avpu/t31`, `c100`, `t40`, `t41` into `common/avpu` | done |
| 6 | ~~Merge `avpu/c100`, `avpu/t40`, `avpu/t41`~~ (folded into task 5) | done |
| 7 | Rename the 3.10.14 ISP/t31 source set to `isp/t31-pp` (different
|   interface generation: vtable vs `private_*` shims); select per-kernel in isp/Kbuild | done |
| 8 | Merge `isp/t41` (+ `t41zrt` headers) into `common/isp`; select firmware blob per kernel | done |
| 9 | Merge `sensor-src/common` + `include` | pending |
| 10 | Merge `sensor-src/t31`, `t40`, `t41`, `t41zrt`, `c100` | pending |
| 11 | Merge `misc/soc-nna` | pending |
| 12 | Remove now-empty duplicate trees and simplify `Kbuild` | pending |
| 13 | Final sweep: update docs, verify build matrix | pending |

## Verification

Each task is verified by rebuilding the affected driver for its kernel(s) where a
toolchain/kernel tree is available; otherwise by a structural diff review showing
the merged source is a superset guarded by version checks. The final task must
build the supported SoC/kernel matrix.
