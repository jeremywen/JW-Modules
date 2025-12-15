#include <string.h>
#include <algorithm>
#include <vector>
#include "JWModules.hpp"

struct Buffer : Module {
	enum ParamIds {
		END_PARAM,
		START_PARAM,
		DRYWET_PARAM,
		FEEDBACK_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		AUDIO_INPUT,
		FREEZE_INPUT,
		END_CV_INPUT,
		START_CV_INPUT,
		DRYWET_CV_INPUT,
		FEEDBACK_CV_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		AUDIO_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};
	
	std::vector<float> delayBuffer;
	int bufferSize = 0;
	int writePos = 0;
	int readPos = 0;
	float delayTime = 0.5f;  // Current delay time in seconds
	int playbackDirection = 1;  // 1 for forward, -1 for backward
	float maxBufferSeconds = 15.0f;  // Maximum buffer size in seconds

	Buffer() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(END_PARAM, 0.001f, maxBufferSeconds, 0.5f, "Loop End", "s");
		configParam(START_PARAM, 0.0f, maxBufferSeconds, 0.0f, "Loop Start", "s");
        configParam(DRYWET_PARAM, 0.0f, 1.0f, 0.5f, "Dry/Wet");
        configParam(FEEDBACK_PARAM, 0.0f, 1.0f, 0.3f, "Feedback");
		configInput(AUDIO_INPUT, "Audio IN");
		configInput(FREEZE_INPUT, "Freeze Gate");
		configInput(END_CV_INPUT, "End CV");
		configInput(START_CV_INPUT, "Start CV");
		configInput(DRYWET_CV_INPUT, "Dry/Wet CV");
		configInput(FEEDBACK_CV_INPUT, "Feedback CV");
		configOutput(AUDIO_OUTPUT, "Audio OUT (Delayed)");
		configBypass(AUDIO_INPUT, AUDIO_OUTPUT);
		
