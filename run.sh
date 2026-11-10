#!/usr/bin/env bash

if [ ! -f img/disk.img ]; then
    echo "Creating 10MB disk.img for filesystem tests..."
    qemu-img create -f raw img/disk.img 10M
fi


qemu-system-x86_64 \
    -boot d \
    -cdrom NwOS.iso\
    -m 700M \
    -nic user,model=e1000 \
    -audiodev sdl,id=snd0 \
    -device intel-hda \
    -device hda-duplex,audiodev=snd0 \
    -drive file=img/disk.img,format=raw,if=ide,index=0 \
    -display sdl,grab-mod=rctrl