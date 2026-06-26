#!/usr/bin/env python3

import argparse
import re
from subprocess import check_output, STDOUT, CalledProcessError
import sys

MAP = {
    "alpha" : ["alpha"],
    "aarch64" : ["aarch64", "aarch64_be"],
    "armv7": ["arm"],
    "armv8-a" : ["aarch64", "aarch64_be"],
    "avr" : ["avr"],
    "hppa1.0" : ["hppa"],
    "i386" : ["i386"],
    "i386:x86-64" : ["x86_64"],
    "Loongarch64" : ["loongarch64"],
    "m68k" : ["m68k"],
    "MicroBlaze" : ["microblaze"],
    "mips:isa64" : ["mips64", "mips64el"],
    "or1k" : ["or1k"],
    "powerpc:common" : ["ppc"],
    "powerpc:common64" : ["ppc64", "ppc64le"],
    "riscv:rv32" : ["riscv32"],
    "riscv:rv64" : ["riscv64"],
    "s390:64-bit" : ["s390x"],
    "sh4" : ["sh4", "sh4eb"],
    "sparc": ["sparc"],
    "sparc:v8plus": ["sparc32plus"],
    "sparc:v9a" : ["sparc64"],
    "xtensa" : ["xtensa", "xtensaeb"]
}


def do_probe(gdb):
    try:
        gdb_out = check_output([gdb,
                               "-ex", "set architecture",
                               "-ex", "quit"], stderr=STDOUT, encoding="utf-8")
    except (OSError) as e:
        sys.exit(e)
    except CalledProcessError as e:
        sys.exit(f'{e}. Output:\n\n{e.output}')

    found_gdb_archs = re.search(r'Valid arguments are (.*)', gdb_out)

    targets = set()
    if found_gdb_archs:
        gdb_archs = found_gdb_archs.group(1).split(", ")
        mapped_gdb_archs = [arch for arch in gdb_archs if arch in MAP]

        targets = {target for arch in mapped_gdb_archs for target in MAP[arch]}

    return targets


def main() -> None:
    parser = argparse.ArgumentParser(description='Probe GDB Architectures')
    parser.add_argument('gdb', help='Path to GDB binary.')

    args = parser.parse_args()

    supported = do_probe(args.gdb)

    print(" ".join(supported))

if __name__ == '__main__':
    main()
