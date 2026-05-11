#include "JWModules.hpp"
#include <cmath>

// Include the RNBO generated code
#define RNBO_LIB_PREFIX rnbo/common
#define RNBO_NO_PATCHERFACTORY
#define rnbomatic HelpDelay_rnbomatic
#define rnbomaticFactoryFunction helpDelayFactoryFunction
#define rnbomaticSetLogger helpDelaySetLogger
#include "../lib/HelpDelay/helpdelay.cpp"
#undef rnbomaticSetLogger
#undef rnbomaticFactoryFunction
#undef rnbomatic
#undef RNBO_NO_PATCHERFACTORY
#undef RNBO_LIB_PREFIX

struct RnboHelpDelay : Module {
	enum ParamIds {
		FEEDBACK_PARAM,
		DRYWET_PARAM,
		TIME_PARAM,
		PLAYBACKRATE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		AUDIO_INPUT,
		FEEDBACK_CV_INPUT,
		DRYWET_CV_INPUT,
		TIME_CV_INPUT,
		PLAYBACKRATE_CV_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		AUDIO_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	RNBO::HelpDelay_rnbomatic<> *helpDelayEngine = nullptr;
	bool rnboInitialized = false;
	double lastFeedback = 0.5;
	double lastDrywet = 0.0;
	double lastTime = 250.0;
	double lastPlaybackrate = 1.0;

	RnboHelpDelay() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(FEEDBACK_PARAM, 0.f, 0.99f, 0.5f, "Feedback");
		configParam(DRYWET_PARAM, 0.f, 1.f, 0.f, "Dry/Wet");
		configParam(TIME_PARAM, 1.f, 1000.f, 250.f, "Time (ms)");
		configParam(PLAYBACKRATE_PARAM, -4.f, 4.f, 1.f, "Playback Rate");
		configInput(AUDIO_INPUT, "Audio In");
		configInput(FEEDBACK_CV_INPUT, "Feedback CV");
		configInput(DRYWET_CV_INPUT, "Dry/Wet CV");
		configInput(TIME_CV_INPUT, "Time CV");
		configInput(PLAYBACKRATE_CV_INPUT, "Playback Rate CV");
		configOutput(AUDIO_OUTPUT, "Audio Out");
		helpDelayEngine = new RNBO::HelpDelay_rnbomatic<>();
	}

	~RnboHelpDelay() {
		if (helpDelayEngine) {
			delete helpDelayEngine;
			helpDelayEngine = nullptr;
		}
	}

	void onSampleRateChange() override {
		if (rnboInitialized && helpDelayEngine) {
			helpDelayEngine->prepareToProcess(APP->engine->getSampleRate(), 64, true);
		}
	}

