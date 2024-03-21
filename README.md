# GudPkg

A UEFI GOP driver for [Generic USB Display (GUD)](https://github.com/notro/gud).

## Building

```sh
$ sudo apt install build-essential uuid-dev iasl git nasm python-is-python3
$ git clone https://github.com/tianocore/edk2 --recursive
$ cd edk2
$ . edksetup.sh
$ git clone https://github.com/mbyzhang/GudPkg
$ build -a X64 -t GCC5 -p GudPkg/GudPkg.dsc
```

