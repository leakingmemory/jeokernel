#!/bin/sh

if grub-file --is-x86-multiboot2  boot32/jeokernel; then
  if rm -f partimg.img diskimg.img &&
  mkdir -p grubimg &&
  bzip2 -dc ${CMAKE_SOURCE_DIR}/partimg.img.bz2 > partimg.img &&
  fuse2fs -o fakeroot partimg.img grubimg; then
    if cp -v boot32/jeokernel grubimg/boot/loader &&
    cp -v boot64/kernel grubimg/boot/kernel &&
    cp -v ${CMAKE_SOURCE_DIR}/grub.cfg grubimg/boot/grub/; then
      echo "Successfully installed files in image"
    else
      sleep 2
      umount grubimg
      exit 1
    fi
    sleep 2
    umount grubimg &&
    bzip2 -dc ${CMAKE_SOURCE_DIR}/diskimg.img.bz2 > diskimg.img &&
    dd if=partimg.img of=diskimg.img seek=2048 bs=512 &&
    mv -v diskimg.img jeokernel.img
  else
    exit 1
  fi
else
  echo "Not valid kernel"
  exit 1
fi
