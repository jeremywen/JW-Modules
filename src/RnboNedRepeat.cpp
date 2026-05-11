#include "JWModules.hpp"
#include <cmath>

// Include the RNBO generated code
#define RNBO_LIB_PREFIX rnbo/common
#define RNBO_NO_PATCHERFACTORY
#define rnbomatic NedRepeat_rnbomatic
#define rnbomaticFactoryFunction nedRepeatFactoryFunction
#define rnbomaticSetLogger nedRepeatSetLogger
#include "../lib/NedRepeat/NedRepeat.cpp"
#undef rnbomaticSetLogger
#undef rnbomaticFactoryFunction
#undef rnbomatic
#undef RNBO_NO_PATCHERFACTORY
#undef RNBO_LIB_PREFIX

struct RnboNedRepeat : Module {
	enum ParamIds {
		REPEAT_LENGTH_PARAM,
		OFFSET_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		AUDIO_L_INPUT,
		AUDIO_R_INPUT,
		REPEAT_LENGTH_CV_INPUT,
		OFFSET_CV_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		AUDIO_L_OUTPUT,
		AUDIO_R_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	RNBO::NedRepeat_rnbomatic<> *nedRepeatEngine = nullptr;
	bool rnboInitialized = false;
	double lastRepeatLength = 0.0;
	double lastOffset = 0.0;

	RnboNedRepeat() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(REPEAT_LENGTH_PARAM, 0.f, 1.f, 0.f, "Repeat Length");
		configParam(OFFSET_PARAM, 0.f, 1.f, 0.f, "Offset");
		configInput(AUDIO_L_INPUT, "Audio Left In");
		configInput(AUDIO_R_INPUT, "Audio Right In");
		configInput(REPEAT_LENGTH_CV_INPUT, "Repeat Length CV");
		configInput(OFFSET_CV_INPUT, "Offset CV");
		configOutput(AUDIO_L_OUTPUT, "Audio Left Out");
		configOutput(AUDIO_R_OUTPUT, "Audio Right Out");
		nedRepeatEngine = new RNBO::NedRepeat_rnbomatic<>();
	}

	~RnboNedRepeat() {
		if (nedRepeatEngine) {
			delete nedRepeatEngine;
			nedRepeatEngine = nullptr;
		}
	}

	void onSampleRateChange() override {
		if (rnboInitialized && nedRepeatEngine) {
			nedRepeatEngine->prepareToProcess(APP->engine->getSampleRate(), 64, true);
		}
	}

