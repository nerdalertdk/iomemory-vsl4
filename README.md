# iomemory-vsl4
iomemory-vsl4 for kernel 4.19


## System
Debian 10 with Kernel 4.19.0-9-amd64
For SanDisk PX600

## Install 
```bash
# ************************ Fusion-Io ************************
# Install Fusion-Io Drivers and Util
# Packaged needed for installation
apt upd
apt install curl fio zip unzip vim git gcc build-essential debhelper dkms linux-headers-$(uname -r)
```

```bash
git clone git@github.com:nerdalertdk/iomemory-vsl4.git /tmp/iomemory
cd /tmp/iomemory/
```

```bash
# ************* DRIVER *************

# Install and compile new kernel module
cp -r Source/iomemory-vsl4-4.3.6 /usr/src/
touch /usr/src/iomemory-vsl4-4.3.6/dkms.conf
cat << EOF >> /usr/src/iomemory-vsl4-4.3.6/dkms.conf
MAKE="'make' DKMS_KERNEL_VERSION=$kernelver"
CLEAN="'make' clean"
BUILT_MODULE_NAME=iomemory-vsl4
BUILT_MODULE_LOCATION=''
PACKAGE_NAME=iomemory-vsl4
PACKAGE_VERSION=4.3.6
DEST_MODULE_LOCATION=/kernel/drivers/block
AUTOINSTALL="Yes"
REMAKE_INITRD=Yes
EOF

#dkms remove iomemory-vsl4/4.3.5 --all
dkms add -m iomemory-vsl4 -v 4.3.6
dkms build -m iomemory-vsl4 -v 4.3.6
dkms install -m iomemory-vsl4 -v 4.3.6
# Check the kernel module install correct
dkms status

# Load new driver
modprobe iomemory-vsl4
modinfo iomemory-vsl4

#Install Fusion-Io tools
dpkg -i Utilities/fio-*

# Check every thing is correct installed
fio-status -a
```


### Fixes
`ticks replaced with nsecs`
 * has no member named ‘ticks’


`current_kernel_time replaced with ktime_get_coarse_real_ts64`
 * implicit declaration of function ‘current_kernel_time’; did you mean ‘current_kernel_time64’? [-Werror=implicit-function-declaration]                
     struct timespec ts = current_kernel_time();


`MODULE_LICENSE "Proprietary" repalced with "GPL"`
 * FATAL: modpost: GPL-incompatible module iomemory-vsl.ko uses GPL-only symbol 'ktime_get_real_seconds'
