#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define max(a, b) ((a) > (b) ? (a) : (b))

char eclevel;
char version;
uint16_t datalen;
char *data;
char res[185 * 185];
int width;
int specs[5];
int mask = 0;

void rect(int x, int y, int w, int h, char c) {
	for (int i = x; i < x + w; i++)
		for (int j = y; j < y + h; j++)
			res[i * width + j] = c;
}

void _encode_numeric() {
	// allocate memory
	int target = specs[1] * specs[2] + specs[3] * specs[4];
	char *encoded = calloc(target, 1);
	// add counters to leave spaces for ecc
	// c1 = how many g1 blocks left
	int c1 = specs[1];
	// c2 = how many bytes left in current block
	int c2 = specs[2];
	// add 0b0001 for numeric and the length
	// curr is current byte
	int curr;
	// bit is the next available bit
	int bit;
	if (version <= 9) {
		// 10 bits
		encoded[0] = 0b00010000 | datalen >> 6;
		encoded[1] = datalen << 2;
		curr = 1;
		bit = 6;
	} else if (version <= 26) {
		// 12 bits
		encoded[0] = 0b00010000 | datalen >> 8;
		encoded[1] = datalen;
		curr = 2;
		bit = 0;
	} else {
		// 14 bits
		encoded[0] = 0b00010000 | datalen >> 10;
		encoded[1] = datalen >> 2;
		encoded[2] = datalen << 6;
		curr = 2;
		bit = 2;
	}
	// add data
	for (int i = 0; i < datalen; i += 3) {
		// group up to 3 digits
		uint16_t num = data[i] - '0';
		if (i + 1 < datalen)
			num = num * 10 + data[i + 1] - '0';
		if (i + 2 < datalen)
			num = num * 10 + data[i + 2] - '0';
		if (num >= 100) {
			// 10 bits
			if (bit == 7) {
				// across 3 bytes
				encoded[curr++] |= num >> 9;
				encoded[curr++] = num >> 1;
				encoded[curr] = num << 7;
				bit = 1;
			} else {
				// across 2 bytes
				encoded[curr++] |= num >> (bit + 2);
				encoded[curr] = num << (6 - bit);
				bit += 2;
				if (bit >= 8) {
					bit -= 8;
					curr++;
				}
			}
		} else if (num >= 10) {
			// 7 bits
			if (bit <= 1) {
				// across 1 byte
				encoded[curr] |= (num << (1 - bit));
				bit += 7;
				if (bit >= 8) {
					bit -= 8;
					curr++;
				}
			} else {
				// across 2 bytes
				encoded[curr++] |= num >> (bit - 1);
				encoded[curr] = num << (9 - bit);
				bit--;
			}
		} else {
			// 4 bits
			if (bit <= 4) {
				// across 1 byte
				encoded[curr] |= (num << (4 - bit));
				bit += 4;
				if (bit >= 8) {
					bit -= 8;
					curr++;
				}
			} else {
				// across 2 bytes
				encoded[curr++] |= num >> (bit - 4);
				encoded[curr] = num << (12 - bit);
				bit = (bit + 4) & 7;
			}
		}
	}
	// add terminator
	if (curr == target - 1 && bit > 4) {
		// dont need 4 bits
		encoded[curr] &= 0xff00 >> bit;
	} else if (bit > 4) {
		// clear until end of byte and one more
		encoded[curr++] &= 0xff00 >> bit;
		encoded[curr++] = 0;
	} else {
		// clear until end of byte
		encoded[curr++] &= 0xf00 >> bit;
	}
	// add padding
	char pad = 0b11101100;
	while (curr < target) {
		encoded[curr++] = pad;
		pad ^= 0b11111101;
	}
	free(data);
	data = encoded;
	datalen = target;
}

uint8_t __conv_alphaval(char c) {
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'A' && c <= 'Z')
		return c - 'A' + 10;
	if (c == ' ')
		return 36;
	if (c == '$')
		return 37;
	if (c == '%')
		return 38;
	if (c == '*')
		return 39;
	if (c == '+')
		return 40;
	if (c == '-')
		return 41;
	if (c == '.')
		return 42;
	if (c == '/')
		return 43;
	if (c == ':')
		return 44;
	return 0xff;
}

