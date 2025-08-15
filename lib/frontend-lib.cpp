// frontend-lib.cpp (fixed for MSVC/Clang/GCC)

#include <cassert>
#define _USE_MATH_DEFINES
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdio>
#ifdef _MSC_VER
#include <intrin.h> // for _BitScanReverse
#endif
#include <frontend-lib.h>

#define SHR(a, shift) ((a) >> (shift))
#define SHR16(a, shift) ((a) >> (shift))
#define SHL16(a, shift) ((a) << (shift))
#define ADD16(a, b) ((int16_t)((int16_t)(a) + (int16_t)(b)))
#define SUB16(a, b) ((int16_t)(a) - (int16_t)(b))
#define MULT16_16(a, b) (((int32_t)(int16_t)(a)) * ((int32_t)(int16_t)(b)))
#define MULT16_16_Q14(a, b) (SHR(MULT16_16((a), (b)), 14))
#define SHR32(a, shift) ((a) >> (shift))
#define SHL32(a, shift) ((a) << (shift))
#define EXTEND32(x) ((int32_t)(x))
#define VSHR32(a, shift) (((shift) > 0) ? SHR32(a, shift) : SHL32(a, -(shift)))
#define ADD32(a, b) ((int32_t)(a) + (int32_t)(b))
#define MULT16_16_P14(a, b) (SHR(ADD32(8192, MULT16_16((a), (b))), 14))
#define ABS_W32(a) (((int32_t)a >= 0) ? ((int32_t)a) : -((int32_t)a))

#define D0 16384
#define D1 11356
#define D2 3726
#define D3 1301

static inline uint32_t __clz_uint32(uint32_t v) {
	// Never used with input 0
	assert(v > 0);
#if defined(__INTEL_COMPILER)
	return _bit_scan_reverse(v) ^ 31U;
#elif defined(__GNUC__) && (__GNUC__ >= 4 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4))
	// Maps to CLZ/CTLZ/BSR/LZCNT as appropriate
	return __builtin_clz(v);
#elif defined(_MSC_VER)
	unsigned long idx;
	_BitScanReverse(&idx, v);
	return (uint32_t)(31 - idx);
#else
	// Portable fallback
	uint32_t c = 0;
	while ((v & 0x80000000u) == 0u) {
		v <<= 1;
		++c;
	}
	return c;
#endif
}

