#!/bin/bash

convert -size 128x64 /tmp/128x64.png -monochrome output.bmp
dd if=output.bmp bs=1 skip=130 | hexdump -Cv | tr -s ' ' | cut -d' ' -f2-17 | grep ' ' | sed "s/^/0x/g" | sed "s/\ /,\ 0x/g" > pic.h.1
cat pic.h.1 | sed "s/\$/,/g" > pic.h

gcc -o scan scan.c && ./scan > ../banded-img.h

