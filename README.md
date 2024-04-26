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
SENSOR=gc2053 ./build.sh t31 3.10
```

### Parameters:
- `<soc>`: Specify the Ingenic SoC model you are using, such as `t31`, `t40`, etc.
- `<kernel_version>`: Indicate the kernel version. Supported versions include `3.10` and `4.4`.
- `<make_args>`: Additional make arguments as required.

Ensure you provide the correct `SOC` environment variable corresponding to your sensor and SoC setup before executing the build command.
