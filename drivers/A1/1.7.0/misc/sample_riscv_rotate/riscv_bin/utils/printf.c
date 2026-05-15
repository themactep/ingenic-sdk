#include <log.h>
#include <stdarg.h>
#if 1

static void serial_putc(const char c)
{
	u32 uart_lsr = UART_BASE + OFF_LSR;
	u32 uart_tdr = UART_BASE + OFF_TDR;

	if (c == '\n') serial_putc('\r');

	/* Wait for fifo to shift out some bytes */
	while ( !((REG8(uart_lsr) & (UART_LSR_TDRQ | UART_LSR_TEMT)) == 0x60) );

	REG8(uart_tdr) = (u8)c;
}

static void serial_puts(const char *s)
{
	while (*s) {
		serial_putc(*s++);
	}
}

static int jz_serial_tstc(void)
{
	u32 uart_lsr = UART_BASE + OFF_LSR;

	if (REG8(uart_lsr) & UART_LSR_DR)
		return 1;

	return 0;
}

char jz_serial_getc(void)
{
	u32 uart_rdr = UART_BASE + OFF_RDR;

	while (!jz_serial_tstc())
		;

	return REG8(uart_rdr);
}

typedef s32 acpi_native_int;
//typedef char *va_list;

/*
 * Storage alignment properties
 */
#define  _AUPBND                (sizeof (acpi_native_int) - 1)
#define  _ADNBND                (sizeof (acpi_native_int) - 1)

/*
 * Variable argument list macro definitions
 */
#define _bnd(X, bnd)            (((sizeof (X)) + (bnd)) & (~(bnd)))
//#define va_arg(ap, T)           (*(T *)(((ap) += (_bnd (T, _AUPBND))) - (_bnd (T,_ADNBND))))
//#define va_end(ap)              (void) 0
//#define va_start(ap, A)         (void) ((ap) = (((char *) &(A)) + (_bnd (A,_AUPBND))))
#if 0
static void serial_put_hex(unsigned int  d)
{
	u8 c[12];
	u8 i;
	for(i = 0; i < 8;i++)
	{
		c[i] = (d >> ((7 - i) * 4)) & 0xf;
		if(c[i] < 10)
			c[i] += 0x30;
		else
			c[i] += (0x41 - 10);
	}
	c[8] = 0;
	serial_puts((const char *)c);

}

static void serial_put_dec(unsigned int  d)
{
        char c[16];
        int i;
        int j = 0;
        int x = d;

        while (x /= 10)
                j++;

        for (i = j; i >= 0; i--) {
                c[i] = d % 10;
		c[i] += 0x30;
                d /= 10;
        }
        c[j + 1] = 0;
	serial_puts(c);
}

int printf(const char *fmt, ...)
{
	const char *s;
	int d;
	va_list ap;

	va_start(ap, fmt);
	while (*fmt) {
		if (*fmt != '%') {
			serial_putc(*fmt++);
			continue;
		}
		switch (*++fmt) {
		case 's':
			s = va_arg(ap, const char *);
			serial_puts(s);
			break;
		case 'x':
		case 'X':
			{
				d = va_arg(ap, int);
				serial_put_hex(d);
				break;
			}
		case 'd':
			d = va_arg(ap, int);
			serial_put_dec(d);
			break;
		case '0':
		case '8':
			{
				break;
			}
			/* Add other specifiers here... */
		default:
			serial_puts("current only support 0x%x or %s\n");
			serial_putc(*fmt);
			break;
		}
			fmt++;
	}
	va_end(ap);

	return 1;
}
#endif

