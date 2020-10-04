#include <string.h>
#include "JWModules.hpp"

enum InputColor {
	ORANGE_INPUT_COLOR,
	YELLOW_INPUT_COLOR,
	PURPLE_INPUT_COLOR,
	BLUE_INPUT_COLOR,
	WHITE_INPUT_COLOR
};

struct Ball {
	Rect box;
	Rect previousBox;
	Vec vel;
	dsp::SchmittTrigger resetTrigger, bumpTrigger;
	dsp::PulseGenerator northPulse, eastPulse, southPulse, westPulse, paddlePulse;
	NVGcolor color;
	void setPosition(float x, float y){
		previousBox.pos.x = box.pos.x;
		previousBox.pos.y = box.pos.y;
		box.pos.x = x;
		box.pos.y = y;
	}
};

struct Paddle {
	Rect box;
	bool locked = true;
	bool visible = true;
	Paddle(){
		box.size.x = 100.0;
		box.size.y = 10.0;
	}
};

struct BouncyBalls : Module {
	enum ParamIds {
		RESET_PARAM,
		TRIG_BTN_PARAM = RESET_PARAM + 4,
		VEL_X_PARAM = TRIG_BTN_PARAM + 4,
		VEL_Y_PARAM = VEL_X_PARAM + 4,
		SPEED_MULT_PARAM = VEL_Y_PARAM + 4,
		SCALE_X_PARAM = SPEED_MULT_PARAM + 4,
		SCALE_Y_PARAM,
		OFFSET_X_VOLTS_PARAM,
		OFFSET_Y_VOLTS_PARAM,
		PAD_ON_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		RESET_INPUT,
		TRIG_INPUT = RESET_INPUT + 4,
		VEL_X_INPUT = TRIG_INPUT + 4,
		VEL_Y_INPUT = VEL_X_INPUT + 4,
		SPEED_MULT_INPUT = VEL_Y_INPUT + 4,
		PAD_POS_X_INPUT = SPEED_MULT_INPUT + 4,
		PAD_POS_Y_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		X_OUTPUT,
		Y_OUTPUT = X_OUTPUT + 4,
		N_OUTPUT = Y_OUTPUT + 4,
		E_OUTPUT = N_OUTPUT + 4,
		S_OUTPUT = E_OUTPUT + 4,
		W_OUTPUT = S_OUTPUT + 4,
		EDGE_HIT_OUTPUT = W_OUTPUT + 4,
		PAD_TRIG_OUTPUT = EDGE_HIT_OUTPUT + 4,
		NUM_OUTPUTS = PAD_TRIG_OUTPUT + 4
	};
	enum LightIds {
		PAD_ON_LIGHT,
		NUM_LIGHTS
	};
	
	float displayWidth = 0, displayHeight = 0;
	float ballRadius = 10;
	float ballStrokeWidth = 2;
	float minVolt = -5, maxVolt = 5;
	float velScale = 0.01;
	float rate = 1.0 / APP->engine->getSampleRate();
	
	Ball *balls = new Ball[4];
	Paddle paddle;

	BouncyBalls() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		for(int x=0; x<4; x++){
			configParam(RESET_PARAM + x, 0.0, 1.0, 0.0, "Reset");
		}
		for(int x=0; x<4; x++){
			configParam(TRIG_BTN_PARAM + x, 0.0, 1.0, 0.0, "Trigger");
		}
		for(int x=0; x<4; x++){
			configParam(VEL_X_PARAM + x, -3.0, 3.0, 0.25, "Velocity X");
		}
		for(int x=0; x<4; x++){
			configParam(VEL_Y_PARAM + x, -3.0, 3.0, 0.5, "Velocity Y");
		}
		for(int x=0; x<4; x++){
			configParam(SPEED_MULT_PARAM + x, 1.0, 20.0, 1.0, "Speed");
		}
		configParam(PAD_ON_PARAM, 0.0, 1.0, 0.0, "Pad On");
		configParam(SCALE_X_PARAM, 0.01, 1.0, 0.5, "Scale X");
		configParam(SCALE_Y_PARAM, 0.01, 1.0, 0.5, "Scale Y");
		configParam(OFFSET_X_VOLTS_PARAM, -5.0, 5.0, 5.0, "Offset X");
		configParam(OFFSET_Y_VOLTS_PARAM, -5.0, 5.0, 5.0, "Offset Y");