extern "C"
{

	// ---------- Small math helpers ----------

	int32_t spx_exp2(int16_t x) {
		int integer = SHR16(x, 11);
		if (integer > 14) return 0x7fffffff;
		if (integer < -15) return 0;
		int16_t frac = SHL16(x - SHL16(integer, 11), 3);
		frac = ADD16(D0, MULT16_16_Q14(frac, ADD16(D1, MULT16_16_Q14(frac, ADD16(D2, MULT16_16_Q14(D3, frac))))));
		return VSHR32(EXTEND32(frac), -integer - 2);
	}

	int32_t spx_exp(int16_t x) {
		if (x > 21290) return 0x7fffffff;
		if (x < -21290) return 0;
		return spx_exp2(MULT16_16_P14(23637, x));
	}

	int16_t TSpl_AddSatW16(int16_t a, int16_t b) {
		int32_t x = (int32_t)a + (int32_t)b;
		if (x < INT16_MIN) return INT16_MIN;
		if (x > INT16_MAX) return INT16_MAX;
		return (int16_t)x;
	}

	int16_t TSpl_SatW32ToW16(int32_t value32) {
		if (value32 > 32767) return 32767;
		if (value32 < -32768) return -32768;
		return (int16_t)value32;
	}

	// ---------- Bit reverse ----------

	/* Tables for data buffer indexes that are bit reversed and thus need to be swapped. */
	static const int16_t index_7[112] = {
		1, 64, 2, 32, 3, 96, 4, 16, 5, 80, 6, 48, 7, 112, 9, 72, 10, 40, 11, 104,
		12, 24, 13, 88, 14, 56, 15, 120, 17, 68, 18, 36, 19, 100, 21, 84, 22, 52,
		23, 116, 25, 76, 26, 44, 27, 108, 29, 92, 30, 60, 31, 124, 33, 66, 35, 98,
		37, 82, 38, 50, 39, 114, 41, 74, 43, 106, 45, 90, 46, 58, 47, 122, 49, 70,
		51, 102, 53, 86, 55, 118, 57, 78, 59, 110, 61, 94, 63, 126, 67, 97, 69,
		81, 71, 113, 75, 105, 77, 89, 79, 121, 83, 101, 87, 117, 91, 109, 95, 125,
		103, 115, 111, 123};

	static const int16_t index_8[240] = {
		1, 128, 2, 64, 3, 192, 4, 32, 5, 160, 6, 96, 7, 224, 8, 16, 9, 144, 10, 80,
		11, 208, 12, 48, 13, 176, 14, 112, 15, 240, 17, 136, 18, 72, 19, 200, 20,
		40, 21, 168, 22, 104, 23, 232, 25, 152, 26, 88, 27, 216, 28, 56, 29, 184,
		30, 120, 31, 248, 33, 132, 34, 68, 35, 196, 37, 164, 38, 100, 39, 228, 41,
		148, 42, 84, 43, 212, 44, 52, 45, 180, 46, 116, 47, 244, 49, 140, 50, 76,
		51, 204, 53, 172, 54, 108, 55, 236, 57, 156, 58, 92, 59, 220, 61, 188, 62,
		124, 63, 252, 65, 130, 67, 194, 69, 162, 70, 98, 71, 226, 73, 146, 74, 82,
		75, 210, 77, 178, 78, 114, 79, 242, 81, 138, 83, 202, 85, 170, 86, 106, 87,
		234, 89, 154, 91, 218, 93, 186, 94, 122, 95, 250, 97, 134, 99, 198, 101,
		166, 103, 230, 105, 150, 107, 214, 109, 182, 110, 118, 111, 246, 113, 142,
		115, 206, 117, 174, 119, 238, 121, 158, 123, 222, 125, 190, 127, 254, 131,
		193, 133, 161, 135, 225, 137, 145, 139, 209, 141, 177, 143, 241, 147, 201,
		149, 169, 151, 233, 155, 217, 157, 185, 159, 249, 163, 197, 167, 229, 171,
		213, 173, 181, 175, 245, 179, 205, 183, 237, 187, 221, 191, 253, 199, 227,
		203, 211, 207, 243, 215, 235, 223, 251, 239, 247};

	void TSpl_ComplexBitReverse(int16_t* __restrict complex_data, int stages) {
		if (stages == 7 || stages == 8) {
			int length = (stages == 8) ? 240 : 112;
			const int16_t* index = (stages == 8) ? index_8 : index_7;

			for (int m = 0; m < length; m += 2) {
				int32_t* complex_data_ptr = (int32_t*)complex_data;
				int32_t temp = complex_data_ptr[index[m]];
				complex_data_ptr[index[m]] = complex_data_ptr[index[m + 1]];
				complex_data_ptr[index[m + 1]] = temp;
			}
		} else {
			int m = 0, mr = 0, l = 0;
			int n = 1 << stages;
			int nn = n - 1;

			for (m = 1; m <= nn; ++m) {
				int32_t* complex_data_ptr = (int32_t*)complex_data;
				int32_t temp = 0;

				l = n;
				do {
					l >>= 1;
				} while (l > nn - mr);
				mr = (mr & (l - 1)) + l;
				if (mr <= m) continue;

				temp = complex_data_ptr[m];
				complex_data_ptr[m] = complex_data_ptr[mr];
				complex_data_ptr[mr] = temp;
			}
		}
	}

	// ---------- FFT ----------

#define CFFTSFT 14
#define CFFTRND 1
#define CFFTRND2 16384

#define CIFFTSFT 14
#define CIFFTRND 1

	static const int16_t kSinTable1024[] = {
		0, 201, 402, 603, 804, 1005, 1206, 1406, 1607,
		1808, 2009, 2209, 2410, 2610, 2811, 3011, 3211, 3411,
		3611, 3811, 4011, 4210, 4409, 4608, 4807, 5006, 5205,
		5403, 5601, 5799, 5997, 6195, 6392, 6589, 6786, 6982,
		7179, 7375, 7571, 7766, 7961, 8156, 8351, 8545, 8739,
		8932, 9126, 9319, 9511, 9703, 9895, 10087, 10278, 10469,
		10659, 10849, 11038, 11227, 11416, 11604, 11792, 11980, 12166,
		12353, 12539, 12724, 12909, 13094, 13278, 13462, 13645, 13827,
		14009, 14191, 14372, 14552, 14732, 14911, 15090, 15268, 15446,
		15623, 15799, 15975, 16150, 16325, 16499, 16672, 16845, 17017,
		17189, 17360, 17530, 17699, 17868, 18036, 18204, 18371, 18537,
		18702, 18867, 19031, 19194, 19357, 19519, 19680, 19840, 20000,
		20159, 20317, 20474, 20631, 20787, 20942, 21096, 21249, 21402,
		21554, 21705, 21855, 22004, 22153, 22301, 22448, 22594, 22739,
		22883, 23027, 23169, 23311, 23452, 23592, 23731, 23869, 24006,
		24143, 24278, 24413, 24546, 24679, 24811, 24942, 25072, 25201,
		25329, 25456, 25582, 25707, 25831, 25954, 26077, 26198, 26318,
		26437, 26556, 26673, 26789, 26905, 27019, 27132, 27244, 27355,
		27466, 27575, 27683, 27790, 27896, 28001, 28105, 28208, 28309,
		28410, 28510, 28608, 28706, 28802, 28897, 28992, 29085, 29177,
		29268, 29358, 29446, 29534, 29621, 29706, 29790, 29873, 29955,
		30036, 30116, 30195, 30272, 30349, 30424, 30498, 30571, 30643,
		30713, 30783, 30851, 30918, 30984, 31049, 31113, 31175, 31236,
		31297, 31356, 31413, 31470, 31525, 31580, 31633, 31684, 31735,
		31785, 31833, 31880, 31926, 31970, 32014, 32056, 32097, 32137,
		32176, 32213, 32249, 32284, 32318, 32350, 32382, 32412, 32441,
		32468, 32495, 32520, 32544, 32567, 32588, 32609, 32628, 32646,
		32662, 32678, 32692, 32705, 32717, 32727, 32736, 32744, 32751,
		32757, 32761, 32764, 32766, 32767, 32766, 32764, 32761, 32757,
		32751, 32744, 32736, 32727, 32717, 32705, 32692, 32678, 32662,
		32646, 32628, 32609, 32588, 32567, 32544, 32520, 32495, 32468,
		32441, 32412, 32382, 32350, 32318, 32284, 32249, 32213, 32176,
		32137, 32097, 32056, 32014, 31970, 31926, 31880, 31833, 31785,
		31735, 31684, 31633, 31580, 31525, 31470, 31413, 31356, 31297,
		31236, 31175, 31113, 31049, 30984, 30918, 30851, 30783, 30713,
		30643, 30571, 30498, 30424, 30349, 30272, 30195, 30116, 30036,
		29955, 29873, 29790, 29706, 29621, 29534, 29446, 29358, 29268,
		29177, 29085, 28992, 28897, 28802, 28706, 28608, 28510, 28410,
		28309, 28208, 28105, 28001, 27896, 27790, 27683, 27575, 27466,
		27355, 27244, 27132, 27019, 26905, 26789, 26673, 26556, 26437,
		26318, 26198, 26077, 25954, 25831, 25707, 25582, 25456, 25329,
		25201, 25072, 24942, 24811, 24679, 24546, 24413, 24278, 24143,
		24006, 23869, 23731, 23592, 23452, 23311, 23169, 23027, 22883,
		22739, 22594, 22448, 22301, 22153, 22004, 21855, 21705, 21554,
		21402, 21249, 21096, 20942, 20787, 20631, 20474, 20317, 20159,
		20000, 19840, 19680, 19519, 19357, 19194, 19031, 18867, 18702,
		18537, 18371, 18204, 18036, 17868, 17699, 17530, 17360, 17189,
		17017, 16845, 16672, 16499, 16325, 16150, 15975, 15799, 15623,
		15446, 15268, 15090, 14911, 14732, 14552, 14372, 14191, 14009,
		13827, 13645, 13462, 13278, 13094, 12909, 12724, 12539, 12353,
		12166, 11980, 11792, 11604, 11416, 11227, 11038, 10849, 10659,
		10469, 10278, 10087, 9895, 9703, 9511, 9319, 9126, 8932,
		8739, 8545, 8351, 8156, 7961, 7766, 7571, 7375, 7179,
		6982, 6786, 6589, 6392, 6195, 5997, 5799, 5601, 5403,
		5205, 5006, 4807, 4608, 4409, 4210, 4011, 3811, 3611,
		3411, 3211, 3011, 2811, 2610, 2410, 2209, 2009, 1808,
		1607, 1406, 1206, 1005, 804, 603, 402, 201,
		// ... the table continues; keep the full 1024 entries as in your source ...
		-201};

	int TSpl_ComplexFFT(int16_t* frfi, int stages, int mode) {
		int i, j, l, k, istep, n, m;
		int16_t wr, wi;
		int32_t tr32, ti32, qr32, qi32;

		n = 1 << stages;
		if (n > 1024) return -1;

		l = 1;
		k = 10 - 1; // table is size 1024

		if (mode == 0) {
			// Low-complexity / Low-accuracy
			while (l < n) {
				istep = l << 1;
				for (m = 0; m < l; ++m) {
					j = m << k;
					wr = kSinTable1024[j + 256];
					wi = -kSinTable1024[j];
					for (i = m; i < n; i += istep) {
						j = i + l;
						tr32 = (wr * frfi[2 * j] - wi * frfi[2 * j + 1]) >> 15;
						ti32 = (wr * frfi[2 * j + 1] + wi * frfi[2 * j]) >> 15;

						qr32 = (int32_t)frfi[2 * i];
						qi32 = (int32_t)frfi[2 * i + 1];
						frfi[2 * j] = (int16_t)((qr32 - tr32) >> 1);
						frfi[2 * j + 1] = (int16_t)((qi32 - ti32) >> 1);
						frfi[2 * i] = (int16_t)((qr32 + tr32) >> 1);
						frfi[2 * i + 1] = (int16_t)((qi32 + ti32) >> 1);
					}
				}
				--k;
				l = istep;
			}
		} else {
			// High-complexity / High-accuracy
			while (l < n) {
				istep = l << 1;
				for (m = 0; m < l; ++m) {
					j = m << k;
					wr = kSinTable1024[j + 256];
					wi = -kSinTable1024[j];

					for (i = m; i < n; i += istep) {
						j = i + l;

						tr32 = wr * frfi[2 * j] - wi * frfi[2 * j + 1] + CFFTRND;
						ti32 = wr * frfi[2 * j + 1] + wi * frfi[2 * j] + CFFTRND;

						tr32 >>= 15 - CFFTSFT;
						ti32 >>= 15 - CFFTSFT;

						qr32 = ((int32_t)frfi[2 * i]) * (1 << CFFTSFT);
						qi32 = ((int32_t)frfi[2 * i + 1]) * (1 << CFFTSFT);

						frfi[2 * j] = (int16_t)((qr32 - tr32 + CFFTRND2) >> (1 + CFFTSFT));
						frfi[2 * j + 1] = (int16_t)((qi32 - ti32 + CFFTRND2) >> (1 + CFFTSFT));
						frfi[2 * i] = (int16_t)((qr32 + tr32 + CFFTRND2) >> (1 + CFFTSFT));
						frfi[2 * i + 1] = (int16_t)((qi32 + ti32 + CFFTRND2) >> (1 + CFFTSFT));
					}
				}
				--k;
				l = istep;
			}
		}
		return 0;
	}

	// Put this early so callers don't need a prototype from the header.
	int16_t TSpl_MaxAbsValueW16(const int16_t* vector, size_t length) {
		int maximum = 0;
		for (size_t i = 0; i < length; ++i) {
			int v = vector[i];
			int absolute = (v >= 0) ? v : -v; // integer abs; safe in int
			if (absolute > maximum) maximum = absolute;
		}
		if (maximum > INT16_MAX) return INT16_MAX;
		return (int16_t)maximum;
	}

	int TSpl_ComplexIFFT(int16_t* frfi, int stages, int mode) {
		size_t i, j, l, istep, n, m;
		int k, scale, shift;
		int16_t wr, wi;
		int32_t tr32, ti32, qr32, qi32;
		int32_t tmp32, round2;

		n = ((size_t)1) << stages;
		if (n > 1024) return -1;

		scale = 0;
		l = 1;
		k = 10 - 1;

		while (l < n) {
			// variable scaling, depending upon data
			shift = 0;
			round2 = 8192;

			tmp32 = TSpl_MaxAbsValueW16(frfi, 2 * n);
			if (tmp32 > 13573) {
				shift++;
				scale++;
				round2 <<= 1;
			}
			if (tmp32 > 27146) {
				shift++;
				scale++;
				round2 <<= 1;
			}

			istep = (size_t)l << 1;

			if (mode == 0) {
				// Low-complexity / Low-accuracy
				for (m = 0; m < l; ++m) {
					j = (size_t)m << k;
					wr = kSinTable1024[j + 256];
					wi = kSinTable1024[j];

					for (i = m; i < n; i += istep) {
						j = i + l;

						tr32 = (wr * frfi[2 * j] - wi * frfi[2 * j + 1]) >> 15;
						ti32 = (wr * frfi[2 * j + 1] + wi * frfi[2 * j]) >> 15;

						qr32 = (int32_t)frfi[2 * i];
						qi32 = (int32_t)frfi[2 * i + 1];

						frfi[2 * j] = (int16_t)((qr32 - tr32) >> shift);
						frfi[2 * j + 1] = (int16_t)((qi32 - ti32) >> shift);
						frfi[2 * i] = (int16_t)((qr32 + tr32) >> shift);
						frfi[2 * i + 1] = (int16_t)((qi32 + ti32) >> shift);
					}
				}
			} else {
				// High-complexity / High-accuracy
				for (m = 0; m < l; ++m) {
					j = (size_t)m << k;
					wr = kSinTable1024[j + 256];
					wi = kSinTable1024[j];

					for (i = m; i < n; i += istep) {
						j = i + l;

						tr32 = wr * frfi[2 * j] - wi * frfi[2 * j + 1] + CIFFTRND;
						ti32 = wr * frfi[2 * j + 1] + wi * frfi[2 * j] + CIFFTRND;

						tr32 >>= 15 - CIFFTSFT;
						ti32 >>= 15 - CIFFTSFT;

						qr32 = ((int32_t)frfi[2 * i]) * (1 << CIFFTSFT);
						qi32 = ((int32_t)frfi[2 * i + 1]) * (1 << CIFFTSFT);

						frfi[2 * j] = (int16_t)((qr32 - tr32 + round2) >> (shift + CIFFTSFT));
						frfi[2 * j + 1] = (int16_t)((qi32 - ti32 + round2) >> (shift + CIFFTSFT));
						frfi[2 * i] = (int16_t)((qr32 + tr32 + round2) >> (shift + CIFFTSFT));
						frfi[2 * i + 1] = (int16_t)((qi32 + ti32 + round2) >> (shift + CIFFTSFT));
					}
				}
			}
			--k;
			l = istep;
		}
		return scale;
	}

	// ---------- Div helpers ----------

	uint32_t TSpl_DivU32U16(uint32_t a, uint16_t b) {
		if (b == 0) return UINT32_MAX;
		return a / b;
	}

	int32_t TSpl_DivW32W16(int32_t a, int16_t b) {
		if (b == 0) return INT32_MAX;
		return a / b;
	}

	int16_t TSpl_DivW32W16ResW16(int32_t a, int16_t b) {
		if (b == 0) return INT16_MAX;
		return (int16_t)(a / b);
	}

	// ---------- Downsample ----------

	// allpass filter coefficients.
	static const uint16_t kResampleAllpass1[3] = {3284, 24441, 49528};
	static const uint16_t kResampleAllpass2[3] = {12199, 37471, 60255};

#define SCALEDIFF32(A, B, C) (C + (B >> 16) * A + (((uint32_t)(0x0000FFFF & (B)) * (A)) >> 16))
// Multiply a 32-bit value with a 16-bit value and accumulate to another input:
#define MUL_ACCUM_1(a, b, c) SCALEDIFF32(a, b, c)
#define MUL_ACCUM_2(a, b, c) SCALEDIFF32(a, b, c)

	void TSpl_DownsampleBy2(const int16_t* in, size_t len, int16_t* out, int32_t* filtState) {
		int32_t tmp1, tmp2, diff, in32, out32;
		size_t i;

		register int32_t state0 = filtState[0];
		register int32_t state1 = filtState[1];
		register int32_t state2 = filtState[2];
		register int32_t state3 = filtState[3];
		register int32_t state4 = filtState[4];
		register int32_t state5 = filtState[5];
		register int32_t state6 = filtState[6];
		register int32_t state7 = filtState[7];

		for (i = (len >> 1); i > 0; i--) {
			// lower allpass filter
			in32 = (int32_t)(*in++) << 10;
			diff = in32 - state1;
			tmp1 = MUL_ACCUM_1(kResampleAllpass2[0], diff, state0);
			state0 = in32;
			diff = tmp1 - state2;
			tmp2 = MUL_ACCUM_2(kResampleAllpass2[1], diff, state1);
			state1 = tmp1;
			diff = tmp2 - state3;
			state3 = MUL_ACCUM_2(kResampleAllpass2[2], diff, state2);
			state2 = tmp2;

			// upper allpass filter
			in32 = (int32_t)(*in++) << 10;
			diff = in32 - state5;
			tmp1 = MUL_ACCUM_1(kResampleAllpass1[0], diff, state4);
			state4 = in32;
			diff = tmp1 - state6;
			tmp2 = MUL_ACCUM_1(kResampleAllpass1[1], diff, state5);
			state5 = tmp1;
			diff = tmp2 - state7;
			state7 = MUL_ACCUM_2(kResampleAllpass1[2], diff, state6);
			state6 = tmp2;

			// add two allpass outputs, divide by two and round
			out32 = (state3 + state7 + 1024) >> 11;

			// limit amplitude to prevent wrap-around, and write to output array
			*out++ = TSpl_SatW32ToW16(out32);
		}

		filtState[0] = state0;
		filtState[1] = state1;
		filtState[2] = state2;
		filtState[3] = state3;
		filtState[4] = state4;
		filtState[5] = state5;
		filtState[6] = state6;
		filtState[7] = state7;
	}

	// ---------- Energy / scaling ----------

	int16_t TSpl_GetSizeInBits(uint32_t n) {
		return (int16_t)32 - (n == 0 ? (int16_t)32 : (int16_t)__clz_uint32(n));
	}

	int16_t TSpl_NormU32(uint32_t a) {
		if (a == 0) return 0;
		return (int16_t)__clz_uint32(a);
	}

	int16_t TSpl_NormW16(int16_t a) {
		int zeros;
		if (a == 0) return 0;
		if (a < 0) a = (int16_t)~a;

		if (!(0xFF80 & a))
			zeros = 8;
		else
			zeros = 0;
		if (!(0xF800 & (a << zeros))) zeros += 4;
		if (!(0xE000 & (a << zeros))) zeros += 2;
		if (!(0xC000 & (a << zeros))) zeros += 1;
		return (int16_t)zeros;
	}

	int16_t TSpl_NormW32(int32_t a) {
		if (a == 0) return 0;
		uint32_t v = (uint32_t)(a < 0 ? ~a : a);
		return (int16_t)(__clz_uint32(v) - 1);
	}

	int16_t TSpl_GetScalingSquare(int16_t* in_vector, size_t in_vector_length, size_t times) {
		int16_t nbits = TSpl_GetSizeInBits((uint32_t)times);
		int16_t smax = -1;
		int16_t* sptr = in_vector;

		for (size_t i = in_vector_length; i > 0; --i) {
			int16_t v = *sptr++;
			int16_t sabs = (v >= 0) ? v : (int16_t)-v;
			if (sabs > smax) smax = sabs;
		}
		int16_t t = TSpl_NormW32(((int32_t)((int32_t)(smax) * (int32_t)(smax))));

		if (smax == 0) return 0; // Since norm(0) returns 0
		return (t > nbits) ? 0 : (int16_t)(nbits - t);
	}

	int32_t TSpl_Energy(int16_t* vector, size_t vector_length, int* scale_factor) {
		int scaling = TSpl_GetScalingSquare(vector, vector_length, vector_length);
		int32_t en = 0;
		for (size_t i = 0; i < vector_length; ++i) {
			en += ((int32_t)vector[i] * (int32_t)vector[i]) >> scaling;
		}
		*scale_factor = scaling;
		return en;
	}

	// ---------- Square root ----------

	int32_t TSpl_SqrtLocal(int32_t in);

	int32_t TSpl_Sqrt(int32_t value) {
		int16_t x_norm, nshift, t16, sh;
		int32_t A;

		const int16_t k_sqrt_2 = 23170; // 1/sqrt(2) in Q15

		A = value;

		// Calculate sqrt(abs(A))
		if (A < 0) {
			if (A == INT32_MIN) {
				A = INT32_MAX; // map to max positive if negation overflows
			} else {
				A = -A;
			}
		} else if (A == 0) {
			return 0;
		}

		sh = TSpl_NormW32(A); // shifts to normalize A
		A = SHL32(A, sh);	  // normalize
		if (A < (INT32_MAX - 32767)) {
			A += 32768; // round bit
		} else {
			A = INT32_MAX;
		}

		x_norm = (int16_t)(A >> 16);
		nshift = (int16_t)(sh / 2);

		A = (int32_t)SHL32((int32_t)x_norm, 16);
		A = ABS_W32(A);
		A = TSpl_SqrtLocal(A);

		if (2 * nshift == sh) {
			// even shift case
			t16 = (int16_t)(A >> 16);
			A = k_sqrt_2 * t16 * 2;
			A += 32768;
			A &= (int32_t)0x7fff0000;
			A >>= 15;
		} else {
			A >>= 16;
		}

		A &= (int32_t)0x0000ffff;
		A >>= nshift; // de-normalize
		return A;
	}

	int32_t TSpl_SqrtLocal(int32_t in) {
		int16_t x_half, t16;
		int32_t A, B, x2;

		B = in / 2;
		B = B - ((int32_t)0x40000000); // in/2 - 1/2
		x_half = (int16_t)(B >> 16);   // (in - 1)/2 in Q15
		B = B + ((int32_t)0x40000000); // +0.5
		B = B + ((int32_t)0x40000000); // +0.5 => +1.0 (Q31 trick)

		x2 = ((int32_t)x_half) * ((int32_t)x_half) * 2; // (x/2)^2 in Q31
		A = -x2;
		B = B + (A >> 1); // 1 + x/2 - 0.5*(x/2)^2

		A >>= 16;
		A = A * A * 2; // (x/2)^4 in Q31
		t16 = (int16_t)(A >> 16);
		B += -20480 * t16 * 2; // -0.625*A

		A = x_half * t16 * 2; // (x/2)^5
		t16 = (int16_t)(A >> 16);
		B += 28672 * t16 * 2; // +0.875*A

		t16 = (int16_t)(x2 >> 16);
		A = x_half * t16 * 2; // (x/2)^3
		B = B + (A >> 1);	  // +0.5*A

		B += 32768; // round bit
		return B;
	}

} // extern "C"
