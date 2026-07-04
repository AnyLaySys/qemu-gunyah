#! /usr/bin/env python3

import hashlib
import os
import sys

sha = hashlib.sha1()
is_file = False
for arg in sys.argv[1:]:
    if arg == '--':
        is_file = True
        continue
    if is_file:
        with open(arg, 'rb') as f:
            for chunk in iter(lambda: f.read(65536), b''):
                sha.update(chunk)
    else:
        sha.update(os.fsencode(arg))
        sha.update(b'\n')

print("_" + sha.hexdigest())