		balls[0].color = nvgRGB(255, 151, 9);//orange
		balls[1].color = nvgRGB(255, 243, 9);//yellow
		balls[2].color = nvgRGB(144, 26, 252);//purple
		balls[3].color = nvgRGB(25, 150, 252);//blue
		for(int i=0; i<4; i++){
			balls[i].box.size.x = ballRadius*2 + ballStrokeWidth*2;
			balls[i].box.size.y = ballRadius*2 + ballStrokeWidth*2;
			balls[i].previousBox.size.x = balls[i].box.size.x;
			balls[i].previousBox.size.y = balls[i].box.size.y;
		}
		lights[PAD_ON_LIGHT].value = 1.0;
	}
	~BouncyBalls() {
		delete [] balls;
	}

	void process(const ProcessArgs &args) override;
	void onReset() override {
		resetBalls();
		paddle.locked = true;
		paddle.visible = true;
		lights[PAD_ON_LIGHT].value = 1.0;
	}

	void onSampleRateChange() override {
		rate = 1.0 / APP->engine->getSampleRate();
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		json_object_set_new(rootJ, "paddleX", json_real(paddle.box.pos.x));
		json_object_set_new(rootJ, "paddleY", json_real(paddle.box.pos.y));
		json_object_set_new(rootJ, "paddleVisible", json_boolean(paddle.visible));
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		json_t *xPosJ = json_object_get(rootJ, "paddleX");
		json_t *yPosJ = json_object_get(rootJ, "paddleY");
		paddle.box.pos.x = json_real_value(xPosJ);
		paddle.box.pos.y = json_real_value(yPosJ);

		json_t *paddleVisibleJ = json_object_get(rootJ, "paddleVisible");
		if (paddleVisibleJ){
			paddle.visible = json_is_true(paddleVisibleJ);
		}
		lights[PAD_ON_LIGHT].value = paddle.visible ? 1.0 : 0.0;
	}

	void resetBallAtIdx(int i){
		float totalBallStartWidth = ballRadius * 4.0 * 3.0;
		balls[i].box.pos.x = (displayWidth*0.5 - totalBallStartWidth*0.5) + ballRadius * 3.0 * i;
		balls[i].box.pos.y = displayHeight * 0.45;
		balls[i].vel.x = 0;
		balls[i].vel.y = 0;
	}

	void resetBalls(){
		for(int i=0; i<4; i++){
			resetBallAtIdx(i);
		}
		paddle.box.pos.x = displayWidth * 0.5 - paddle.box.size.x * 0.5;
		paddle.box.pos.y = displayHeight - 30;
	}
};

