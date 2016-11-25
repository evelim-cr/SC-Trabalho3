#!/bin/bash

qemu-system-x86_64 \
    -device i82559er,netdev=net0 \
    -netdev tap,ifname=tap0,id=net0,script=up.sh,downscript=no \
    hda.qcow2
