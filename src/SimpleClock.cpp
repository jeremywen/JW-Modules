#include "JWModules.hpp"
#include "dsp/digital.hpp"

struct SimpleClock : Module {
	enum ParamIds {
		CLOCK_PARAM,
		RUN_PARAM,
		PROB_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		NUM_INPUTS
	};
	enum OutputIds {
		GATES_OUTPUT,
		RESET_OUTPUT,
		NUM_OUTPUTS
	};

	bool running = true;
	bool rndReset = false;
	SchmittTrigger runningTrigger;
	float runningLight = 0.0;
	int beat = 0;
	float phase = 0.0;
	PulseGenerator gatePulse;
	PulseGenerator resetPulse;

	SimpleClock() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS) {}
	void step();

	json_t *toJson() {
		json_t *rootJ = json_object();
		json_object_set_new(rootJ, "running", json_boolean(running));
		return rootJ;
	}

	void fromJson(json_t *rootJ) {
		json_t *runningJ = json_object_get(rootJ, "running");
		if (runningJ)
			running = json_is_true(runningJ);
	}

	void initialize() {
	}

	void randomize() {
	}
};


//TODO ADD SWING KNOB

/* 
SWING - Roger Linn https://www.attackmagazine.com/features/interview/roger-linn-swing-groove-magic-mpc-timing/
"I merely delay the second 16th note within each 8th note. In other words, I delay all the even-numbered 16th notes 
within the beat (2, 4, 6, 8, etc.) In my products I describe the swing amount in terms of the ratio of time duration
between the first and second 16th notes within each 8th note. For example, 50% is no swing, meaning that both 16th 
notes within each 8th note are given equal timing. And 66% means perfect triplet swing, meaning that the first
16th note of each pair gets 2/3 of the time, and the second 16th note gets 1/3, so the second 16th note falls on 
a perfect 8th note triplet. The fun comes in the in-between settings. For example, a 90 BPM swing groove will
feel looser at 62% than at a perfect swing setting of 66%. And for straight 16th-note beats (no swing), a swing 
setting of 54% will loosen up the feel without it sounding like swing. Between 50% and around 70% are lots of
wonderful little settings that, for a particular beat and tempo, can change a rigid beat into something that makes people move."" */
void SimpleClock::step() {
	if (runningTrigger.process(params[RUN_PARAM].value)) {
		running = !running;
		if(running){
			resetPulse.trigger(0.01);
		}
	}
	runningLight = running ? 1.0 : 0.0;

	bool nextStep = false;
	if (running) {
		float clockTime = powf(2.0, params[CLOCK_PARAM].value);
		phase += clockTime / gSampleRate;
		// printf("clockTime:%f phase:%f\n", clockTime, phase);
		if (phase >= 1.0) {
			phase -= 1.0;
			nextStep = true;
		}
	}
	if (nextStep) {
		float probScaled = rescalef(params[PROB_PARAM].value, -2, 6, 0, 1);
		if(randomf() < probScaled){
			resetPulse.trigger(0.01);
		}
		gatePulse.trigger(1e-3);

		beat = (beat + 1) % 16;//in music the first beat is one so increment before
		// if(beat%2 == 0){
		// 	printf("even beat %i, %f\n", beat, phase);
		// } else {
		// 	printf("odd beat %i, %f\n", beat, phase);

		// }
	}

	bool gpulse = gatePulse.process(1.0 / gSampleRate);
	bool rpulse = resetPulse.process(1.0 / gSampleRate);
	outputs[RESET_OUTPUT].value = running && rpulse ? 10.0 : 0.0;
	outputs[GATES_OUTPUT].value = running && gpulse ? 10.0 : 0.0;
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

	addParam(createParam<Davies1900hSmallBlackKnob>(Vec(17, 40), module, SimpleClock::CLOCK_PARAM, -2.0, 6.0, 2.0));
	addOutput(createOutput<PJ301MPort>(Vec(18, 78), module, SimpleClock::GATES_OUTPUT));
	addParam(createParam<LEDButton>(Vec(21, 140), module, SimpleClock::RUN_PARAM, 0.0, 1.0, 0.0));
	addChild(createValueLight<SmallLight<MyBlueValueLight>>(Vec(26, 140+5), &module->runningLight));

	addOutput(createOutput<PJ301MPort>(Vec(18, 198), module, SimpleClock::RESET_OUTPUT));
	addParam(createParam<Davies1900hSmallBlackKnob>(Vec(16, 280), module, SimpleClock::PROB_PARAM, -2.0, 6.0, -2));
}
