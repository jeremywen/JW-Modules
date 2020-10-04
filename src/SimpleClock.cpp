#include "JWModules.hpp"

struct SimpleClock : Module {
	enum ParamIds {
		CLOCK_PARAM,
		RUN_PARAM,
		PROB_PARAM,
		RESET_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		NUM_INPUTS
	};
	enum OutputIds {
		CLOCK_OUTPUT,
		RESET_OUTPUT,
		DIV_4_OUTPUT,
		DIV_8_OUTPUT,
		DIV_16_OUTPUT,
		DIV_32_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		RUNNING_LIGHT,
		NUM_LIGHTS
	};

	bool running = true;
	bool rndReset = false;
	dsp::SchmittTrigger runningTrigger;
	dsp::SchmittTrigger resetTrigger;
	float runningLight = 0.0;
	int clockMult = 1;
	float phase = 0.0;
	dsp::PulseGenerator gatePulse;
	dsp::PulseGenerator resetPulse;
	int stepCount = 0;
	const float lightLambda = 0.075;

	SimpleClock() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(RUN_PARAM, 0.0, 1.0, 0.0, "Run");
		configParam(CLOCK_PARAM, -2.0, 6.0, 1.0, "BPM", "", 2.f, 60.f);
		configParam(RESET_PARAM, 0.0, 1.0, 0.0, "Reset");
		configParam(PROB_PARAM, -2.0, 6.0, -2, "Random Reset Probability");
	}

	void process(const ProcessArgs &args) override;

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		json_object_set_new(rootJ, "clockMult", json_integer(clockMult));
		json_object_set_new(rootJ, "running", json_boolean(running));
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		clockMult = json_integer_value(json_object_get(rootJ, "clockMult"));
		if(clockMult < 1) clockMult = 1;//for old files without clockMult property
		json_t *runningJ = json_object_get(rootJ, "running");
		if (runningJ){
			running = json_is_true(runningJ);
		}
	}

	void onReset() override {
		clockMult = 1;
		resetClock();
	}

	void resetClock() {
		phase = 0.0;
		resetPulse.trigger(0.01);
		stepCount = 0;
	}
};

void SimpleClock::process(const ProcessArgs &args) {
	if (runningTrigger.process(params[RUN_PARAM].getValue())) {
		running = !running;
		if(running){
			resetClock();
		}
	}
	lights[RUNNING_LIGHT].value = running ? 1.0 : 0.0;

	if (resetTrigger.process(params[RESET_PARAM].getValue())){
		resetClock();
	}

	bool nextStep = false;
	if (running) {
		float clockTime = powf(2.0, params[CLOCK_PARAM].getValue());
		clockTime = clockTime * clockMult;
		phase += clockTime / args.sampleRate;
		if (phase >= 1.0) {
			phase -= 1.0;
			nextStep = true;
		}
	}
	if (nextStep) {
		stepCount = (stepCount + 1) % 256;
		float probScaled = rescalefjw(params[PROB_PARAM].getValue(), -2, 6, 0, 1);
		if(random::uniform() < probScaled){
			resetPulse.trigger(0.01);
		}
		gatePulse.trigger(1e-3);
	}

	bool gpulse = running && gatePulse.process(1.0 / args.sampleRate);
	bool rpulse = running && resetPulse.process(1.0 / args.sampleRate);
	outputs[RESET_OUTPUT].setVoltage(rpulse ? 10.0 : 0.0);
	outputs[CLOCK_OUTPUT].setVoltage(gpulse ? 10.0 : 0.0);
	outputs[DIV_4_OUTPUT].setVoltage(gpulse && (stepCount % 4 == 0) ? 10.0 : 0.0);
	outputs[DIV_8_OUTPUT].setVoltage(gpulse && (stepCount % 8 == 0) ? 10.0 : 0.0);
	outputs[DIV_16_OUTPUT].setVoltage(gpulse && (stepCount % 16 == 0) ? 10.0 : 0.0);
	outputs[DIV_32_OUTPUT].setVoltage(gpulse && (stepCount % 32 == 0) ? 10.0 : 0.0);
}

struct BPMKnob : SmallWhiteKnob {
	BPMKnob(){ paramQuantity = NULL; }
	std::string formatCurrentValue() override {
		if(paramQuantity != NULL){
			return std::to_string(static_cast<unsigned int>(round(powf(2.0, paramQuantity->getValue())*60.0))) + " BPM";
		}
		return "";
	}
};

struct SimpleClockWidget : ModuleWidget { 
	SimpleClockWidget(SimpleClock *module); 
	void appendContextMenu(Menu *menu) override;
};

