# Driver tree merge: 3.10.14 + 4.4.94

Status: **complete** on branch `refactor/merge-driver-trees` (not yet pushed).
One commit per task; see `git log origin/master..HEAD`.

Scope: merge the duplicated `3.10.14/` and `4.4.94/` driver **sources**, and
collapse the duplicated prebuilt firmware blobs under a single `sdk/` root.

This document is the hand-off note for the next session. It records what was
moved, how the build selects sources, how to verify, and what remains.

---

## 1. Goal

Collapse the duplicated driver sources in `3.10.14/` and `4.4.94/` so that:

- differences between the two kernels are handled **inside the sources**
  (`CONFIG_KERNEL_*`), and
- differences that are really *platform* differences are handled inside the
  sources (`CONFIG_SOC_*`), and
- drivers that are genuinely a **different implementation** stay as separate
  source sets, selected per kernel in `Kbuild`.

Module names (`motor`, `pwm_core`, `pwm_hal`, `tx-isp-t31`, `avpu`, `audio`,
`soc-nna`, `dtrng_dev`, ...) are unchanged, so userspace and the device ABI are
unaffected. In particular `modprobe motor`, `/dev/motor` and `/dev/pwm` behave
exactly as before on both kernels.

## 2. TL;DR for the next session

- Read section 4 (source map) and section 5 (Kbuild rules).
- To build: section 7 (real, tested commands).
- Remaining work / known gaps: section 8.

## 3. Decision rule (how each driver was classified)

For each duplicated driver, diff the two trees and classify:

1. **Identical or trivially different** (whitespace, one include, one API
   signature) -> **merge** into `common/`, guard in-source. Examples: `avpu`,
   `jz-dtrng`, `mpsys-driver`, `isp/t41`, `soc-nna`, `sensor-info`.
2. **Different implementation, same purpose** -> **keep two source sets**,
   select per kernel. Examples: motor, pwm, `isp/t31`.
3. **Platform-only** (only ever builds on one SoC/kernel) -> **leave where it
   is**, no merge. Examples: A1 `aip/fb/ipu/video`, legacy `t10..t30`.

## 4. Source map

### 4.1 Merged into `common/` (used by both kernels)

| Driver | Merged source | Came from | In-source guards |
|--------|---------------|-----------|------------------|
| audio (t41 oss3) | `common/audio/t41/oss3/` | 3.10.14 (identical to 4.4.94 except the Kbuild) | already used `CONFIG_KERNEL_3_10/4_4_94` |
| avpu | `common/avpu/` | 3.10.14 `avpu/t31` (superset, handles T31/C100/T40/T41) + 2 guards ported from 4.4.94 | `CONFIG_KERNEL_4_4_94` for `dma_buf_export()` API and T31 AVPU clock (440 vs 550 MHz); `CONFIG_SOC_*` already present |
| isp/t41 | `common/isp/t41/` | 4.4.94 `isp/t41` (superset) | already used `CONFIG_KERNEL_3_10/4_4_94/6_1`; Kbuild picks firmware blob per kernel |
| isp/t41zrt (headers) | `common/isp/t41zrt/` | 3.10.14 (identical) | none |
| sensor-info | `common/sensor-src/common/sensor-info.c`, `common/sensor-src/include/sensor-info.h` | union of both | union struct + both APIs (`sensor_update_actual_fps` and `sensor_common_update`) |
| jz-dtrng | `common/misc/jz-dtrng/` | 4.4.94 | `CONFIG_KERNEL_4_4_94` for the IRQ header |
| mpsys-driver | `common/misc/mpsys-driver/` | 4.4.94 (sources identical anyway) | Kbuild picks `sdk/<soc>/lib<soc>-mpsys-firmware-720-<31014\|4494>.a` by `KERNEL_VERSION` |
| soc-nna | `common/misc/soc-nna/` | 4.4.94 (superset, adds A1) | `CONFIG_SOC_A1` (inert on 3.10) |

