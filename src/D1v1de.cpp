#include "JWModules.hpp"

struct D1v1de : Module {
	enum ParamIds {
		DIV_PARAM,
		COLOR_PARAM,
		OFFSET_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CLOCK_INPUT,
		RESET_INPUT,
		DIV_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		CLOCK_OUTPUT,
		POS_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	int ticks = 0;
	float smpRate = APP->engine->getSampleRate();	
	float oneOverRate = 1.0 / smpRate;	
	bool resetMode = false;
	dsp::SchmittTrigger resetTrig;
	dsp::SchmittTrigger clockTrig;
	dsp::PulseGenerator gatePulse;

	// Gate pulse length in seconds (controls CLOCK_OUTPUT pulse width)
	float gatePulseLenSec = 0.005f;

	D1v1de() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(DIV_PARAM, 1, 64, 4, "Division");
		configParam(OFFSET_PARAM, 0, 64, 64, "Offset");
		configInput(CLOCK_INPUT, "Clock");
		configInput(RESET_INPUT, "Reset");
		configInput(DIV_INPUT, "Division");
		configOutput(CLOCK_OUTPUT, "Clock");
		configOutput(POS_OUTPUT, "Position");
		configBypass(CLOCK_INPUT, CLOCK_OUTPUT);
	}

	void onSampleRateChange() override {
		smpRate = APP->engine->getSampleRate();	
		oneOverRate = 1.0 / smpRate;
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		json_object_set_new(rootJ, "blockColor", json_integer(int(params[COLOR_PARAM].getValue())));
		json_object_set_new(rootJ, "gatePulseLenSec", json_real(gatePulseLenSec));
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		params[COLOR_PARAM].setValue(json_integer_value(json_object_get(rootJ, "blockColor")));
		json_t *gatePulseLenSecJ = json_object_get(rootJ, "gatePulseLenSec");
		if (gatePulseLenSecJ) {
			gatePulseLenSec = (float) json_number_value(gatePulseLenSecJ);
			gatePulseLenSec = clampfjw(gatePulseLenSec, 0.001f, 1.0f);
		}
	}

	void onReset() override {
		params[COLOR_PARAM].setValue(0);
		resetSeq();
	}

	void resetSeq() {
		ticks = 0;
	}

	int getDivInt(){
		int divInt = 1;
		if(inputs[DIV_INPUT].isConnected()){
			divInt = round(rescalefjw(inputs[DIV_INPUT].getVoltage(),0, 10, 1, 64));
		} else {
			divInt = round(params[DIV_PARAM].getValue());
		}
		return clampijw(divInt, 1, 64);
	}

	int getOffsetInt(){
		return int(params[OFFSET_PARAM].getValue());
	}

	void onRandomize() override {
		params[COLOR_PARAM].setValue(int(random::uniform()*4));
	}

	void process(const ProcessArgs &args) override {
		bool pulseOut = false;
		if (resetTrig.process(inputs[RESET_INPUT].getVoltage())) {
			resetMode = true;
		}
		if (clockTrig.process(inputs[CLOCK_INPUT].getVoltage())) {
			if(resetMode){
				resetMode = false;
				ticks = -1;
			}
			int divInt = getDivInt();
			int offsetInt = getOffsetInt();
			ticks++;
				if(ticks == offsetInt){
				pulseOut = true;
					gatePulse.trigger(gatePulseLenSec);
			}
			if(ticks == divInt){
				if(!pulseOut){
					pulseOut = true;
						gatePulse.trigger(gatePulseLenSec);
				}
			}
			if(ticks >= divInt){
				resetSeq();
			}
			outputs[POS_OUTPUT].setVoltage(rescalefjw(ticks, 0.0, divInt-1, 0.0, 10.0));
		}
		bool pulse = gatePulse.process(oneOverRate);
		outputs[CLOCK_OUTPUT].setVoltage(pulse ? 10.0 : 0.0);
	}

};

struct D1v1deDisplay : LightWidget {
	D1v1de *module;
	D1v1deDisplay(){}

