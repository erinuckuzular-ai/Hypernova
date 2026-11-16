#!/usr/bin/env python3
"""Compare the output of DspCoreCheckJuce and DspCoreCheckNoJuce (Tests/DspCoreCheck.cpp).

Every section must match bit for bit. Exits 0 when they all do, 1 otherwise, and names any that don't.
"""
import struct
import sys


def read_sections(path):
    sections = {}
    order = []
    with open(path, "rb") as f:
        data = f.read()
    pos = 0
    while pos < len(data):
        (n,) = struct.unpack_from("<I", data, pos); pos += 4
        name = data[pos:pos + n].decode(); pos += n
        (b,) = struct.unpack_from("<Q", data, pos); pos += 8
        sections[name] = data[pos:pos + b]; pos += b
        order.append(name)
    return order, sections


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    order_a, a = read_sections(sys.argv[1])
    order_b, b = read_sections(sys.argv[2])
    bad = 0
    if order_a != order_b:
        print("section lists differ:", sorted(set(order_a) ^ set(order_b)))
        bad += 1
    total = 0
    for name in order_a:
        if name not in b:
            continue
        total += len(a[name])
        if a[name] != b[name]:
            diff = sum(1 for x, y in zip(a[name], b[name]) if x != y) + abs(len(a[name]) - len(b[name]))
            print(f"MISMATCH {name}: {diff} of {len(a[name])} bytes differ")
            bad += 1
    print(f"{len(order_a)} sections, {total:,} bytes compared: " + ("all identical" if bad == 0 else f"{bad} differ"))
    return 0 if bad == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
