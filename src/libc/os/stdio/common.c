
#include <stdio.h>
#include "file.h"

FILE _file_table[_STDIO_MAX_OPEN_FILES];

FILE __stdin = { 0, 0, 0, 0, 0, NULL };
FILE __stdout = { 1, 0, 0, 0, 0, NULL };
FILE __stderr = { 2, 0, 0, 0, 0, NULL };