	void drawLayer(const DrawArgs &args, int layer) override {
		nvgGlobalTint(args.vg, color::WHITE);
		//background
		nvgFillColor(args.vg, nvgRGB(0, 0, 0));
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
		nvgFill(args.vg);

		if(layer == 1){
			int divInt = module ? module->getDivInt() : 4;
			float rowHeight = box.size.y / divInt;

			//grid
			nvgStrokeColor(args.vg, nvgRGB(60, 70, 73));
			for(int i=1;i<divInt;i++){
				nvgStrokeWidth(args.vg, 1);
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg, 0, i * rowHeight);
				nvgLineTo(args.vg, box.size.x, i * rowHeight);
				nvgStroke(args.vg);
			}

			if(!module){ return; }

			//offset
			if(module->params[D1v1de::OFFSET_PARAM].getValue() > 0){
				float offsetTopY = module->getOffsetInt() * rowHeight;
				nvgFillColor(args.vg, nvgRGB(60, 70, 73));//line color
				if(offsetTopY+rowHeight < box.size.y+2){
					nvgBeginPath(args.vg);
					nvgRect(args.vg, 0, offsetTopY, box.size.x, rowHeight);
					nvgFill(args.vg);
				}
			}

			//block
			switch(int(module->params[D1v1de::COLOR_PARAM].getValue())){
				case 0: nvgFillColor(args.vg, nvgRGB(25, 150, 252)); break;//blue
				case 1: nvgFillColor(args.vg, nvgRGB(255, 151, 9)); break;//orange
				case 2: nvgFillColor(args.vg, nvgRGB(255, 243, 9)); break;//yellow
				case 3: nvgFillColor(args.vg, nvgRGB(144, 26, 252)); break;//purple
			}

			float topY = module->ticks * rowHeight;
			if(topY+rowHeight < box.size.y+2){
				nvgBeginPath(args.vg);
				nvgRect(args.vg, 0, topY, box.size.x, rowHeight);
				nvgFill(args.vg);
			}
		}
		Widget::drawLayer(args, layer);
	}
};

struct D1v1deWidget : ModuleWidget {
	D1v1deWidget(D1v1de *module);
	JwSmallSnapKnob *divKnob;
	void appendContextMenu(Menu *menu) override;
	void step() override;
};

void D1v1deWidget::step() {
	ModuleWidget::step();
	D1v1de *d1v = dynamic_cast<D1v1de*>(module);
	if(d1v && d1v->inputs[D1v1de::DIV_INPUT].isConnected()){
		divKnob->getParamQuantity()->setValue(d1v->getDivInt());
		divKnob->step();
	}
}

D1v1deWidget::D1v1deWidget(D1v1de *module) {
	setModule(module);
	box.size = Vec(RACK_GRID_WIDTH*8, RACK_GRID_HEIGHT);

	setPanel(createPanel(
		asset::plugin(pluginInstance, "res/D1v1de.svg"), 
		asset::plugin(pluginInstance, "res/dark/D1v1de.svg")
	));

	D1v1deDisplay *display = new D1v1deDisplay();
	display->module = module;
	display->box.pos = Vec(0, 15);
	display->box.size = Vec(box.size.x, 250);
	addChild(display);

	addChild(createWidget<Screw_J>(Vec(16, 2)));
	addChild(createWidget<Screw_J>(Vec(16, 365)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 2)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 365)));

	divKnob = dynamic_cast<JwSmallSnapKnob*>(createParam<JwSmallSnapKnob>(Vec(35, 278), module, D1v1de::DIV_PARAM));
	CenteredLabel* const divLabel = new CenteredLabel(12);
	divLabel->box.pos = Vec(24, 159);
	divLabel->text = "";
	addChild(divLabel);
	divKnob->connectLabel(divLabel, module);
	addParam(divKnob);

	SmallWhiteKnob *offsetKnob = dynamic_cast<SmallWhiteKnob*>(createParam<SmallWhiteKnob>(Vec(88, 278), module, D1v1de::OFFSET_PARAM));
	CenteredLabel* const offsetLabel = new CenteredLabel(12);
	offsetLabel->box.pos = Vec(51, 159);
	offsetLabel->text = "";
	addChild(offsetLabel);
	offsetKnob->connectLabel(offsetLabel, module);
	addParam(offsetKnob);

	addInput(createInput<TinyPJ301MPort>(Vec(13, 283), module, D1v1de::RESET_INPUT));
	addInput(createInput<TinyPJ301MPort>(Vec(65, 283), module, D1v1de::DIV_INPUT));

	addInput(createInput<TinyPJ301MPort>(Vec(13, 345), module, D1v1de::CLOCK_INPUT));
	addOutput(createOutput<TinyPJ301MPort>(Vec(65.5, 345), module, D1v1de::POS_OUTPUT));
	addOutput(createOutput<TinyPJ301MPort>(Vec(94, 345), module, D1v1de::CLOCK_OUTPUT));
}

