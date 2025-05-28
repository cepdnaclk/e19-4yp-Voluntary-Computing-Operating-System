#!/bin/bash

LFS_DISK="$1"

# Wipe existing partitions
sudo wipefs -a "$LFS_DISK"

# Partition the loop device
sudo fdisk "$LFS_DISK" << EOF
o
n
p
1

+100M
a
n
p
2


w
EOF
