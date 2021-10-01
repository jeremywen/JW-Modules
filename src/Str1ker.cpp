#include "JWModules.hpp"
#include "../lib/oscpack/osc/OscOutboundPacketStream.h"
#include "../lib/oscpack/ip/UdpSocket.h"


#define ADDRESS "127.0.0.1"
#define OUTPUT_BUFFER_SIZE 1024

struct Str1ker : Module {
	enum ParamIds {
		CLOCK_100s_PARAM,
		CLOCK_10s_PARAM,
		CLOCK_1s_PARAM,
		CLOCK_DOT_100s_PARAM,
		ON_OFF_SWITCH,
		FADER_RANGE_PARAM,
		RESET_BTN_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CLOCK_100s_INPUT,
		CLOCK_10s_INPUT,
		CLOCK_1s_INPUT,
		CLOCK_DOT_100s_INPUT,
		BPM_INPUT,
		FADER_INPUT,
		RESET_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		RESET_OUTPUT,
		CLOCK_OUTPUT,
		BPM_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	bool running = true;
	bool lastStepWasRunning = false;
	bool oscOn = false;
	float phase = 0.0;
	float smpRate = APP->engine->getSampleRate();	
	float oneOverRate = 1.0 / smpRate;	
	float totalBpm = 120.0;
	float faderVal = 0;
	int clockMult = 4;
	int oscPort = 7013;
	dsp::SchmittTrigger runningTrigger;
	dsp::SchmittTrigger resetTrigger;
	dsp::PulseGenerator gatePulse;
	dsp::PulseGenerator resetPulse;
	IpEndpointName ipEndpointName = IpEndpointName(ADDRESS, oscPort);
	UdpTransmitSocket transmitSocket = UdpTransmitSocket(ipEndpointName);

	Str1ker() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(CLOCK_100s_PARAM, 0.0, 10.0, 1.0, "100's Value");
		configParam(CLOCK_10s_PARAM, 0.0, 10.0, 2.0, "10's Value");
		configParam(CLOCK_1s_PARAM, 0.0, 10.0, 0.0, "1's Value");
		configParam(CLOCK_DOT_100s_PARAM, 0, 128.0, 64.0, ".00's Value");
		configParam(ON_OFF_SWITCH, 0.0, 1.0, 1.0, "On/Off");
		configParam(RESET_BTN_PARAM, 0.0, 1.0, 0.0, "Reset");
		configParam(FADER_RANGE_PARAM, 1, 50, 1, "Fader Range");
		paramQuantities[CLOCK_100s_PARAM]->snapEnabled = true;
		paramQuantities[CLOCK_10s_PARAM]->snapEnabled = true;
		paramQuantities[CLOCK_1s_PARAM]->snapEnabled = true;
		transmitSocket.SetAllowReuse(true);
	}

	void updateOscPort(){
		if(oscOn){
			ipEndpointName.port = oscPort;
			transmitSocket.Connect(ipEndpointName);
		}
	}

	void sendBpmOverOsc(){
		if(oscOn){
			// EXAMPLE: https://github.com/MariadeAnton/oscpack/blob/master/examples/SimpleSend.cpp

			char buffer[OUTPUT_BUFFER_SIZE];
			osc::OutboundPacketStream p( buffer, OUTPUT_BUFFER_SIZE );
			
			p << osc::BeginBundleImmediate
				<< osc::BeginMessage( "/bpm" ) << totalBpm << osc::EndMessage
				<< osc::EndBundle;
			
			transmitSocket.Send( p.Data(), p.Size() );
		}
	}

	void onSampleRateChange() override {
		smpRate = APP->engine->getSampleRate();	
		oneOverRate = 1.0 / smpRate;	
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		json_object_set_new(rootJ, "clockMult", json_integer(clockMult));
		json_object_set_new(rootJ, "faderVal", json_real(faderVal));
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		clockMult = json_integer_value(json_object_get(rootJ, "clockMult"));
		faderVal = json_real_value(json_object_get(rootJ, "faderVal"));
	}