void BouncyBalls::process(const ProcessArgs &args) {	
	for(int i=0; i<4; i++){
		Ball &b = balls[i];
		Vec velocity = Vec(params[VEL_X_PARAM + i].getValue() + inputs[VEL_X_INPUT + i].getVoltage(), 
						   params[VEL_Y_PARAM + i].getValue() + inputs[VEL_Y_INPUT + i].getVoltage());

		if (b.resetTrigger.process(inputs[RESET_INPUT + i].getVoltage() + params[RESET_PARAM + i].getValue())) {
			resetBallAtIdx(i);
			b.vel = Vec(velocity.mult(velScale));
		}

		if (b.bumpTrigger.process(inputs[TRIG_INPUT + i].getVoltage() + params[TRIG_BTN_PARAM + i].getValue())) {
			b.vel = b.vel.plus(velocity.mult(velScale));
		}

		if(paddle.visible && b.box.intersects(paddle.box)){
			
			if(b.previousBox.getBottomRight().y < paddle.box.getTopRight().y || //ball was above
			   b.previousBox.getTopRight().y > paddle.box.getBottomRight().y){ //ball was below
				b.vel.y *= -1;
			}

			if(b.previousBox.getBottomRight().x < paddle.box.getBottomLeft().x || //ball was left
			   b.previousBox.getBottomLeft().x > paddle.box.getBottomRight().x){ //ball was right
				b.vel.x *= -1;
			}

			b.paddlePulse.trigger(1e-3);
		}

		bool hitEdge = false;
		if(b.box.pos.x + b.box.size.x >= displayWidth){
			b.vel.x *= -1;
			b.eastPulse.trigger(1e-3);
			hitEdge = true;
		}

		if(b.box.pos.x <= 0){
			b.vel.x *= -1;
			b.westPulse.trigger(1e-3);
			hitEdge = true;
		}

		if(b.box.pos.y + b.box.size.y >= displayHeight){
			b.vel.y *= -1;
			b.southPulse.trigger(1e-3);
			hitEdge = true;
		}

		if(b.box.pos.y <= 0){
			b.vel.y *= -1;
			b.northPulse.trigger(1e-3);
			hitEdge = true;
		}

		if(paddle.visible && inputs[PAD_POS_X_INPUT].isConnected()){
			paddle.box.pos.x = -50 + clampfjw(rescalefjw(inputs[PAD_POS_X_INPUT].getVoltage(), -5, 5, 50, displayWidth - 50), 50, displayWidth - 50);
		}
		if(paddle.visible && inputs[PAD_POS_Y_INPUT].isConnected()){
			paddle.box.pos.y = clampfjw(rescalefjw(inputs[PAD_POS_Y_INPUT].getVoltage(), -5, 5, 0, displayHeight - 10), 0, displayHeight - 10);
		}

		if(outputs[X_OUTPUT + i].isConnected())outputs[X_OUTPUT + i].setVoltage((rescalefjw(b.box.pos.x, 0, displayWidth, minVolt, maxVolt) + params[OFFSET_X_VOLTS_PARAM].getValue()) * params[SCALE_X_PARAM].getValue());
		if(outputs[Y_OUTPUT + i].isConnected())outputs[Y_OUTPUT + i].setVoltage((rescalefjw(b.box.pos.y, 0, displayHeight, maxVolt, minVolt) + params[OFFSET_Y_VOLTS_PARAM].getValue()) * params[SCALE_Y_PARAM].getValue());//y is inverted because gui coords
		if(outputs[N_OUTPUT + i].isConnected())outputs[N_OUTPUT + i].setVoltage(b.northPulse.process(rate) ? 10.0 : 0.0);
		if(outputs[E_OUTPUT + i].isConnected())outputs[E_OUTPUT + i].setVoltage(b.eastPulse.process(rate) ? 10.0 : 0.0);
		if(outputs[S_OUTPUT + i].isConnected())outputs[S_OUTPUT + i].setVoltage(b.southPulse.process(rate) ? 10.0 : 0.0);
		if(outputs[W_OUTPUT + i].isConnected())outputs[W_OUTPUT + i].setVoltage(b.westPulse.process(rate) ? 10.0 : 0.0);
		if(outputs[EDGE_HIT_OUTPUT + i].isConnected())outputs[EDGE_HIT_OUTPUT + i].setVoltage(hitEdge ? 10.0 : 0.0);
		if(outputs[PAD_TRIG_OUTPUT + i].isConnected())outputs[PAD_TRIG_OUTPUT + i].setVoltage(b.paddlePulse.process(rate) ? 10.0 : 0.0);

		Vec newPos = b.box.pos.plus(b.vel.mult(params[SPEED_MULT_PARAM + i].getValue() + inputs[SPEED_MULT_INPUT + i].getVoltage()));
		b.setPosition(
			clampfjw(newPos.x, 0, displayWidth), 
			clampfjw(newPos.y, 0, displayHeight)
		);
	}
}

struct BouncyBallDisplay : Widget {
	BouncyBalls *module;
	BouncyBallDisplay(){}

	void onHover(const event::Hover &e) override {
		Widget::onHover(e);
		if(module){
			if(!module->paddle.locked && !module->inputs[BouncyBalls::PAD_POS_X_INPUT].isConnected()){
				module->paddle.box.pos.x = -50 + clampfjw(e.pos.x, 50, box.size.x - 50);
			}
			if(!module->paddle.locked && !module->inputs[BouncyBalls::PAD_POS_Y_INPUT].isConnected()){
				module->paddle.box.pos.y = clampfjw(e.pos.y, 0, box.size.y - 10);
			}
		}
	}

	void onButton(const event::Button &e) override {
		Widget::onButton(e);
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			if(module){
				module->paddle.locked = !module->paddle.locked;
			}
		}
	}

	void draw(const DrawArgs &args) override {
		//background
		nvgFillColor(args.vg, nvgRGB(20, 30, 33));
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
		nvgFill(args.vg);
		
		if(module != NULL){
			//paddle
			if(module->paddle.visible){
				nvgFillColor(args.vg, nvgRGB(255, 255, 255));
				nvgBeginPath(args.vg);
				nvgRect(args.vg, module->paddle.box.pos.x, module->paddle.box.pos.y, 100, 10);
				nvgFill(args.vg);
			}
			//balls
			for(int i=0; i<4; i++){
				nvgFillColor(args.vg, module->balls[i].color);
				nvgStrokeColor(args.vg, module->balls[i].color);
				nvgStrokeWidth(args.vg, 2);
				nvgBeginPath(args.vg);
				Vec ctr = module->balls[i].box.getCenter();
				nvgCircle(args.vg, ctr.x, ctr.y, module->ballRadius);
				nvgFill(args.vg);
				nvgStroke(args.vg);
			}
		} else {
			//TODO maybe draw some balls and a paddle in preview
		}
	}
};

