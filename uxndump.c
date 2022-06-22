// SPDX-License-Identifier: ISC
// copyright (C) 2022 SASANO Takayoshi <uaa@uaa.org.uk>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

static uint8_t *buf;
static size_t filesize;

static char *opecodeX[] = {
	NULL,  "INC", "POP", "NIP", "SWP", "ROT", "DUP", "OVR",
	"EQU", "NEQ", "GTH", "LTH", "JMP", "JCN", "JSR", "STH",
	"LDZ", "STZ", "LDR", "STR", "LDA", "STA", "DEI", "DEO",
	"ADD", "SUB", "MUL", "DIV", "AND", "ORA", "EOR", "SFT",
};

static char *opecode0[] = {
	"BRK", "20 ", "40 ", "60 ", "LIT", "LIT", "LIT", "LIT",
};

#define HEXDUMP_WIDTH		3
#define INSTRUCTION_STRING_LEN	3
#define MODIFIER_STRING_LEN	3
#define	OFFSET			0x100

static int put_space(char *out, int len)
{
	int i;

	/* simply put space (for padding) */
	for (i = 0; i < len; i++)
		*out++ = ' ';

	*out = '\0';

	return i;
}

/* hex values should use lower-case */
static int put_hex8(char *out, uint8_t value)
{
	return sprintf(out, "%02x ", value);
}

static int put_hex16(char *out, uint16_t value)
{
	return sprintf(out, "%04x ", value);
}

static int put_hexdump(char *out, uint8_t *bin, int len)
{
	int i, n, l;

	n = 0;
	for (i = 0; i < len; i++) {
		l = put_hex8(out, *bin++);
		n += l;
		out += l;
	}

	return n;
}

static int put_asciidump(char *out, uint8_t *bin, int len)
{
	int i;

	for (i = 0; i < len; i++) {
		/* avoid comment identifier, '(' and ')' */
		*out++ = (isascii(*bin) && !iscntrl(*bin) &&
			  (*bin != '(') && (*bin != ')')) ?
			*bin : '.';
		bin++;
	}

	*out = '\0';

	return i;
}

static int get_immediate_size(uint8_t opecode)
{
	if ((opecode & 0x9f) == 0x80) {
		/* LIT */
		return (opecode & 0x20) ? 2 : 1;
	} else {
		/* others */
		return 0;
	}
}

static int put_instruction(char *p, uint8_t opecode)
{
	int op;
	char *p0, *m;

	p0 = p;

	op = opecode & 0x1f;
	if (op) {
		m = opecodeX[op];
	} else {
		m = opecode0[opecode >> 5];

		if (opecode & 0x80) {
			/* LIT: k modifier is no need to display */
			opecode &= ~0x80;
		} else {
			/* BRK/reserved instructions: no modifier */
			opecode &= ~0xe0;
		}
	}

	strcpy(p, m);
	p += strlen(m);

	if (opecode & 0x20) *p++ = '2';
	if (opecode & 0x80) *p++ = 'k';
	if (opecode & 0x40) *p++ = 'r';
	*p = '\0';

	return p - p0;
}

static void put_disassemble(char *line, uint8_t *code, int immediate_size)
{
	char *p;

	/* instruction */
	p = line;
	line += put_instruction(line, *code);

	/* immediate (if needed) */
	if (immediate_size) {
		line += put_space(line,
				  INSTRUCTION_STRING_LEN +
				  MODIFIER_STRING_LEN + 1 - (line - p));
		switch (immediate_size) {
		case 1:
			line += put_hex8(line, *(code + 1));
			break;
		case 2:
			line += put_hex16(line,
					  (*(code + 1) << 8) | *(code + 2));
			break;
		default:
			break;
		}
	}
}

static void put_dump(char *line, uint8_t *code, int len)
{
	int i;

	for (i = 0; i < len; i++)
		line += put_hex8(line, code[i]);
}

static int build_line(char *line, uint16_t address, uint8_t *code, int remain)
{
	int len, dump;
	char *p;

	len = get_immediate_size(*code) + 1;
	dump = 0;

	/* if remains is not enough, use hex dump instead of disassemble */
	if (remain < len) {
		len = remain;
		dump = 1;
	}		

	/* address, hex dump and ascii dump is commented-out */
	line += sprintf(line, "( ");
	line += put_hex16(line, address + OFFSET);

	p = line;
	line += put_hexdump(line, code, len);
	line += put_space(line, (HEXDUMP_WIDTH * 3) - (line - p));

	p = line;
	line += put_asciidump(line, code, len);
	line += put_space(line, HEXDUMP_WIDTH - (line - p));
	line += sprintf(line, " )\t");

	if (dump)
		put_dump(line, code, len);
	else
		put_disassemble(line, code, len - 1);
		
	/* the result of put_xxxx() terminates with '\0' */

	return len;
}

static void disassemble_all(void)
{
	int i;
	char line[128];

	printf("|%04x\n", OFFSET);

	for (i = 0; i < filesize;) {
		i += build_line(line, i, &buf[i], filesize - i);
		printf("%s\n", line);
	}
}	

static int loadfile(char *filename)
{
	int v = -1;
	FILE *fp;

	fp = fopen(filename, "r");
	if (fp == NULL)
		goto fin0;

	fseek(fp, 0, SEEK_END);
	filesize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	buf = malloc(filesize);
	if (buf == NULL)
		goto fin1;

	fread(buf, filesize, 1, fp);
	v = 0;
	
fin1:
	fclose(fp);
fin0:
	return v;
}

int main(int argc, char *argv[])
{
	if (argc < 2) {
		printf("%s [filename]\n", argv[0]);
		goto fin0;
	}

	if (loadfile(argv[1])) {
		printf("file open error\n");
		goto fin0;
	}

	disassemble_all();
	free(buf);

fin0:
	return 0;
}
