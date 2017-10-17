#include "JWModules.hpp"
#include "dsp/digital.hpp"


struct SimpleClock : Module {
	enum ParamIds {
		CLOCK_PARAM,
		RUN_PARAM,
		GATE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CLOCK_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		GATES_OUTPUT,
		RESET_OUTPUT,
		NUM_OUTPUTS
	};

	bool running = true;
	SchmittTrigger clockTrigger; // for external clock
	// For buttons
	SchmittTrigger runningTrigger;
	SchmittTrigger gateTriggers[8];
	float phase = 0.0;
	int index = 0;

	enum GateMode {
		TRIGGER,
		RETRIGGER,
		CONTINUOUS,
	};
	GateMode gateMode = TRIGGER;
	PulseGenerator gatePulse;

	// Lights
	float runningLight = 0.0;

	SimpleClock() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS) {}
	void step();

	json_t *toJson() {
		json_t *rootJ = json_object();

		// running
		json_object_set_new(rootJ, "running", json_boolean(running));

		return rootJ;
	}

	void fromJson(json_t *rootJ) {
		// running
		json_t *runningJ = json_object_get(rootJ, "running");
		if (runningJ)
			running = json_is_true(runningJ);

	}

	void initialize() {
	}

	void randomize() {
	}
};


void SimpleClock::step() {
	const float lightLambda = 0.075;
	// Run
	if (runningTrigger.process(params[RUN_PARAM].value)) {
		running = !running;
	}
	runningLight = running ? 1.0 : 0.0;

	bool nextStep = false;

	if (running) {
		// Internal clock
		float clockTime = powf(2.0, params[CLOCK_PARAM].value + inputs[CLOCK_INPUT].value);
		phase += clockTime / gSampleRate;
		if (phase >= 1.0) {
			phase -= 1.0;
			nextStep = true;
		}
	}

	if (nextStep) {
		// Advance step
		int numSteps = 8;
		index += 1;
		if (index >= numSteps) {
			index = 0;
		}
		gatePulse.trigger(1e-3);
	}

	bool pulse = gatePulse.process(1.0 / gSampleRate);

	// Rows
	bool gatesOn = running;
	if (gateMode == TRIGGER)
		gatesOn = gatesOn && pulse;
	else if (gateMode == RETRIGGER)
		gatesOn = gatesOn && !pulse;

	// Outputs
	outputs[RESET_OUTPUT].value = index == 0 ? 10.0 : 0.0;
	outputs[GATES_OUTPUT].value = gatesOn ? 10.0 : 0.0;
}


SimpleClockWidget::SimpleClockWidget() {
	SimpleClock *module = new SimpleClock();
	setModule(module);
	box.size = Vec(15*4, 380);

	{
		SVGPanel *panel = new SVGPanel();
		panel->box.size = box.size;
		panel->setBackground(SVG::load(assetPlugin(plugin, "res/SimpleClock.svg")));
		addChild(panel);
	}

	addChild(createScrew<ScrewSilver>(Vec(15, 0)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x-30, 0)));
	addChild(createScrew<ScrewSilver>(Vec(15, 365)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x-30, 365)));

	addParam(createParam<Davies1900hSmallBlackKnob>(Vec(16, 50), module, SimpleClock::CLOCK_PARAM, -2.0, 6.0, 2.0));

	addParam(createParam<LEDButton>(Vec(21, 130), module, SimpleClock::RUN_PARAM, 0.0, 1.0, 0.0));
	addChild(createValueLight<SmallLight<GreenValueLight>>(Vec(26, 135), &module->runningLight));

	addOutput(createOutput<PJ301MPort>(Vec(18, 218), module, SimpleClock::RESET_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(18, 318), module, SimpleClock::GATES_OUTPUT));
}
