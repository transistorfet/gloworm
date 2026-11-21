#!/bin/bash
#
# Generate a diff of formatting changes suggested by clang-format
#
# The existing coding style can't quite be configured with clang-format (at least version 23)
# but this script will at least generate a diff so that unintentional style differences can be
# manually fixed
#

# configuration options
FMTCMD=clang-format-23
FMTFILE=tools/code/clang-format.yaml

#######

THISDIR=$(dirname "$0")
SRCROOT=$(realpath "$THISDIR/../..")
SED_REMOVE_MACROS='/^#define/{ :a /\\$/{ N; ba; }; d; }'

for FILE in $(find "$SRCROOT/src" "$SRCROOT/include" "$SRCROOT/tests" -name "*.[ch]" -print); do
	# remove all #defines from the diff since clang-format can't align them correctly
	echo "### $FILE"
	$FMTCMD "--style=file:$SRCROOT/$FMTFILE" "$FILE" | sed -e "$SED_REMOVE_MACROS" | diff -u <(cat "$FILE" | sed -e "$SED_REMOVE_MACROS") -
done

