#!/bin/bash

set -euo pipefail

CONFIGS=$(ls config/tests)

for CONFIG in $CONFIGS; do
	NAME="${CONFIG%.*}"
	echo ""
	echo ">>> Building $NAME"
	echo ""
	make O=build/tests/$NAME C=build/tests/$NAME.config olddefconfig from=config/tests/$CONFIG overwrite=y
	make O=build/tests/$NAME C=build/tests/$NAME.config strict=y
done

