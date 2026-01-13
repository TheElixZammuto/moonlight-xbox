// clang-format off
#include "pch.h"
// clang-format on
#include "FrameQueue.h"
#include "Utils.hpp"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <thread>

using namespace moonlight_xbox_dx;

static inline bool isFrameIDR(AVFrame *frame) {
	return frame->pict_type == AV_PICTURE_TYPE_I;
}

FrameQueue &FrameQueue::instance() {
	static FrameQueue inst;
	return inst;
}

FrameQueue::FrameQueue()
    : _capacity(0),
      _head(0),
      _tail(0),
      _count(0),
      _droppedLast(false),
      _maxCapacity(5), // should not exceed swapchain BufferCount
      _highWaterMark(3),
      _paused(true) {    // caller will call start()

	_capacity = _maxCapacity;
	_buffer.assign(_capacity, nullptr);
	_head = _tail = _count = 0;
}

void FrameQueue::setPaused(bool p) {
	_paused.store(p, std::memory_order_release);
	if (p) {
		// Wake any waiters so they can exit
		_cv.notify_all();
	}
}

void FrameQueue::start() {
	setPaused(false);
	Utils::Logf("FrameQueue started\n");
}

void FrameQueue::stop() {
	setPaused(true);
	clear();
	Utils::Logf("FrameQueue stopped\n");
}

std::size_t FrameQueue::count() const {
	std::lock_guard<std::mutex> lock(_mutex);
	return static_cast<std::size_t>(_count);
}

bool FrameQueue::isEmpty() const {
	return count() == 0;
}

void FrameQueue::clear() {
	std::lock_guard<std::mutex> lock(_mutex);
	// free all AVFrame in the queue
	while (_count > 0) {
		AVFrame *frame = popFrame();
		dropFrame(frame);
	}
	_head = _tail = _count = 0;
	std::fill(_buffer.begin(), _buffer.end(), nullptr);
}

void FrameQueue::setHighWaterMark(int hwm) {
	std::lock_guard<std::mutex> lock(_mutex);
	_highWaterMark = hwm;
}

int FrameQueue::highWaterMark() const {
	std::lock_guard<std::mutex> lock(_mutex);
	return _highWaterMark;
}

// Push into buffer at _tail (requires _mutex held)
void FrameQueue::pushFrame(AVFrame *frame) {
	_buffer[_tail] = frame;
	_tail = (_tail + 1) % _capacity;
	_count++;

	// Wake waiting consumer
	_cv.notify_one();

	FQLog("[-> %s pts: %.3fms] enqueue frame, queue size %d/%d\n",
		isFrameIDR(frame) ? "IDR" : "P",
		frame->pts / 90.0, _count, _highWaterMark);
}

// Pop oldest frame from _head (requires _mutex held)
AVFrame* FrameQueue::popFrame() {
	if (_count == 0) {
		return nullptr;
	}

	AVFrame *frame = _buffer[_head];
	_buffer[_head] = nullptr;
	_head = (_head + 1) % _capacity;
	_count--;
	return frame;
}

// Peek next frame (requires _mutex held)
AVFrame* FrameQueue::peekFrame() {
	if (_count == 0) {
		return nullptr;
	}
	return _buffer[_head];
}

void FrameQueue::enumerateFrames(const std::function<void(AVFrame *frame, std::size_t idx, bool &stop)> &func) const {
	std::lock_guard<std::mutex> lock(_mutex);
	bool stop = false;
	for (int i = 0; i < _count; ++i) {
		int idx = (_head + i) % _capacity;
		AVFrame *f = _buffer[idx];
		func(f, static_cast<std::size_t>(i), stop);
		if (stop) {
			break;
		}
	}
}

void FrameQueue::dropFrame(AVFrame *frame) {
	if (frame) {
		FQLog("! dropped frame [pts: %.3fms]\n", frame->pts / 90.0);
		av_frame_free(&frame);
	}
}

int FrameQueue::unsafeEnqueue(AVFrame *frame, int frameDropTarget) {
	int dropCount = 0;

	// Always accept IDR frames, allow exceeding HWM
	if (isFrameIDR(frame) || _count < frameDropTarget) {
		// in the unlikely event of a full queue, blindly drop the oldest
		// we don't care if it's an IDR because we just got a new one
		if (_count == _capacity) {
			AVFrame *oldest = popFrame();
			if (oldest) {
				dropFrame(oldest);
				dropCount = 1;
			}
		}
		pushFrame(frame);
		_droppedLast = false;
	} else {
		if (!_droppedLast) {
			// alternate between: dropping newest...
			dropFrame(frame);
			dropCount = 1;
			_droppedLast = true;
		} else {
			// and: dropping oldest & enqueue new
			AVFrame *oldest = popFrame();
			if (oldest) {
				dropFrame(oldest);
				dropCount = 1;
			}
			pushFrame(frame);
			_droppedLast = false;
		}
	}
	return dropCount;
}

// Enqueue with simple alternate-drop logic
int FrameQueue::enqueue(AVFrame *frame) {
	std::lock_guard<std::mutex> lock(_mutex);
	return unsafeEnqueue(frame, _highWaterMark);
}

// Allows the render loop to wait if the queue is empty
// Optional param: wait until the queue contains N items
// Optional timeout in milliseconds
void FrameQueue::waitForEnqueue(int num) {
	std::unique_lock<std::mutex> lock(_mutex);
	while (!_paused.load(std::memory_order_acquire) && _count < num) {
		// This waits forever until a frame arrives
		_cv.wait(lock);
	}
}

void FrameQueue::waitForEnqueue(int num, double timeoutMs) {
	std::unique_lock<std::mutex> lock(_mutex);
	auto deadline = std::chrono::steady_clock::now() +
	                std::chrono::microseconds(static_cast<long long>(timeoutMs * 1000.0));

	while (_count < num && !_paused.load(std::memory_order_acquire)) {
		auto now = std::chrono::steady_clock::now();
		if (now >= deadline) break;

		// wait_for overshoots a lot, so wait in small chunks
		auto remaining = std::chrono::duration_cast<std::chrono::microseconds>(deadline - now);
		auto chunk = remaining / 4;
		if (chunk > std::chrono::microseconds(1000)) chunk = std::chrono::microseconds(1000);
		if (chunk < std::chrono::microseconds(250))  chunk = std::chrono::microseconds(250);

		_cv.wait_for(lock, chunk);
	}
}

AVFrame* FrameQueue::dequeue() {
	std::lock_guard<std::mutex> lock(_mutex);
	AVFrame *frame = nullptr;

	if (_count > 0) {
		frame = popFrame();
		FQLog("[<- pts: %.3fms] dequeue frame, queue size %d/%d\n",
			frame->pts / 90.0, _count, _highWaterMark);
	}
	return frame;
}

AVFrame* FrameQueue::dequeueWithTimeout(double timeoutSeconds) {
	if (paused()) {
		return nullptr;
	}

	const int64_t startQpc = QpcNow();
	const int64_t deadlineQpc = startQpc + MsToQpc(timeoutSeconds * 1000.0);

	int round = 0;

	// Always attempt to dequeue at least once
	do {
        if (round > 0) {
            std::this_thread::sleep_for(std::chrono::microseconds(100)); // 0.1 ms
        }
        AVFrame *frame = dequeue();
        if (frame) {
            return frame;
        }
        round++;
    } while (QpcNow() < deadlineQpc);

	FQLog("dequeueWithTimeout timed out after %.3fms\n", QpcToMs(QpcNow() - startQpc));
	return nullptr;
}
