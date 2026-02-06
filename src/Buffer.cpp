#include <string.h>
#include <algorithm>
#include <vector>
#include "JWModules.hpp"

struct Buffer : Module {
	enum ParamIds {
		END_PARAM,
		START_PARAM,
		DRYWET_PARAM,
		FREEZE_TOGGLE_PARAM,
		WET_ON_FREEZE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		AUDIO_INPUT,
		FREEZE_INPUT,
		END_CV_INPUT,
		START_CV_INPUT,
		DRYWET_CV_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		AUDIO_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		FREEZE_LIGHT,
		NUM_LIGHTS
	};
	
	std::vector<float> delayBuffer;
	int bufferSize = 0;
	int writePos = 0;
	int readPos = 0;
	float delayTime = 0.5f;  // Current delay time in seconds
	int playbackDirection = 1;  // 1 for forward, -1 for backward
	float maxBufferSeconds = 1.0f;  // Maximum buffer size in seconds (default)

	// Smoothed dry/wet to avoid clicks on abrupt changes (e.g., on freeze)
	float dryWetOut = 0.5f;

	// Short envelope applied when freeze toggles to reduce pops
	float transitionEnv = 1.0f;
	int transitionFadeSamples = 0;
	int transitionFadeRemaining = 0;

	// DC blocker for wet signal to suppress clicks from DC steps
	float hp_a = 0.99f; // coefficient computed from sample rate
	float wetHP_y = 0.0f;
	float wetHP_prevX = 0.0f;

	// Generic de-click ramp for abrupt wet changes
	float prevWetOut = 0.0f;
	int declickTotalSamples = 0;
	int declickRemainingSamples = 0;
	float declickPrev = 0.0f;

	// Track direction changes for additional envelope
	int lastPlaybackDirection = 1;

	// Pending realignment (non-freeze) applied exactly at wrap boundary
	int pendingLoopStartNF = -1;
	int pendingLoopLengthNF = -1;

	// Freeze behavior: gate vs toggle
	enum FreezeMode { FM_GATE, FM_TOGGLE };
	FreezeMode freezeMode = FM_TOGGLE;
	dsp::SchmittTrigger freezeEdge;
	dsp::SchmittTrigger freezeButtonEdge;
	bool freezeLatched = false;
	// Cached loop when freeze engages (with zero-cross alignment)
	bool wasFrozen = false;
	int frozenLoopStart = 0;
	int frozenLoopLength = 0;
	int pendingFrozenLoopLength = -1;

	Buffer() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(END_PARAM, 0.001f, maxBufferSeconds, 0.5f, "Loop End", "s");
		configParam(START_PARAM, 0.0f, maxBufferSeconds, 0.0f, "Loop Start", "s");
        configParam(DRYWET_PARAM, 0.0f, 1.0f, 0.5f, "Dry/Wet");
		configButton(FREEZE_TOGGLE_PARAM, "Freeze Toggle");
		configParam(WET_ON_FREEZE_PARAM, 0.0f, 1.0f, 0.0f, "Wet on Freeze");
		configInput(AUDIO_INPUT, "Audio IN");
		configInput(FREEZE_INPUT, "Freeze Gate");
		configInput(END_CV_INPUT, "End CV");
		configInput(START_CV_INPUT, "Start CV");
		configInput(DRYWET_CV_INPUT, "Dry/Wet CV");
		configOutput(AUDIO_OUTPUT, "Audio OUT (Delayed)");
		configBypass(AUDIO_INPUT, AUDIO_OUTPUT);
		
