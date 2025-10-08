// PacerRational.h
// Support methods for handling fractional refresh rates in Qpc integer base

#pragma once
#include <cstdint>
#include <immintrin.h>
#include <intrin.h>

#pragma intrinsic(_umul128, _udiv128)

// Represents a period in QPC ticks using an integer part plus a fractional accumulator.
// This avoids drift at fractional Hz such as 59.94 or 119.88.
struct QpcRationalPeriod {
	int64_t baseTicks = 0;       // Integer ticks added each step
	uint64_t remainder = 0;      // numerator for fractional tick
	uint32_t den = 1;            // denominator for fractional tick
	uint64_t accum = 0;          // accumulator for fractional carry
	int64_t nextDeadlineQpc = 0; // absolute QPC for next tick

	// Initialize from exact Hz as a rational number: Hz = num/den.
	// Period(QPC) = QpcFreq * den / num.
	void initFromHz(uint32_t hzNum, uint32_t hzDen, int64_t baseQpc) {
		const uint64_t f = (uint64_t)QpcFreq();

		uint64_t hi = 0;
		uint64_t lo = _umul128(f, (uint64_t)hzDen, &hi);
		uint64_t r = 0;
		uint64_t q = _udiv128(hi, lo, hzNum, &r);

		baseTicks = (int64_t)q;
		remainder = r;
		den = hzNum;
		accum = 0;
		nextDeadlineQpc = baseQpc;
	}

	// Advance to the next deadline
	void step() {
		nextDeadlineQpc += baseTicks;
		if (remainder != 0) {
			accum += remainder;
			if (accum >= den) {
				// Carry one extra QPC tick occasionally to account for the fractional period
				nextDeadlineQpc += 1;
				accum -= den;
			}
		}
	}
};