int strnlen (const char *str, int maxlen)
{
	const char *char_ptr, *end_ptr = str + maxlen;
	const unsigned long int *longword_ptr;
	unsigned long int longword, himagic, lomagic;//, magic_bits

	if (maxlen == 0)
		return 0;

	if (end_ptr < str)
		end_ptr = (const char *) ~0UL;

	/*
	 * Handle the first few characters by reading one character at a time.
	 * Do this until CHAR_PTR is aligned on a longword boundary.
	 */
	for (char_ptr = str; ((unsigned long int) char_ptr
			      & (sizeof (longword) - 1)) != 0;
	     ++char_ptr)
		if (*char_ptr == '\0') {
			if (char_ptr > end_ptr)
				char_ptr = end_ptr;
			return char_ptr - str;
		}

	/* All these elucidatory comments refer to 4-byte longwords,
	   but the theory applies equally well to 8-byte longwords.  */

	longword_ptr = (unsigned long int *) char_ptr;

	/* Bits 31, 24, 16, and 8 of this number are zero.  Call these bits
	   the "holes."  Note that there is a hole just to the left of
	   each byte, with an extra at the end:

	   bits:  01111110 11111110 11111110 11111111
	   bytes: AAAAAAAA BBBBBBBB CCCCCCCC DDDDDDDD

	   The 1-bits make sure that carries propagate to the next 0-bit.
	   The 0-bits provide holes for carries to fall into.  */
//  magic_bits = 0x7efefeffL;
	himagic = 0x80808080L;
	lomagic = 0x01010101L;
	if (sizeof (longword) > 4) {
		/* 64-bit version of the magic.  */
		/* Do the shift in two steps to avoid a warning if long has 32 bits.  */
//      magic_bits = ((0x7efefefeL << 16) << 16) | 0xfefefeffL;
		himagic = ((himagic << 16) << 16) | himagic;
		lomagic = ((lomagic << 16) << 16) | lomagic;
	}
	if (sizeof (longword) > 8)
		return -1;

	/* Instead of the traditional loop which tests each character,
	   we will test a longword at a time.  The tricky part is testing
	   if *any of the four* bytes in the longword in question are zero.  */
	while (longword_ptr < (unsigned long int *) end_ptr) {
		/* We tentatively exit the loop if adding MAGIC_BITS to
		   LONGWORD fails to change any of the hole bits of LONGWORD.

		   1) Is this safe?  Will it catch all the zero bytes?
		   Suppose there is a byte with all zeros.  Any carry bits
		   propagating from its left will fall into the hole at its
		   least significant bit and stop.  Since there will be no
		   carry from its most significant bit, the LSB of the
		   byte to the left will be unchanged, and the zero will be
		   detected.

		   2) Is this worthwhile?  Will it ignore everything except
		   zero bytes?  Suppose every byte of LONGWORD has a bit set
		   somewhere.  There will be a carry into bit 8.  If bit 8
		   is set, this will carry into bit 16.  If bit 8 is clear,
		   one of bits 9-15 must be set, so there will be a carry
		   into bit 16.  Similarly, there will be a carry into bit
		   24.  If one of bits 24-30 is set, there will be a carry
		   into bit 31, so all of the hole bits will be changed.

		   The one misfire occurs when bits 24-30 are clear and bit
		   31 is set; in this case, the hole at bit 31 is not
		   changed.  If we had access to the processor carry flag,
		   we could close this loophole by putting the fourth hole
		   at bit 32!

		   So it ignores everything except 128's, when they're aligned
		   properly.  */

		longword = *longword_ptr++;

		if ((longword - lomagic) & himagic) {
			/* Which of the bytes was the zero?  If none of them were, it was
			   a misfire; continue the search.  */

			const char *cp = (const char *) (longword_ptr - 1);

			char_ptr = cp;
			if (cp[0] == 0)
				break;
			char_ptr = cp + 1;
			if (cp[1] == 0)
				break;
			char_ptr = cp + 2;
			if (cp[2] == 0)
				break;
			char_ptr = cp + 3;
			if (cp[3] == 0)
				break;
			if (sizeof (longword) > 4) {
				char_ptr = cp + 4;
				if (cp[4] == 0)
					break;
				char_ptr = cp + 5;
				if (cp[5] == 0)
					break;
				char_ptr = cp + 6;
				if (cp[6] == 0)
					break;
				char_ptr = cp + 7;
				if (cp[7] == 0)
					break;
			}
		}
		char_ptr = end_ptr;
	}

	if (char_ptr > end_ptr)
		char_ptr = end_ptr;
	return char_ptr - str;
}