		// Initialize buffer based on current sample rate
		allocateBuffer();
	}

	~Buffer() {
	}

	// Find the nearest zero crossing around index within +/- searchRadius
	int findNearestZeroCrossing(int index, int searchRadius) {
		int bestIdx = index;
		float bestAbs = fabsf(delayBuffer[index]);
		for (int d = 1; d <= searchRadius; ++d) {
			int i1 = (index + d) % bufferSize;
			int i0 = (i1 - 1 + bufferSize) % bufferSize;
			if (delayBuffer[i1] * delayBuffer[i0] <= 0.f) {
				float a = fabsf(delayBuffer[i1]);
				if (a < bestAbs) { bestAbs = a; bestIdx = i1; }
				break;
			}
			int j1 = (index - d + bufferSize) % bufferSize;
			int j0 = (j1 - 1 + bufferSize) % bufferSize;
			if (delayBuffer[j1] * delayBuffer[j0] <= 0.f) {
				float a = fabsf(delayBuffer[j1]);
				if (a < bestAbs) { bestAbs = a; bestIdx = j1; }
				break;
			}
		}
		return bestIdx;
	}

	void allocateBuffer() {
		float sampleRate = APP->engine->getSampleRate();
		bufferSize = (int)(sampleRate * maxBufferSeconds);
		delayBuffer.resize(bufferSize, 0.f);
		// Reset positions safely within new buffer size
		writePos = 0;
		delayTime = params[END_PARAM].getValue();
		int delaySamples = (int)(sampleRate * delayTime);
		readPos = (writePos - delaySamples + bufferSize) % bufferSize;

		// Compute high-pass coefficient for ~10 Hz cutoff
		float fc = 10.0f;
		hp_a = expf(-2.0f * (float)M_PI * fc / sampleRate);

		// Update parameter ranges to reflect new seconds max
		if (paramQuantities.size() > END_PARAM && paramQuantities[END_PARAM]) {
			paramQuantities[END_PARAM]->minValue = 0.001f;
			paramQuantities[END_PARAM]->maxValue = maxBufferSeconds;
		}
		if (paramQuantities.size() > START_PARAM && paramQuantities[START_PARAM]) {
			paramQuantities[START_PARAM]->minValue = 0.0f;
			paramQuantities[START_PARAM]->maxValue = maxBufferSeconds;
		}
	}

	void onRandomize() override {
	}

	void onReset() override {
		for(int i = 0; i < bufferSize; i++) {
			delayBuffer[i] = 0.f;
		}
		writePos = 0;
		delayTime = params[END_PARAM].getValue();
		float sampleRate = APP->engine->getSampleRate();
		int delaySamples = (int)(sampleRate * delayTime);
		readPos = (writePos - delaySamples + bufferSize) % bufferSize;
	}

	void onSampleRateChange() override {
		allocateBuffer();
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		json_object_set_new(rootJ, "maxBufferSeconds", json_real(maxBufferSeconds));
		json_object_set_new(rootJ, "freezeMode", json_integer((int)freezeMode));
		json_object_set_new(rootJ, "freezeLatched", json_integer(freezeLatched ? 1 : 0));
		json_object_set_new(rootJ, "wetOnFreeze", json_integer(params[WET_ON_FREEZE_PARAM].getValue() > 0.5f ? 1 : 0));
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		json_t *maxBufferSecondsJ = json_object_get(rootJ, "maxBufferSeconds");
		if (maxBufferSecondsJ) {
			maxBufferSeconds = json_real_value(maxBufferSecondsJ);
			allocateBuffer();
			// Keep current knobs within new bounds
			float endV = params[END_PARAM].getValue();
			float startV = params[START_PARAM].getValue();
			params[END_PARAM].setValue(clamp(endV, 0.001f, maxBufferSeconds));
			params[START_PARAM].setValue(clamp(startV, 0.0f, maxBufferSeconds));
		}
		if (json_t *fmJ = json_object_get(rootJ, "freezeMode")) {
			int m = json_integer_value(fmJ);
			freezeMode = (m == (int)FM_TOGGLE) ? FM_TOGGLE : FM_GATE;
		}
		if (json_t *flJ = json_object_get(rootJ, "freezeLatched")) {
			freezeLatched = json_integer_value(flJ) != 0;
		}
		if (json_t *wofJ = json_object_get(rootJ, "wetOnFreeze")) {
			params[WET_ON_FREEZE_PARAM].setValue(json_integer_value(wofJ) != 0 ? 1.0f : 0.0f);
		}
	}

	void process(const ProcessArgs &args) override {
		if (bufferSize == 0) return;

		// Defensive: ensure indices are in range if buffer size changed
		if (writePos < 0 || writePos >= bufferSize) {
			writePos = (bufferSize + (writePos % bufferSize)) % bufferSize;
		}
		if (readPos < 0 || readPos >= bufferSize) {
			readPos = (bufferSize + (readPos % bufferSize)) % bufferSize;
		}
		
		// Freeze behavior: gate or toggle on rising edge
		bool frozen = false;
		float freezeV = inputs[FREEZE_INPUT].getVoltage();
		// UI button toggles latched state in any mode
		if (freezeButtonEdge.process(params[FREEZE_TOGGLE_PARAM].getValue())) {
			freezeLatched = !freezeLatched;
		}
		if (freezeMode == FM_GATE) {
			// Gate mode: gate OR latched button
			frozen = (freezeV > 1.0f) || freezeLatched;
		}
		else {
			// Toggle mode: input acts as a trigger to toggle
			if (freezeEdge.process(freezeV)) {
				freezeLatched = !freezeLatched;
			}
			frozen = freezeLatched;
		}
		
		// Get input signal
		float input = inputs[AUDIO_INPUT].getVoltage();
		
		// Calculate loop parameters (knob + CV)
		float endTime = params[END_PARAM].getValue();
		float startTime = params[START_PARAM].getValue();
		
		// Add CV inputs (0-10V maps to 0-15s, so 1.5s per volt)
		if (inputs[END_CV_INPUT].isConnected()) {
			endTime += inputs[END_CV_INPUT].getVoltage() * 1.5f;
		}
		if (inputs[START_CV_INPUT].isConnected()) {
			startTime += inputs[START_CV_INPUT].getVoltage() * 1.5f;
		}
		
		// Clamp values to valid range
		endTime = clamp(endTime, 0.001f, maxBufferSeconds);
		startTime = clamp(startTime, 0.0f, maxBufferSeconds);
		
		int endSamples = (int)(args.sampleRate * endTime);
		int startSamples = (int)(args.sampleRate * startTime);
		
		// Determine playback direction
		if (startTime > endTime) {
			playbackDirection = -1;  // Backward
		} else {
			playbackDirection = 1;   // Forward
		}
		// If direction flips, apply quick transition envelope
		if (playbackDirection != lastPlaybackDirection) {
			transitionFadeSamples = std::max(1, (int)(args.sampleRate * 0.008f));
			transitionFadeRemaining = transitionFadeSamples;
			lastPlaybackDirection = playbackDirection;
		}
		
		// Calculate loop positions and length
		int loopStart, loopLength;//, loopEnd;
		if (playbackDirection == 1) {
			// Forward: from start to end
			loopStart = (writePos - startSamples - (endSamples - startSamples) + bufferSize * 3) % bufferSize;
			// loopEnd = (writePos - startSamples + bufferSize * 2) % bufferSize;
			loopLength = (endSamples - startSamples);
		} else {
			// Backward: from start to end (start is further back)
			loopStart = (writePos - startSamples + bufferSize * 2) % bufferSize;
			// loopEnd = (writePos - endSamples + bufferSize * 2) % bufferSize;
			loopLength = (startSamples - endSamples);
		}
		if (loopLength < 1) loopLength = 1;

		// On freeze engagement, align start to nearest zero crossing and cache
		if (frozen && !wasFrozen) {
			int searchRadius = std::min(bufferSize / 64, (int)(args.sampleRate * 0.012f)); // up to ~12ms
			if (searchRadius < 8) searchRadius = 8;
			// Align loop start
			frozenLoopStart = findNearestZeroCrossing(loopStart, searchRadius);
			// Compute and align loop end
			int rawEndIdx = (playbackDirection == 1)
				? (loopStart + loopLength) % bufferSize
				: (loopStart - loopLength + bufferSize) % bufferSize;
			int frozenLoopEnd = findNearestZeroCrossing(rawEndIdx, searchRadius);
			// Recompute length according to direction
			if (playbackDirection == 1) {
				frozenLoopLength = (frozenLoopEnd - frozenLoopStart + bufferSize) % bufferSize;
			}
			else {
				frozenLoopLength = (frozenLoopStart - frozenLoopEnd + bufferSize) % bufferSize;
			}
			if (frozenLoopLength < 1) frozenLoopLength = 1;
			// Reset read head to start of frozen loop to avoid boundary jump click;
			// fade-in at the start will smooth the transition.
			readPos = frozenLoopStart;
			// Start a short attack envelope (~10ms)
			transitionFadeSamples = std::max(1, (int)(args.sampleRate * 0.015f));
			transitionFadeRemaining = transitionFadeSamples;
			wasFrozen = true;
		}
		else if (!frozen && wasFrozen) {
			wasFrozen = false;
			// Also apply a short envelope when leaving freeze
			transitionFadeSamples = std::max(1, (int)(args.sampleRate * 0.012f));
			transitionFadeRemaining = transitionFadeSamples;
		}

		int activeLoopStart = frozen ? frozenLoopStart : loopStart;
		int activeLoopLength = frozen ? frozenLoopLength : loopLength;

		// Pre-compute candidate zero-crossing alignment when not frozen,
		// and apply it exactly at the wrap boundary to avoid mid-loop jumps.
		if (!frozen) {
			int searchRadius = std::min(bufferSize / 128, (int)(args.sampleRate * 0.006f)); // up to ~6ms
			if (searchRadius < 4) searchRadius = 4;
			int candidateStart = findNearestZeroCrossing(loopStart, searchRadius);
			int rawEndIdxNF = (playbackDirection == 1)
				? (loopStart + loopLength) % bufferSize
				: (loopStart - loopLength + bufferSize) % bufferSize;
			int candidateEnd = findNearestZeroCrossing(rawEndIdxNF, searchRadius);
			int candidateLen = (playbackDirection == 1)
				? (candidateEnd - candidateStart + bufferSize) % bufferSize
				: (candidateStart - candidateEnd + bufferSize) % bufferSize;
			if (candidateLen < 1) candidateLen = 1;
			pendingLoopStartNF = candidateStart;
			pendingLoopLengthNF = candidateLen;
		}

		// While frozen, allow changing loop length (and end) via knobs/CV
		// Keep the frozen start anchored, re-align the end to a nearby zero crossing.
		// Defer applying the new length until the loop wraps to start to avoid mid-loop boundary moves.
		if (frozen) {
			int searchRadius = std::min(bufferSize / 64, (int)(args.sampleRate * 0.012f));
			if (searchRadius < 8) searchRadius = 8;
			int desiredLen = loopLength; // based on current knobs
			if (desiredLen < 1) desiredLen = 1;
			int desiredEndIdx = (playbackDirection == 1)
				? (activeLoopStart + desiredLen) % bufferSize
				: (activeLoopStart - desiredLen + bufferSize) % bufferSize;
			int alignedEnd = findNearestZeroCrossing(desiredEndIdx, searchRadius);
			int newLen = (playbackDirection == 1)
				? (alignedEnd - activeLoopStart + bufferSize) % bufferSize
				: (activeLoopStart - alignedEnd + bufferSize) % bufferSize;
			if (newLen < 1) newLen = 1;
			pendingFrozenLoopLength = newLen; // apply on wrap
		}
		
		// (moved) write happens after wet is computed
		
		// Advance read position in the appropriate direction
		if (playbackDirection == 1) {
			readPos = (readPos + 1) % bufferSize;
		} else {
			readPos = (readPos - 1 + bufferSize) % bufferSize;
		}
		
		// Keep readPos within the loop boundaries
		if (playbackDirection == 1) {
			// Forward playback: check if we've passed the end
			int distanceFromStart = (readPos - activeLoopStart + bufferSize) % bufferSize;
			if (distanceFromStart >= activeLoopLength) {
				// Apply pending non-freeze alignment at wrap
				if (!frozen && pendingLoopStartNF >= 0 && pendingLoopLengthNF > 0) {
					activeLoopStart = pendingLoopStartNF;
					activeLoopLength = pendingLoopLengthNF;
					pendingLoopStartNF = -1;
					pendingLoopLengthNF = -1;
				}
				// If frozen and have a pending frozen length update, apply it now at wrap
				if (frozen && pendingFrozenLoopLength > 0) {
					frozenLoopLength = pendingFrozenLoopLength;
					activeLoopLength = frozenLoopLength;
					pendingFrozenLoopLength = -1;
					transitionFadeSamples = std::max(1, (int)(args.sampleRate * 0.015f));
					transitionFadeRemaining = transitionFadeSamples;
				}
				readPos = activeLoopStart;
			}
		} else {
			// Backward playback: check if we've passed the end (which comes before start in time)
			int distanceFromStart = (activeLoopStart - readPos + bufferSize) % bufferSize;
			if (distanceFromStart >= activeLoopLength) {
				// Apply pending alignment on wrap
				if (!frozen && pendingLoopStartNF >= 0 && pendingLoopLengthNF > 0) {
					activeLoopStart = pendingLoopStartNF;
					activeLoopLength = pendingLoopLengthNF;
					pendingLoopStartNF = -1;
					pendingLoopLengthNF = -1;
				}
				if (frozen && pendingFrozenLoopLength > 0) {
					frozenLoopLength = pendingFrozenLoopLength;
					activeLoopLength = frozenLoopLength;
					pendingFrozenLoopLength = -1;
					transitionFadeSamples = std::max(1, (int)(args.sampleRate * 0.015f));
					transitionFadeRemaining = transitionFadeSamples;
				}
				readPos = activeLoopStart;
			}
		}
		
		// Read from buffer with crossfade at loop point
		float wet = delayBuffer[readPos];

		// Adaptive fade length based on boundary jump magnitude
		int endPrev = (activeLoopStart + activeLoopLength - 1 + bufferSize) % bufferSize;
		int endPrev2 = (endPrev - 1 + bufferSize) % bufferSize;
		int endNext = activeLoopStart;
		int startNext = (endNext + 1) % bufferSize;
		// Consider amplitude and slope discontinuity across boundary
		float ampJump = fabsf(delayBuffer[endNext] - delayBuffer[endPrev]);
		float slopeEnd = delayBuffer[endPrev] - delayBuffer[endPrev2];
		float slopeStart = delayBuffer[startNext] - delayBuffer[endNext];
		float jumpMag = ampJump + 0.5f * fabsf(slopeEnd - slopeStart);
		float baseMs = frozen ? 35.0f : 12.0f;
		float addMs = std::min(45.0f, jumpMag * 12.0f); // up to +45ms if large discontinuity
		int fadeLength = (int)(args.sampleRate * (baseMs + addMs) / 1000.0f);
		int maxFadePercent = (activeLoopLength < args.sampleRate * 0.1f) ? 50 : 33;
		fadeLength = std::min(fadeLength, activeLoopLength * maxFadePercent / 100);
		if (fadeLength < 64) fadeLength = std::min(128, std::max(16, activeLoopLength / 4));

		// Apply crossfade near loop boundaries.
		// Use a small fade when not frozen to reduce clicks (especially in reverse),
		// and a larger adaptive fade when frozen.
		// Use longer equal-power crossfades when frozen to minimize clicks
		// (fadeLength computed adaptively above)

		if (playbackDirection == 1) {
			// Forward playback
			int distanceFromStart = (readPos - activeLoopStart + bufferSize) % bufferSize;
			int distanceFromEnd = activeLoopLength - distanceFromStart;
			// Crossfade at the end
			if (distanceFromEnd < fadeLength) {
				float t = (float)distanceFromEnd / (float)fadeLength; // 1 -> start of fade, 0 -> boundary
				int offset = fadeLength - distanceFromEnd;
				offset = std::max(0, std::min(fadeLength - 1, offset));
				int wrapReadPos = (activeLoopStart + offset + bufferSize) % bufferSize;
				// 3-tap low-pass around both current and wrap positions to soften high-frequency discontinuity
				int currPrev = (readPos - 1 + bufferSize) % bufferSize;
				int currNext = (readPos + 1) % bufferSize;
				int wrapPrev = (wrapReadPos - 1 + bufferSize) % bufferSize;
				int wrapNext = (wrapReadPos + 1) % bufferSize;
				float wetLP = 0.25f * delayBuffer[currPrev] + 0.5f * delayBuffer[readPos] + 0.25f * delayBuffer[currNext];
				float wrapLP = 0.25f * delayBuffer[wrapPrev] + 0.5f * delayBuffer[wrapReadPos] + 0.25f * delayBuffer[wrapNext];
				// Equal-power crossfade window
				float a = cosf(t * (float)M_PI * 0.5f);
				float b = sinf(t * (float)M_PI * 0.5f);
				wet = wetLP * a + wrapLP * b;
			}
			// Fade in at the start
			else if (distanceFromStart < fadeLength) {
				float t = (float)distanceFromStart / (float)fadeLength; // 0 -> boundary, 1 -> end of fade region
				float b = sinf(t * (float)M_PI * 0.5f);
				wet = wet * b;
			}
		}
		else {
			// Backward playback
			int distanceFromStart = (activeLoopStart - readPos + bufferSize) % bufferSize;
			int distanceFromEnd = activeLoopLength - distanceFromStart;
			// Crossfade at the end
			if (distanceFromEnd < fadeLength) {
				float t = (float)distanceFromEnd / (float)fadeLength;
				int offset = fadeLength - distanceFromEnd;
				offset = std::max(0, std::min(fadeLength - 1, offset));
				int wrapReadPos = (activeLoopStart - offset + bufferSize) % bufferSize;
				// 3-tap low-pass around both current and wrap positions
				int currPrev = (readPos - 1 + bufferSize) % bufferSize;
				int currNext = (readPos + 1) % bufferSize;
				int wrapPrev = (wrapReadPos - 1 + bufferSize) % bufferSize;
				int wrapNext = (wrapReadPos + 1) % bufferSize;
				float wetLP = 0.25f * delayBuffer[currPrev] + 0.5f * delayBuffer[readPos] + 0.25f * delayBuffer[currNext];
				float wrapLP = 0.25f * delayBuffer[wrapPrev] + 0.5f * delayBuffer[wrapReadPos] + 0.25f * delayBuffer[wrapNext];
				float a = cosf(t * (float)M_PI * 0.5f);
				float b = sinf(t * (float)M_PI * 0.5f);
				wet = wetLP * a + wrapLP * b;
			}
			// Fade in at the start
			else if (distanceFromStart < fadeLength) {
				float t = (float)distanceFromStart / (float)fadeLength;
				float b = sinf(t * (float)M_PI * 0.5f);
				wet = wet * b;
			}
		}

		// Apply transition envelope when toggling freeze (attack-only to avoid pops)
		if (transitionFadeRemaining > 0) {
			float prog = (float)(transitionFadeSamples - transitionFadeRemaining) / (float)transitionFadeSamples;
			transitionEnv = sinf(prog * (float)M_PI * 0.5f);
			transitionFadeRemaining--;
		}
		else {
			transitionEnv = 1.0f;
		}
		wet *= transitionEnv;

		// High-pass filter the wet signal to suppress DC step pops
		float hp_x = wet;
		wetHP_y = hp_a * (wetHP_y + hp_x - wetHP_prevX);
		wetHP_prevX = hp_x;
		wet = wetHP_y;

		// De-click smoothing: if wet jumps by a large amount, ramp over ~5–10ms
		float jumpThresh = 0.6f; // volts
		int rampSamples = std::max(1, (int)(args.sampleRate * 0.008f));
		float diffWet = fabsf(wet - prevWetOut);
		if (declickRemainingSamples <= 0 && diffWet > jumpThresh) {
			declickPrev = prevWetOut;
			declickTotalSamples = rampSamples;
			declickRemainingSamples = declickTotalSamples;
		}
		float wetSmoothed = wet;
		if (declickRemainingSamples > 0) {
			float prog = (float)(declickTotalSamples - declickRemainingSamples) / (float)declickTotalSamples;
			// Smooth ramp from previous output to current wet
			wetSmoothed = declickPrev + prog * (wet - declickPrev);
			declickRemainingSamples--;
		}
		prevWetOut = wetSmoothed;
		wet = wetSmoothed;
		
		// Apply Dry/Wet mix (0-1), with CV (0-10V adds 0..1), smoothed to avoid clicks
		float targetDW = params[DRYWET_PARAM].getValue();
		if (inputs[DRYWET_CV_INPUT].isConnected()) {
			float cv = inputs[DRYWET_CV_INPUT].getVoltage();
			targetDW = clamp(targetDW + cv / 10.f, 0.f, 1.f);
		}
		// Optional: force full wet when frozen and switch enabled
		if (frozen && params[WET_ON_FREEZE_PARAM].getValue() > 0.5f) {
			targetDW = 1.0f;
		}
		// Slew dry/wet over ~10ms to reduce pops
		float dwStep = 1.0f / std::max(1.0f, args.sampleRate * 0.010f);
		float diffDW = targetDW - dryWetOut;
		if (diffDW > dwStep) dryWetOut += dwStep;
		else if (diffDW < -dwStep) dryWetOut -= dwStep;
		else dryWetOut = targetDW;

		// Use equal-power crossfade to keep perceived loudness uniform
		float t = dryWetOut;
		float a = cosf(t * (float)M_PI * 0.5f); // dry gain
		float b = sinf(t * (float)M_PI * 0.5f); // wet gain
		float mixed = input * a + wet * b;

		// Now write to buffer (no feedback applied)
		if (!frozen) {
			float writeSample = input;
			// Prevent runaway: soft limit to avoid clipping explosions
			if (writeSample > 10.f) writeSample = 10.f;
			else if (writeSample < -10.f) writeSample = -10.f;
			delayBuffer[writePos] = writeSample;
			writePos = (writePos + 1) % bufferSize;
		}
		else {
			// Frozen: do not write to the buffer at all
		}
		outputs[AUDIO_OUTPUT].setVoltage(mixed);
		// Update LED to reflect frozen state
		lights[FREEZE_LIGHT].setBrightness(frozen ? 1.0f : 0.0f);
	}

};

