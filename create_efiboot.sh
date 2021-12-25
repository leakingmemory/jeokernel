#!/bin/sh

if grub-file --is-x86-multiboot2  boot32/jeokernel; then
  if bzip2 -dc ${CMAKE_SOURCE_DIR}/efipart.img.start.bz2 > efipart.img.start &&
  bzip2 -dc ${CMAKE_SOURCE_DIR}/efipart.img.efi.bz2 > efipart.img.efi &&
  bzip2 -dc ${CMAKE_SOURCE_DIR}/efipart.img.fs.bz2 > efipart.img.fs &&
  bzip2 -dc ${CMAKE_SOURCE_DIR}/efipart.img.end.bz2 > efipart.img.end &&
  cp -vp ${CMAKE_SOURCE_DIR}/mtools.conf mtools.conf &&
  grub-mkimage --format=x86_64-efi "--prefix=(hd0,gpt1)/boot/grub" part_gpt fat > bootx64.efi &&
  env MTOOLSRC=mtools.conf mdel a:/efi/boot/bootx64.efi &&
  env MTOOLSRC=mtools.conf mcopy bootx64.efi a:/efi/boot/bootx64.efi &&
  env MTOOLSRC=mtools.conf mcopy ${CMAKE_SOURCE_DIR}/grubefi.cfg a:/boot/grub/grub.cfg &&
  env MTOOLSRC=mtools.conf mdir a:/boot &&
  env MTOOLSRC=mtools.conf mdir a:/boot/grub/grub.cfg &&
  mkdir -p grubimg &&
  fuse2fs -o fakeroot efipart.img.fs grubimg; then
    mkdir grubimg/boot &&
    if cp -v boot32/jeokernel grubimg/boot/loader &&
    cp -v kernel/kernel grubimg/boot/kernel; then
      echo "Successfully installed files in image"
    else
      sleep 2
      umount grubimg
      exit 1
    fi
    sleep 2
    umount grubimg &&
    dd if=efipart.img.start of=jeokernelefi.img bs=1048576 count=1 &&
    rm efipart.img.start &&
    dd if=efipart.img.efi of=jeokernelefi.img bs=1048576 count=33 seek=1 &&
    rm efipart.img.efi &&
    dd if=efipart.img.fs of=jeokernelefi.img bs=1048576 count=29 seek=34 &&
    rm efipart.img.fs &&
    dd if=efipart.img.end of=jeokernelefi.img bs=1048576 count=1 seek=63 &&
    rm efipart.img.end &&
    echo "Successfully installed files in image"
  else
    exit 1
  fi
else
  echo "Not valid kernel"
  exit 1
fi