	void process(const ProcessArgs &args) override {
		if (!nedRepeatEngine) return;

		if (!rnboInitialized) {
			nedRepeatEngine->initialize();
			// NedRepeat uses transport-gated phasor logic; start transport so repeats can run.
			nedRepeatEngine->processTransportEvent(0, (RNBO::TransportState)1);
			nedRepeatEngine->prepareToProcess(args.sampleRate, 64, true);
			rnboInitialized = true;
		}

		auto clampd = [](double x, double lo, double hi) {
			return (x < lo) ? lo : (x > hi ? hi : x);
		};

		double repeatLength = (double)params[REPEAT_LENGTH_PARAM].getValue();
		double offset = (double)params[OFFSET_PARAM].getValue();

		if (inputs[REPEAT_LENGTH_CV_INPUT].isConnected()) {
			double repeatLengthCV = (double)inputs[REPEAT_LENGTH_CV_INPUT].getVoltage() / 10.0;
			repeatLength = clampd(repeatLength + repeatLengthCV, 0.0, 1.0);
		}
		if (inputs[OFFSET_CV_INPUT].isConnected()) {
			double offsetCV = (double)inputs[OFFSET_CV_INPUT].getVoltage() / 10.0;
			offset = clampd(offset + offsetCV, 0.0, 1.0);
		}

		if (repeatLength != lastRepeatLength) {
			nedRepeatEngine->setParameterValue(0, repeatLength, 0);
			lastRepeatLength = repeatLength;
		}
		if (offset != lastOffset) {
			nedRepeatEngine->setParameterValue(1, offset, 0);
			lastOffset = offset;
		}

		double inL = inputs[AUDIO_L_INPUT].isConnected() ? (double)inputs[AUDIO_L_INPUT].getVoltage() / 5.0 : 0.0;
		double inR = inputs[AUDIO_R_INPUT].isConnected() ? (double)inputs[AUDIO_R_INPUT].getVoltage() / 5.0 : inL;
		double outL = 0.0;
		double outR = 0.0;

		double* rnboInputs[2] = {&inL, &inR};
		double* rnboOutputs[2] = {&outL, &outR};

		// Process one sample per Rack frame.
		nedRepeatEngine->process(rnboInputs, 2, rnboOutputs, 2, 1);

		float scaledOutL = clamp((float)outL * 5.f, -10.f, 10.f);
		float scaledOutR = clamp((float)outR * 5.f, -10.f, 10.f);

		outputs[AUDIO_L_OUTPUT].setVoltage(scaledOutL);
		outputs[AUDIO_R_OUTPUT].setVoltage(scaledOutR);
		outputs[AUDIO_L_OUTPUT].setChannels(1);
		outputs[AUDIO_R_OUTPUT].setChannels(1);
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

struct RnboNedRepeatWidget : ModuleWidget {
	RnboNedRepeatWidget(RnboNedRepeat *module);
	~RnboNedRepeatWidget() {
	}
	void appendContextMenu(Menu *menu) override;
};

RnboNedRepeatWidget::RnboNedRepeatWidget(RnboNedRepeat *module) {
	setModule(module);
	box.size = Vec(RACK_GRID_WIDTH * 8, RACK_GRID_HEIGHT);

	setPanel(createPanel(
		asset::plugin(pluginInstance, "res/RnboNedRepeat.svg"),
		asset::plugin(pluginInstance, "res/RnboNedRepeat.svg")
	));

	addChild(createWidget<Screw_J>(Vec(16, 2)));
	addChild(createWidget<Screw_J>(Vec(16, 365)));
	addChild(createWidget<Screw_W>(Vec(box.size.x - 29, 2)));
	addChild(createWidget<Screw_W>(Vec(box.size.x - 29, 365)));

	addParam(createParamCentered<RoundBlackKnob>(Vec(40.f, 100.f), module, RnboNedRepeat::REPEAT_LENGTH_PARAM));
	addInput(createInputCentered<PJ301MPort>(Vec(80.f, 100.f), module, RnboNedRepeat::REPEAT_LENGTH_CV_INPUT));
	addParam(createParamCentered<RoundBlackKnob>(Vec(40.f, 170.f), module, RnboNedRepeat::OFFSET_PARAM));
	addInput(createInputCentered<PJ301MPort>(Vec(80.f, 170.f), module, RnboNedRepeat::OFFSET_CV_INPUT));

	addInput(createInputCentered<PJ301MPort>(Vec(25.f, 300.f), module, RnboNedRepeat::AUDIO_L_INPUT));
	addInput(createInputCentered<PJ301MPort>(Vec(25.f, 335.f), module, RnboNedRepeat::AUDIO_R_INPUT));
	addOutput(createOutputCentered<PJ301MPort>(Vec(box.size.x - 25.f, 300.f), module, RnboNedRepeat::AUDIO_L_OUTPUT));
	addOutput(createOutputCentered<PJ301MPort>(Vec(box.size.x - 25.f, 335.f), module, RnboNedRepeat::AUDIO_R_OUTPUT));
}

void RnboNedRepeatWidget::appendContextMenu(Menu *menu) {
	MenuLabel *spacerLabel = new MenuLabel();
	menu->addChild(spacerLabel);
}

Model *modelRnboNedRepeat = createModel<RnboNedRepeat, RnboNedRepeatWidget>("RnboNedRepeat");
