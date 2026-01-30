#!/usr/bin/env python3
import sys

def name(i):
    if i < 0x20:
        return \
            ['NUL ', 'SOH ', 'STX ', 'ETX ', 'EOT ', 'ENQ ', 'ACK ', 'BEL ',
             'BS  ', 'HT  ', 'LF  ', 'VT  ', 'FF  ', 'CR  ', 'SO  ', 'SI  ',
             'DLE ', 'DC1 ', 'DC2 ', 'DC3 ', 'DC4 ', 'NAK ', 'SYN ', 'ETB ',
             'CAN ', 'EM  ', 'SUB ', 'ESC ', 'FS  ', 'GS  ', 'RS  ', 'US  '][i]
    elif i == 0x7f:
        return 'DEL '

for i in range(256):
    if chr(i) == ' ':
        sys.stdout.write('1 /* SPC  */, ')
    elif chr(i) == '\t':
        sys.stdout.write('1 /* HT   */, ')
    elif (0x21 <= i and i < 0x7f):
        sys.stdout.write('1 /* {}    */, '.format(chr(i)))
    elif 0x80 <= i:
        sys.stdout.write('1 /* {} */, '.format(hex(i)))
    else:
        sys.stdout.write('0 /* {} */, '.format(name(i)))
    if (i + 1)%4 == 0:
        sys.stdout.write('\n')
