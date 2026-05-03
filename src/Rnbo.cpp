#include "JWModules.hpp"

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

		// Initialize RNBO engine on first process
		if (!rnboInitialized) {
			rnboEngine->initialize();
			rnboEngine->prepareToProcess(args.sampleRate, 64, true);
			rnboInitialized = true;
		}

		// Current RNBO export has 3 inputs and 1 output.
		// RNBO patch uses these as frequency-driving signals, so scale Rack volts up.
		const double inputScale = 100.0;
		double in0 = inputs[AUDIO_IN + 0].isConnected() ? (double)inputs[AUDIO_IN + 0].getVoltage() * inputScale : 1.0;
		double in1 = inputs[AUDIO_IN + 1].isConnected() ? (double)inputs[AUDIO_IN + 1].getVoltage() * inputScale : 1.0;
		double in2 = inputs[AUDIO_IN + 2].isConnected() ? (double)inputs[AUDIO_IN + 2].getVoltage() * inputScale : 1.0;
		const double* rnboInputs[3] = {&in0, &in1, &in2};

		double out0 = 0.0;
		double* rnboOutputs[1] = {&out0};

		// Process one sample per Rack frame.
		rnboEngine->process(rnboInputs, 3, rnboOutputs, 1, 1);

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
