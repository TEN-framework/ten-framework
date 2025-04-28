#!/bin/bash

wget -O- https://ahrefs.com/blog/most-visited-websites/ | grep most-visited-websites-us | \
	sed -E 's/class="column-2">/|/g' | tr '|' '\n' | \
	sed 's/<.*//g' | grep -v Domain | grep -v Josh | sort | uniq