void _encode_alpha() {
	// allocate memory
	int target = specs[1] * specs[2] + specs[3] * specs[4];
	char *encoded = calloc(target, 1);
	// add 0010 for alphanumeric and the length
	int curr;
	int bit;
	if (version <= 9) {
		// 9 bits
		encoded[0] = 0b00100000 | datalen >> 5;
		encoded[1] = datalen << 3;
		curr = 1;
		bit = 5;
	} else if (version <= 26) {
		// 11 bits
		encoded[0] = 0b00100000 | datalen >> 7;
		encoded[1] = datalen << 1;
		curr = 1;
		bit = 7;
	} else {
		// 13 bits
		encoded[0] = 0b00100000 | datalen >> 9;
		encoded[1] = datalen >> 1;
		encoded[2] = datalen << 7;
		curr = 2;
		bit = 1;
	}
	// add data
	for (int i = 0; i < datalen; i += 2) {
		int num = __conv_alphaval(data[i]);
		if (i + 1 < datalen)
			num = num * 45 + __conv_alphaval(data[i + 1]);
		if (num >= 45) {
			// 11 bits
			if (bit >= 6) {
				// across 3 bytes
				encoded[curr++] |= num >> (15 - bit);
				encoded[curr++] |= num >> (bit - 5);
				encoded[curr] |= num << (bit + 1);
				bit -= 5;
			} else {
				// across 2 bytes
				encoded[curr++] |= num >> (bit + 3);
				encoded[curr] |= num << (5 - bit);
				bit += 3;
				if (bit >= 8) {
					bit -= 8;
					curr++;
				}
			}
		} else {
			// 6 bits
			if (bit > 2) {
				// across 2 bytes
				encoded[curr++] |= num >> (bit - 2);
				encoded[curr] |= num << (10 - bit);
				bit -= 2;
			} else {
				// across 1 byte
				encoded[curr] |= num << (2 - bit);
				bit += 6;
				if (bit >= 8) {
					bit -= 8;
					curr++;
				}
			}
		}
	}
	// add terminator
	if (curr == target - 1 && bit > 4) {
		// dont need 4 bits
		encoded[curr] &= 0xff00 >> bit;
	} else if (bit > 4) {
		// clear until end of byte and one more
		encoded[curr++] &= 0xff00 >> bit;
		encoded[curr++] = 0;
	} else {
		// clear until end of byte
		encoded[curr++] &= 0xf00 >> bit;
	}
	// add padding
	char pad = 0b11101100;
	while (curr < target) {
		encoded[curr++] = pad;
		pad ^= 0b11111101;
	}
	free(data);
	data = encoded;
	datalen = target;
}

void _encode_byte() {
	// allocate memory
	int target = specs[1] * specs[2] + specs[3] * specs[4];
	char *encoded = malloc(target);
	// add 0b0100 for ascii and the length
	int curr;
	if (version <= 9) {
		// 8 bits
		encoded[0] = 0b01000000 | datalen >> 4;
		encoded[1] = datalen << 4;
		curr = 1;
	} else {
		// 16 bits
		encoded[0] = 0b01000000 | datalen >> 12;
		encoded[1] = datalen >> 4;
		encoded[2] = datalen << 4;
		curr = 2;
	}
	// add data
	for (int i = 0; i < datalen; i++) {
		encoded[curr] |= data[i] >> 4;
		encoded[++curr] = data[i] << 4;
	}
	// clear low 4 bits to indicate end
	encoded[curr] &= 0b11110000;
	// add padding
	char pad = 0b11101100;
	while (curr < target - 1) {
		encoded[++curr] = pad;
		pad ^= 0b11111101;
	}
	free(data);
	data = encoded;
	datalen = target;
}

void encode_data() {
	// open file
	FILE *f = fopen("ecc", "r");
	// get to the right line (version * 4 + eclevel)
	int i = version * 4 + (eclevel ^ 1) - 4;
	while (i--) {
		while (getc(f) != '\n')
			;
	}
	// read data
	fscanf(f, "%d %d %d %d %d", specs, specs + 1, specs + 2, specs + 3, specs + 4);
	fclose(f);
	// figure out what type of data the input is
	char alpha = 0;
	char byte = 0;
	for (int i = 0; i < datalen; i++) {
		if (data[i] >= '0' && data[i] <= '9')
			continue;
		if (__conv_alphaval(data[i]) != 0xff)
			alpha = 1;
		else if (data[i] >= 'a' && data[i] <= 'z') {
			byte = 1;
			break;
		}
	}
	// encode data
	if (byte)
		_encode_byte();
	else if (alpha)
		_encode_alpha();
	else
		_encode_numeric();
}

