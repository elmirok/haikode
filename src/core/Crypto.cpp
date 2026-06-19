#include "core/Crypto.h"

#include <array>
#include <iomanip>
#include <initializer_list>
#include <random>
#include <sstream>

namespace {

uint32_t
rotateRight(uint32_t value, uint32_t bits)
{
	return (value >> bits) | (value << (32 - bits));
}

uint32_t
choose(uint32_t x, uint32_t y, uint32_t z)
{
	return (x & y) ^ (~x & z);
}

uint32_t
majority(uint32_t x, uint32_t y, uint32_t z)
{
	return (x & y) ^ (x & z) ^ (y & z);
}

uint32_t
bigSigma0(uint32_t x)
{
	return rotateRight(x, 2) ^ rotateRight(x, 13) ^ rotateRight(x, 22);
}

uint32_t
bigSigma1(uint32_t x)
{
	return rotateRight(x, 6) ^ rotateRight(x, 11) ^ rotateRight(x, 25);
}

uint32_t
smallSigma0(uint32_t x)
{
	return rotateRight(x, 7) ^ rotateRight(x, 18) ^ (x >> 3);
}

uint32_t
smallSigma1(uint32_t x)
{
	return rotateRight(x, 17) ^ rotateRight(x, 19) ^ (x >> 10);
}

const std::array<uint32_t, 64> kRoundConstants = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
	0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
	0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
	0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
	0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
	0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
	0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
	0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
	0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

}

namespace Crypto {

std::vector<uint8_t>
Sha256Bytes(const std::string& input)
{
	std::vector<uint8_t> data(input.begin(), input.end());
	const uint64_t bitLength = static_cast<uint64_t>(data.size()) * 8;

	data.push_back(0x80);
	while ((data.size() % 64) != 56)
		data.push_back(0);

	for (int shift = 56; shift >= 0; shift -= 8)
		data.push_back(static_cast<uint8_t>((bitLength >> shift) & 0xff));

	uint32_t h0 = 0x6a09e667;
	uint32_t h1 = 0xbb67ae85;
	uint32_t h2 = 0x3c6ef372;
	uint32_t h3 = 0xa54ff53a;
	uint32_t h4 = 0x510e527f;
	uint32_t h5 = 0x9b05688c;
	uint32_t h6 = 0x1f83d9ab;
	uint32_t h7 = 0x5be0cd19;

	for (size_t offset = 0; offset < data.size(); offset += 64) {
		std::array<uint32_t, 64> words{};
		for (size_t i = 0; i < 16; ++i) {
			const size_t j = offset + i * 4;
			words[i] = (static_cast<uint32_t>(data[j]) << 24)
				| (static_cast<uint32_t>(data[j + 1]) << 16)
				| (static_cast<uint32_t>(data[j + 2]) << 8)
				| static_cast<uint32_t>(data[j + 3]);
		}

		for (size_t i = 16; i < 64; ++i) {
			words[i] = smallSigma1(words[i - 2]) + words[i - 7]
				+ smallSigma0(words[i - 15]) + words[i - 16];
		}

		uint32_t a = h0;
		uint32_t b = h1;
		uint32_t c = h2;
		uint32_t d = h3;
		uint32_t e = h4;
		uint32_t f = h5;
		uint32_t g = h6;
		uint32_t h = h7;

		for (size_t i = 0; i < 64; ++i) {
			const uint32_t temp1 = h + bigSigma1(e) + choose(e, f, g)
				+ kRoundConstants[i] + words[i];
			const uint32_t temp2 = bigSigma0(a) + majority(a, b, c);
			h = g;
			g = f;
			f = e;
			e = d + temp1;
			d = c;
			c = b;
			b = a;
			a = temp1 + temp2;
		}

		h0 += a;
		h1 += b;
		h2 += c;
		h3 += d;
		h4 += e;
		h5 += f;
		h6 += g;
		h7 += h;
	}

	std::vector<uint8_t> digest;
	digest.reserve(32);
	for (uint32_t word : {h0, h1, h2, h3, h4, h5, h6, h7}) {
		digest.push_back(static_cast<uint8_t>((word >> 24) & 0xff));
		digest.push_back(static_cast<uint8_t>((word >> 16) & 0xff));
		digest.push_back(static_cast<uint8_t>((word >> 8) & 0xff));
		digest.push_back(static_cast<uint8_t>(word & 0xff));
	}
	return digest;
}

std::string
Sha256Hex(const std::string& input)
{
	std::ostringstream out;
	out << std::hex << std::setfill('0');
	for (uint8_t byte : Sha256Bytes(input))
		out << std::setw(2) << static_cast<int>(byte);
	return out.str();
}

std::string
Base64UrlEncode(const std::vector<uint8_t>& bytes)
{
	static const char* alphabet =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

	std::string out;
	size_t i = 0;
	while (i + 3 <= bytes.size()) {
		const uint32_t value = (static_cast<uint32_t>(bytes[i]) << 16)
			| (static_cast<uint32_t>(bytes[i + 1]) << 8)
			| static_cast<uint32_t>(bytes[i + 2]);
		out.push_back(alphabet[(value >> 18) & 0x3f]);
		out.push_back(alphabet[(value >> 12) & 0x3f]);
		out.push_back(alphabet[(value >> 6) & 0x3f]);
		out.push_back(alphabet[value & 0x3f]);
		i += 3;
	}

	const size_t remaining = bytes.size() - i;
	if (remaining == 1) {
		const uint32_t value = static_cast<uint32_t>(bytes[i]) << 16;
		out.push_back(alphabet[(value >> 18) & 0x3f]);
		out.push_back(alphabet[(value >> 12) & 0x3f]);
	} else if (remaining == 2) {
		const uint32_t value = (static_cast<uint32_t>(bytes[i]) << 16)
			| (static_cast<uint32_t>(bytes[i + 1]) << 8);
		out.push_back(alphabet[(value >> 18) & 0x3f]);
		out.push_back(alphabet[(value >> 12) & 0x3f]);
		out.push_back(alphabet[(value >> 6) & 0x3f]);
	}

	return out;
}

std::string
RandomBase64Url(size_t byteCount)
{
	std::random_device random;
	std::vector<uint8_t> bytes(byteCount);
	for (uint8_t& byte : bytes)
		byte = static_cast<uint8_t>(random());
	return Base64UrlEncode(bytes);
}

}
