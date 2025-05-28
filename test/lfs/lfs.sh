#!/bin/bash

export LFS=/mnt/lfs
export LFS_TGT=x86_64-lfs-linux-gnu
export IMG_FILE=lfs_disk.img
export IMG_SIZE=20G

# Create image file if it doesn't exist
if [ ! -f "$IMG_FILE" ]; then
    echo "Creating virtual disk image..."
    fallocate -l "$IMG_SIZE" "$IMG_FILE" || exit 1
fi

if ! grep -q "$LFS" /proc/mounts; then
    # Find a free loop device and associate it with the image (with -P for partition scan)
    LOOP_DEV=$(sudo losetup -f)
    sudo losetup -P "$LOOP_DEV" "$IMG_FILE"

    # Partition the loop device using setupdisk.sh
    sudo bash setupdisk.sh "$LOOP_DEV"

    # Refresh the kernel's view of the partition table
    sudo partprobe "$LOOP_DEV"
    sleep 1  # Give time for /dev/loopXp1 and /dev/loopXp2 to appear

    # Format the partitions
    sudo mkfs.ext2 -F "${LOOP_DEV}p1"
    sudo mkfs.ext2 -F "${LOOP_DEV}p2"

    export LFS_DISK="$LOOP_DEV"

    # Create mount point and mount the second partition
    sudo mkdir -p "$LFS"
    sudo mount "${LFS_DISK}p2" "$LFS"
    sudo chown -v $USER "$LFS"
fi

# Create basic directory structure
mkdir -pv $LFS/{sources,tools,boot,etc,bin,lib,sbin,usr,var}

# Add lib64 if system is 64-bit
case $(uname -m) in 
    x86_64) mkdir -pv $LFS/lib64 ;;
esac

# Optional cleanup on script exit (uncomment to enable)
# cleanup() {
#     sudo umount -R "$LFS" 2>/dev/null
#     sudo losetup -d "$LOOP_DEV" 2>/dev/null
# }
# trap cleanup EXIT