struct ColorMenuItem : MenuItem {
	int val;
	D1v1de *module;
	void onAction(const event::Action &e) override {
		module->params[D1v1de::COLOR_PARAM].setValue(val);
	}
	void step() override {
		rightText = (int(module->params[D1v1de::COLOR_PARAM].getValue()) == val) ? "✔" : "";
	}
};

// Context menu slider to control CLOCK_OUTPUT pulse length (milliseconds)
struct D1vGatePulseLengthQuantity : Quantity {
	D1v1de* module = nullptr;
	void setValue(float value) override {
		if (!module) return;
		module->gatePulseLenSec = clampfjw(value, getMinValue(), getMaxValue());
	}
	float getValue() override {
		return module ? module->gatePulseLenSec : getDefaultValue();
	}
	float getMinValue() override { return 0.001f; } // 1 ms
	float getMaxValue() override { return 1.0f; }   // 1000 ms
	float getDefaultValue() override { return 0.005f; }
	float getDisplayValue() override { return getValue() * 1000.f; }
	void setDisplayValue(float displayValue) override { setValue(displayValue / 1000.f); }
	int getDisplayPrecision() override { return 0; }
	std::string getLabel() override { return "Gate Pulse Length"; }
	std::string getUnit() override { return "ms"; }
};

struct D1vGatePulseLengthSlider : ui::Slider {
	D1vGatePulseLengthSlider() {
		quantity = new D1vGatePulseLengthQuantity;
	}
	~D1vGatePulseLengthSlider() {
		delete quantity;
	}
};

void D1v1deWidget::appendContextMenu(Menu *menu) {
	{	
		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);
	}

	D1v1de *d1v = dynamic_cast<D1v1de*>(module);
	{
		ColorMenuItem *item = new ColorMenuItem();
		item->text = "Blue";
		item->val = 0;
		item->module = d1v;
		menu->addChild(item);
	}
	{
		ColorMenuItem *item = new ColorMenuItem();
		item->text = "Orange";
		item->val = 1;
		item->module = d1v;
		menu->addChild(item);
	}
	{
		ColorMenuItem *item = new ColorMenuItem();
		item->text = "Yellow";
		item->val = 2;
		item->module = d1v;
		menu->addChild(item);
	}
	{
		ColorMenuItem *item = new ColorMenuItem();
		item->text = "Purple";
		item->val = 3;
		item->module = d1v;
		menu->addChild(item);
	}

	// Gate pulse length slider
	MenuLabel *gatePulseLabel = new MenuLabel();
	gatePulseLabel->text = "Gate Pulse Length";
	menu->addChild(gatePulseLabel);

	D1vGatePulseLengthSlider* gateSlider = new D1vGatePulseLengthSlider();
	static_cast<D1vGatePulseLengthQuantity*>(gateSlider->quantity)->module = d1v;
	gateSlider->box.size.x = 220.0f;
	menu->addChild(gateSlider);
}

Model *modelD1v1de = createModel<D1v1de, D1v1deWidget>("D1v1de");