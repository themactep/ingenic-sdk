## Opensourced Ingenic kernel modules

### Build manually

```console
export PATH=/path/to/openipc_sdk/bin:$PATH
make \
    ARCH=mips CROSS_COMPILE=mipsel-openipc-linux-musl- \
    SOC=t31 \
    -C ~/git/firmware/output/build/linux-3.10.14 \
    M=$PWD
```

where `SOC` can be set to options `t31`, `t40` and other wip.
