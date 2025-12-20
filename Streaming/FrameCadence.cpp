#include "pch.h"
#include "FrameCadence.h"

#include <algorithm> // std::clamp

FrameCadence::FrameCadence()
    : m_displayPeriodMs(1000.0 / 59.94),
      m_streamPeriodMs(1000.0 / 59.94) {
}

void FrameCadence::init(double hz, double fps) {
	// Initialize the display period from the provided refresh rate.
	setDisplayHz(hz);

	if (fps <= 0.0) {
		fps = 60.0;
	}

	double periodMs = 1000.0 / fps;
	periodMs = std::clamp(periodMs, m_minStreamPeriodMs, m_maxStreamPeriodMs);

	m_streamPeriodEwmaMs = periodMs;
	m_lastPts90k = 0;
	m_haveLastPts = false;

	// Initialize the stream period.
	m_streamPeriodMs.store(periodMs, std::memory_order_release);

	// Start cadence phase at zero.
	m_phase = 0.0;
}

// Called by Pacer getNextVBlankQpc (main thread)
void FrameCadence::setDisplayHz(double hz) {
	if (hz <= 0.0) {
		hz = 59.94;
	}

	const double periodMs = 1000.0 / hz;
	m_displayPeriodMs.store(periodMs, std::memory_order_release);
}

// Decoder thread only
void FrameCadence::observeFramePts(int64_t pts90k) {
	if (pts90k == INT64_MIN) {
		return;
	}

	if (m_haveLastPts) {
		const int64_t deltaPts = pts90k - m_lastPts90k;
		if (deltaPts > 0) {
			double deltaMs = static_cast<double>(deltaPts) / 90.0;
			deltaMs = std::clamp(deltaMs, m_minStreamPeriodMs, m_maxStreamPeriodMs);

			// EWMA smoothing of stream period.
			m_streamPeriodEwmaMs =
			    deltaMs * m_streamEwmaAlpha +
			    m_streamPeriodEwmaMs * (1.0 - m_streamEwmaAlpha);

			// Publish the updated stream period.
			m_streamPeriodMs.store(m_streamPeriodEwmaMs, std::memory_order_release);
		}
	}

	m_lastPts90k = pts90k;
	m_haveLastPts = true;
}

int FrameCadence::decideAdvanceCount() {
	// Main thread only.
	// Uses a fractional phase accumulator to decide how many frames to advance.

	double streamMs = m_streamPeriodMs.load(std::memory_order_acquire);
	double displayMs = m_displayPeriodMs.load(std::memory_order_acquire);

	// Defensive fallbacks.
	if (streamMs <= 0.0) {
		streamMs = 1000.0 / 60.0;
	}
	if (displayMs <= 0.0) {
		displayMs = 1000.0 / 60.0;
	}

	// Stream frames per present interval.
	const double framesPerPresent = displayMs / streamMs;

	// Accumulate fractional frames owed.
	m_phase += framesPerPresent;

	// Consume whole frames, clamped to the maximum allowed per present.
	int advanceCount = static_cast<int>(m_phase);
	if (advanceCount < 0) {
		advanceCount = 0;
	} else if (advanceCount > m_maxAdvancePerPresent) {
		advanceCount = m_maxAdvancePerPresent;
	}

	m_phase -= advanceCount;

	// Keep phase bounded and numerically stable.
	if (m_phase < 0.0) {
		m_phase = 0.0;
	} else if (m_phase >= 1.0) {
		m_phase -= static_cast<int>(m_phase);
	}

	return advanceCount;
}

// Accessors

double FrameCadence::displayHz() const {
	const double periodMs = m_displayPeriodMs.load(std::memory_order_acquire);
	return (periodMs > 0.0) ? (1000.0 / periodMs) : 0.0;
}

double FrameCadence::displayPeriodMs() const {
	return m_displayPeriodMs.load(std::memory_order_acquire);
}

double FrameCadence::streamPeriodMs() const {
	return m_streamPeriodMs.load(std::memory_order_acquire);
}

double FrameCadence::streamFps() const {
	const double periodMs = m_streamPeriodMs.load(std::memory_order_acquire);
	return (periodMs > 0.0) ? (1000.0 / periodMs) : 0.0;
}