	void process(const ProcessArgs &args) override {
		if (!helpDelayEngine) return;

		auto clampd = [](double x, double lo, double hi) {
			return (x < lo) ? lo : (x > hi ? hi : x);
		};

		// Initialize RNBO engine on first process
		if (!rnboInitialized) {
			helpDelayEngine->initialize();
			helpDelayEngine->prepareToProcess(args.sampleRate, 64, true);
			rnboInitialized = true;
		}

		// Get parameters from Rack and update RNBO if changed
		double feedback = (double)params[FEEDBACK_PARAM].getValue();
		double drywet = (double)params[DRYWET_PARAM].getValue();
		double time = (double)params[TIME_PARAM].getValue();
		double playbackrate = (double)params[PLAYBACKRATE_PARAM].getValue();

		// Apply CV modulation
		if (inputs[FEEDBACK_CV_INPUT].isConnected()) {
			double feedbackCV = (double)inputs[FEEDBACK_CV_INPUT].getVoltage() / 10.0; // 0-10V to 0-1
			feedback = clampd(feedback + feedbackCV * 0.99, 0.0, 0.99);
		}
		if (inputs[DRYWET_CV_INPUT].isConnected()) {
			double drywetCV = (double)inputs[DRYWET_CV_INPUT].getVoltage() / 10.0; // 0-10V to 0-1
			drywet = clampd(drywet + drywetCV, 0.0, 1.0);
		}
		if (inputs[TIME_CV_INPUT].isConnected()) {
			double timeCV = (double)inputs[TIME_CV_INPUT].getVoltage() / 10.0; // 0-10V to 0-1
			time = clampd(time + timeCV * 999.0, 1.0, 1000.0);
		}
		if (inputs[PLAYBACKRATE_CV_INPUT].isConnected()) {
			double playbackrateCV = (double)inputs[PLAYBACKRATE_CV_INPUT].getVoltage() / 10.0; // -10 to +10V to -1 to +1
			playbackrate = clampd(playbackrate + playbackrateCV * 4.0, -4.0, 4.0);
		}

		if (feedback != lastFeedback) {
			helpDelayEngine->setParameterValue(0, feedback, 0);
			lastFeedback = feedback;
		}
		if (drywet != lastDrywet) {
			helpDelayEngine->setParameterValue(1, drywet, 0);
			lastDrywet = drywet;
		}
		if (time != lastTime) {
			helpDelayEngine->setParameterValue(2, time, 0);
			lastTime = time;
		}
		if (playbackrate != lastPlaybackrate) {
			helpDelayEngine->setParameterValue(3, playbackrate, 0);
			lastPlaybackrate = playbackrate;
		}

		// Get input and prepare output
		double in0 = inputs[AUDIO_INPUT].isConnected() ? (double)inputs[AUDIO_INPUT].getVoltage() / 5.0 : 0.0;
		double out0 = 0.0;
		double* rnboInputs[1] = {&in0};
		double* rnboOutputs[1] = {&out0};

		// Process one sample per Rack frame.
		helpDelayEngine->process(rnboInputs, 1, rnboOutputs, 1, 1);

		// RNBO audio is generally normalized around +/-1. Rack audio is typically +/-5V.
		float scaledOut = clamp((float)out0 * 5.f, -10.f, 10.f);

		// Output to single audio output
		outputs[AUDIO_OUTPUT].setVoltage(scaledOut);
		outputs[AUDIO_OUTPUT].setChannels(1);
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

struct RnboHelpDelayWidget : ModuleWidget {
	RnboHelpDelayWidget(RnboHelpDelay *module);
	~RnboHelpDelayWidget(){ 
	}
	void appendContextMenu(Menu *menu) override;
};

RnboHelpDelayWidget::RnboHelpDelayWidget(RnboHelpDelay *module) {
	setModule(module);
	box.size = Vec(RACK_GRID_WIDTH * 8, RACK_GRID_HEIGHT);

	setPanel(createPanel(
		asset::plugin(pluginInstance, "res/RnboHelpDelay.svg"), 
		asset::plugin(pluginInstance, "res/RnboHelpDelay.svg")
	));

	addChild(createWidget<Screw_J>(Vec(16, 2)));
	addChild(createWidget<Screw_J>(Vec(16, 365)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 2)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 365)));

	// Add parameters and CV inputs for each
	addParam(createParamCentered<RoundBlackKnob>(Vec(35.f, 80.f), module, RnboHelpDelay::FEEDBACK_PARAM));
	addInput(createInputCentered<PJ301MPort>(Vec(65.f, 80.f), module, RnboHelpDelay::FEEDBACK_CV_INPUT));

	addParam(createParamCentered<RoundBlackKnob>(Vec(35.f, 130.f), module, RnboHelpDelay::DRYWET_PARAM));
	addInput(createInputCentered<PJ301MPort>(Vec(65.f, 130.f), module, RnboHelpDelay::DRYWET_CV_INPUT));

	addParam(createParamCentered<RoundBlackKnob>(Vec(35.f, 180.f), module, RnboHelpDelay::TIME_PARAM));
	addInput(createInputCentered<PJ301MPort>(Vec(65.f, 180.f), module, RnboHelpDelay::TIME_CV_INPUT));

	addParam(createParamCentered<RoundBlackKnob>(Vec(35.f, 230.f), module, RnboHelpDelay::PLAYBACKRATE_PARAM));
	addInput(createInputCentered<PJ301MPort>(Vec(65.f, 230.f), module, RnboHelpDelay::PLAYBACKRATE_CV_INPUT));

	// Add input and output ports
	addInput(createInputCentered<PJ301MPort>(Vec(25, 300.f), module, RnboHelpDelay::AUDIO_INPUT));
	addOutput(createOutputCentered<PJ301MPort>(Vec(box.size.x - 25, 300.f), module, RnboHelpDelay::AUDIO_OUTPUT));
}

void RnboHelpDelayWidget::appendContextMenu(Menu *menu) {
	MenuLabel *spacerLabel = new MenuLabel();
	menu->addChild(spacerLabel);
}

Model *modelRnboHelpDelay = createModel<RnboHelpDelay, RnboHelpDelayWidget>("RnboHelpDelay");