uint8_t _mul2p8(uint8_t a, uint8_t b) {
	uint8_t ans = 0;
	uint8_t m = 0b10000000;
	// standard gf(2^8) multiplication
	while (m) {
		if (ans & 0b10000000)
			ans = (ans << 1) ^ 0b00011101;
		else
			ans <<= 1;
		if (b & m)
			ans ^= a;
		m >>= 1;
	}
	return ans;
}

// out MAY be the same as a
void _mulpoly_scal(uint8_t *a, int n, uint8_t b, uint8_t *out) {
	// multiply each element by b
	for (int i = 0; i < n; i++)
		out[i] = _mul2p8(a[i], b);
}

// out MUST NOT be the same as a or b
int _mulpoly_poly(uint8_t *a, int n, uint8_t *b, int m, uint8_t *out) {
	// clear out
	memset(out, 0, n + m - 1);
	// standard polynomial multiplication
	for (int i = 0; i < n; i++) {
		for (int j = 0; j < m; j++) {
			out[i + j] ^= _mul2p8(a[i], b[j]);
		}
	}
	return n + m;
}

void _gen_poly(int n, uint8_t *out) {
	// start with x + a^0
	memset(out, 0, n + 1);
	out[0] = 1;
	out[1] = 1;
	uint8_t tmp[2] = {1, 2};
	uint8_t tmp2[n + 1];
	// multiply by x + a^1, x + a^2, ...
	for (int i = 1; i < n; i++) {
		_mulpoly_poly(out, i + 1, tmp, 2, tmp2);
		memcpy(out, tmp2, i + 2);
		if (tmp[1] & 0b10000000)
			tmp[1] = (tmp[1] << 1) ^ 0b00011101;
		else
			tmp[1] <<= 1;
	}
}

void reed_solomon(char *data, int len, int ecclen, char *out) {
	// make a copy of data
	char *tmp = malloc(len + ecclen);
	memcpy(tmp, data, len);
	memset(tmp + len, 0, ecclen);
	// generate generator polynomial
	uint8_t gen[ecclen + 1];
	_gen_poly(ecclen, gen);
	// calculate ecc
	for (int i = 0; i < len; i++) {
		int f = tmp[i];
		if (f == 0)
			continue;
		if (f != 1) {
			for (int j = 0; j < ecclen + 1; j++) {
				tmp[i + j] ^= _mul2p8(gen[j], f);
			}
		}
	}
	// copy ecc to out
	memcpy(out, tmp + len, ecclen);
}

void ecc() {
	// calculate ecc for each block
	int curr = 0;
	data = realloc(data, datalen + specs[0] * (specs[1] + specs[3]));
	// group 1
	for (int i = 0; i < specs[1]; i++) {
		reed_solomon(data + curr, specs[2], specs[0], data + datalen + i * specs[0]);
		curr += specs[2];
	}
	// group 2
	for (int i = 0; i < specs[3]; i++) {
		reed_solomon(data + curr, specs[4], specs[0], data + datalen + (specs[1] + i) * specs[0]);
		curr += specs[4];
	}
}

void interleave() {
	// allocate memory
	char *interleaved = malloc(datalen + specs[0] * (specs[1] + specs[3]));
	int curr = 0;
	for (int c = 0; c < max(specs[2], specs[4]); c++) {
		// group 1
		if (c < specs[2])
			for (int i = 0; i < specs[1]; i++)
				interleaved[curr++] = data[i * specs[2] + c];
		// group 2
		if (c < specs[4])
			for (int i = 0; i < specs[3]; i++)
				interleaved[curr++] = data[specs[1] * specs[2] + i * specs[4] + c];
	}
	// copy ecc
	for (int c = 0; c < specs[0]; c++) {
		// all groups are the same length
		for (int i = 0; i < specs[1] + specs[3]; i++)
			interleaved[curr++] = data[datalen + i * specs[0] + c];
	}
	free(data);
	data = interleaved;
	datalen = curr;
}

void quiet() {
	// quiet zones
	for (int i = 0; i < width; i++)
		for (int j = 0; j < width; j++)
			if (i < 4 || i >= width - 4 || j < 4 || j >= width - 4)
				res[i * width + j] = 0;
}

void finder() {
	// top left
	rect(4, 4, 8, 8, 0);
	rect(4, 4, 7, 7, 1);
	rect(5, 5, 5, 5, 0);
	rect(6, 6, 3, 3, 1);
	// top right
	rect(width - 12, 4, 8, 8, 0);
	rect(width - 11, 4, 7, 7, 1);
	rect(width - 10, 5, 5, 5, 0);
	rect(width - 9, 6, 3, 3, 1);
	// bottom left
	rect(4, width - 12, 8, 8, 0);
	rect(4, width - 11, 7, 7, 1);
	rect(5, width - 10, 5, 5, 0);
	rect(6, width - 9, 3, 3, 1);
}