	void onReset() override {
		resetFader();
		clockMult = 4;
		resetClock();
	}

	void resetFader() {
		faderVal = 0;
	}

	void resetClock() {
		phase = 0.0;
		resetPulse.trigger(0.01);
	}

	void process(const ProcessArgs &args) override {
		bool nextStep = false;
		running = params[ON_OFF_SWITCH].getValue();
		if ((running && !lastStepWasRunning) || resetTrigger.process(params[RESET_BTN_PARAM].getValue() + inputs[RESET_INPUT].getVoltage())) {
			resetClock();
			nextStep = true;
		}

		if(inputs[FADER_INPUT].isConnected()){
			faderVal = clampfjw(rescalefjw(inputs[FADER_INPUT].getVoltage(), 0.0, 10.0, -0.5, 0.5), -0.5, 0.5);
		}

		if(useBpmIn()){
			setBpmParamsFromInput();
		}

		updateTotalBpm();
		if (running) {
			phase += (totalBpm * clockMult / 60.0) / smpRate;
			if (phase >= 1.0) {
				phase -= 1.0;
				nextStep = true;
			}
		}
		if (nextStep) {
			gatePulse.trigger(1e-3);
			sendBpmOverOsc();
		}

		outputs[RESET_OUTPUT].setVoltage(resetPulse.process(oneOverRate) ? 10.0 : 0.0);
		//do i not send out a clock when there is a reset????????????????????????????????????????????????????????
		outputs[CLOCK_OUTPUT].setVoltage(running && gatePulse.process(oneOverRate) ? 10.0 : 0.0);
		if(outputs[BPM_OUTPUT].isConnected()){
			outputs[BPM_OUTPUT].setVoltage(rescalefjw(totalBpm, 0.0, 1000.0, 0.0, 10.0));
		}
		lastStepWasRunning = running;
	}

	bool useBpmIn(){
		return inputs[BPM_INPUT].isConnected() && inputs[BPM_INPUT].getVoltage() > 0;
	}

	void setBpmParamsFromInput(){
		float bpmIn = rescalefjw(inputs[BPM_INPUT].getVoltage(), 0.0, 10.0, 0.0, 1000.0);

		params[CLOCK_100s_PARAM].setValue(int(bpmIn) / 100);
		bpmIn -= params[CLOCK_100s_PARAM].getValue() * 100;

		params[CLOCK_10s_PARAM].setValue(int(bpmIn) / 10);
		bpmIn -= params[CLOCK_10s_PARAM].getValue() * 10;

		params[CLOCK_1s_PARAM].setValue(int(bpmIn));
		bpmIn -= params[CLOCK_1s_PARAM].getValue();

		params[CLOCK_DOT_100s_PARAM].setValue(rescalefjw(bpmIn, -0.64, 0.64, 0.0, 128.0));
	}

	void updateTotalBpm(){
		float clock100s = 0;
		float clock10s = 0;
		float clock1s = 0;
		float clockDot100s = 0;
		/////////////////////////////////////////////
		if(inputs[CLOCK_100s_INPUT].isConnected()){
			clock100s = getKnobValForInput(CLOCK_100s_INPUT);
		} else {
			clock100s = params[CLOCK_100s_PARAM].getValue();
		}
		clock100s *= 100;
		/////////////////////////////////////////////
		if(inputs[CLOCK_10s_INPUT].isConnected()){
			clock10s = getKnobValForInput(CLOCK_10s_INPUT);
		} else {
			clock10s = params[CLOCK_10s_PARAM].getValue();
		}
		clock10s *= 10;
		/////////////////////////////////////////////
		if(inputs[CLOCK_1s_INPUT].isConnected()){
			clock1s = getKnobValForInput(CLOCK_1s_INPUT);
		} else {
			clock1s = params[CLOCK_1s_PARAM].getValue();
		}
		/////////////////////////////////////////////
		if(inputs[CLOCK_DOT_100s_INPUT].isConnected()){
			clockDot100s = getKnobValForInput(CLOCK_DOT_100s_INPUT);
		} else {
			clockDot100s = params[CLOCK_DOT_100s_PARAM].getValue();
		}
		clockDot100s = rescalefjw(clockDot100s, 0.0, 128.0, -0.64, 0.64);
		/////////////////////////////////////////////
		totalBpm = clock100s + clock10s + clock1s + clockDot100s;
		totalBpm += totalBpm * (faderVal * (params[FADER_RANGE_PARAM].getValue() / 50.0));
	}

