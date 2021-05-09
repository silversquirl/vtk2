#!/usr/bin/env python3
import mmap, sys

mm = mmap.mmap(sys.stdin.fileno(), 0, access=mmap.ACCESS_READ)
print('const char ' + sys.argv[1] + '[] = {\n\t', end='')
n = 0
for b in mm:
	s = str(b[0]) + ','
	n += len(s)
	if n > 100:
		print('\n\t', end='')
		n = 0
	print(s, end='')
print('\n};')