struct BufferWidget : ModuleWidget { 
	BufferWidget(Buffer *module); 
	void appendContextMenu(Menu *menu) override;
};

struct BufferSizeValueItem : MenuItem {
	Buffer *module;
	float seconds;
	void onAction(const event::Action &e) override {
		// Save current relative positions (0.0 to 1.0)
		float oldMax = module->maxBufferSeconds;
		float endRatio = module->params[Buffer::END_PARAM].getValue() / oldMax;
		float startRatio = module->params[Buffer::START_PARAM].getValue() / oldMax;
		
		// Update buffer size
		module->maxBufferSeconds = seconds;
		module->allocateBuffer();
		
		// Update parameter ranges
		module->paramQuantities[Buffer::END_PARAM]->maxValue = seconds;
		module->paramQuantities[Buffer::START_PARAM]->maxValue = seconds;
		
		// Set parameters to same relative positions
		module->params[Buffer::END_PARAM].setValue(endRatio * seconds);
		module->params[Buffer::START_PARAM].setValue(startRatio * seconds);
	}
};

struct BufferSizeItem : MenuItem {
	Buffer *module;
	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		float sizes[] = {0.5f, 1.0f, 2.0f, 5.0f, 10.0f, 15.0f, 30.0f};
		for (float size : sizes) {
			BufferSizeValueItem *item = new BufferSizeValueItem;
			// Label formatting: show ms for sub-second, otherwise seconds
			if (size < 1.0f) {
				item->text = string::f("%.0f ms", size * 1000.0f);
			}
			else if (size == 2.0f) {
				item->text = "2 seconds";
			}
			else {
				item->text = string::f("%.0f seconds", size);
			}
			item->rightText = CHECKMARK(module->maxBufferSeconds == size);
			item->module = module;
			item->seconds = size;
			menu->addChild(item);
		}
		return menu;
	}
};

