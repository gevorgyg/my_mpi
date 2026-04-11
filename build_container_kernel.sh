#!/bin/sh
# Build a kernel for macOS container tool.
# Run inside a Linux build container:
#   container run -it -v /Volumes/LinuxKernelDev/linux:/linux alpine:latest sh
#   apk add build-base flex bison bc perl openssl-dev elfutils-dev diffutils findutils
#   cd /linux && sh build_container_kernel.sh

set -e

echo "==> Cleaning..."
make mrproper

echo "==> Starting from defconfig..."
make defconfig

echo "==> Merging container options..."
scripts/config \
  --enable VSOCKETS \
  --enable VIRTIO_VSOCKETS \
  --enable VIRTIO_FS \
  --enable FUSE_FS \
  --enable OVERLAY_FS \
  --enable SQUASHFS \
  --enable SQUASHFS_XZ \
  --enable SQUASHFS_ZSTD \
  --enable NET_9P \
  --enable NET_9P_VIRTIO \
  --enable 9P_FS \
  --enable 9P_FS_POSIX_ACL \
  --enable USER_NS \
  --enable POSIX_MQUEUE \
  --enable BLK_DEV_INITRD

echo "==> Resolving dependencies..."
make olddefconfig

echo "==> Building Image..."
make -j$(nproc) Image

echo "==> Done: arch/arm64/boot/Image"
