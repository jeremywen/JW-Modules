#include "JWModules.hpp"
#include <cmath>

// Include the RNBO generated code
#define RNBO_LIB_PREFIX rnbo/common
#define RNBO_NO_PATCHERFACTORY
#define rnbomatic NedLFO1_rnbomatic
#define rnbomaticFactoryFunction nedLFO1FactoryFunction
#define rnbomaticSetLogger nedLFO1SetLogger
#include "../lib/NedLFO1/nedlfo1.cpp"
#undef rnbomaticSetLogger
#undef rnbomaticFactoryFunction
#undef rnbomatic
#undef RNBO_NO_PATCHERFACTORY
#undef RNBO_LIB_PREFIX

struct RnboNedLFO1 : Module {
	enum ParamIds {
		TYPE_PARAM,
		FREQUENCY_PARAM,
		CHANCE_PARAM,
		OUTPUT_RANGE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		TYPE_CV_INPUT,
		FREQUENCY_CV_INPUT,
		CHANCE_CV_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		LFO_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	RNBO::NedLFO1_rnbomatic<> *nedLFO1Engine = nullptr;
	bool rnboInitialized = false;
	double lastType = 0.0;
	double lastFrequency = 0.1;
	double lastChance = 1.0;

	RnboNedLFO1() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(TYPE_PARAM, 0.f, 5.f, 0.f, "Type");
		configParam(FREQUENCY_PARAM, 0.1f, 40.f, 0.1f, "Frequency", " Hz");
		configParam(CHANCE_PARAM, 0.f, 1.f, 1.f, "Chance");
		configSwitch(OUTPUT_RANGE_PARAM, 0.f, 1.f, 0.f, "Output Range", {"0 to 10V", "-5 to 5V"});
		configInput(TYPE_CV_INPUT, "Type CV");
		configInput(FREQUENCY_CV_INPUT, "Frequency CV");
		configInput(CHANCE_CV_INPUT, "Chance CV");
		configOutput(LFO_OUTPUT, "LFO Out");
		nedLFO1Engine = new RNBO::NedLFO1_rnbomatic<>();
	}

	~RnboNedLFO1() {
		if (nedLFO1Engine) {
			delete nedLFO1Engine;
			nedLFO1Engine = nullptr;
		}
	}

	void onSampleRateChange() override {
		if (rnboInitialized && nedLFO1Engine) {
			nedLFO1Engine->prepareToProcess(APP->engine->getSampleRate(), 64, true);
		}
	}

