#include "pch.h"
#include "FloatBuffer.h"
#include "../Utils.hpp"

#include <algorithm>
#include <cfloat>
#include <cstring>
#include <sstream>
#include <stdexcept>

using namespace moonlight_xbox_dx;

FloatBuffer::FloatBuffer(std::size_t capacity) :
    buffer_(capacity),
    capacity_(capacity),
    count_(0),
    head_(0),
    min_(FLT_MAX),
    max_(-FLT_MAX),
    sum_(0.0f)
{
	if (!is_power_of_two(capacity_)) {
		throw std::invalid_argument("FloatBuffer capacity must be a power of two");
	}
}

void FloatBuffer::push(float value) noexcept
{
	std::lock_guard<std::mutex> lock(mtx_);

	const bool was_full = (count_ == capacity_);
	float evicted = 0.0f;

	if (was_full) {
		// We are about to overwrite the oldest value at head_.
		evicted = buffer_[head_];
		sum_ -= static_cast<double>(evicted);
	} else {
		++count_;
	}

	buffer_[head_] = value;
	head_ = (head_ + 1) & (capacity_ - 1);

	// Update cheap aggregates.
	sum_ += static_cast<double>(value);
	if (value < min_) min_ = value;
	if (value > max_) max_ = value;

	// If we evicted the current min or max, recompute
	if (was_full && (evicted == min_ || evicted == max_)) {
		recompute_min_max_unsafe();
	}
}

std::size_t FloatBuffer::copyInto(float *outBuffer, float &out_min, float &out_max) const
{
	std::lock_guard<std::mutex> lock(mtx_);

	if (count_ == 0) {
		out_min = 0.0f;
		out_max = 0.0f;
		return 0;
	}

	// Oldest element lives at tail.
	const std::size_t tail = (head_ + capacity_ - count_) & (capacity_ - 1);
	const std::size_t first = std::min(capacity_ - tail, count_);

	// First contiguous chunk.
	std::memcpy(outBuffer, buffer_.data() + tail, first * sizeof(float));

	// If wrapped, copy remaining prefix from index 0.
	if (first < count_) {
		std::memcpy(outBuffer + first, buffer_.data(), (count_ - first) * sizeof(float));
	}

	out_min = min_;
	out_max = max_;
	return count_;
}

void FloatBuffer::clear() noexcept
{
	std::lock_guard<std::mutex> lock(mtx_);
	head_ = 0;
	count_ = 0;
	min_ = FLT_MAX;
	max_ = -FLT_MAX;
	sum_ = 0.0f;
	// Note: we intentionally do not zero buffer_ for performance.
}

std::size_t FloatBuffer::size() const noexcept
{
	std::lock_guard<std::mutex> lock(mtx_);
	return count_;
}

bool FloatBuffer::is_full() const noexcept
{
	std::lock_guard<std::mutex> lock(mtx_);
	return count_ == capacity_;
}

float FloatBuffer::average() const noexcept
{
	std::lock_guard<std::mutex> lock(mtx_);
	return (count_ > 0) ? static_cast<float>(sum_ / static_cast<double>(count_)) : 0.0f;
}

double FloatBuffer::sum() const noexcept
{
	std::lock_guard<std::mutex> lock(mtx_);
	return (count_ > 0) ? sum_ : 0.0f;
}

bool FloatBuffer::is_power_of_two(std::size_t x) noexcept
{
	return x != 0 && (x & (x - 1)) == 0;
}

void FloatBuffer::recompute_min_max_unsafe() noexcept
{
	float mn = FLT_MAX;
	float mx = -FLT_MAX;

	const std::size_t tail = (head_ + capacity_ - count_) & (capacity_ - 1);
	const std::size_t first = std::min(capacity_ - tail, count_);

	const float *p0 = buffer_.data() + tail;
	for (std::size_t i = 0; i < first; ++i) {
		const float v = p0[i];
		if (v < mn) mn = v;
		if (v > mx) mx = v;
	}

	if (first < count_) {
		const float *p1 = buffer_.data();
		for (std::size_t i = 0; i < (count_ - first); ++i) {
			const float v = p1[i];
			if (v < mn) mn = v;
			if (v > mx) mx = v;
		}
	}

	min_ = mn;
	max_ = mx;
}

void FloatBuffer::dump() const noexcept
{
	std::lock_guard<std::mutex> lock(mtx_);

	if (count_ == 0) {
		Utils::Logf("[FloatBuffer empty]\n");
		return;
	}

	std::ostringstream oss;
	oss << "[FloatBuffer size=" << count_ << "/" << capacity_ << "] ";

	// Start from oldest element
	std::size_t tail = (head_ + capacity_ - count_) & (capacity_ - 1);
	std::size_t first = std::min(capacity_ - tail, count_);

	// first contiguous segment
	for (std::size_t i = 0; i < first; ++i) {
		if (i > 0) oss << ',';
		oss << buffer_[tail + i];
	}

	// wraparound segment
	for (std::size_t i = 0; i < count_ - first; ++i) {
		oss << ',' << buffer_[i];
	}

	Utils::Logf("%s\n", oss.str().c_str());
}