void alignment_pattern() {
	int i = version - 1;
	FILE *f = fopen("alignment", "r");
	// get to the version-th line
	while (i--) {
		while (getc(f) != '\n')
			;
	}
	// make array of values
	int x[8];
	int n = 0;
	// read values until newline
	while (1) {
		fscanf(f, "%d", x + n++);
		if (getc(f) == '\n')
			break;
	}
	fclose(f);
	// draw
	for (int i = 0; i < n; i++) {
		for (int j = 0; j < n; j++) {
			// only if not overlapping
			if (res[x[i] * width + x[j]] != 2)
				continue;
			rect(x[i] - 2, x[j] - 2, 5, 5, 1);
			rect(x[i] - 1, x[j] - 1, 3, 3, 0);
			res[x[i] * width + x[j]] = 1;
		}
	}
}

void timing() {
	for (int i = 12; i < width - 12; i++) {
		if (res[i * width + 10] == 2)
			res[i * width + 10] = !(i % 2);
		if (res[10 * width + i] == 2)
			res[10 * width + i] = !(i % 2);
	}
}

void format_string() {
	// generate
	uint16_t format = eclevel << 3 | mask;
	// prepare
	const uint16_t gen = 0b10100110111;
	uint16_t ecc = format << 10;
	// divide
	while (ecc >> 10) {
		ecc ^= gen << (31 - __builtin_clz(ecc >> 10));
	}
	// concatenate
	format = (format << 10) | ecc;
	// mask
	format ^= 0b101010000010010;
	for (int i = 0; i < 7; i++) {
		// bottom
		res[13 * width - 5 - i] = (format >> (14 - i)) & 1;
		// left
		res[(i + 4 + i / 6) * width + 12] = (format >> (14 - i)) & 1;
	}
	for (int i = 0; i < 8; i++) {
		// right
		res[(width - 12 + i) * width + 12] = (format >> (7 - i)) & 1;
		// top
		res[12 * width + (12 - i - (i > 1))] = (format >> (7 - i)) & 1;
	}
}

void version_string() {
	// prepare
	uint32_t ec = version << 12;
	const uint32_t gen = 0b1111100100101;
	// divide
	while (ec >> 12) {
		ec ^= gen << (31 - __builtin_clz(ec >> 12));
	}
	// concatenate
	ec = (version << 12) | ec;
	for (int i = 0; i < 18; i++) {
		// bottom
		res[(5 + i / 3) * width - 15 + i % 3] = ec & 1;
		// top
		res[(width - 15 + i % 3) * width + 4 + i / 3] = ec & 1;
		ec >>= 1;
	}
}

uint8_t _mask(int x, int y) {
	// return 0;
	x -= 4;
	y -= 4;
	switch (mask) {
	case 0:
		return (x + y) % 2 == 0;
	case 1:
		return y % 2 == 0;
	case 2:
		return x % 3 == 0;
	case 3:
		return (x + y) % 3 == 0;
	case 4:
		return (x / 3 + y / 2) % 2 == 0;
	case 5:
		return (x * y) % 2 + (x * y) % 3 == 0;
	case 6:
		return ((x * y) % 2 + (x * y) % 3) % 2 == 0;
	case 7:
		return ((x * y) % 3 + (x + y) % 2) % 2 == 0;
	}
}

void place() {
	// place data
	int x = width - 5;
	int y = width - 5;
	int dir = -1;
	int byte = 0;
	int bit = 7;
	// shifted to the left
	char shift = 0;
	while (byte < datalen) {
		// skip occupied modules
		if (res[(x - shift) * width + y] == 2) {
			res[(x - shift) * width + y] = _mask(x - shift, y) ^ ((data[byte] >> bit) & 1);
			bit--;
			if (bit < 0) {
				bit = 7;
				byte++;
			}
		}
		// only change y if shifted
		y += shift * dir;
		// flip shift
		shift = !shift;
		// when reached end of column
		if (y < 4 || y >= width - 4) {
			y -= dir;
			dir = -dir;
			// go left
			x -= 2;
			// skip timing
			x -= (x == 10);
		}
	}
	// add remaining bits
	if (y != width - 12) {
		if (res[4 * width + y] == 2)
			res[4 * width + y] = 0;
		for (int i = y + 1; i < width - 12; i++) {
			res[4 * width + i] = 0;
			res[5 * width + i] = 0;
		}
	}
}