BufferWidget::BufferWidget(Buffer *module) {
	setModule(module);
	box.size = Vec(RACK_GRID_WIDTH*3, RACK_GRID_HEIGHT);

	setPanel(createPanel(
		asset::plugin(pluginInstance, "res/Buffer.svg"), 
		asset::plugin(pluginInstance, "res/Buffer.svg")
	));

	addChild(createWidget<Screw_J>(Vec(16, 2)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 365)));

	addParam(createParam<SmallWhiteKnob>(Vec(9, 46), module, Buffer::START_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(14, 73), module, Buffer::START_CV_INPUT));

	addParam(createParam<SmallWhiteKnob>(Vec(9, 104), module, Buffer::END_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(14, 131), module, Buffer::END_CV_INPUT));
	
	addChild(createLight<SmallLight<BlueLight>>(Vec(18, 168), module, Buffer::FREEZE_LIGHT));
	addInput(createInput<TinyPJ301MPort>(Vec(5, 179), module, Buffer::FREEZE_INPUT));
	addParam(createParam<TinyButton>(Vec(25, 179), module, Buffer::FREEZE_TOGGLE_PARAM));
	// Switch to force full wet while frozen
	addParam(createParam<JwHorizontalSwitch>(Vec(13, 220), module, Buffer::WET_ON_FREEZE_PARAM));
	
	
	addParam(createParam<SmallWhiteKnob>(Vec(9, 255), module, Buffer::DRYWET_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(14, 282), module, Buffer::DRYWET_CV_INPUT));

	addInput(createInput<TinyPJ301MPort>(Vec(5, 320), module, Buffer::AUDIO_INPUT));
	addOutput(createOutput<TinyPJ301MPort>(Vec(25, 320), module, Buffer::AUDIO_OUTPUT));

}

