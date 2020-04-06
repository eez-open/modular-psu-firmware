- Copy `/ports/bb3/mpconfigport.h` over `/ports/bare-arm/mpconfigport.h`

- Go to `/ports/bare-arm` and `make USER_C_MODULES=/mnt/c/Users/Martin/Dropbox/Code/EEZ/modular-psu-firmware/src/third_party/micropython/ports/bb3/mod`, it doesn't matter if fails.

- Copy:

    - `moduledefs.h`
    - `mpversion.h`
    - `qstrdefs.generated.h` 
    
    from `/ports/bare-arm/build/genhdr` to `/ports/bb3/genhdr`

- Start build

```
cd /mnt/c/work/eez/modular-psu-firmware/src/third_party/micropython/ports/bare-arm
cp ../bb3/mpconfigport.h mpconfigport.h

make clean
make USER_C_MODULES=/mnt/c/work/eez/modular-psu-firmware/src/third_party/micropython/ports/bb3/mod

cp build/genhdr/moduledefs.h ../bb3/genhdr/moduledefs.h
cp build/genhdr/mpversion.h ../bb3/genhdr/mpversion.h
cp build/genhdr/qstrdefs.generated.h ../bb3/genhdr/qstrdefs.generated.h
```

The build doesn't need to succeed or finish - you can stop if after files are generated, i.e. after you see first `CC ..` in terminal.

