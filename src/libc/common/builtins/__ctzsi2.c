
extern int __ffsdi2(unsigned long a);

int __ctzsi2(unsigned int a)
{
	return __ffsdi2(a);
}

