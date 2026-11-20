#!/bin/bash

set -euo pipefail

if [ ! -v MOA_PATH ]; then
	echo "Error: \$MOA_PATH not set. Set it to the base directory of the moa repository"
	echo "https://github.com/transistorfet/moa"
	exit 1
fi

CARGO="cargo +nightly -Z unstable-options -C $MOA_PATH"
CONFIGS=$(ls config/tests-moa)

echo ""
echo ">>> Building moa"
echo ""

$CARGO build -p moa-console --bin moa-gloworm-bench

for CONFIG in $CONFIGS; do
	NAME="${CONFIG%.*}"
	echo ""
	echo ">>> Building $NAME"
	echo ""
	make O=build/tests-moa/$NAME C=build/tests-moa/$NAME.config olddefconfig from=config/tests-moa/$CONFIG overwrite=y
	make O=build/tests-moa/$NAME C=build/tests-moa/$NAME.config strict=y

	echo ""
	echo ">>> Testing $NAME"
	echo ""

	ATA_IMG=""
	if echo "$NAME" | grep -q "minix"; then
		ATA_IMG="$PWD/tools/testing/images/minix-no-net"
	fi

	if echo "$NAME" | grep -q "ext2"; then
		ATA_IMG="$PWD/tools/testing/images/ext2-no-net"
	fi

	$CARGO run -p moa-console --bin moa-gloworm-bench -- \
		-c $PWD/build/tests-moa/$NAME.config \
		--ata-img "$ATA_IMG" \
		$PWD/build/tests-moa/$NAME/src/kernel/kernel.bin
done
