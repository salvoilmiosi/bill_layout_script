#include "binary_io.h"

#include <cstring>

void binary_ofstream::writeData(const void *data, int length) {
	write(reinterpret_cast<const char *>(data), length);
}

void binary_ofstream::writeByte(uint8_t num) {
    writeData(&num, 1);
}

void binary_ofstream::writeShort(uint16_t num) {
	uint8_t str[2];
	str[0] = (num & 0xff00) >> (8 * 1);
	str[1] = (num & 0x00ff) >> (8 * 0);

	writeData(str, 2);
}

void binary_ofstream::writeInt(uint32_t num) {
	uint8_t str[4];
	str[0] = (num & 0xff000000) >> (8 * 3);
	str[1] = (num & 0x00ff0000) >> (8 * 2);
	str[2] = (num & 0x0000ff00) >> (8 * 1);
	str[3] = (num & 0x000000ff) >> (8 * 0);

	writeData(str, 4);
}

void binary_ofstream::writeLong(uint64_t num) {
	uint8_t str[8];
	str[0] = (num & 0xff00000000000000ll) >> (8 * 7);
	str[1] = (num & 0x00ff000000000000ll) >> (8 * 6);
	str[2] = (num & 0x0000ff0000000000ll) >> (8 * 5);
	str[3] = (num & 0x000000ff00000000ll) >> (8 * 4);
	str[0] = (num & 0x00000000ff000000ll) >> (8 * 3);
	str[1] = (num & 0x0000000000ff0000ll) >> (8 * 2);
	str[2] = (num & 0x000000000000ff00ll) >> (8 * 1);
	str[3] = (num & 0x00000000000000ffll) >> (8 * 0);

	writeData(str, 8);
}

void binary_ofstream::writeString(const std::string &str) {
	int len = (int)str.size();
	if (len <= 0) {
		writeInt(0xffffffff);
	} else {
		writeInt(len);
		writeData(str.c_str(), len);
	}
}

void binary_ifstream::readData(void *out, int length) {
	read(reinterpret_cast<char *>(out), length);
}

uint8_t binary_ifstream::readByte() {
	return get();
}

uint16_t binary_ifstream::readShort() {
	uint8_t data[2];
	readData(data, 2);
    return
        (data[0] << 8 & 0xff00) |
        (data[1]      & 0x00ff);
}

uint32_t binary_ifstream::readInt() {
	uint8_t data[4];
	readData(data, 4);
    return
        (data[0] << (8 * 3) & 0xff000000) |
        (data[1] << (8 * 2) & 0x00ff0000) |
        (data[2] << (8 * 1) & 0x0000ff00) |
        (data[3] << (8 * 0) & 0x000000ff);
}

uint64_t binary_ifstream::readLong() {
    char data[8];
	readData(data, 8);
        
    return
        ((uint64_t) data[0] << (8 * 7) & 0xff00000000000000ll) |
        ((uint64_t) data[1] << (8 * 6) & 0x00ff000000000000ll) |
        ((uint64_t) data[2] << (8 * 5) & 0x0000ff0000000000ll) |
        ((uint64_t) data[3] << (8 * 4) & 0x000000ff00000000ll) |
        ((uint64_t) data[4] << (8 * 3) & 0x00000000ff000000ll) |
        ((uint64_t) data[5] << (8 * 2) & 0x0000000000ff0000ll) |
        ((uint64_t) data[6] << (8 * 1) & 0x000000000000ff00ll) |
        ((uint64_t) data[7] << (8 * 0) & 0x00000000000000ffll);
}

std::string binary_ifstream::readString() {
    uint32_t length = readInt();
	std::string out;
    if (length != 0xffffffff) {
		char *data = new char[length + 1];
		memset(data, 0, length + 1);
		readData(data, length);
		out = data;
		delete[]data;
    }
    return out;
}

#define pack754_32(f) (pack754((f), 32, 8))
#define pack754_64(f) (pack754((f), 64, 11))
#define unpack754_32(i) (unpack754((i), 32, 8))
#define unpack754_64(i) (unpack754((i), 64, 11))

static uint64_t pack754(long double f, unsigned bits, unsigned expbits)
{
	long double fnorm;
	int shift;
	long long sign, exp, significand;
	unsigned significandbits = bits - expbits - 1; // -1 for sign bit

	if (f == 0.0) return 0; // get this special case out of the way

	// check sign and begin normalization
	if (f < 0) { sign = 1; fnorm = -f; }
	else { sign = 0; fnorm = f; }

	// get the normalized form of f and track the exponent
	shift = 0;
	while(fnorm >= 2.0) { fnorm /= 2.0; shift++; }
	while(fnorm < 1.0) { fnorm *= 2.0; shift--; }
	fnorm = fnorm - 1.0;

	// calculate the binary form (non-float) of the significand data
	significand = fnorm * ((1LL<<significandbits) + 0.5f);

	// get the biased exponent
	exp = shift + ((1<<(expbits-1)) - 1); // shift + bias

	// return the final answer
	return (sign<<(bits-1)) | (exp<<(bits-expbits-1)) | significand;
}

static long double unpack754(uint64_t i, unsigned bits, unsigned expbits)
{
	long double result;
	long long shift;
	unsigned bias;
	unsigned significandbits = bits - expbits - 1; // -1 for sign bit

	if (i == 0) return 0.0;

	// pull the significand
	result = (i&((1LL<<significandbits)-1)); // mask
	result /= (1LL<<significandbits); // convert back to float
	result += 1.0f; // add the one back on

	// deal with the exponent
	bias = (1<<(expbits-1)) - 1;
	shift = ((i>>significandbits)&((1LL<<expbits)-1)) - bias;
	while(shift > 0) { result *= 2.0; shift--; }
	while(shift < 0) { result /= 2.0; shift++; }

	// sign it
	result *= (i>>(bits-1))&1? -1.0: 1.0;

	return result;
}

float binary_ifstream::readFloat() {
	uint32_t i = readInt();
	return unpack754_32(i);
}

double binary_ifstream::readDouble() {
	uint64_t i = readLong();
	return unpack754_64(i);
}

void binary_ofstream::writeFloat(float num) {
	uint32_t i = pack754_32(num);
	writeInt(i);
}

void binary_ofstream::writeDouble(double num) {
	uint64_t i = pack754_64(num);
	writeLong(i);
}