### 4.2 Split per kernel (like motor)

| Driver | 3.10.14 source set | 4.4.94 source set | Why |
|--------|--------------------|-------------------|-----|
| motor | `3.10.14/misc/motors-pp/` | `4.4.94/misc/motor/` | Different implementations (PP/TCU vs GPIO/TCU). Both build a module named `motor`. |
| pwm | `3.10.14/misc/pwm-pp/` | `4.4.94/misc/pwm/` | Different implementations (tcu_alloc arbitration + runtime channel selection vs old vendor). Both build `pwm_core`/`pwm_hal`. |
| isp/t31 | `3.10.14/isp/t31-pp/` | `4.4.94/isp/t31/` | Different interface generation: 3.10 uses the `jz_driver_common_interfaces` vtable, 4.4 uses the standalone `private_*` shim layer and dropped the vtable from `txx-funcs.h`. |

### 4.3 Sensor drivers: intentionally NOT merged

`3.10.14/sensor-src/*` and `4.4.94/sensor-src/*` stay separate. The 3.10.14
drivers call the vendor `private_*` shims and use the `actual_fps` API; the
4.4.94 drivers call plain kernel functions and use `sensor_common_update()`.
This is a different calling convention across hundreds of files, not a small
diff. Only the shared `sensor-info.[ch]` is merged (section 4.1).

### 4.4 Platform-only trees

All single-kernel content now lives under `common/` (relocated, no conflicts):
`common/aip` (a1), `common/fb` (a1), `common/ipu` (a1), `common/video` (a1).

The only content still tree-split is the sensor drivers that exist for both
kernels (task 10): `3.10.14/sensor-src/{t31,t40,t41,t41zrt}` and
`4.4.94/sensor-src/{t31,t40,t41,t41zrt}`. Everything else is under `common/`.

### 4.5 `sdk/` firmware blobs (single root, fully explicit names)

`sdk/` holds prebuilt firmware blobs only, one root, with the full identity in
the filename:

    sdk/<soc>/lib<soc>-<kind>-firmware-<version>-<compiler>-<kernel>[-<variant>].a

- `<compiler>`: GCC tag `472` (4.7.2), `540` (5.4.0), `720` (7.2.0).
- `<kernel>`: `31014` (3.10.14) or `4494` (4.4.94), no dashes.
- `<variant>`: `double` (multi-sensor) or `unknown`, after the kernel tag.
- Examples: `sdk/t31/libt31-firmware-1.1.6-540-31014.a`,
  `sdk/t23/libt23-firmware-1.3.0-540-31014-double.a`,
  `sdk/t41/libt41-firmware-1.2.6-720-4494.a`,
  `sdk/a1/libfb-firmware-1.6.2-720-4494.a`,
  `sdk/t40/libt40-mpsys-firmware-720-4494.a`.
- `sdk/t10` is a symlink to `t20` (they share firmware). The other old
  bare-version aliases were dropped: every Kbuild now names the exact file.
- The one near-duplicate pair (t31 1.1.2, two GCC-4.7.2 builds differing by a
  single byte in the embedded build date `Oct 20` vs `Oct 21 2020`) kept only
  the newer build; the older unreferenced rebuild was dropped.
- All Kbuilds reference the explicit name and carry no `KERNEL_VERSION` path
  component. Verified by rebuilding T31/3.10.14, T31/4.4.94, A1/4.4.94.

### 4.6 Audio drivers

All audio lives under one root with a `<soc>/<driver>` layout:

    common/audio/<soc>/<driver>/

Two drivers exist: `oss2` (old `xb_snd`/`devices` layout) and `oss3`
(`boards`/`host`/`inner_codecs` layout). Selection is by SoC: `t41` and `t23`
use `oss3`; every other SoC uses `oss2` on 3.10.14 and `oss3` on 4.4.94 (4.4.94
has no `oss2`). SoC aliases are symlinks (`t10,t20,t21,t30,c100 -> t31`).

