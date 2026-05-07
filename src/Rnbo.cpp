#include "JWModules.hpp"
#include <cmath>

// Include the RNBO generated code
#include "../lib/rnbo/rnbo_source.cpp"

struct Rnbo : Module {
	enum ParamIds {
		NUM_PARAMS
	};
	enum InputIds {
		ENUMS(AUDIO_IN, 8),
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(AUDIO_OUT, 8),
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	RNBO::rnbomatic<> *rnboEngine = nullptr;
	bool rnboInitialized = false;
	bool lfoParamsInitialized = false;
	double lastType = 0.0;
	double lastFrequency = 0.1;
	double lastChance = 1.0;

	Rnbo() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		rnboEngine = new RNBO::rnbomatic<>();
	}

	~Rnbo() {
		if (rnboEngine) {
			delete rnboEngine;
			rnboEngine = nullptr;
		}
	}

	void onSampleRateChange() override {
		if (rnboInitialized && rnboEngine) {
			rnboEngine->prepareToProcess(APP->engine->getSampleRate(), 64, true);
		}
	}

	void process(const ProcessArgs &args) override {
		if (!rnboEngine) return;

		auto clampd = [](double x, double lo, double hi) {
			return (x < lo) ? lo : (x > hi ? hi : x);
		};

		// Initialize RNBO engine on first process
		if (!rnboInitialized) {
			rnboEngine->initialize();
			rnboEngine->prepareToProcess(args.sampleRate, 64, true);
			rnboInitialized = true;
			lfoParamsInitialized = false;
		}

		// Updated RNBO export is an LFO with 0 audio inputs and 3 parameters.
		// Use the first three Rack inputs as CV controls: type, frequency, and chance.
		double type = inputs[AUDIO_IN + 0].isConnected() ? clampd((double)inputs[AUDIO_IN + 0].getVoltage() * 0.5, 0.0, 5.0) : 0.0;
		type = std::round(type);
		double frequency = inputs[AUDIO_IN + 1].isConnected() ? clampd(0.1 + ((double)inputs[AUDIO_IN + 1].getVoltage() / 10.0) * (40.0 - 0.1), 0.1, 40.0) : 0.1;
		double chance = inputs[AUDIO_IN + 2].isConnected() ? clampd((double)inputs[AUDIO_IN + 2].getVoltage() / 10.0, 0.0, 1.0) : 1.0;

		if (!lfoParamsInitialized || type != lastType) {
			rnboEngine->setParameterValue(0, type, 0);
			lastType = type;
		}

		if (!lfoParamsInitialized || frequency != lastFrequency) {
			rnboEngine->setParameterValue(1, frequency, 0);
			lastFrequency = frequency;
		}

		if (!lfoParamsInitialized || chance != lastChance) {
			rnboEngine->setParameterValue(2, chance, 0);
			lastChance = chance;
		}

		lfoParamsInitialized = true;

		double out0 = 0.0;
		double* rnboOutputs[1] = {&out0};

		// Process one sample per Rack frame.
		rnboEngine->process(nullptr, 0, rnboOutputs, 1, 1);

		// RNBO audio is generally normalized around +/-1. Rack audio is typically +/-5V.
		float scaledOut = clamp((float)out0 * 5.f, -10.f, 10.f);

		// Route RNBO's single output to output 1 only.
		outputs[AUDIO_OUT + 0].setVoltage(scaledOut);
		outputs[AUDIO_OUT + 0].setChannels(1);

		// Force remaining outputs silent.
		for (int i = 1; i < 8; i++) {
			outputs[AUDIO_OUT + i].setVoltage(0.f);
			outputs[AUDIO_OUT + i].setChannels(1);
		}
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
	}

	void onReset() override {
		rnboInitialized = false;
		lfoParamsInitialized = false;
	}

	void onRandomize() override {
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// WIDGET
///////////////////////////////////////////////////////////////////////////////////////////////////

struct RnboWidget : ModuleWidget {
	RnboWidget(Rnbo *module);
	~RnboWidget(){ 
	}
	void appendContextMenu(Menu *menu) override;
};

RnboWidget::RnboWidget(Rnbo *module) {
	setModule(module);
	box.size = Vec(RACK_GRID_WIDTH * 8, RACK_GRID_HEIGHT);

	setPanel(createPanel(
		asset::plugin(pluginInstance, "res/Rnbo.svg"), 
		asset::plugin(pluginInstance, "res/Rnbo.svg")
	));

	addChild(createWidget<Screw_J>(Vec(16, 2)));
	addChild(createWidget<Screw_J>(Vec(16, 365)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 2)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 365)));

	// Add input ports
	for (int i = 0; i < 8; i++) {
		addInput(createInputCentered<PJ301MPort>(
			Vec(25, 60 + i * 40),
			module,
			Rnbo::AUDIO_IN + i
		));
	}

	// Add output ports
	for (int i = 0; i < 8; i++) {
		addOutput(createOutputCentered<PJ301MPort>(
			Vec(box.size.x - 25, 60 + i * 40),
			module,
			Rnbo::AUDIO_OUT + i
		));
	}
}

void RnboWidget::appendContextMenu(Menu *menu) {
	MenuLabel *spacerLabel = new MenuLabel();
	menu->addChild(spacerLabel);
}

Model *modelRnbo = createModel<Rnbo, RnboWidget>("Rnbo");