struct BouncyBallsWidget : ModuleWidget {
	BouncyBallsWidget(BouncyBalls *module);
	void addButton(Vec pos, int param);
	void addColoredPort(int color, Vec pos, int param, bool input);
};

struct PaddleVisibleButton : TinyButton {
	void onButton(const event::Button &e) override {
		TinyButton::onButton(e);
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			BouncyBallsWidget *widg = this->getAncestorOfType<BouncyBallsWidget>();
			if(widg->module){
				BouncyBalls *bbs = dynamic_cast<BouncyBalls*>(widg->module);
				bbs->paddle.visible = !bbs->paddle.visible;
				bbs->lights[BouncyBalls::PAD_ON_LIGHT].value = bbs->paddle.visible ? 1.0 : 0.0;
			}
		}
	}
};

BouncyBallsWidget::BouncyBallsWidget(BouncyBalls *module) {
	setModule(module);
	box.size = Vec(RACK_GRID_WIDTH*48, RACK_GRID_HEIGHT);

	SVGPanel *panel = new SVGPanel();
	panel->box.size = box.size;
	panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/BouncyBalls.svg")));
	addChild(panel);

	BouncyBallDisplay *display = new BouncyBallDisplay();
	display->module = module;
	display->box.pos = Vec(270, 2);
	display->box.size = Vec(box.size.x - display->box.pos.x - 2, RACK_GRID_HEIGHT - 4);
	addChild(display);

	if(module != NULL){
		module->displayWidth = display->box.size.x;
		module->displayHeight = display->box.size.y;
		module->resetBalls();
	}

	addChild(createWidget<Screw_J>(Vec(31, 365)));
	addChild(createWidget<Screw_W>(Vec(46, 365)));

	/////////////////////// INPUTS ///////////////////////
	float topY = 13.0, leftX = 40.0, xMult = 55.0, yAdder = 34.0, knobDist = 17.0;
	for(int x=0; x<4; x++){
		addColoredPort(x, Vec(leftX + x * xMult, topY), BouncyBalls::RESET_INPUT + x, true);
		addButton(Vec(leftX + knobDist + x * xMult, topY-5), BouncyBalls::RESET_PARAM + x);
	}
	topY+=yAdder;
	for(int x=0; x<4; x++){
		addColoredPort(x, Vec(leftX + x * xMult, topY), BouncyBalls::TRIG_INPUT + x, true);
		addButton(Vec(leftX + knobDist + x * xMult, topY-5), BouncyBalls::TRIG_BTN_PARAM + x);
	}
	topY+=yAdder;
	for(int x=0; x<4; x++){
		addColoredPort(x, Vec(leftX + x * xMult, topY), BouncyBalls::VEL_X_INPUT + x, true);
		addParam(createParam<SmallWhiteKnob>(Vec(leftX + knobDist + x * xMult, topY-5), module, BouncyBalls::VEL_X_PARAM + x));
	}
	topY+=yAdder;
	for(int x=0; x<4; x++){
		addColoredPort(x, Vec(leftX + x * xMult, topY), BouncyBalls::VEL_Y_INPUT + x, true);
		addParam(createParam<SmallWhiteKnob>(Vec(leftX + knobDist + x * xMult, topY-5), module, BouncyBalls::VEL_Y_PARAM + x));
	}
	topY+=yAdder;
	for(int x=0; x<4; x++){
		addColoredPort(x, Vec(leftX + x * xMult, topY), BouncyBalls::SPEED_MULT_INPUT + x, true);
		addParam(createParam<SmallWhiteKnob>(Vec(leftX + knobDist + x * xMult, topY-5), module, BouncyBalls::SPEED_MULT_PARAM + x));
	}
	
	/////////////////////// OUTPUTS ///////////////////////
	xMult = 25.0;
	yAdder = 25.0;
	topY+=yAdder + 5;
	leftX = 100;
	for(int x=0; x<4; x++){
		addColoredPort(x, Vec(leftX + x * xMult, topY), BouncyBalls::X_OUTPUT + x, false);
	}
	topY+=yAdder;
	for(int x=0; x<4; x++){
		addColoredPort(x, Vec(leftX + x * xMult, topY), BouncyBalls::Y_OUTPUT + x, false);
	}
	topY+=yAdder;
	for(int x=0; x<4; x++){
		addColoredPort(x, Vec(leftX + x * xMult, topY), BouncyBalls::N_OUTPUT + x, false);
	}
	topY+=yAdder;
	for(int x=0; x<4; x++){
		addColoredPort(x, Vec(leftX + x * xMult, topY), BouncyBalls::E_OUTPUT + x, false);
	}
	topY+=yAdder;
	for(int x=0; x<4; x++){
		addColoredPort(x, Vec(leftX + x * xMult, topY), BouncyBalls::S_OUTPUT + x, false);
	}
	topY+=yAdder;
	for(int x=0; x<4; x++){
		addColoredPort(x, Vec(leftX + x * xMult, topY), BouncyBalls::W_OUTPUT + x, false);
	}
	topY+=yAdder;
	for(int x=0; x<4; x++){
		addColoredPort(x, Vec(leftX + x * xMult, topY), BouncyBalls::EDGE_HIT_OUTPUT + x, false);
	}
	topY+=yAdder;
	for(int x=0; x<4; x++){
		addColoredPort(x, Vec(leftX + x * xMult, topY), BouncyBalls::PAD_TRIG_OUTPUT + x, false);
	}

	//white pad pos
	addColoredPort(WHITE_INPUT_COLOR, Vec(38, 220), BouncyBalls::PAD_POS_X_INPUT, true);
	addColoredPort(WHITE_INPUT_COLOR, Vec(38, 245), BouncyBalls::PAD_POS_Y_INPUT, true);

	addParam(createParam<PaddleVisibleButton>(Vec(38, 270), module, BouncyBalls::PAD_ON_PARAM));
	addChild(createLight<SmallLight<MyBlueValueLight>>(Vec(38+3.75, 270+3.75), module, BouncyBalls::PAD_ON_LIGHT));

	//scale and offset
	addParam(createParam<SmallWhiteKnob>(Vec(222, 200), module, BouncyBalls::SCALE_X_PARAM));
	addParam(createParam<SmallWhiteKnob>(Vec(222, 242), module, BouncyBalls::SCALE_Y_PARAM));
	addParam(createParam<SmallWhiteKnob>(Vec(222, 290), module, BouncyBalls::OFFSET_X_VOLTS_PARAM));
	addParam(createParam<SmallWhiteKnob>(Vec(222, 338), module, BouncyBalls::OFFSET_Y_VOLTS_PARAM));
}

