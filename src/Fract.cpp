#include "JWModules.hpp"

struct Fract : Module {
	static constexpr float MAX_DELAY_SECONDS = 10.f;
	static constexpr int MAX_POLY_CHANNELS = 16;

	enum ParamIds {
		FRACTION_PARAM,
		OFFSET_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		DELAY_INPUT,
		CLOCK_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		DELAYED_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	dsp::SchmittTrigger clockTrigger;

	float clockElapsedSec = 0.f;
	float beatIntervalSec = 0.5f;
	bool hasSeenClock = false;
	float sampleRate = 44100.f;
	std::vector<std::vector<float>> delayBuffers;
	int writeIndex = 0;

	void resizeDelayBuffer() {
		int bufferSize = std::max(1, (int)std::ceil(sampleRate * MAX_DELAY_SECONDS) + 1);
		delayBuffers.assign(MAX_POLY_CHANNELS, std::vector<float>(bufferSize, 0.f));
		writeIndex = 0;
	}

	struct FractionParamQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			int n = (int)std::round(getDisplayValue());
			Fract* fract = dynamic_cast<Fract*>(module);
			if (fract != NULL) {
				float delayFraction = clampfjw((float)n / 16.f, 0.f, 1.f);
				float fractionMs = fract->beatIntervalSec * delayFraction * 1000.f;
				return string::f("%d/16 (%.1f ms)", n, fractionMs);
			}
			return string::f("%d/16", n);
		}
	};

	struct MsOffsetKnob : SmallWhiteKnob {
		std::string formatCurrentValue() override {
			if (getParamQuantity() != NULL) {
				int ms = (int)std::round(getParamQuantity()->getDisplayValue());
				return string::f("%d ms", ms);
			}
			return "";
		}
	};

	Fract() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam<FractionParamQuantity>(FRACTION_PARAM, 0.f, 16.f, 8.f, "Delay fraction", "");
		paramQuantities[FRACTION_PARAM]->snapEnabled = true;
		configParam(OFFSET_PARAM, -1000.f, 1000.f, 0.f, "Offset", " ms");
		configInput(DELAY_INPUT, "Signal to delay");
		configInput(CLOCK_INPUT, "Clock");
		configOutput(DELAYED_OUTPUT, "Delayed");
		resizeDelayBuffer();
	}

	void onSampleRateChange() override {
		sampleRate = APP->engine->getSampleRate();
		resizeDelayBuffer();
	}

	void onReset() override {
		clockElapsedSec = 0.f;
		beatIntervalSec = 0.5f;
		hasSeenClock = false;
		for (std::vector<float>& buffer : delayBuffers) {
			std::fill(buffer.begin(), buffer.end(), 0.f);
		}
		writeIndex = 0;
	}

	void process(const ProcessArgs& args) override {
		clockElapsedSec += args.sampleTime;

		if (clockTrigger.process(inputs[CLOCK_INPUT].getVoltage())) {
			if (hasSeenClock) {
				beatIntervalSec = std::max(clockElapsedSec, 0.0001f);
			}
			hasSeenClock = true;
			clockElapsedSec = 0.f;
		}

		float fraction16ths = std::round(params[FRACTION_PARAM].getValue());
		float delayFraction = clampfjw(fraction16ths / 16.f, 0.f, 1.f);
		float fractionDelaySec = clampfjw(beatIntervalSec * delayFraction, 0.f, MAX_DELAY_SECONDS);
		float offsetSec = params[OFFSET_PARAM].getValue() * 0.001f;
		float delaySec = clampfjw(fractionDelaySec + offsetSec, 0.f, MAX_DELAY_SECONDS);
		int delaySamples = clampijw((int)std::round(delaySec * sampleRate), 0, (int)delayBuffers[0].size() - 1);

		int channels = std::max(inputs[DELAY_INPUT].getChannels(), 1);
		int readIndex = writeIndex - delaySamples;
		if (readIndex < 0) {
			readIndex += (int)delayBuffers[0].size();
		}
		for (int channel = 0; channel < channels; channel++) {
			float input = inputs[DELAY_INPUT].getVoltage(channel);
			delayBuffers[channel][writeIndex] = input;
			float output = delayBuffers[channel][readIndex];
			outputs[DELAYED_OUTPUT].setVoltage(output, channel);
		}
		writeIndex++;
		if (writeIndex >= (int)delayBuffers[0].size()) {
			writeIndex = 0;
		}

		outputs[DELAYED_OUTPUT].setChannels(channels);
	}
};

struct FractFractionKnob : JwSmallSnapKnob {
	std::string formatCurrentValue() override {
		if (getParamQuantity() != NULL) {
			int n = (int)std::round(getParamQuantity()->getDisplayValue());
			return string::f("%d/16", n);
		}
		return "";
	}
};

struct FractWidget : ModuleWidget {
	FractWidget(Fract* module) {
		setModule(module);
		box.size = Vec(RACK_GRID_WIDTH * 4, RACK_GRID_HEIGHT);

		setPanel(createPanel(
			asset::plugin(pluginInstance, "res/Fract.svg"),
			asset::plugin(pluginInstance, "res/Fract.svg")
		));

		addChild(createWidget<Screw_J>(Vec(16, 2)));
		addChild(createWidget<Screw_J>(Vec(16, 365)));
		addChild(createWidget<Screw_W>(Vec(box.size.x - 29, 2)));
		addChild(createWidget<Screw_W>(Vec(box.size.x - 29, 365)));

		FractFractionKnob* fractionKnob = dynamic_cast<FractFractionKnob*>(createParam<FractFractionKnob>(Vec(17, 105), module, Fract::FRACTION_PARAM));
		CenteredLabel* const fractionLabel = new CenteredLabel;
		fractionLabel->box.pos = Vec(15, 72);
		fractionLabel->text = "";
		fractionKnob->connectLabel(fractionLabel, module);
		addChild(fractionLabel);
		addParam(fractionKnob);

		Fract::MsOffsetKnob* offsetKnob = dynamic_cast<Fract::MsOffsetKnob*>(createParam<Fract::MsOffsetKnob>(Vec(17, 173), module, Fract::OFFSET_PARAM));
		CenteredLabel* const offsetLabel = new CenteredLabel;
		offsetLabel->box.pos = Vec(15, 105);
		offsetLabel->text = "";
		offsetKnob->connectLabel(offsetLabel, module);
		addChild(offsetLabel);
		addParam(offsetKnob);

		addInput(createInput<PJ301MPort>(Vec(18, 55), module, Fract::CLOCK_INPUT));
		addInput(createInput<PJ301MPort>(Vec(18, 245), module, Fract::DELAY_INPUT));
		addOutput(createOutput<PJ301MPort>(Vec(18, 290), module, Fract::DELAYED_OUTPUT));
	}
};

Model* modelFract = createModel<Fract, FractWidget>("Fract");
