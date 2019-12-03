- Get the latest version of MP from https://github.com/micropython/micropython

- From the latest version of MP take only what is needed to build `ports/bare-arm` and copy here, which is this:
    - `/docs/conf.py`
    - `/drivers/bus/spi.h`
    - `/extmod/extmod.mk`
    - selected *.c i *.h files from `/extmod` (`make` will complain if something is missing)
    - `/lib/embed/abort_.c`
    - `/lib/utils/interrupt_char.c`
    - `/lib/utils/interrupt_char.h`
    - `/lib/utils/printf.c`
    - all files from `/ports/bare-arm`
    - all files from `/py`

- Go to `/ports/bare-arm`

- Backup `/ports/bare-arm/mpconfigport.h` to `/ports/bare-arm/mpconfigport.original.h`

```
cd /mnt/c/Users/Martin/Dropbox/Code/EEZ/modular-psu-firmware/src/third_party/micropython/ports/bare-arm
cp mpconfigport.h mpconfigport.original.h
```
