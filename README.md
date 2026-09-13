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

Drivers shared by both kernels live in `common/`; per-kernel or per-SoC
differences are handled in the sources via `CONFIG_KERNEL_*` / `CONFIG_SOC_*`
guards. Drivers that are genuinely a different implementation per kernel
(e.g. motor, pwm, t31 ISP) keep separate source sets and are selected per
kernel in the top-level `Kbuild`.

Prebuilt firmware blobs live in a single top-level `sdk/` directory. The
version (and build/kernel tag where relevant) is part of the filename, so a
blob is selected purely by its path, e.g.
`sdk/t31/libt31-firmware-1.1.6-540.a`, `sdk/t41/libt41-firmware-1.2.6-3-10-14.a`,
`sdk/a1/libfb-firmware-1.6.2.a`. Byte-identical duplicates are kept as
symlinks (e.g. `sdk/t10 -> t20`).

See `docs/merge-driver-trees-roadmap.md` for the full source map, Kbuild
selection rules and build/verification notes.
