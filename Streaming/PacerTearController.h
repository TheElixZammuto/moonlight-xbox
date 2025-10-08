#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

class EmaBiasCorrector {
  public:
	explicit EmaBiasCorrector(double halfLifeSamples = 60.0,
	                          double targetEarlyMs = 0.05,
	                          double maxPerFrameStepMs = 0.20,
	                          double clampInputMs = 1.0)
	    : targetEarlyMs_(targetEarlyMs),
	      maxPerFrameStepMs_(std::max(0.0, maxPerFrameStepMs)),
	      clampInputMs_(std::max(0.0, clampInputMs)) {
		setHalfLife(halfLifeSamples);
	}

	void setHalfLife(double halfLifeSamples) {
		if (halfLifeSamples <= 1.0)
			alpha_ = 1.0;
		else
			alpha_ = 1.0 - std::pow(2.0, -1.0 / halfLifeSamples);
	}

	void addSample(double driftMs) {
		const double x = std::clamp(driftMs, -clampInputMs_, clampInputMs_);
		if (!has_) {
			ema_ = x;
			has_ = true;
		} else {
			ema_ += alpha_ * (x - ema_);
		}
		const double raw = ema_ - targetEarlyMs_;
		correctionMs_ = std::clamp(raw, -maxPerFrameStepMs_, +maxPerFrameStepMs_);
	}

	double correctionMs() const {
		return has_ ? correctionMs_ : 0.0;
	}
	double emaMs() const {
		return has_ ? ema_ : 0.0;
	}

  private:
	double alpha_ = 0.1;
	double ema_ = 0.0;
	double correctionMs_ = 0.0;
	double targetEarlyMs_ = 0.05;
	double maxPerFrameStepMs_ = 0.25;
	double clampInputMs_ = 2.0;
	bool has_ = false;
};

class TearController {
  public:
	explicit TearController(double desiredTearFraction = 0.0,
	                        double vblankPeriodMs = 0.0,
	                        double safetyOffsetMs = 0.02)
	    : ready_(false),
		  desiredTearFraction_(desiredTearFraction),
	      vblankPeriodMs_(vblankPeriodMs),
	      safetyOffsetMs_(std::max(0.0, safetyOffsetMs)),
	      qpcPerMs_(UsToQpc(1000)) {
	}

	void initializeTiming(double vblankPeriodMs, double safetyOffsetMs) {
		vblankPeriodMs_ = vblankPeriodMs;
		safetyOffsetMs_ = safetyOffsetMs;
		ready_ = true;
	}

	void setTearOffset(double f) {
		desiredTearFraction_.store(std::clamp(f, 0.0, 1.0), std::memory_order_release);
	}

	double getTearOffset() const {
		return desiredTearFraction_.load(std::memory_order_acquire);
	}

	int64_t targetPresentQpc(int64_t nextVblankQpc) const {
		if (!ready_) {
			throw std::runtime_error("TearController is not initialized");
		}
		const double offsetMs = desiredTearFraction_.load(std::memory_order_acquire) * vblankPeriodMs_ + safetyOffsetMs_ + ema_.correctionMs();
		const double clamped = std::clamp(offsetMs, 0.0, std::max(0.0, vblankPeriodMs_ - 0.10)); // make sure we don't push into next vblank
		return nextVblankQpc + (int64_t)std::llround(clamped * qpcPerMs_);
	}

	void addDriftSample(double driftMs) {
		ema_.addSample(driftMs);
	}

  private:
    bool ready_;
	std::atomic<double> desiredTearFraction_;
	double vblankPeriodMs_;
	double safetyOffsetMs_;
	int64_t qpcPerMs_;
	EmaBiasCorrector ema_;
};