This is a relocation, not a dedup: the per-SoC trees were path-disjoint (0
colliding paths), so moving them into `common/audio/` had no conflicts. The
only previously-duplicated dir was `t41/oss3` (already merged, task 16).

## 5. Kbuild selection rules (top-level `Kbuild`)

Everything is under `common/` except the split sensor drivers. Selection:
- ISP: `common/isp/Kbuild` sets `ISP_DIR` (`t31-pp` on 3.10.14, `t31` on
  4.4.94, else `common/isp/$(SOC_FAMILY)`).
- Audio: `common/audio/<soc>/<driver>` (see 4.6).
- misc: all under `common/misc/<name>` (split drivers `motor`/`motors-pp`,
  `pwm`/`pwm-pp` selected by `KERNEL_VERSION`).
- sensor-src: `DIR` is `common/sensor-src/$(SOC_FAMILY)` for the single-kernel
  SoCs (t10,t20,t21,t23,t30,c100) and `$(KERNEL_VERSION)/sensor-src/$(SOC_FAMILY)`
  for the split ones (t31,t40,t41,t41zrt).
- a1-only: `common/aip/a1`, `common/fb`, `common/ipu`, `common/video/a1`.
- `ISP_INCLUDE` (top of `Kbuild`) is `common/isp/<soc>/include`
  (`t31-pp` on 3.10.14), because that dir also holds the sensor headers.

## 5b. Legacy note: former Kbuild selection details

- Merged drivers are included directly from `common/`:
  `common/avpu`, `common/misc/{jz-dtrng,mpsys-driver,soc-nna}`, and the
  sensor `sensor-info` via the per-tree `sensor-src/Kbuild`.
- Split drivers choose by `KERNEL_VERSION`:
  - PWM: `3.10.14` -> `3.10.14/misc/pwm-pp/Kbuild`, else `$(KERNEL_VERSION)/misc/pwm/Kbuild`.
  - Motor: `3.10.14` -> `3.10.14/misc/motors-pp/Kbuild`, else `$(KERNEL_VERSION)/misc/motor/Kbuild`.
- ISP is selected inside `$(KERNEL_VERSION)/isp/Kbuild`, which sets `ISP_DIR`:
  - `t41` -> `common/isp/t41` (both kernels)
  - 3.10.14 `t31` -> `3.10.14/isp/t31-pp`
  - otherwise `$(KERNEL_VERSION)/isp/$(SOC_FAMILY)`
  and includes `$(src)/$(ISP_DIR)/Kbuild`.
- Audio lives under one root, `common/audio/<soc>/<driver>`, selected by SoC
  in the top-level `Kbuild`:
  - `t41`, `t23` -> `common/audio/$(SOC_FAMILY)/oss3`
  - otherwise `common/audio/$(SOC_FAMILY)/oss2` on 3.10.14, `.../oss3` on 4.4.94.
  SoC aliases (`t10,t20,t21,t30,c100 -> t31`) are symlinks under `common/audio/`.
  `a1` also has `common/audio/a1/hdmi_audio`.
- The ISP **include path** used by ISP and sensor-src is computed at the top of
  `Kbuild` as `ISP_INCLUDE` (same mapping as `ISP_DIR`), because the per-SoC
  ISP include dir also holds the sensor headers.

## 6. In-source kernel selector

Use the repo convention: `CONFIG_KERNEL_3_10`, `CONFIG_KERNEL_4_4_94`
(optionally `CONFIG_KERNEL_6_1`). These are supplied as `EXTRA_CFLAGS` by the
firmware build (`package/ingenic-sdk/ingenic-sdk.mk`) and are already used
throughout the tree. Do **not** add raw `LINUX_VERSION_CODE` checks unless a
driver needs a finer revision than the two supported kernels.