SimpleClockWidget::SimpleClockWidget(SimpleClock *module) {
	setModule(module);
	box.size = Vec(RACK_GRID_WIDTH*4, RACK_GRID_HEIGHT);

	{
		SVGPanel *panel = new SVGPanel();
		panel->box.size = box.size;
		panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/WavHeadPanel.svg")));
		addChild(panel);
	}

	addChild(createWidget<Screw_J>(Vec(16, 2)));
	addChild(createWidget<Screw_J>(Vec(16, 365)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 2)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 365)));

	CenteredLabel* const titleLabel = new CenteredLabel(16);
	titleLabel->box.pos = Vec(15, 15);
	titleLabel->text = "Clock";
	addChild(titleLabel);

	addParam(createParam<TinyButton>(Vec(23, 40), module, SimpleClock::RUN_PARAM));
	addChild(createLight<SmallLight<MyBlueValueLight>>(Vec(23+3.75, 40+3.75), module, SimpleClock::RUNNING_LIGHT));
	
	BPMKnob *clockKnob = dynamic_cast<BPMKnob*>(createParam<BPMKnob>(Vec(17, 60), module, SimpleClock::CLOCK_PARAM));
	CenteredLabel* const bpmLabel = new CenteredLabel;
	bpmLabel->box.pos = Vec(15, 50);
	bpmLabel->text = "120 BPM";
	if(module){
		clockKnob->connectLabel(bpmLabel, module);
	}
	addChild(bpmLabel);
	addParam(clockKnob);

	addOutput(createOutput<PJ301MPort>(Vec(18, 105), module, SimpleClock::CLOCK_OUTPUT));

	CenteredLabel* const resetLabel = new CenteredLabel;
	resetLabel->box.pos = Vec(15, 75);
	resetLabel->text = "Reset";
	addChild(resetLabel);

	addParam(createParam<TinyButton>(Vec(23, 155), module, SimpleClock::RESET_PARAM));
	addOutput(createOutput<PJ301MPort>(Vec(18, 175), module, SimpleClock::RESET_OUTPUT));


	CenteredLabel* const rndResetLabel = new CenteredLabel(10);
	rndResetLabel->box.pos = Vec(15, 108);
	rndResetLabel->text = "Rnd Rst";
	addChild(rndResetLabel);
	addParam(createParam<SmallWhiteKnob>(Vec(17, 220), module, SimpleClock::PROB_PARAM));


	CenteredLabel* const div4Label = new CenteredLabel(10);
	div4Label->box.pos = Vec(8, 133);
	div4Label->text = "/4";
	addChild(div4Label);

	CenteredLabel* const div8Label = new CenteredLabel(10);
	div8Label->box.pos = Vec(21, 133);
	div8Label->text = "/8";
	addChild(div8Label);

	CenteredLabel* const div16Label = new CenteredLabel(10);
	div16Label->box.pos = Vec(8, 153);
	div16Label->text = "/16";
	addChild(div16Label);

	CenteredLabel* const div32Label = new CenteredLabel(10);
	div32Label->box.pos = Vec(21, 153);
	div32Label->text = "/32";
	addChild(div32Label);

	addOutput(createOutput<TinyPJ301MPort>(Vec(10, 270), module, SimpleClock::DIV_4_OUTPUT));
	addOutput(createOutput<TinyPJ301MPort>(Vec(34, 270), module, SimpleClock::DIV_8_OUTPUT));
	addOutput(createOutput<TinyPJ301MPort>(Vec(10, 310), module, SimpleClock::DIV_16_OUTPUT));
	addOutput(createOutput<TinyPJ301MPort>(Vec(34, 310), module, SimpleClock::DIV_32_OUTPUT));
}

struct ClockMultMenuItem : MenuItem {
	SimpleClock *sClock;
	int val;
	void onAction(const event::Action &e) override {
		sClock->clockMult = val;
	}
	void step() override {
		rightText = (sClock->clockMult == val) ? "✔" : "";
	}
};

void SimpleClockWidget::appendContextMenu(Menu *menu) {
	{	
		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);
	}
	SimpleClock *sClock = dynamic_cast<SimpleClock*>(module);
	{
		ClockMultMenuItem *item = new ClockMultMenuItem();
		item->text = "1/4 Notes";
		item->sClock = sClock;
		item->val = 1;
		menu->addChild(item);
	}
	{
		ClockMultMenuItem *item = new ClockMultMenuItem();
		item->text = "1/8 Notes";
		item->sClock = sClock;
		item->val = 2;
		menu->addChild(item);
	}
	{
		ClockMultMenuItem *item = new ClockMultMenuItem();
		item->text = "1/16 Notes";
		item->sClock = sClock;
		item->val = 4;
		menu->addChild(item);
	}
	{
		ClockMultMenuItem *item = new ClockMultMenuItem();
		item->text = "1/32 Notes";
		item->sClock = sClock;
		item->val = 8;
		menu->addChild(item);
	}
	{
		ClockMultMenuItem *item = new ClockMultMenuItem();
		item->text = "1/64 Notes";
		item->sClock = sClock;
		item->val = 16;
		menu->addChild(item);
	}
}
Model *modelSimpleClock = createModel<SimpleClock, SimpleClockWidget>("SimpleClock");
