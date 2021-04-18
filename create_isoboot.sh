#!/bin/sh

if grub-file --is-x86-multiboot2  boot32/jeokernel; then
  mkdir -p mkdir -p iso/boot/grub
  cp -pv boot32/jeokernel iso/boot/loader
  cp -pv boot64/kernel iso/boot/kernel
  cp -pv ../grub.cfg iso/boot/grub/
  grub-mkrescue -o jeokernel.iso iso/
else
  echo "Not valid kernel"
  exit 1
fi