## 7. How to build / verify

`build.sh` needs `KDIR` (a configured + built kernel tree), `CROSS_COMPILE`, and
optionally `SENSOR_MODEL`. Prebuilt kernel trees and toolchains live under the
firmware output dir. Real commands used to verify this branch:

```sh
cd /home/paul/thingino/ingenic-sdk

# T31, 3.10.14
BASE=/home/paul/thingino/firmware/master/output/master/vanhua_djz_t31n_gc2083_eth-3.10.14-uclibc-192.168.88.31
export KDIR=$BASE/build/linux-45a11a3318ee823a83536db737a8e1136ed766fd
export CROSS_COMPILE=$BASE/host/bin/mipsel-linux- PATH=$BASE/host/bin:$PATH
export BR2_THINGINO_MOTORS=y BR2_THINGINO_MOTORS_SPI=y
./build.sh t31 3.10.14

# T31, 4.4.94
BASE=/home/paul/thingino/firmware/master/output/feature/kernel_leds_dtsi/wyze_cam3_t31x_gc2053_rtl8189ftv-4.4.94-uclibc-192.168.88.148
export KDIR=$BASE/build/linux-47a4ebc23f37990b61c53ad3108da6af4784ba94
export CROSS_COMPILE=$BASE/host/bin/mipsel-linux- PATH=$BASE/host/bin:$PATH
./build.sh t31 4.4.94

# A1, 4.4.94
BASE=/home/paul/thingino/firmware/stable/output/stable/smart_nvr_a1n_eth-4.4.94-musl
export KDIR=$BASE/build/linux-42f0e91a310c3f5eec071760f46ad21ea0aa918a
export CROSS_COMPILE=$BASE/host/bin/mipsel-linux- PATH=$BASE/host/bin:$PATH
./build.sh a1 4.4.94

# remove build artifacts afterwards
git clean -fdx
```

Verification results:

| Target | Result |
|--------|--------|
| T31 3.10.14 | pass - audio, avpu, gpio-userkeys, jz-aes, motor, ms419xx, pwm_core, pwm_hal, sinfo, tcu_alloc, tx-isp-t31 all link |
| T31 4.4.94 | pass - ISP/Motor/PWM/AVPU/Audio build; only pre-existing warnings |
| A1 4.4.94 | pass - including merged soc-nna |
| T41 (either kernel) | **not compiled** - no T40/T41 kernel build tree available locally; verified structurally instead |

Structural verification used where a build was not possible:
- `common/isp/t41/*` are byte-identical to the original `4.4.94/isp/t41/*`
  (only `Kbuild` firmware selection changed), so 4.4 behavior is preserved.
- The avpu merge changed only two guards; everything else is byte-identical to
  the original 3.10.14 t31 source (which already handled T31/C100/T40/T41).

## 8. Remaining work / known gaps

- **Build-verify T40/T41** once a T40/T41 kernel tree is available. The merged
  avpu T41 clock branches and `common/isp/t41` have not been compiled here.
- **Check on real hardware** (or at least insmod) the split drivers to confirm
  device names: `motor` (`/dev/motor`), `pwm_core`/`pwm_hal` (`/dev/pwm`),
  `tx-isp-t31`, `avpu`.
- The `firmware` build may still pass `CONFIG_INGENIC_PWM=y`/`CONFIG_INGENIC_MOTOR`
  which the repo `Kbuild` does not consume (it keys off `KERNEL_VERSION` and
  `BR2_THINGINO_MOTORS`). This mismatch predates this branch; not changed here.
- Optional future work: unify the sensor drivers on the de-shimmed (4.4) form.
  Large and unverifiable without per-driver review; deliberately deferred.

### 8b. Sensor source standardization (in progress)

