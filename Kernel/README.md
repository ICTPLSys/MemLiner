Linux kernel
============

There are several guides for kernel developers and users. These guides can
be rendered in a number of formats, like HTML and PDF. Please read
Documentation/admin-guide/README.rst first.

In order to build the documentation, use ``make htmldocs`` or
``make pdfdocs``.  The formatted documentation can also be read online at:

    https://www.kernel.org/doc/html/latest/

There are various text files in the Documentation/ subdirectory,
several of them using the Restructured Text markup notation.

Please read the Documentation/process/changes.rst file, as it contains the
requirements for building and running the kernel, and information about
the problems which may result by upgrading your kernel.

# Linux-5.11 kernel with fastswap

This repo is a fork of Linux kernel (version 5.11). We port [fastswap](https://github.com/clusterfarmem/fastswap) to support swapping to remote memory on another machine via RDMA.

To use remote memory, remoteswap is needed as a collection of a deamon (on remote memory machine) and a client (on local CPU machine). Remoteswap is available at https://github.com/jiaweixiao/remoteswap/tree/linux-5.11-vanilla.

## Dependencies

The current version is tested and recommended on Ubuntu 20.04.

* Hardware: Mellanox ConnectX-5 (InfiniBand)
* RDMA NIC driver: MLNX_OFED 5.6-2.0.9.0

## Build & Install

1. Build and install the kernel.
2. Update grub and change the default launching kernel to "Linux 5.11.0".
3. Reboot system, download and compile the MLNX_OFED driver.
4. Deploy remoteswap on both local CPU server and remote memory server (follow instructions there).
5. Everything should be ready to go after remoteswap established the connection.

### Kernel
```bash
# prerequisites
sudo apt-get install git fakeroot build-essential ncurses-dev xz-utils libssl-dev bc flex libelf-dev bison
# initialize the build config
cp config .config
# build kernel (might take a while...)
sudo ./build_kernel.sh build
# install built kernel and kernel modules
sudo ./build_kernel.sh install
# (optional) install built kernel only. No need to run this after `./build_kernel.sh install'.
# This is useful when you have installed kernel modules by `./build_kernel.sh install`,
# and you want to replace only the kernel after some modifications.
# (sudo priviledge will be needed during the script running)
./build_kernel.sh replace
```

### Grub
Modify GRUB_CMDLINE_LINUX_DEFAULT in /etc/default/grub with:
```
transparent_hugepage=madvise mitigations=off iommu=pt msr.allow_writes=on intel_pstate=disable
```
and run:
```
sudo update-grub
sudo reboot
```
Disable **hyperthread** in BIOS.

### MLNX_OFED driver

Make sure the machine is booted with the compiled kernel before installing the driver.
You might need to change the grub settings before rebooting the machine to make sure it will enter our compiled kernel.
You don't need to reinstall the driver as long as the kernel version is not changed.
Download from Check https://network.nvidia.com/products/infiniband-drivers/linux/mlnx_ofed/.
```bash
# compile and install
sudo ./mlnxofedinstall --add-kernel-support
# restart the service to enable it
sudo /etc/init.d/openibd restart
```

