# iomemory-vsl4
iomemory-vsl4 for kernel 4.19

## System
Debian 10 with Kernel 4.19.0-9-amd64

## Install 

```bash
# ************************ Tools ************************
# Install Tools
apt update
apt install curl fio zip unzip vim git
```

```bash
# ************************ Fusion-Io ************************
# Install Fusion-Io Drivers and Util
# Packaged needed for installation
apt install gcc build-essential debhelper dkms linux-headers-$(uname -r)
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

# Check every thing is correct installed
fio-status -a
```