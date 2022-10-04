aarch64-linux-gnu-as -c guest-aarch64-code.S
aarch64-linux-gnu-objcopy -S -O binary a.out guest-aarch64-code.bin
