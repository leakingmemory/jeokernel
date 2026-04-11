#!/bin/sh

# The UEFI 64-bit shim image creation script
# It puts the shim at /EFI/BOOT/BOOTX64.EFI (the default boot path for 64-bit UEFI)

if [ -z "${CMAKE_SOURCE_DIR}" ]; then
    echo "CMAKE_SOURCE_DIR not set"
    exit 1
fi

if [ -f "uefi64/jeokernel_shim.efi" ]; then
    SHIM_PATH="uefi64/jeokernel_shim.efi"
else
    echo "jeokernel_shim.efi not found"
    exit 1
fi

# We use the same partition strategy as create_efiboot.sh,
# but without GRUB and without the kernel file system part (since kernel is embedded)
# We'll use the EFI partition part (efipart.img.efi)

# Cleanup any previous runs
rm -f uefi64_efipart.img.start uefi64_efipart.img.efi uefi64_efipart.img.fs uefi64_efipart.img.end uefi64_mtools.conf

if bzip2 -dc ${CMAKE_SOURCE_DIR}/efipart.img.start.bz2 > uefi64_efipart.img.start &&
   bzip2 -dc ${CMAKE_SOURCE_DIR}/efipart.img.end.bz2 > uefi64_efipart.img.end; then

    dd if=/dev/zero of=uefi64_efipart.img.efi bs=1048576 count=33
    mformat -i uefi64_efipart.img.efi -h 32 -t 32 -n 64 -c 1

    # Create a local mtools.conf for this operation
    echo "drive a: file=\"uefi64_efipart.img.efi\"" > uefi64_mtools.conf

    # Ensure the directory exists (it might already in the base image)
    # Redirecting to /dev/null because it might already exist
    env MTOOLSRC=uefi64_mtools.conf mmd a:/efi
    env MTOOLSRC=uefi64_mtools.conf mmd a:/efi/boot

    # Copy the shim to the default location for 64-bit UEFI
    env MTOOLSRC=uefi64_mtools.conf mcopy -o ${SHIM_PATH} a:/efi/boot/bootx64.efi

    # Create a dummy FS partition filled with zeros to maintain same disk layout as create_efiboot.sh
    # (29 MB)
    dd if=/dev/zero of=uefi64_efipart.img.fs bs=1048576 count=29

    # Combine parts into final image
    dd if=uefi64_efipart.img.start of=jeokernel_uefi64.img bs=1048576 count=1
    dd if=uefi64_efipart.img.efi of=jeokernel_uefi64.img bs=1048576 count=33 seek=1
    dd if=uefi64_efipart.img.fs of=jeokernel_uefi64.img bs=1048576 count=29 seek=34
    dd if=uefi64_efipart.img.end of=jeokernel_uefi64.img bs=1048576 count=1 seek=63

    rm uefi64_efipart.img.start uefi64_efipart.img.efi uefi64_efipart.img.fs uefi64_efipart.img.end uefi64_mtools.conf

    echo "Successfully created jeokernel_uefi64.img"
else
    echo "Failed to decompress image parts"
    exit 1
fi
