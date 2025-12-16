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

	// Gate pulse length in seconds for CLOCK_OUTPUT and divisions
	float gatePulseLenSec = 0.005f;
	// Reset pulse length in seconds for RESET_OUTPUT
	float resetPulseLenSec = 0.01f;

	SimpleClock() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(RUN_PARAM, 0.0, 1.0, 0.0, "Run");
		configParam(CLOCK_PARAM, -2.0, 6.0, 1.0, "BPM", "", 2.f, 60.f);
		configParam(RESET_PARAM, 0.0, 1.0, 0.0, "Reset");
		configParam(PROB_PARAM, -2.0, 6.0, -2, "Random Reset Probability");
		configOutput(CLOCK_OUTPUT, "Clock");
		configOutput(RESET_OUTPUT, "Reset");
		configOutput(DIV_4_OUTPUT, "/4");
		configOutput(DIV_8_OUTPUT, "/8");
		configOutput(DIV_16_OUTPUT, "/16");
		configOutput(DIV_32_OUTPUT, "/32");
	}

	void process(const ProcessArgs &args) override;

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		json_object_set_new(rootJ, "clockMult", json_integer(clockMult));
		json_object_set_new(rootJ, "running", json_boolean(running));
		// gate pulse length
		json_object_set_new(rootJ, "gatePulseLenSec", json_real(gatePulseLenSec));
		// reset pulse length
		json_object_set_new(rootJ, "resetPulseLenSec", json_real(resetPulseLenSec));
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		clockMult = json_integer_value(json_object_get(rootJ, "clockMult"));
		if(clockMult < 1) clockMult = 1;//for old files without clockMult property
		json_t *runningJ = json_object_get(rootJ, "running");
		if (runningJ){
			running = json_is_true(runningJ);
		}
		// gate pulse length
		json_t *gatePulseLenSecJ = json_object_get(rootJ, "gatePulseLenSec");
		if (gatePulseLenSecJ) {
			gatePulseLenSec = (float) json_number_value(gatePulseLenSecJ);
			gatePulseLenSec = clampfjw(gatePulseLenSec, 0.001f, 1.0f);
		}
		// reset pulse length
		json_t *resetPulseLenSecJ = json_object_get(rootJ, "resetPulseLenSec");
		if (resetPulseLenSecJ) {
			resetPulseLenSec = (float) json_number_value(resetPulseLenSecJ);
			resetPulseLenSec = clampfjw(resetPulseLenSec, 0.001f, 1.0f);
		}
	}

	void onReset() override {
		clockMult = 1;
		resetClock();
	}

	void resetClock() {
		phase = 0.0;
		resetPulse.trigger(resetPulseLenSec);
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
			resetPulse.trigger(resetPulseLenSec);
		}
		gatePulse.trigger(gatePulseLenSec);
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
	BPMKnob(){ 
		// ParamQuantity* paramQuantity = getParamQuantity();
		// paramQuantity = NULL; 
	}
	std::string formatCurrentValue() override {
		if(getParamQuantity() != NULL){
			return std::to_string(static_cast<unsigned int>(round(powf(2.0, getParamQuantity()->getValue())*60.0))) + " BPM";
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
	
	setPanel(createPanel(
		asset::plugin(pluginInstance, "res/SimpleClock.svg"), 
		asset::plugin(pluginInstance, "res/dark/SimpleClock.svg")
	));

	addChild(createWidget<Screw_J>(Vec(16, 2)));
	addChild(createWidget<Screw_J>(Vec(16, 365)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 2)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 365)));

	addParam(createParam<TinyButton>(Vec(23, 40), module, SimpleClock::RUN_PARAM));
	addChild(createLight<SmallLight<MyBlueValueLight>>(Vec(23+3.75, 40+3.75), module, SimpleClock::RUNNING_LIGHT));
	
	BPMKnob *clockKnob = dynamic_cast<BPMKnob*>(createParam<BPMKnob>(Vec(17, 60), module, SimpleClock::CLOCK_PARAM));
	CenteredLabel* const bpmLabel = new CenteredLabel;
	bpmLabel->box.pos = Vec(15, 50);
	clockKnob->connectLabel(bpmLabel, module);
	addChild(bpmLabel);
	addParam(clockKnob);

	addOutput(createOutput<PJ301MPort>(Vec(18, 105), module, SimpleClock::CLOCK_OUTPUT));

	addParam(createParam<TinyButton>(Vec(23, 155), module, SimpleClock::RESET_PARAM));
	addOutput(createOutput<PJ301MPort>(Vec(18, 175), module, SimpleClock::RESET_OUTPUT));

	addParam(createParam<SmallWhiteKnob>(Vec(17, 220), module, SimpleClock::PROB_PARAM));

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

	// Gate pulse length slider
	{
		MenuLabel *spacerLabelGate = new MenuLabel();
		menu->addChild(spacerLabelGate);
		MenuLabel *gatePulseLabel = new MenuLabel();
		gatePulseLabel->text = "Gate Pulse Length";
		menu->addChild(gatePulseLabel);
	}

	{
		GatePulseMsSlider* gateSlider = new GatePulseMsSlider();
		{
			auto qp = static_cast<GatePulseMsQuantity*>(gateSlider->quantity);
			qp->getSeconds = [sClock](){ return sClock->gatePulseLenSec; };
			qp->setSeconds = [sClock](float v){ sClock->gatePulseLenSec = v; };
			qp->defaultSeconds = 0.1f;
			qp->label = "Gate Pulse Length";
		}
		gateSlider->box.size.x = 220.0f;
		menu->addChild(gateSlider);
	}

	// Reset pulse length slider
	{
		MenuLabel *spacerLabelReset = new MenuLabel();
		menu->addChild(spacerLabelReset);
		MenuLabel *resetPulseLabel = new MenuLabel();
		resetPulseLabel->text = "Reset Pulse Length";
		menu->addChild(resetPulseLabel);

		GatePulseMsSlider* resetSlider = new GatePulseMsSlider();
		{
			auto qp = static_cast<GatePulseMsQuantity*>(resetSlider->quantity);
			qp->getSeconds = [sClock](){ return sClock->resetPulseLenSec; };
			qp->setSeconds = [sClock](float v){ sClock->resetPulseLenSec = v; };
			qp->defaultSeconds = 0.01f;
			qp->label = "Reset Pulse Length";
		}
		resetSlider->box.size.x = 220.0f;
		menu->addChild(resetSlider);
	}
}
Model *modelSimpleClock = createModel<SimpleClock, SimpleClockWidget>("SimpleClock");
