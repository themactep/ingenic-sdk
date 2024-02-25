## Opensourced Ingenic kernel modules

### Building

```console
export PATH=/path/to/openipc_sdk/bin:$PATH
make ARCH=mips CROSS_COMPILE=mipsel-openipc-linux-musl- \
    -C ~/git/firmware/output/build/linux-3.10.14 \
    SOC=t31 M=$PWD
```

where `SOC` is your Ingenic SoC model: `t31`, `t40`, etc.