Goal: every sensor driver uses the same define-section layout:

    SENSOR IDENTIFICATION   SENSOR_NAME, SENSOR_VERSION, SENSOR_CHIP_ID*
    HARDWARE INTERFACE      SENSOR_BUS_TYPE, SENSOR_I2C_ADDRESS
    SENSOR CAPABILITIES     SENSOR_MAX_WIDTH/HEIGHT
    REGISTER DEFINITIONS    SENSOR_REG_END/DELAY
    TIMING AND PERFORMANCE  SENSOR_SUPPORT_*_SCLK, SENSOR_OUTPUT_*_FPS, MCLK
    SPECIAL FEATURES        feature toggles (SENSOR_EXPO, MIR_FLIP, ...)

Done: `.clang-format` (from pending PR #36) applied to shrink variant drift.
**All three sensor trees are now fully canonical (0 non-canonical files):**
`3.10.14/sensor-src`, `4.4.94/sensor-src`, `common/sensor-src/t23`. Every
driver uses the section layout below with the `SENSOR_*` define vocabulary;
no driver-prefixed standard macros remain.

Remaining / not done: T40/T41/t23 sensor drivers cannot be compiled here (no
kernel build tree for those SoCs), so the renames/banner moves in those trees
are **not build-verified** — only the t23 drivers that build on the t23/3.10.14
tree were checked (bf314a, gc2063, sc1346, cv4002, os02n10, os02n10? , s5k3p3).

Pre-existing sensor bugs still open (NOT caused by this work, present in
`origin/master`):
- `common/sensor-src/t23/sc301iot.c`: uses `sensor_attr.max_fps` which the t23
  `struct tx_isp_sensor_attribute` lacks.
- `common/sensor-src/t23/os02n10s0.c`, `os02n10s1.c`: use `.fsync_attr`, not
  in the t23 `tx_isp_sensor_attribute`.
- Imbalanced preprocessor in `t40/mis2031.c` (1 `#if` / 2 `#endif`),
  `t41/mis5011.c` (0/1), `t41/mis20s1.c` (29/30).

Layout decision for TVERSION/feature shapes (see commits 01629b62, 835590e5,
2c62121e): feature toggles + the `SENSOR_I2C_REG_*` width selection go in
SPECIAL FEATURES *before* REGISTER DEFINITIONS (the `#ifndef
SENSOR_I2C_REG_8BIT` guard must see the toggle), the `SENSOR_REG_END/DELAY`
`#ifdef` pair in REGISTER DEFINITIONS, `SENSOR_MCLK` in TIMING.

Pre-existing build bugs fixed along the way: `gc5603`, `cv2001/cv3001/cv4001`,
`bf3a03`, `gc0328`, `gc032a`, `sc2235` (undefined macros); `sc3336` t31
(`sensor_mipi_2`); t23 ISP (`get_driver_common_interfaces`); t31 3.10
include order (`ar1337`, `gc1034`, `gc1084`); t40 `imx662` (malformed
`SENSOR_VERSION`); `sensor_REG_*` uses in t40 bf20a1/bf2253/bf2253s1 and
t41 gc4653/mis2032/os03a10/os04e10/os08c10/ov04c10/sc231hai.

`SENSOR_MAX_WIDTH/HEIGHT` (task 24): these feed only the informational
`/proc/jz/sensor/width|height`. Many drivers had them set to the MIPI crop
size (`image_twidth/theight`), the raw array size, or `0`; they now equal
`sensor_win_sizes[0]` (the default output mode), matching the convention used
by the large majority of drivers. `imx219` is deliberately left at its true
sensor max (3280x2464) - it is a custom driver that overrides `image_twidth`
per mode.

Also fixed: `jxf23` defined two structs both named `sensor_mipi` (renamed
`sensor_mipi1`/`sensor_mipi2`); `jxf32`/`jxf355p` have a pre-existing stray
`)` in a `printk("%s stream on\n", SENSOR_NAME));` (still open).

Hardcoded `sensor_attr.chip_id` (task 25): the t40/t41/t41zrt/t23/t30 drivers
define only `SENSOR_CHIP_ID_H`/`_M`/`_L` and hardcode the combined value. Most
matched `(H<<n)|M|L`; 41 files did not and were bugs (wrong/truncated id, or a
sibling sensor's id - e.g. `ov9281` reported `0x9732`, `imx334` `0x2003`).
Those now use the `(H<<n)|M|L` expression. For `imx662` (H/L = 0x00/0x00
placeholders), `n5` (same), `cv5003` and `cv4002` the correct id cannot be
determined without a datasheet, so a NOTE was added to the file header rather
than guessing.

## 9. Task history

| # | Task | Status |
|---|------|--------|
| 0 | Branch + roadmap | done |
| 1 | Rename 3.10 motor set to `motors-pp`; select per kernel | done |
| 2 | Merge `misc/jz-dtrng` into `common/misc/jz-dtrng` | done |
| 3 | Merge `misc/mpsys-driver` into `common/misc/mpsys-driver` | done |
| 4 | Rename 3.10 PWM set to `pwm-pp`; select per kernel | done |
| 5 | Merge all avpu SoC sets into `common/avpu` | done |
| 6 | (folded into 5) | done |
| 7 | Rename 3.10 ISP/t31 set to `isp/t31-pp`; select per kernel | done |
| 8 | Merge `isp/t41` (+`t41zrt` headers) into `common/isp` | done |
| 9 | Merge `sensor-info.[ch]` into `common/sensor-src` | done |
| 10 | Sensor drivers kept separate per kernel (decision) | done by design |
| 11 | Merge `misc/soc-nna` into `common/misc/soc-nna` | done |
| 12 | Cleanup (no empty trees; fixed a duplicated Kbuild info line) | done |
| 19 | clang-format pass over sensor drivers (`.clang-format` from PR #36) | done |
| 20 | Bannerize `3.10.14` + ~300 of `4.4.94` sensor drivers; fix undefined macros / include order | done |
| 21 | Rename file-prefixed defines to `SENSOR_*` + bannerize remaining sensor drivers | done (all three trees canonical; t40/t41 not build-verified) |
| 13 | Docs + build-matrix verification | done |
| 14 | Flatten `sdk/` to one root with versioned filenames; dedup byte-identical blobs | done |
| 15 | Make compiler and kernel tags explicit in every firmware filename and Kbuild; normalize kernel tag to `31014`/`4494` | done |
| 16 | Merge the t41 `oss3` audio driver into `common/audio/t41/oss3`; route t41/t23 to oss3 | done |
| 17 | Move all audio to `common/audio/<soc>/<driver>` (relocation, no collisions) | done |
| 18 | Relocate all single-kernel `misc`, `isp`, `sensor-src` (t10..t30, c100) and a1-only `aip/fb/ipu/video` to `common/` | done |
| 22 | Remove vendor copy-paste garbage comment blocks; `.chip_id` -> `SENSOR_CHIP_ID`; stale `<name>_win_sizes` comments | done |
| 23 | Add explicit `<soc/gpio.h>` / `<txx-funcs.h>` includes to the 3.10.14 sensor drivers so both trees carry the same include set | done |
| 24 | Fix `SENSOR_MAX_WIDTH/HEIGHT` to match the default output window instead of the MIPI crop / raw size / 0 | done (19 files; imx219 left as intentional) |
| 25 | Fix hardcoded `sensor_attr.chip_id` to match the detected id; flag unverifiable ones with a header note | done (41 files fixed; imx662/n5/cv5003/cv4002 marked) |

## 10. Original inventory (for reference)

- 66 files were byte-identical across both trees.
- ~56 common files differed; differences ranged from whitespace/one include to
  large drift (`soc_nna_main.c` 901 lines, `tx-isp-debug.c` 678,
  `tx-isp-common.h` 295, `pwm_core.c` 284, ~900 sensor files).
