#pragma once

#include <atomic>
#include <cstdint>

// This class tracks the current display refresh rate and the incoming stream framerate
// and instructs the render thread whether it should display a new frame or the previous frame.
//
// This class is thread-safe when used as documented, atomics are used for performance.

class FrameCadence {
  public:
	explicit FrameCadence();

	// Call once before use, set initial state
	void init(double hz, double fps);

	// Called by Pacer getNextVblankQpc
	// Publishes the latest display period. No locks.
	void setDisplayHz(double hz);

	// Called by decoder thread when a decoded frame arrives.
	// Updates EWMA of stream period and publishes it. No locks.
	void observeFramePts(int64_t pts90k);

	// Decide how many stream frames should be consumed for this present interval (main thread).
	//
	// Return value:
	//   0  -> reuse the current displayed frame
	//   1  -> advance to the next frame
	//   2+ -> drop some incoming frames (e.g. 120 fps stream on 60 Hz display)
	int decideAdvanceCount();

	// Lock-free accessors.
	double displayHz() const;
	double displayPeriodMs() const;
	double streamPeriodMs() const;
	double streamFps() const;

  private:
	// Decoder-thread owned state.
	int64_t m_lastPts90k = 0;
	bool m_haveLastPts = false;

	double m_streamPeriodEwmaMs = 0.0;
	double m_streamEwmaAlpha = 0.10;

	double m_minStreamPeriodMs = 1000.0 / 240.0;
	double m_maxStreamPeriodMs = 1000.0 / 1.0;

	// Main-thread owned state.
	double m_phase = 0.0;
	int m_maxAdvancePerPresent = 2;

	// Published values (written by pacer or decoder, read by main).
	std::atomic<double> m_displayPeriodMs;
	std::atomic<double> m_streamPeriodMs;
};
