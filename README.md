## Open Source Ingenic kernel modules

### Building Ingenic Kernel Modules

#### How to Build

To compile the kernel modules for Ingenic SoCs, run the following command from your terminal:

```console
./build.sh <soc> <kernel_version> <make_args>
./build.sh clean
```

Example for building the kernel module for the GC2053 sensor on a T31 SoC with kernel version 3.10:

```console
SENSOR_MODEL=gc2053 ./build.sh t31 3.10
```

### Parameters:
- `<soc>`: Specify the Ingenic SoC model you are using, such as `t31`, `t40`, etc.
- `<kernel_version>`: Indicate the kernel version. Supported versions include `3.10` and `4.4`.
- `<make_args>`: Additional make arguments as required.

Ensure you provide the correct `SOC` environment variable corresponding to your sensor and SoC setup before executing the build command.

### Source layout

Driver sources live under `common/`, organized by subsystem and SoC
(`common/isp/<soc>`, `common/audio/<soc>/<driver>`, `common/misc/<name>`,
`common/avpu`, `common/sensor-src/<soc>`, ...). Per-kernel or per-SoC
differences are handled in the sources via `CONFIG_KERNEL_*` / `CONFIG_SOC_*`
guards. Drivers that are genuinely a different implementation per kernel
(e.g. motor, pwm, t31 ISP) keep separate source sets and are selected per
kernel in the top-level `Kbuild`.

The only content still kept per tree is the sensor drivers that exist for both
kernels (`3.10.14/sensor-src/{t31,t40,t41,t41zrt}` and the 4.4.94 counterparts);
they use different calling conventions and are not yet unified.

Prebuilt firmware blobs live in a single top-level `sdk/` directory. Every
filename carries its full identity so a blob is selected purely by its path:

    sdk/<soc>/lib<soc>-<kind>-firmware-<version>-<compiler>-<kernel>[-<variant>].a

where `<compiler>` is the GCC version tag (`472` = 4.7.2, `540` = 5.4.0,
`720` = 7.2.0) and `<kernel>` is the target kernel (`31014` = 3.10.14,
`4494` = 4.4.94). Examples:
`sdk/t31/libt31-firmware-1.1.6-540-31014.a`,
`sdk/t41/libt41-firmware-1.2.6-720-4494.a`,
`sdk/a1/libfb-firmware-1.6.2-720-4494.a`.
Byte-identical duplicates are kept as symlinks (e.g. `sdk/t10 -> t20`).

See `docs/merge-driver-trees-roadmap.md` for the full source map, Kbuild
selection rules and build/verification notes.
