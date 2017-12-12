#include <string.h>
#include "JWModules.hpp"
#include "dsp/digital.hpp"

enum InputColor {
	ORANGE_INPUT_COLOR,
	YELLOW_INPUT_COLOR,
	PURPLE_INPUT_COLOR,
	BLUE_INPUT_COLOR,
	WHITE_INPUT_COLOR
};

struct Ball {
	Rect box;
	Vec vel;
	SchmittTrigger resetTrigger, bumpTrigger;
	PulseGenerator northPulse, eastPulse, southPulse, westPulse, paddlePulse;
	NVGcolor color;
};

struct Paddle {
	Rect box;
	bool locked = true;
	Paddle(){
		box.size.x = 100.0;
		box.size.y = 10.0;
	}
};

struct BouncyBall : Module {
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
	
	float displayWidth = 0, displayHeight = 0;
	float ballRadius = 10;
	float ballStrokeWidth = 2;
	float minVolt = -5, maxVolt = 5;
	float velScale = 0.01;
	float rate = 1.0 / engineGetSampleRate();
	
	Ball *balls = new Ball[4];
	Paddle paddle;

	BouncyBall() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS) {
		balls[0].color = nvgRGB(255, 151, 9);//orange
		balls[1].color = nvgRGB(255, 243, 9);//yellow
		balls[2].color = nvgRGB(144, 26, 252);//purple
		balls[3].color = nvgRGB(25, 150, 252);//blue
		for(int i=0; i<4; i++){
			balls[i].box.size.x = ballRadius*2 + ballStrokeWidth*2;
			balls[i].box.size.y = ballRadius*2 + ballStrokeWidth*2;
		}
	}
	~BouncyBall() {
		delete [] balls;
	}

	void step() override;
	void reset() override {
		resetBalls();
		paddle.locked = true;
	}

	void onSampleRateChange() override {
		rate = 1.0 / engineGetSampleRate();
	}

	json_t *toJson() override {
		json_t *rootJ = json_object();
		json_object_set_new(rootJ, "paddleX", json_real(paddle.box.pos.x));
		json_object_set_new(rootJ, "paddleY", json_real(paddle.box.pos.y));
		return rootJ;
	}

	void fromJson(json_t *rootJ) override {
		json_t *xPosJ = json_object_get(rootJ, "paddleX");
		json_t *yPosJ = json_object_get(rootJ, "paddleY");
		paddle.box.pos.x = json_real_value(xPosJ);
		paddle.box.pos.y = json_real_value(yPosJ);
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

void BouncyBall::step() {
	for(int i=0; i<4; i++){
		Ball &b = balls[i];
		Vec velocity = Vec(params[VEL_X_PARAM + i].value + inputs[VEL_X_INPUT + i].value, 
						   params[VEL_Y_PARAM + i].value + inputs[VEL_Y_INPUT + i].value);

		if (b.resetTrigger.process(inputs[RESET_INPUT + i].value + params[RESET_PARAM + i].value)) {
			resetBallAtIdx(i);
			b.vel = Vec(velocity.mult(velScale));
		}

		if (b.bumpTrigger.process(inputs[TRIG_INPUT + i].value + params[TRIG_BTN_PARAM + i].value)) {
			b.vel = b.vel.plus(velocity.mult(velScale));
		}

		if(b.box.intersects(paddle.box)){
			b.vel.y *= -1;
			b.vel.x *= -1;
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

		if(inputs[PAD_POS_X_INPUT].active){
			paddle.box.pos.x = -50 + clampf(rescalef(inputs[PAD_POS_X_INPUT].value, -5, 5, 50, displayWidth - 50), 50, displayWidth - 50);
		}
		if(inputs[PAD_POS_Y_INPUT].active){
			paddle.box.pos.y = clampf(rescalef(inputs[PAD_POS_Y_INPUT].value, -5, 5, 0, displayHeight - 10), 0, displayHeight - 10);
		}

		//TODO rotate corners of rectangle

		if(outputs[X_OUTPUT + i].active)outputs[X_OUTPUT + i].value = (rescalef(b.box.pos.x, 0, displayWidth, minVolt, maxVolt) + params[OFFSET_X_VOLTS_PARAM].value) * params[SCALE_X_PARAM].value;
		if(outputs[Y_OUTPUT + i].active)outputs[Y_OUTPUT + i].value = (rescalef(b.box.pos.y, 0, displayHeight, maxVolt, minVolt) + params[OFFSET_Y_VOLTS_PARAM].value) * params[SCALE_Y_PARAM].value;//y is inverted because gui coords
		if(outputs[N_OUTPUT + i].active)outputs[N_OUTPUT + i].value = b.northPulse.process(rate) ? 10.0 : 0.0;
		if(outputs[E_OUTPUT + i].active)outputs[E_OUTPUT + i].value = b.eastPulse.process(rate) ? 10.0 : 0.0;
		if(outputs[S_OUTPUT + i].active)outputs[S_OUTPUT + i].value = b.southPulse.process(rate) ? 10.0 : 0.0;
		if(outputs[W_OUTPUT + i].active)outputs[W_OUTPUT + i].value = b.westPulse.process(rate) ? 10.0 : 0.0;
		if(outputs[EDGE_HIT_OUTPUT + i].active)outputs[EDGE_HIT_OUTPUT + i].value = hitEdge ? 10.0 : 0.0;
		if(outputs[PAD_TRIG_OUTPUT + i].active)outputs[PAD_TRIG_OUTPUT + i].value = b.paddlePulse.process(rate) ? 10.0 : 0.0;

		Vec newPos = b.box.pos.plus(b.vel.mult(params[SPEED_MULT_PARAM + i].value + inputs[SPEED_MULT_INPUT + i].value));
		b.box.pos.x = clampf(newPos.x, 0, displayWidth);
		b.box.pos.y = clampf(newPos.y, 0, displayHeight);
	}
}

struct BouncyBallDisplay : Widget {
	BouncyBall *module;
	BouncyBallDisplay(){}

	void onMouseMove(EventMouseMove &e) override {
		Widget::onMouseMove(e);
		BouncyBall* m = dynamic_cast<BouncyBall*>(module);
		if(!m->paddle.locked && !m->inputs[BouncyBall::PAD_POS_X_INPUT].active){
			m->paddle.box.pos.x = -50 + clampf(e.pos.x, 50, box.size.x - 50);
		}
		if(!m->paddle.locked && !m->inputs[BouncyBall::PAD_POS_Y_INPUT].active){
			m->paddle.box.pos.y = clampf(e.pos.y, 0, box.size.y - 10);
		}
	}

	void onMouseDown(EventMouseDown &e) override {
		Widget::onMouseDown(e);
		BouncyBall* m = dynamic_cast<BouncyBall*>(module);
		m->paddle.locked = !m->paddle.locked;
	}

	void draw(NVGcontext *vg) override {
		//background
		nvgFillColor(vg, nvgRGB(20, 30, 33));
		nvgBeginPath(vg);
		nvgRect(vg, 0, 0, box.size.x, box.size.y);
		nvgFill(vg);
			
		//paddle
		nvgFillColor(vg, nvgRGB(255, 255, 255));
		nvgBeginPath(vg);
		nvgRect(vg, module->paddle.box.pos.x, module->paddle.box.pos.y, 100, 10);
		nvgFill(vg);

		for(int i=0; i<4; i++){
			nvgFillColor(vg, module->balls[i].color);
			nvgStrokeColor(vg, module->balls[i].color);
			nvgStrokeWidth(vg, 2);
			nvgBeginPath(vg);
			Vec ctr = module->balls[i].box.getCenter();
			nvgCircle(vg, ctr.x, ctr.y, module->ballRadius);
			nvgFill(vg);
			nvgStroke(vg);
		}
	}
};

BouncyBallsWidget::BouncyBallsWidget() {
	BouncyBall *module = new BouncyBall();
	setModule(module);
	box.size = Vec(RACK_GRID_WIDTH*48, RACK_GRID_HEIGHT);

	SVGPanel *panel = new SVGPanel();
	panel->box.size = box.size;
	panel->setBackground(SVG::load(assetPlugin(plugin, "res/BouncyBalls.svg")));
	addChild(panel);

	BouncyBallDisplay *display = new BouncyBallDisplay();
	display->module = module;
	display->box.pos = Vec(270, 2);
	display->box.size = Vec(box.size.x - display->box.pos.x - 2, RACK_GRID_HEIGHT - 4);
	addChild(display);
	module->displayWidth = display->box.size.x;
	module->displayHeight = display->box.size.y;
	module->resetBalls();

	addChild(createScrew<Screw_J>(Vec(31, 365)));
	addChild(createScrew<Screw_W>(Vec(46, 365)));

	/////////////////////// INPUTS ///////////////////////
	float topY = 13.0, leftX = 40.0, xMult = 55.0, yAdder = 34.0, knobDist = 17.0;
	for(int x=0; x<4; x++){
		addColoredPort(x, Vec(leftX + x * xMult, topY), BouncyBall::RESET_INPUT + x, true);
		addButton(Vec(leftX + knobDist + x * xMult, topY-5), BouncyBall::RESET_PARAM + x);
	}
	topY+=yAdder;
	for(int x=0; x<4; x++){
		addColoredPort(x, Vec(leftX + x * xMult, topY), BouncyBall::TRIG_INPUT + x, true);
		addButton(Vec(leftX + knobDist + x * xMult, topY-5), BouncyBall::TRIG_BTN_PARAM + x);
	}
	topY+=yAdder;
	for(int x=0; x<4; x++){
		addColoredPort(x, Vec(leftX + x * xMult, topY), BouncyBall::VEL_X_INPUT + x, true);
		addParam(createParam<SmallWhiteKnob>(Vec(leftX + knobDist + x * xMult, topY-5), module, BouncyBall::VEL_X_PARAM + x, -3.0, 3.0, 0.25));
	}
	topY+=yAdder;
	for(int x=0; x<4; x++){
		addColoredPort(x, Vec(leftX + x * xMult, topY), BouncyBall::VEL_Y_INPUT + x, true);
		addParam(createParam<SmallWhiteKnob>(Vec(leftX + knobDist + x * xMult, topY-5), module, BouncyBall::VEL_Y_PARAM + x, -3.0, 3.0, 0.5));
	}
	topY+=yAdder;
	for(int x=0; x<4; x++){
		addColoredPort(x, Vec(leftX + x * xMult, topY), BouncyBall::SPEED_MULT_INPUT + x, true);
		addParam(createParam<SmallWhiteKnob>(Vec(leftX + knobDist + x * xMult, topY-5), module, BouncyBall::SPEED_MULT_PARAM + x, 1.0, 20.0, 1.0));
	}
	
	/////////////////////// OUTPUTS ///////////////////////
	xMult = 25.0;
	yAdder = 25.0;
	topY+=yAdder + 5;
	leftX = 100;
	for(int x=0; x<4; x++){
		addColoredPort(x, Vec(leftX + x * xMult, topY), BouncyBall::X_OUTPUT + x, false);
	}
	topY+=yAdder;
	for(int x=0; x<4; x++){
		addColoredPort(x, Vec(leftX + x * xMult, topY), BouncyBall::Y_OUTPUT + x, false);
	}
	topY+=yAdder;
	for(int x=0; x<4; x++){
		addColoredPort(x, Vec(leftX + x * xMult, topY), BouncyBall::N_OUTPUT + x, false);
	}
	topY+=yAdder;
	for(int x=0; x<4; x++){
		addColoredPort(x, Vec(leftX + x * xMult, topY), BouncyBall::E_OUTPUT + x, false);
	}
	topY+=yAdder;
	for(int x=0; x<4; x++){
		addColoredPort(x, Vec(leftX + x * xMult, topY), BouncyBall::S_OUTPUT + x, false);
	}
	topY+=yAdder;
	for(int x=0; x<4; x++){
		addColoredPort(x, Vec(leftX + x * xMult, topY), BouncyBall::W_OUTPUT + x, false);
	}
	topY+=yAdder;
	for(int x=0; x<4; x++){
		addColoredPort(x, Vec(leftX + x * xMult, topY), BouncyBall::EDGE_HIT_OUTPUT + x, false);
	}
	topY+=yAdder;
	for(int x=0; x<4; x++){
		addColoredPort(x, Vec(leftX + x * xMult, topY), BouncyBall::PAD_TRIG_OUTPUT + x, false);
	}

	//white pad pos
	addColoredPort(WHITE_INPUT_COLOR, Vec(38, 225), BouncyBall::PAD_POS_X_INPUT, true);
	addColoredPort(WHITE_INPUT_COLOR, Vec(38, 250), BouncyBall::PAD_POS_Y_INPUT, true);

	//scale and offset
	addParam(createParam<SmallWhiteKnob>(Vec(222, 200), module, BouncyBall::SCALE_X_PARAM, 0.01, 1.0, 0.5));
	addParam(createParam<SmallWhiteKnob>(Vec(222, 242), module, BouncyBall::SCALE_Y_PARAM, 0.01, 1.0, 0.5));
	addParam(createParam<SmallWhiteKnob>(Vec(222, 290), module, BouncyBall::OFFSET_X_VOLTS_PARAM, -5.0, 5.0, 5.0));
	addParam(createParam<SmallWhiteKnob>(Vec(222, 338), module, BouncyBall::OFFSET_Y_VOLTS_PARAM, -5.0, 5.0, 5.0));
}

void BouncyBallsWidget::addButton(Vec pos, int param) {
	addParam(createParam<SmallButton>(pos, module, param, 0.0, 1.0, 0.0));
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
