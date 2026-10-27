#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <vector>

class SlidingWindowCounter {
  public:
	explicit SlidingWindowCounter(
	    uint32_t windowSeconds = 60,
	    uint32_t bucketIntervalMs = 1000)
	    : m_bucketInterval(bucketIntervalMs) {
		const uint64_t windowMs = uint64_t(windowSeconds) * 1000;

		if (windowMs == 0 || bucketIntervalMs == 0 || windowMs % bucketIntervalMs != 0) {
			throw std::invalid_argument("Window must be a positive multiple of bucket interval");
		}

		m_buckets.resize(static_cast<std::size_t>(windowMs / bucketIntervalMs));
	}

	void Add(uint64_t count) {
		std::lock_guard<std::mutex> lock(m_mutex);
		const auto tick = currentTick();
		auto &bucket = m_buckets[tick % m_buckets.size()];

		// This slot may contain data from a previous trip around the ring.
		if (!bucket.used || bucket.tick != tick) {
			bucket = Bucket{};
			bucket.tick = tick;
			bucket.used = true;
		}

		bucket.count += count;
	}

	uint64_t GetTotal() const {
		std::lock_guard<std::mutex> lock(m_mutex);
		const auto tick = currentTick();
		uint64_t total = 0;

		for (const auto &bucket : m_buckets) {
			if (bucket.used && tick - bucket.tick < m_buckets.size()) {
				total += bucket.count;
			}
		}

		return total;
	}

	void Reset() {
		std::lock_guard<std::mutex> lock(m_mutex);
		for (auto &bucket : m_buckets) {
			bucket = Bucket{};
		}
		m_origin = Clock::now();
	}

  private:
	using Clock = std::chrono::steady_clock;

	struct Bucket {
		uint64_t tick = 0;
		uint64_t count = 0;
		bool used = false;
	};

	// Caller holds m_mutex. Ticks advance even when no data arrives.
	uint64_t currentTick() const {
		const auto elapsed =
		    std::chrono::duration_cast<std::chrono::milliseconds>(
		        Clock::now() - m_origin);

		return static_cast<uint64_t>(elapsed.count() / m_bucketInterval.count());
	}

	const std::chrono::milliseconds m_bucketInterval;
	Clock::time_point m_origin = Clock::now();
	std::vector<Bucket> m_buckets;
	mutable std::mutex m_mutex;
};