void BouncyBallsWidget::addButton(Vec pos, int param) {
	addParam(createParam<SmallButton>(pos, module, param));
}

void BouncyBallsWidget::addColoredPort(int color, Vec pos, int param, bool input) {
	switch(color){
		case ORANGE_INPUT_COLOR:
			if(input) { addInput(createInput<Orange_TinyPJ301MPort>(pos, module, param)); } 
			else { addOutput(createOutput<Orange_TinyPJ301MPort>(pos, module, param));	}
			break;
		case YELLOW_INPUT_COLOR:
			if(input) { addInput(createInput<Yellow_TinyPJ301MPort>(pos, module, param)); } 
			else { addOutput(createOutput<Yellow_TinyPJ301MPort>(pos, module, param));	}
			break;
		case PURPLE_INPUT_COLOR:
			if(input) { addInput(createInput<Purple_TinyPJ301MPort>(pos, module, param)); } 
			else { addOutput(createOutput<Purple_TinyPJ301MPort>(pos, module, param));	}
			break;
		case BLUE_INPUT_COLOR:
			if(input) { addInput(createInput<Blue_TinyPJ301MPort>(pos, module, param)); } 
			else { addOutput(createOutput<Blue_TinyPJ301MPort>(pos, module, param));	}
			break;
		case WHITE_INPUT_COLOR:
			if(input) { addInput(createInput<White_TinyPJ301MPort>(pos, module, param)); } 
			else { addOutput(createOutput<White_TinyPJ301MPort>(pos, module, param));	}
			break;
	}
}

Model *modelBouncyBalls = createModel<BouncyBalls, BouncyBallsWidget>("BouncyBalls");
