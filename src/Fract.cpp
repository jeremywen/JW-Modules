#include "JWModules.hpp"

struct Fract : Module {
	enum ParamIds {
		FRACTION_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CLOCK_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		TRIG_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	dsp::SchmittTrigger clockTrigger;
	dsp::PulseGenerator gatePulse;

	float clockElapsedSec = 0.f;
	float beatIntervalSec = 0.5f;
	bool hasSeenClock = false;

	bool pendingTrig = false;
	float pendingDelaySec = 0.f;

	// Trigger pulse length in seconds.
	float gatePulseLenSec = 0.005f;

	Fract() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(FRACTION_PARAM, 0.f, 16.f, 8.f, "Delay fraction", " /16 beat");
		paramQuantities[FRACTION_PARAM]->snapEnabled = true;
		configInput(CLOCK_INPUT, "Clock");
		configOutput(TRIG_OUTPUT, "Delayed Trigger");
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "gatePulseLenSec", json_real(gatePulseLenSec));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* gatePulseLenSecJ = json_object_get(rootJ, "gatePulseLenSec");
		if (gatePulseLenSecJ) {
			gatePulseLenSec = (float)json_number_value(gatePulseLenSecJ);
			gatePulseLenSec = clampfjw(gatePulseLenSec, 0.001f, 10.0f);
		}
	}

	void onReset() override {
		clockElapsedSec = 0.f;
		beatIntervalSec = 0.5f;
		hasSeenClock = false;
		pendingTrig = false;
		pendingDelaySec = 0.f;
	}

	void process(const ProcessArgs& args) override {
		clockElapsedSec += args.sampleTime;

		if (clockTrigger.process(inputs[CLOCK_INPUT].getVoltage())) {
			if (hasSeenClock) {
				beatIntervalSec = std::max(clockElapsedSec, 0.0001f);
			}
			hasSeenClock = true;
			clockElapsedSec = 0.f;

			float fraction16ths = std::round(params[FRACTION_PARAM].getValue());
			float delayFraction = clampfjw(fraction16ths / 16.f, 0.f, 1.f);
			float delaySec = beatIntervalSec * delayFraction;
			if (delaySec <= 0.f) {
				gatePulse.trigger(gatePulseLenSec);
				pendingTrig = false;
				pendingDelaySec = 0.f;
			}
			else {
				pendingTrig = true;
				pendingDelaySec = delaySec;
			}
		}

		if (pendingTrig) {
			pendingDelaySec -= args.sampleTime;
			if (pendingDelaySec <= 0.f) {
				gatePulse.trigger(gatePulseLenSec);
				pendingTrig = false;
				pendingDelaySec = 0.f;
			}
		}

		bool pulse = gatePulse.process(args.sampleTime);
		outputs[TRIG_OUTPUT].setVoltage(pulse ? 10.f : 0.f);
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

		FractFractionKnob* fractionKnob = dynamic_cast<FractFractionKnob*>(createParam<FractFractionKnob>(Vec(17, 118), module, Fract::FRACTION_PARAM));
		CenteredLabel* const fractionLabel = new CenteredLabel;
		fractionLabel->box.pos = Vec(15, 80);
		fractionLabel->text = "";
		fractionKnob->connectLabel(fractionLabel, module);
		addChild(fractionLabel);
		addParam(fractionKnob);

		addInput(createInput<PJ301MPort>(Vec(18, 215), module, Fract::CLOCK_INPUT));
		addOutput(createOutput<PJ301MPort>(Vec(18, 290), module, Fract::TRIG_OUTPUT));
	}
};

Model* modelFract = createModel<Fract, FractWidget>("Fract");
