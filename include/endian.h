
#ifndef _ENDIAN_H
#define _ENDIAN_H

#include <bits/byteswap.h>

#if BYTE_ORDER == BIG_ENDIAN

#define be16toh(x)	((x))
#define be32toh(x)	((x))
#define be64toh(x)	((x))

#define htobe16(x)	((x))
#define htobe32(x)	((x))
#define htobe64(x)	((x))

#define htole16(x)	__bswap_16((x))
#define htole32(x)	__bswap_32((x))
#define htole64(x)	__bswap_64((x))

#define le16toh(x)	__bswap_16((x))
#define le32toh(x)	__bswap_32((x))
#define le64toh(x)	__bswap_64((x))

#elif BYTE_ORDER == LITTLE_ENDIAN

#define be16toh(x)	__bswap_16((x))
#define be32toh(x)	__bswap_32((x))
#define be64toh(x)	__bswap_64((x))

#define htobe16(x)	__bswap_16((x))
#define htobe32(x)	__bswap_32((x))
#define htobe64(x)	__bswap_64((x))

#define htole16(x)	((x))
#define htole32(x)	((x))
#define htole64(x)	((x))

#define le16toh(x)	((x))
#define le32toh(x)	((x))
#define le64toh(x)	((x))

#else

#error "BYTE_ORDER is not set correctly"

#endif

#endif

