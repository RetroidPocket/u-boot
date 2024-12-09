# Building U-Boot for Retroid Pocket SM8250

## Prerequisites

You will need either an x86 machine with an aarch64 cross-compiler installed, or an existing aarch64 system. You will also need the device tree compiler dtc, python3 (including setuptools and pyelftools), swig, GNU make and git

On an x86_64 Debian (or derivates such as Ubuntu) system, you can install the required dependencies with:
```
sudo apt install device-tree-compiler build-essential gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu make python3 python3-dev libssl-dev python3-pyelftools python3-setuptools swig git mkbootimg
```

## Fetching the repositories
`git clone https://github.com/RetroidPocket/u-boot.git -b retroidpocket-sm8250`

## Building from source

For Retroid Pocket 5
```
cd u-boot
make CROSS_COMPILE=aarch64-linux-gnu- O=.output qcom_defconfig retroidpocket.config
make CROSS_COMPILE=aarch64-linux-gnu- O=.output -j$(nproc) DEVICE_TREE=qcom/sm8250-retroidpocket-rp5
gzip .output/u-boot-nodtb.bin -c > .output/u-boot-nodtb.bin.gz
cat .output/u-boot-nodtb.bin.gz .output/dts/upstream/src/arm64/qcom/sm8250-retroidpocket-rp5.dtb > .output/kernel-dtb
mkbootimg --kernel_offset '0x00008000' --pagesize '4096' --kernel .output/kernel-dtb -o .output/u-boot.img --cmdline "nodtbo"
```
For Retroid Pocket Mini
```
cd u-boot
make CROSS_COMPILE=aarch64-linux-gnu- O=.output qcom_defconfig retroidpocket.config
make CROSS_COMPILE=aarch64-linux-gnu- O=.output -j$(nproc) DEVICE_TREE=qcom/sm8250-retroidpocket-rpmini
gzip .output/u-boot-nodtb.bin -c > .output/u-boot-nodtb.bin.gz
cat .output/u-boot-nodtb.bin.gz .output/dts/upstream/src/arm64/qcom/sm8250-retroidpocket-rpmini.dtb > .output/kernel-dtb
mkbootimg --kernel_offset '0x00008000' --pagesize '4096' --kernel .output/kernel-dtb -o .output/u-boot.img --cmdline "nodtbo"
```

## Booting u-boot
You can test by booting the unit to fastboot mode (Power + Volume Down) then boot the image:

`fastboot boot .output/u-boot.img`

## Flashing u-boot
Retroid Pocket SM8250 platform have a loader partition to boot alternative OSes.
You can flash the u-boot.img in fastboot with this command:

`fastboot flash loader .output/u-boot.img`

Then you can boot a loader using (Power + Volume Up), or by selecting default boot to Loader instead of Android in fastboot mode.
