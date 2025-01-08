#!/bin/bash

declare -A relocs

for f in /usr/bin/*; do
	echo "Processing $f"
	if file $f | grep -q ELF; then
		echo "...Collecting relocs"
		# ELF found, collect relocs
		relocs_in_f=$(llvm-readelf --relocs $f | grep -oP "\bR_X86_64.*?\b")
		for l in $relocs_in_f; do
			relocs[$l]=$((1+relocs[$l]))
		done
	fi
done

for k in "${!relocs[@]}"; do
  echo "$k => ${relocs[$k]}"
done

