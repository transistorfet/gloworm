
int __ffsdi2(unsigned long a)
{
	int index = 0;

	if ((a & 0xffff) == 0) {
		index += 16;
		a >>= 16;
	}
	if ((a & 0xff) == 0) {
		index += 8;
		a >>= 8;
	}
	if ((a & 0xf) == 0) {
		index += 4;
		a >>= 4;
	}
	if ((a & 0x3) == 0) {
		index += 2;
		a >>= 2;
	}
	if ((a & 0x1) == 0) {
		index += 1;
	}
	return index;
}

