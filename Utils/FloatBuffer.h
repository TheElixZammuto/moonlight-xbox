#pragma once

#include <cstddef>
#include <mutex>
#include <vector>

class FloatBuffer
{
  public:
	static constexpr std::size_t kDefaultCapacity = 512;

	explicit FloatBuffer(std::size_t capacity = kDefaultCapacity);

	void push(float value) noexcept;
	int copyInto(float *outBuffer, std::size_t outSize, float &out_min, float &out_max) const;
	void clear() noexcept;

	std::size_t capacity() const noexcept
	{
		return capacity_;
	}
	bool is_full() const noexcept;
	std::size_t size() const noexcept;

	float average() const noexcept;
	double sum() const noexcept;

	void dump() const noexcept;

  private:
	static bool is_power_of_two(std::size_t x) noexcept;
	void recompute_min_max_unsafe() noexcept;

  private:
	std::vector<float> buffer_;  // backing storage, length == capacity_
	const std::size_t capacity_; // power of two
	std::size_t count_;          // number of valid elements (0..capacity_)
	std::size_t head_;           // index where the next push will write
	float min_;                  // current minimum across valid window
	float max_;                  // current maximum across valid window
	double sum_;                 // running sum for O(1) average
	mutable std::mutex mtx_;     // guards all mutable state
};