int _penalty1() {
	// 5 or more consecutive modules of the same color
	int penalty = 0;
	for (int y = 4; y < width - 4; y++) {
		int run = 1;
		for (int x = 5; x < width - 4; x++) {
			if (res[x * width + y] == res[(x - 1) * width + y]) {
				run++;
				if (run == 5)
					penalty += 3;
				else if (run > 5)
					penalty++;
			} else {
				run = 1;
			}
		}
	}
	for (int x = 4; x < width - 4; x++) {
		int run = 1;
		for (int y = 5; y < width - 4; y++) {
			if (res[x * width + y] == res[x * width + y - 1]) {
				run++;
				if (run == 5)
					penalty += 3;
				else if (run > 5)
					penalty++;
			} else {
				run = 1;
			}
		}
	}
	return penalty;
}

int _penalty2() {
	// 2x2 blocks of the same color
	int penalty = 0;
	for (int x = 4; x < width - 5; x++) {
		for (int y = 4; y < width - 5; y++) {
			if (res[x * width + y] == res[(x + 1) * width + y] && res[x * width + y] == res[x * width + y + 1] && res[x * width + y] == res[(x + 1) * width + y + 1])
				penalty += 3;
		}
	}
	return penalty;
}

int _penalty3() {
	// 2 kernels
	char k1[] = {0, 0, 0, 0, 1, 0, 1, 1, 1, 0, 1};
	char k2[] = {1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0};
	int penalty = 0;
	for (int x = 0; x < width - 10; x++) {
		for (int y = 0; y < width - 10; y++) {
			int s1 = 0;
			int s2 = 0;
			int s3 = 0;
			int s4 = 0;
			for (int i = 0; i < 11; i++) {
				s1 += (res[(x + i) * width + y] == k1[i]);
				s2 += (res[x * width + (y + i)] == k1[i]);
				s3 += (res[(x + i) * width + y] == k2[i]);
				s4 += (res[x * width + (y + i)] == k2[i]);
			}
			if (s1 == 11)
				penalty += 40;
			if (s2 == 11)
				penalty += 40;
			if (s3 == 11)
				penalty += 40;
			if (s4 == 11)
				penalty += 40;
		}
	}
	return penalty - 40 * 12;
}

int _penalty4() {
	int black = 0;
	for (int i = 4; i < width - 4; i++)
		for (int j = 4; j < width - 4; j++)
			black += res[i * width + j];
	int total = (width - 8) * (width - 8);
	return abs(black * 100 / total - 50) / 5 * 10;
}

int score_mask() { return _penalty1() + _penalty2() + _penalty3() + _penalty4(); }

void test_masks() {
	int best = 0;
	int best_score = 1000000;
	for (int i = 0; i < 8; i++) {
		mask = i;
		memset(res, 2, width * width);
		quiet();
		finder();
		if (version > 1)
			alignment_pattern();
		timing();
		res[13 * width - 12] = 1;
		format_string();
		if (version >= 7) {
			version_string();
		}
		place();
		int score = score_mask();
		if (score < best_score) {
			best = i;
			best_score = score;
		}
	}
	mask = best;
	memset(res, 2, width * width);
	quiet();
	finder();
	if (version > 1)
		alignment_pattern();
	timing();
	res[13 * width - 12] = 1;
	format_string();
	if (version >= 7) {
		version_string();
	}
	place();
}

int main() {
	// read input
	version = getchar();
	eclevel = getchar();
	datalen = (getchar() << 8) + getchar();
	data = malloc(datalen);
	fread(data, 1, datalen, stdin);
	// encode data
	encode_data();
	for (int i = 0; i < datalen; i++)
		fprintf(stderr, "%02x", (unsigned char)data[i]);
	putc(10, stderr);
	// add error correction
	ecc();
	// interleave
	interleave();
	// initialize array
	width = 25 + 4 * version;
	memset(res, 2, width * width);
	// generate
	quiet();
	finder();
	if (version > 1)
		alignment_pattern();
	timing();
	// dark module
	res[13 * width - 12] = 1;
	format_string();
	// version string
	if (version >= 7) {
		version_string();
	}
	// try all masks
	test_masks();
	// output
	for (int i = 0; i < width; i++) {
		for (int j = 0; j < width; j++) {
			putchar(res[i * width + j] + '0');
		}
	}
	return 0;
}