	float getKnobValForInput(int i){
		switch(i){
			case CLOCK_100s_INPUT:{
				return roundf(clampfjw(inputs[CLOCK_100s_INPUT].getVoltage(), 0.0, 10.0));
				break;
			}
			case CLOCK_10s_INPUT:{
				return roundf(clampfjw(inputs[CLOCK_10s_INPUT].getVoltage(), 0.0, 10.0));
				break;
			}
			case CLOCK_1s_INPUT:{
				return roundf(clampfjw(inputs[CLOCK_1s_INPUT].getVoltage(), 0.0, 10.0));
				break;
			}
			case CLOCK_DOT_100s_INPUT:{
				return rescalefjw(clampfjw(inputs[CLOCK_DOT_100s_INPUT].getVoltage(), 0.0, 10.0), 0.0, 10.0, 0.0, 128.0);
				break;
			}
		}
		return 0;
	}

	std::string bpmString() {
		char text[128];
		snprintf(text, sizeof(text), "%5.2f", totalBpm);
		return text;
	}

};

struct Str1kerWidget : ModuleWidget { 
	Str1kerWidget(Str1ker *module);
	CenteredLabel* const totalBpmLabel = new CenteredLabel(18);
	void appendContextMenu(Menu *menu) override;
	BPMPartKnob* knobs[4];
	void step() override;
};

void Str1kerWidget::step() {
	ModuleWidget::step();
	Str1ker *str1ker = dynamic_cast<Str1ker*>(module);
	if(str1ker){
		for(int i=0;i<4;i++){
			if(str1ker->useBpmIn()){
				//update knobs
				knobs[i]->getParamQuantity()->setValue(str1ker->params[Str1ker::CLOCK_100s_INPUT + i].getValue());
				// knobs[i]->dirty = true;//TODO FIX?
				knobs[i]->step();
			} else if(str1ker->inputs[Str1ker::CLOCK_100s_INPUT + i].isConnected()){
				//move the knob based on input val
				knobs[i]->getParamQuantity()->setValue(str1ker->getKnobValForInput(Str1ker::CLOCK_100s_INPUT + i));
				// knobs[i]->dirty = true;//TODO FIX?
				knobs[i]->step();
			}
		}
		str1ker->updateTotalBpm();
		totalBpmLabel->text = str1ker->bpmString();
	}
}

struct FaderDisplay : LightWidget {
	Str1ker *module;
	float initY = 0;
	float dragY = 0;
	FaderDisplay(){}

	void onButton(const event::Button &e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			e.consume(this);
			initY = e.pos.y;
		} else if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_RIGHT) {
			e.consume(this);
			module->resetFader();
		}
	}

	// void onDoubleClick(const event::DoubleClick &e) override {
	// 	e.consume(this);
	// 	module->resetFader();
	// }
	
	void onDragStart(const event::DragStart &e) override {
		if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
			dragY = APP->scene->mousePos.y;
		}
	}

	void onDragMove(const event::DragMove &e) override {
		if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
			float newDragY = APP->scene->mousePos.y;
			module->faderVal = 0.5 - (clampfjw(initY + (newDragY - dragY) - 30.0, 0.0, 180.0) / 180.0);
		}
	}

	void draw(const DrawArgs &args) override {
		//line
		nvgFillColor(args.vg, nvgRGB(255, 255, 255));
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 10, 15, 1, 220);
		nvgFill(args.vg);

		if(!module){ return; }

		//handle?
		nvgFillColor(args.vg, nvgRGB(25, 150, 252));//blue
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 5, 15 + ((0.5-module->faderVal) * 180), 10, 40);
		nvgFill(args.vg);

	}
};

