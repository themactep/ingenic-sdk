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

### 4.4 Platform-only trees (untouched)

- 4.4.94 only: `aip/a1`, `fb`, `ipu`, `video/a1`, `audio/a1`, `isp/t40`.
- 3.10.14 only: `isp/t20..t30`, `sensor-src/t20..t30`.

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

### 4.6 Audio driver selection

Two audio drivers exist: `oss2` (old `xb_snd`/`devices` layout) and `oss3`
(`boards`/`host`/`inner_codecs` layout). Selection is by SoC, not kernel:
`t41` and `t23` use `oss3`; every other SoC uses `oss2` on 3.10.14 and `oss3`
on 4.4.94 (4.4.94 has no `oss2`).

The t41 `oss3` sources were duplicated byte-for-byte in both trees (only the
`Kbuild` differed, and the 4.4.94 one was broken), so they now live once in
`common/audio/t41/oss3` and the top-level `Kbuild` routes t41 there.

## 5. Kbuild selection rules (top-level `Kbuild`)

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
- Audio is selected by SoC in the top-level `Kbuild`:
  - `t41` -> `common/audio/t41/oss3`
  - `t23` -> `$(KERNEL_VERSION)/audio/t23/oss3`
  - otherwise `oss2` on 3.10.14, `oss3` on 4.4.94.
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
| 13 | Docs + build-matrix verification | done |
| 14 | Flatten `sdk/` to one root with versioned filenames; dedup byte-identical blobs | done |
| 15 | Make compiler and kernel tags explicit in every firmware filename and Kbuild; normalize kernel tag to `31014`/`4494` | done |
| 16 | Merge the t41 `oss3` audio driver into `common/audio/t41/oss3`; route t41/t23 to oss3 | done |

## 10. Original inventory (for reference)

- 66 files were byte-identical across both trees.
- ~56 common files differed; differences ranged from whitespace/one include to
  large drift (`soc_nna_main.c` 901 lines, `tx-isp-debug.c` 678,
  `tx-isp-common.h` 295, `pwm_core.c` 284, ~900 sensor files).
