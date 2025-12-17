#include "JWModules.hpp"

struct Timer : Module {
	enum ParamIds {
		RESET_PARAM,
		RUN_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		RESET_INPUT,
		RUN_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		CLOCK_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};
	dsp::SchmittTrigger runTrigger;
	dsp::SchmittTrigger resetTrigger;
	dsp::PulseGenerator gatePulse;

	float phase = 0.0;
	int seconds = 0;
	int refreshRate = 0;
	bool running = false;
	bool resetMode = false;
    bool initialRowSet = false;

	Timer() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(RUN_PARAM, 0.0, 1.0, 0.0, "Start / Stop");
		configParam(RESET_PARAM, 0.0, 1.0, 0.0, "Reset");
		configInput(RUN_INPUT, "Start / Stop");
		configInput(RESET_INPUT, "Reset");
		configOutput(CLOCK_OUTPUT, "Trigger every second");
	}

	void process(const ProcessArgs &args) override;

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		json_object_set_new(rootJ, "running", json_boolean(running));
		json_object_set_new(rootJ, "seconds", json_integer(seconds));

		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		json_t *runningJ = json_object_get(rootJ, "running");
		if (runningJ)
			running = json_is_true(runningJ);

		json_t *secondsJ = json_object_get(rootJ, "seconds");
		if (secondsJ)
			seconds = json_integer_value(secondsJ);
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// STEP
///////////////////////////////////////////////////////////////////////////////////////////////////
void Timer::process(const ProcessArgs &args) {

	if (resetTrigger.process(inputs[RESET_INPUT].getVoltage()+params[RESET_PARAM].getValue())) {
		phase = 0.0;
		seconds = 0;
	}

	if (runTrigger.process(inputs[RUN_INPUT].getVoltage()+params[RUN_PARAM].getValue())) {
		running = !running;
	}

	if(running){
		phase += 1.0 / args.sampleRate;
		if(phase >= 1.0){
			seconds++;
			phase = 0;
			gatePulse.trigger(1e-3);
		}
	}

	bool gpulse = running && gatePulse.process(1.0 / args.sampleRate);
	outputs[CLOCK_OUTPUT].setVoltage(gpulse ? 10.0 : 0.0);
};

struct TimerWidget : ModuleWidget {
	TimerWidget(Timer *module);
	~TimerWidget(){ }
	void appendContextMenu(Menu *menu) override;
};

struct TimeDisplay : LightWidget {
	Timer *module;

	TimeDisplay() {}

	void drawLayer(const DrawArgs &args, int layer) override {
		nvgFillColor(args.vg, nvgRGB(0, 0, 0));
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
		nvgFill(args.vg);

		if(module == NULL) return;

		if(layer == 1){
			nvgFillColor(args.vg, nvgRGB(25, 150, 252)); //blue
			nvgFontSize(args.vg, 20);
			int hours = module->seconds / 3600;
			int minutes = (module->seconds - hours * 3600) / 60;
			int seconds = (module->seconds - hours * 3600) % 60;
			std::string hoursStr = std::to_string(static_cast<unsigned int>(hours)) + "h";
			std::string minutesStr = std::to_string(static_cast<unsigned int>(minutes)) + "m";
			std::string secondsStr = std::to_string(static_cast<unsigned int>(seconds)) + "s";
			nvgText(args.vg, 10, 25, hoursStr.c_str(), NULL);
			nvgText(args.vg, 10, 50, minutesStr.c_str(), NULL);
			nvgText(args.vg, 10, 75, secondsStr.c_str(), NULL);
		}
		Widget::drawLayer(args, layer);
	}
};

TimerWidget::TimerWidget(Timer *module) {
	setModule(module);
	box.size = Vec(RACK_GRID_WIDTH*4, RACK_GRID_HEIGHT);

	setPanel(createPanel(
		asset::plugin(pluginInstance, "res/Timer.svg"), 
		asset::plugin(pluginInstance, "res/Timer.svg")
	));

	addChild(createWidget<Screw_J>(Vec(16, 2)));
	addChild(createWidget<Screw_J>(Vec(16, 365)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 2)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 365)));

    TimeDisplay* timeDisplay = createWidget<TimeDisplay>(Vec(5, 30));
    timeDisplay->box.size = Vec(50, 90);
    timeDisplay->module = module;
    addChild(timeDisplay);

	///// RUN /////
	addParam(createParam<TinyButton>(Vec(22.5, 157), module, Timer::RUN_PARAM));
	addInput(createInput<PJ301MPort>(Vec(18, 175), module, Timer::RUN_INPUT));

	///// RESET /////
	addParam(createParam<TinyButton>(Vec(22.5, 227), module, Timer::RESET_PARAM));
	addInput(createInput<PJ301MPort>(Vec(18, 244), module, Timer::RESET_INPUT));

	addOutput(createOutput<PJ301MPort>(Vec(18, 295), module, Timer::CLOCK_OUTPUT));

};

void TimerWidget::appendContextMenu(Menu *menu) {}

Model *modelTimer = createModel<Timer, TimerWidget>("Timer");