Str1kerWidget::Str1kerWidget(Str1ker *module) {
	setModule(module);
	box.size = Vec(RACK_GRID_WIDTH*8, RACK_GRID_HEIGHT);

	{
		SVGPanel *panel = new SVGPanel();
		panel->box.size = box.size;
		panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Str1ker.svg")));
		addChild(panel);
	}

	addChild(createWidget<Screw_J>(Vec(16, 2)));
	addChild(createWidget<Screw_J>(Vec(16, 365)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 2)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 365)));

	FaderDisplay *display = new FaderDisplay();
	display->module = module;
	display->box.pos = Vec(57, 80);
	display->box.size = Vec(21, 250);
	addChild(display);

	totalBpmLabel->box.pos = Vec(21, 30);
	addChild(totalBpmLabel);

	/////////////////////////////////// LEFT SIDE ///////////////////////////////////
	{
		addInput(createInput<TinyPJ301MPort>(Vec(3, 101), module, Str1ker::CLOCK_100s_INPUT));
		knobs[0] = dynamic_cast<BPMPartKnob*>(createParam<BPMPartKnob>(Vec(20, 96), module, Str1ker::CLOCK_100s_PARAM));
		knobs[0]->connectLabel(totalBpmLabel, module);
		addParam(knobs[0]);
	}
	{
		addInput(createInput<TinyPJ301MPort>(Vec(3, 161), module, Str1ker::CLOCK_10s_INPUT));
		knobs[1] = dynamic_cast<BPMPartKnob*>(createParam<BPMPartKnob>(Vec(20, 156), module, Str1ker::CLOCK_10s_PARAM));
		knobs[1]->connectLabel(totalBpmLabel, module);
		addParam(knobs[1]);
	}
	{
		addInput(createInput<TinyPJ301MPort>(Vec(3, 222), module, Str1ker::CLOCK_1s_INPUT));
		knobs[2] = dynamic_cast<BPMPartKnob*>(createParam<BPMPartKnob>(Vec(20, 217), module, Str1ker::CLOCK_1s_PARAM));
		knobs[2]->connectLabel(totalBpmLabel, module);
		addParam(knobs[2]);
	}
	{
		addInput(createInput<TinyPJ301MPort>(Vec(3, 281), module, Str1ker::CLOCK_DOT_100s_INPUT));
		//NOTE: midi might not reach last vals because it is 0-127 and we have 129 (-0.64 to +0.64) but whatever
		knobs[3] = dynamic_cast<BPMPartKnob*>(createParam<BPMPartKnob>(Vec(20, 276), module, Str1ker::CLOCK_DOT_100s_PARAM));
		knobs[3]->connectLabel(totalBpmLabel, module);
		addParam(knobs[3]);
	}
	addInput(createInput<TinyPJ301MPort>(Vec(23, 330), module, Str1ker::FADER_INPUT));
	
	/////////////////////////////////// RIGHT SIDE ///////////////////////////////////
	addParam(createParam<JwHorizontalSwitch>(Vec(89, 85), module, Str1ker::ON_OFF_SWITCH));
	addOutput(createOutput<TinyPJ301MPort>(Vec(93, 113), module, Str1ker::CLOCK_OUTPUT));

	addInput(createInput<TinyPJ301MPort>(Vec(93, 145), module, Str1ker::RESET_INPUT));
	addParam(createParam<SmallButton>(Vec(88, 175), module, Str1ker::RESET_BTN_PARAM));
	addOutput(createOutput<TinyPJ301MPort>(Vec(93, 208), module, Str1ker::RESET_OUTPUT));

	addOutput(createOutput<TinyPJ301MPort>(Vec(93, 243), module, Str1ker::BPM_OUTPUT));
	addInput(createInput<TinyPJ301MPort>(Vec(93, 277), module, Str1ker::BPM_INPUT));
	{
		JwSmallSnapKnob *faderRangeKnob = dynamic_cast<JwSmallSnapKnob*>(createParam<JwSmallSnapKnob>(Vec(88, 313), module, Str1ker::FADER_RANGE_PARAM));
		CenteredLabel* const faderRangeLabel = new CenteredLabel;
		faderRangeLabel->box.pos = Vec(50, 177);
		faderRangeLabel->text = "";
		faderRangeKnob->connectLabel(faderRangeLabel, module);
		addChild(faderRangeLabel);
		addParam(faderRangeKnob);
	}
}