	void process(const ProcessArgs &args) override {
		if (!nedLFO1Engine) return;

		// Initialize RNBO engine on first process
		if (!rnboInitialized) {
			nedLFO1Engine->initialize();
			nedLFO1Engine->prepareToProcess(args.sampleRate, 64, true);
			rnboInitialized = true;
		}

		auto clampd = [](double x, double lo, double hi) {
			return (x < lo) ? lo : (x > hi ? hi : x);
		};

		// Get parameters from Rack and apply CV modulation
		double type = (double)params[TYPE_PARAM].getValue();
		double frequency = (double)params[FREQUENCY_PARAM].getValue();
		double chance = (double)params[CHANCE_PARAM].getValue();

		if (inputs[TYPE_CV_INPUT].isConnected()) {
			double typeCV = (double)inputs[TYPE_CV_INPUT].getVoltage() / 2.0;  // 0-10V maps to 0-5
			type = clampd(type + typeCV, 0.0, 5.0);
		}
		if (inputs[FREQUENCY_CV_INPUT].isConnected()) {
			double freqCV = (double)inputs[FREQUENCY_CV_INPUT].getVoltage();  // 0-10V
			double freqRange = 40.0 - 0.1;  // 39.9 Hz range
			double cvFreq = (freqCV / 10.0) * freqRange + 0.1;
			frequency = clampd(frequency + cvFreq, 0.1, 40.0);
		}
		if (inputs[CHANCE_CV_INPUT].isConnected()) {
			double chanceCV = (double)inputs[CHANCE_CV_INPUT].getVoltage() / 10.0;  // 0-10V maps to 0-1
			chance = clampd(chance + chanceCV, 0.0, 1.0);
		}

		if (type != lastType) {
			nedLFO1Engine->setParameterValue(0, type, 0);
			lastType = type;
		}
		if (frequency != lastFrequency) {
			nedLFO1Engine->setParameterValue(1, frequency, 0);
			lastFrequency = frequency;
		}
		if (chance != lastChance) {
			nedLFO1Engine->setParameterValue(2, chance, 0);
			lastChance = chance;
		}

		double dummy_in = 0.0;  // Dummy input (NedLFO1 has 0 inputs but process() expects a valid pointer)
		double out0 = 0.0;
		double* rnboInputs[1] = {&dummy_in};
		double* rnboOutputs[1] = {&out0};

		// Process one sample per Rack frame (0 inputs, 1 output).
		nedLFO1Engine->process(rnboInputs, 0, rnboOutputs, 1, 1);

		// RNBO outputs 0..1 here; map to either unipolar (0..10V) or bipolar (-5..5V).
		float normalizedOut = clamp((float)out0, 0.f, 1.f);
		bool bipolarMode = params[OUTPUT_RANGE_PARAM].getValue() > 0.5f;
		float scaledOut = bipolarMode ? (normalizedOut * 10.f - 5.f) : (normalizedOut * 10.f);

		outputs[LFO_OUTPUT].setVoltage(scaledOut);
		outputs[LFO_OUTPUT].setChannels(1);
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

struct RnboNedLFO1Widget : ModuleWidget {
	RnboNedLFO1Widget(RnboNedLFO1 *module);
	~RnboNedLFO1Widget(){ 
	}
	void appendContextMenu(Menu *menu) override;
};

RnboNedLFO1Widget::RnboNedLFO1Widget(RnboNedLFO1 *module) {
	setModule(module);
	box.size = Vec(RACK_GRID_WIDTH * 8, RACK_GRID_HEIGHT);

	setPanel(createPanel(
		asset::plugin(pluginInstance, "res/RnboNedLFO1.svg"), 
		asset::plugin(pluginInstance, "res/RnboNedLFO1.svg")
	));

	addChild(createWidget<Screw_J>(Vec(16, 2)));
	addChild(createWidget<Screw_J>(Vec(16, 365)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 2)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 365)));

	// Add parameters
	addParam(createParamCentered<RoundBlackKnob>(Vec(40.f, 80.f), module, RnboNedLFO1::TYPE_PARAM));
	addInput(createInputCentered<PJ301MPort>(Vec(80.f, 80.f), module, RnboNedLFO1::TYPE_CV_INPUT));
	addParam(createParamCentered<RoundBlackKnob>(Vec(40.f, 150.f), module, RnboNedLFO1::FREQUENCY_PARAM));
	addInput(createInputCentered<PJ301MPort>(Vec(80.f, 150.f), module, RnboNedLFO1::FREQUENCY_CV_INPUT));
	addParam(createParamCentered<RoundBlackKnob>(Vec(40.f, 220.f), module, RnboNedLFO1::CHANCE_PARAM));
	addInput(createInputCentered<PJ301MPort>(Vec(80.f, 220.f), module, RnboNedLFO1::CHANCE_CV_INPUT));
	addParam(createParamCentered<CKSS>(Vec(40.f, 270.f), module, RnboNedLFO1::OUTPUT_RANGE_PARAM));

	// Add output port
	addOutput(createOutputCentered<PJ301MPort>(Vec(box.size.x / 2, 320.f), module, RnboNedLFO1::LFO_OUTPUT));
}

void RnboNedLFO1Widget::appendContextMenu(Menu *menu) {
	MenuLabel *spacerLabel = new MenuLabel();
	menu->addChild(spacerLabel);
}

Model *modelRnboNedLFO1 = createModel<RnboNedLFO1, RnboNedLFO1Widget>("RnboNedLFO1");