#define isdigit(c) ((c) >= '0' && (c) <= '9')
static int skip_atoi(const char **s)
{
	int i = 0;

	while (isdigit(**s))
		i = i * 10 + *((*s)++) - '0';
	return i;
}

#define ZEROPAD    1		/* pad with zero */
#define SIGN       2		/* unsigned/signed long */
#define PLUS       4		/* show plus */
#define SPACE      8		/* space if plus */
#define LEFT       16		/* left justified */
#define SMALL      32		/* Must be 32 == 0x20 */
#define SPECIAL    64		/* 0x */

#define __do_div(n, base) ({						\
			int __res;					\
			__res = ((unsigned long) n) % (unsigned) base;	\
			n = ((unsigned long) n) / (unsigned) base;	\
			__res; })

static char *number(char *str, long num, int base, int size, int precision,
		    int type)
{
	/* we are called with base 8, 10 or 16, only, thus don't need "G..." */
	static const char digits[16] = "0123456789ABCDEF"; /* "GHIJKLMNOPQRSTUVWXYZ"; */

	char tmp[66];
	char c, sign, locase;
	int i;

	/* locase = 0 or 0x20. ORing digits or letters with 'locase'
	 * produces same digits or (maybe lowercased) letters */
	locase = (type & SMALL);
	if (type & LEFT)
		type &= ~ZEROPAD;
	if (base < 2 || base > 36)
		return NULL;
	c = (type & ZEROPAD) ? '0' : ' ';
	sign = 0;
	if (type & SIGN) {
		if (num < 0) {
			sign = '-';
			num = -num;
			size--;
		} else if (type & PLUS) {
			sign = '+';
			size--;
		} else if (type & SPACE) {
			sign = ' ';
			size--;
		}
	}
	if (type & SPECIAL) {
		if (base == 16)
			size -= 2;
		else if (base == 8)
			size--;
	}
	i = 0;
	if (num == 0)
		tmp[i++] = '0';
	else
		while (num != 0)
			tmp[i++] = (digits[__do_div(num, base)] | locase);
	if (i > precision)
		precision = i;
	size -= precision;
	if (!(type & (ZEROPAD + LEFT)))
		while (size-- > 0)
			*str++ = ' ';
	if (sign)
		*str++ = sign;
	if (type & SPECIAL) {
		if (base == 8)
			*str++ = '0';
		else if (base == 16) {
			*str++ = '0';
			*str++ = ('X' | locase);
		}
	}
	if (!(type & LEFT))
		while (size-- > 0)
			*str++ = c;
	while (i < precision--)
		*str++ = '0';
	while (i-- > 0)
		*str++ = tmp[i];
	while (size-- > 0)
		*str++ = ' ';
	return str;
}

