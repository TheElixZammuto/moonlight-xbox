#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "../Utils/FloatBuffer.h"
#include "Utils.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
}

class FrameQueue {
  public:
	// Singleton
	static FrameQueue &instance();

	std::size_t count() const;
	bool isEmpty() const;
	void clear();
	void setHighWaterMark(int hwm);
	int highWaterMark() const;
	int maxCapacity() const {
		return _maxCapacity;
	}

	// Queue operations
	int enqueue(AVFrame *frame);
	AVFrame* dequeue();
	AVFrame* dequeueWithTimeout(double timeoutSeconds);
	void waitForEnqueue(int num = 1);
	void waitForEnqueue(int num = 1, double timeoutMs = 1000.0 / 60);

	void start();
	void stop();

  private:
  	FrameQueue();
	FrameQueue(const FrameQueue &) = delete;
	FrameQueue &operator=(const FrameQueue &) = delete;

	void setPaused(bool paused);
	bool paused() const {
		return _paused.load(std::memory_order_acquire);
	}

	// Internal helpers (must be called with _mutex held)
	int unsafeEnqueue(AVFrame *frame, int frameDropTarget);
	void pushFrame(AVFrame *frame);
	AVFrame* popFrame();
	AVFrame* peekFrame();
	void dropFrame(AVFrame *frame);

	void enumerateFrames(
	    const std::function<void(AVFrame *frame, std::size_t idx, bool &stop)> &func) const;

	// Members

	mutable std::mutex _mutex;
	std::condition_variable _cv;

	std::vector<AVFrame*> _buffer;
	int _capacity;
	int _head;
	int _tail;
	int _count;
	bool _droppedLast;

	const int _maxCapacity;
	int _highWaterMark;

	std::atomic<bool> _paused;
};