void BufferWidget::appendContextMenu(Menu *menu) {
	Buffer *buffer = dynamic_cast<Buffer*>(module);
	MenuLabel *spacerLabel = new MenuLabel();
	menu->addChild(spacerLabel);

	BufferSizeItem *sizeItem = new BufferSizeItem;
	sizeItem->text = "Buffer Size";
	sizeItem->rightText = string::f("%.0fs", buffer->maxBufferSeconds) + " " + RIGHT_ARROW;
	sizeItem->module = buffer;
	menu->addChild(sizeItem);

	// Freeze controls
	MenuLabel *freezeLabel = new MenuLabel();
	freezeLabel->text = "Freeze";
	menu->addChild(freezeLabel);

	struct FreezeModeValueItem : MenuItem {
		Buffer *module;
		Buffer::FreezeMode mode;
		void onAction(const event::Action &e) override {
			module->freezeMode = mode;
		}
		void step() override {
			rightText = CHECKMARK(module->freezeMode == mode);
			MenuItem::step();
		}
	};

	struct FreezeModeItem : MenuItem {
		Buffer *module;
		Menu *createChildMenu() override {
			Menu *menu = new Menu;
			FreezeModeValueItem *gateItem = new FreezeModeValueItem;
			gateItem->text = "Gate";
			gateItem->module = module;
			gateItem->mode = Buffer::FM_GATE;
			menu->addChild(gateItem);

			FreezeModeValueItem *toggleItem = new FreezeModeValueItem;
			toggleItem->text = "Toggle on trigger";
			toggleItem->module = module;
			toggleItem->mode = Buffer::FM_TOGGLE;
			menu->addChild(toggleItem);
			return menu;
		}
	};

	FreezeModeItem *fmItem = new FreezeModeItem;
	fmItem->text = "Freeze mode";
	fmItem->rightText = RIGHT_ARROW;
	fmItem->module = buffer;
	menu->addChild(fmItem);

	// UI provides a dedicated freeze toggle button and LED; no menu toggle needed.
}

Model *modelBuffer = createModel<Buffer, BufferWidget>("Buffer");