int vsprintf(char *buf, const char *fmt, va_list args)
{
	int len;
	unsigned long num;
	int i, base;
	char *str;
	const char *s;

	int flags;		/* flags to number() */

	int field_width;	/* width of output field */
	int precision;		/* min. # of digits for integers; max
				   number of chars for from string */
	int qualifier;        /* 'h', 'l', or 'L' for integer fields */

	for (str = buf; *fmt; ++fmt) {
		if (*fmt != '%') {
			*str++ = *fmt;
			continue;
		}

		/* process flags */
		flags = 0;
	repeat:
		++fmt;        /* this also skips first '%' */
		switch (*fmt) {
		case '-':
			flags |= LEFT;
			goto repeat;
		case '+':
			flags |= PLUS;
			goto repeat;
		case ' ':
			flags |= SPACE;
			goto repeat;
		case '#':
			flags |= SPECIAL;
			goto repeat;
		case '0':
			flags |= ZEROPAD;
			goto repeat;
		}

		/* get field width */
		field_width = -1;
		if (isdigit(*fmt))
			field_width = skip_atoi(&fmt);
		else if (*fmt == '*') {
			++fmt;
			/* it's the next argument */
			field_width = va_arg(args, int);
			if (field_width < 0) {
				field_width = -field_width;
				flags |= LEFT;
			}
		}

		/* get the precision */
		precision = -1;
		if (*fmt == '.') {
			++fmt;
			if (isdigit(*fmt))
				precision = skip_atoi(&fmt);
			else if (*fmt == '*') {
				++fmt;
				/* it's the next argument */
				precision = va_arg(args, int);
			}
			if (precision < 0)
				precision = 0;
		}

		/* get the conversion qualifier */
		qualifier = -1;
		if (*fmt == 'h' || *fmt == 'l' || *fmt == 'L') {
			qualifier = *fmt;
			++fmt;
		}

		/* default base */
		base = 10;

		switch (*fmt) {
		case 'c':
			if (!(flags & LEFT))
				while (--field_width > 0)
					*str++ = ' ';
			*str++ = (unsigned char)va_arg(args, int);
			while (--field_width > 0)
				*str++ = ' ';
			continue;
		case 's':
			s = va_arg(args, char *);
			len = strnlen(s, precision);

			if (!(flags & LEFT))
				while (len < field_width--)
					*str++ = ' ';
			for (i = 0; i < len; ++i)
				*str++ = *s++;
			while (len < field_width--)
				*str++ = ' ';
			continue;

		case 'p':
			if (field_width == -1) {
				field_width = 2 * sizeof(void *);
				flags |= ZEROPAD;
			}
			str = number(str,
				     (unsigned long)va_arg(args, void *), 16,
				     field_width, precision, flags);
			continue;
		case 'n':
			if (qualifier == 'l') {
				long *ip = va_arg(args, long *);
				*ip = (str - buf);
			} else {
				int *ip = va_arg(args, int *);
				*ip = (str - buf);
			}
			continue;
		case '%':
			*str++ = '%';
			continue;

			/* integer number formats - set up the flags and "break" */
		case 'o':
			base = 8;
			break;
		case 'x':
			flags |= SMALL;
		case 'X':
			base = 16;
			break;
		case 'd':
		case 'i':
			flags |= SIGN;
		case 'u':
			break;

		default:
			*str++ = '%';
			if (*fmt)
				*str++ = *fmt;
			else
				--fmt;
			continue;
		}
		if (qualifier == 'l')
			num = va_arg(args, unsigned long);
		else if (qualifier == 'h') {
			num = (unsigned short)va_arg(args, int);
			if (flags & SIGN)
				num = (short)num;
		} else if (flags & SIGN)
			num = va_arg(args, int);
		else
			num = va_arg(args, unsigned int);
		str = number(str, num, base, field_width, precision, flags);
	}
	*str = '\0';

	return str - buf;
}

int sprintf(char *buf, const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	i = vsprintf(buf, fmt, args);
	va_end(args);

	return i;
}

int printf(const char *fmt, ...)
{
#if 1
	char printf_buf[1024];
	va_list args;
	int printed;

	va_start(args, fmt);
	printed = vsprintf(printf_buf, fmt, args);
	va_end(args);

	serial_puts(printf_buf);

	return printed;
	#else
	return 0;
	#endif

}

#define BUF_MAX 256
char data_buf[BUF_MAX];
char *scanf(void)
{
	int i = 0;

	for(i = 0; i < BUF_MAX; i++) {
		data_buf[i] = jz_serial_getc();
		if(data_buf[i] == '\r' || i == BUF_MAX - 1) {
			data_buf[i] = ' ';
			break;
		}
		printf("%c", data_buf[i]);
	}
	printf("\n");

	if(i == BUF_MAX) {
		printf("warn!!!!!!input data > 256\n");
	}

	return data_buf;
}
#endif
