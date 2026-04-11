import struct
import sys

def set_dynamic_base(filepath):
    with open(filepath, 'r+b') as f:
        f.seek(0x3c)
        pe_offset = struct.unpack('<I', f.read(4))[0]
        # DllCharacteristics is at Optional Header Offset + 70 (for PE32)
        # Optional Header starts at PE Offset + 24
        # So it is at PE Offset + 24 + 70 = 94
        f.seek(pe_offset + 94)
        val = struct.unpack('<H', f.read(2))[0]
        f.seek(pe_offset + 94)
        f.write(struct.pack('<H', val | 0x40))

if __name__ == '__main__':
    set_dynamic_base(sys.argv[1])