struct MultMenuItem : MenuItem {
	Str1ker *str1;
	int val;
	void onAction(const event::Action &e) override {
		str1->clockMult = val;
	}
	void step() override {
		rightText = (str1->clockMult == val) ? "✔" : "";
	}
};

struct OscOnMenuItem : MenuItem {
	Str1ker *str1;
	void onAction(const event::Action &e) override {
		str1->oscOn = !str1->oscOn;
	}
	void step() override {
		rightText = (str1->oscOn) ? "✔" : "";
	}
};

struct OscPortMenuItem : MenuItem {
	Str1ker *str1;
	int val;
	void onAction(const event::Action &e) override {
		str1->oscPort = val;
		str1->updateOscPort();
	}
	void step() override {
		rightText = (str1->oscPort == val) ? "✔" : "";
	}
};

void Str1kerWidget::appendContextMenu(Menu *menu) {
	{	
		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);
	}
	Str1ker *str1 = dynamic_cast<Str1ker*>(module);
	{
		MultMenuItem *item = new MultMenuItem();
		item->text = "1/4 Notes";
		item->str1 = str1;
		item->val = 1;
		menu->addChild(item);
	}
	{
		MultMenuItem *item = new MultMenuItem();
		item->text = "1/8 Notes";
		item->str1 = str1;
		item->val = 2;
		menu->addChild(item);
	}
	{
		MultMenuItem *item = new MultMenuItem();
		item->text = "1/16 Notes";
		item->str1 = str1;
		item->val = 4;
		menu->addChild(item);
	}
	{
		MultMenuItem *item = new MultMenuItem();
		item->text = "1/32 Notes";
		item->str1 = str1;
		item->val = 8;
		menu->addChild(item);
	}
	{
		MultMenuItem *item = new MultMenuItem();
		item->text = "1/64 Notes";
		item->str1 = str1;
		item->val = 16;
		menu->addChild(item);
	}
	{	
		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);
	}
	{
		OscOnMenuItem *item = new OscOnMenuItem();
		item->text = "OSC On";
		item->str1 = str1;
		menu->addChild(item);
	}
	{
		OscPortMenuItem *item = new OscPortMenuItem();
		item->text = "OSC Port 7013";
		item->str1 = str1;
		item->val = 7013;
		menu->addChild(item);
	}
	{
		OscPortMenuItem *item = new OscPortMenuItem();
		item->text = "OSC Port 8013";
		item->str1 = str1;
		item->val = 8013;
		menu->addChild(item);
	}
	{
		OscPortMenuItem *item = new OscPortMenuItem();
		item->text = "OSC Port 9013";
		item->str1 = str1;
		item->val = 9013;
		menu->addChild(item);
	}
	{
		OscPortMenuItem *item = new OscPortMenuItem();
		item->text = "OSC Port 10013";
		item->str1 = str1;
		item->val = 10013;
		menu->addChild(item);
	}
}

Model *modelStr1ker = createModel<Str1ker, Str1kerWidget>("Str1ker");