		// Initialize buffer based on current sample rate
		allocateBuffer();
	}

	~Buffer() {
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
		
		// Check if freeze gate is high
		bool frozen = inputs[FREEZE_INPUT].getVoltage() > 1.0f;
		
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
			int distanceFromStart = (readPos - loopStart + bufferSize) % bufferSize;
			if (distanceFromStart >= loopLength) {
				readPos = loopStart;
			}
		} else {
			// Backward playback: check if we've passed the end (which comes before start in time)
			int distanceFromStart = (loopStart - readPos + bufferSize) % bufferSize;
			if (distanceFromStart >= loopLength) {
				readPos = loopStart;
			}
		}
		
		// Read from buffer with crossfade at loop point
		float wet = delayBuffer[readPos];

		// Apply crossfade near loop boundaries.
		// Use a small fade when not frozen to reduce clicks (especially in reverse),
		// and a larger adaptive fade when frozen.
		int fadeLength = (int)(args.sampleRate * (frozen ? 0.010f : 0.005f)); // 10ms frozen, 5ms normal
		int maxFadePercent = (loopLength < args.sampleRate * 0.1f) ? 40 : 25;
		fadeLength = std::min(fadeLength, loopLength * maxFadePercent / 100);
		if (fadeLength < 16) fadeLength = std::min(16, std::max(1, loopLength / 4));

		if (playbackDirection == 1) {
			// Forward playback
			int distanceFromStart = (readPos - loopStart + bufferSize) % bufferSize;
			int distanceFromEnd = loopLength - distanceFromStart;
			// Crossfade at the end
			if (distanceFromEnd < fadeLength) {
				float fadePos = (float)distanceFromEnd / (float)fadeLength;
				int wrapReadPos = (loopStart + (distanceFromEnd - fadeLength) + bufferSize) % bufferSize;
				float wrapSample = delayBuffer[wrapReadPos];
				wet = wet * fadePos + wrapSample * (1.0f - fadePos);
			}
			// Fade in at the start
			else if (distanceFromStart < fadeLength) {
				float fadePos = (float)distanceFromStart / (float)fadeLength;
				wet = wet * fadePos;
			}
		}
		else {
			// Backward playback
			int distanceFromStart = (loopStart - readPos + bufferSize) % bufferSize;
			int distanceFromEnd = loopLength - distanceFromStart;
			// Crossfade at the end
			if (distanceFromEnd < fadeLength) {
				float fadePos = (float)distanceFromEnd / (float)fadeLength;
				int wrapReadPos = (loopStart - (distanceFromEnd - fadeLength) + bufferSize) % bufferSize;
				float wrapSample = delayBuffer[wrapReadPos];
				wet = wet * fadePos + wrapSample * (1.0f - fadePos);
			}
			// Fade in at the start
			else if (distanceFromStart < fadeLength) {
				float fadePos = (float)distanceFromStart / (float)fadeLength;
				wet = wet * fadePos;
			}
		}
		
		// Apply Dry/Wet mix (0-1), with CV (0-10V adds 0..1)
		float dryWet = params[DRYWET_PARAM].getValue();
		if (inputs[DRYWET_CV_INPUT].isConnected()) {
			float cv = inputs[DRYWET_CV_INPUT].getVoltage();
			dryWet = clamp(dryWet + cv / 10.f, 0.f, 1.f);
		}
		// Use equal-power crossfade to keep perceived loudness uniform
		float t = dryWet;
		float a = cosf(t * (float)M_PI * 0.5f); // dry gain
		float b = sinf(t * (float)M_PI * 0.5f); // wet gain
		float mixed = input * a + wet * b;

		// Now write to buffer
		if (!frozen) {
			float fb = params[FEEDBACK_PARAM].getValue();
			if (inputs[FEEDBACK_CV_INPUT].isConnected()) {
				float fcv = inputs[FEEDBACK_CV_INPUT].getVoltage();
				fb = clamp(fb + fcv / 10.f, 0.f, 1.f);
			}
			float writeSample = input + wet * fb;
			// Prevent runaway: soft limit to avoid clipping explosions
			if (writeSample > 10.f) writeSample = 10.f;
			else if (writeSample < -10.f) writeSample = -10.f;
			delayBuffer[writePos] = writeSample;
			writePos = (writePos + 1) % bufferSize;
		}
		else {
			// Include feedback while frozen by writing into the loop region along the read path.
			// This recirculates the loop content and applies decay based on feedback.
			float fb = params[FEEDBACK_PARAM].getValue();
			if (inputs[FEEDBACK_CV_INPUT].isConnected()) {
				float fcv = inputs[FEEDBACK_CV_INPUT].getVoltage();
				fb = clamp(fb + fcv / 10.f, 0.f, 1.f);
			}
			// Determine the next index along the playback direction within the loop
			int writeIndex = (playbackDirection == 1)
				? (readPos + 1) % bufferSize
				: (readPos - 1 + bufferSize) % bufferSize;
			// Blend existing content with feedback from the current wet sample
			float writeSample = delayBuffer[writeIndex] * (1.f - fb) + wet * fb;
			if (writeSample > 10.f) writeSample = 10.f;
			else if (writeSample < -10.f) writeSample = -10.f;
			delayBuffer[writeIndex] = writeSample;
		}
		outputs[AUDIO_OUTPUT].setVoltage(mixed);
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
		asset::plugin(pluginInstance, "res/dark/Buffer.svg")
	));

	addChild(createWidget<Screw_J>(Vec(16, 2)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 365)));

	addParam(createParam<SmallWhiteKnob>(Vec(7, 40), module, Buffer::END_PARAM));
	addParam(createParam<SmallWhiteKnob>(Vec(7, 75), module, Buffer::START_PARAM));
	addParam(createParam<SmallWhiteKnob>(Vec(7, 110), module, Buffer::DRYWET_PARAM));
	addParam(createParam<SmallWhiteKnob>(Vec(7, 145), module, Buffer::FEEDBACK_PARAM));

	addInput(createInput<TinyPJ301MPort>(Vec(15, 210), module, Buffer::FREEZE_INPUT));
	addInput(createInput<TinyPJ301MPort>(Vec(5, 235), module, Buffer::START_CV_INPUT));
	addInput(createInput<TinyPJ301MPort>(Vec(25, 235), module, Buffer::END_CV_INPUT));
	addInput(createInput<TinyPJ301MPort>(Vec(15, 260), module, Buffer::DRYWET_CV_INPUT));
	addInput(createInput<TinyPJ301MPort>(Vec(15, 285), module, Buffer::FEEDBACK_CV_INPUT));
	addInput(createInput<TinyPJ301MPort>(Vec(5, 310), module, Buffer::AUDIO_INPUT));
	addOutput(createOutput<TinyPJ301MPort>(Vec(25, 310), module, Buffer::AUDIO_OUTPUT));

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
}

Model *modelBuffer = createModel<Buffer, BufferWidget>("Buffer");
