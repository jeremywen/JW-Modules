#include <string.h>
#include "JWModules.hpp"
#include "dsp/digital.hpp"

struct BouncyBall : Module {
	enum ParamIds {
		GATE_PARAM,
		SCALE_X_PARAM,
		SCALE_Y_PARAM,
		OFFSET_X_VOLTS_PARAM,
		OFFSET_Y_VOLTS_PARAM,
		VELOCITY_X_PARAM,
		VELOCITY_Y_PARAM,
		SPEED_MULT_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		RESET_INPUT,
		BUMP_INPUT,
		VELOCITY_X_INPUT,
		VELOCITY_Y_INPUT,
		SPEED_MULT_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		X_OUTPUT,
		Y_OUTPUT,
		NORTH_GATE_OUTPUT,
		EAST_GATE_OUTPUT,
		SOUTH_GATE_OUTPUT,
		WEST_GATE_OUTPUT,
		NUM_OUTPUTS
	};
	
	float minX = 0, minY = 0, maxX = 0, maxY = 0;
	float displayWidth = 0, displayHeight = 0;
	float ballRadius = 10;
	float ballStrokeWidth = 2;
	float minVolt = -5, maxVolt = 5;
	float velScale = 0.01;

	bool lastTickWasEdgeHit = false;
	long consecutiveEdgeHits = 0;

	Vec velocity;
	Vec ballPos;
	Vec ballVel;

	PulseGenerator northGatePulse, eastGatePulse, southGatePulse, westGatePulse;
	SchmittTrigger resetTrigger;
	SchmittTrigger bumpTrigger;

	BouncyBall() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS) {}
	void step();
	void reset(){}

	json_t *toJson() {
		json_t *rootJ = json_object();
		return rootJ;
	}

	void fromJson(json_t *rootJ) {
	}

	void updateMinMax(){
		float distToMid = ballRadius + ballStrokeWidth;
		minX = distToMid;
		minY = distToMid;
		maxX = displayWidth - distToMid;
		maxY = displayHeight - distToMid;
	}

	void resetBall(){
		ballPos.x = displayWidth * 0.5;
		ballPos.y = displayHeight * 0.5;
		consecutiveEdgeHits = 0;
		lastTickWasEdgeHit = false;
	}
};

void BouncyBall::step() {
	velocity = Vec(params[VELOCITY_X_PARAM].value + inputs[VELOCITY_X_INPUT].value, 
		           params[VELOCITY_Y_PARAM].value + inputs[VELOCITY_Y_INPUT].value);

	if (resetTrigger.process(inputs[RESET_INPUT].value)) {
		resetBall();
	  	ballVel = Vec(velocity.mult(velScale));
	}

	if (bumpTrigger.process(inputs[BUMP_INPUT].value)) {
	  	ballVel = ballVel.plus(velocity.mult(velScale));
	}

	if(consecutiveEdgeHits < 5){//don't want to get stuck triggering on edges

		bool edgeHit = false;
	  	if(ballPos.x >= maxX){
	  		ballVel.x *= -1;//-1 reverses ball with no speed change
			eastGatePulse.trigger(1e-3);
			edgeHit = true;
	  	}

	  	if(ballPos.x <= minX){
	  		ballVel.x *= -1;//-1 reverses ball with no speed change
			westGatePulse.trigger(1e-3);
			edgeHit = true;
	  	}

	  	if(ballPos.y >= maxY){
	  		ballVel.y *= -1;//-1 reverses ball with no speed change
			southGatePulse.trigger(1e-3);
			edgeHit = true;
	  	}

	  	if(ballPos.y <= minY){
	  		ballVel.y *= -1;//-1 reverses ball with no speed change
			northGatePulse.trigger(1e-3);
			edgeHit = true;
	  	}

	  	if(edgeHit){
	  		consecutiveEdgeHits++;
			lastTickWasEdgeHit = true;
	  	} else {
	  		consecutiveEdgeHits = 0;
			lastTickWasEdgeHit = false;
	  	}
	}

	bool northPulse = northGatePulse.process(1.0 / engineGetSampleRate());
	bool eastPulse = eastGatePulse.process(1.0 / engineGetSampleRate());
	bool southPulse = southGatePulse.process(1.0 / engineGetSampleRate());
	bool westPulse = westGatePulse.process(1.0 / engineGetSampleRate());
	outputs[NORTH_GATE_OUTPUT].value = northPulse ? 10.0 : 0.0;
	outputs[EAST_GATE_OUTPUT].value = eastPulse ? 10.0 : 0.0;
	outputs[SOUTH_GATE_OUTPUT].value = southPulse ? 10.0 : 0.0;
	outputs[WEST_GATE_OUTPUT].value = westPulse ? 10.0 : 0.0;

	outputs[X_OUTPUT].value = (rescalef(ballPos.x, minX, maxX, minVolt, maxVolt) + params[OFFSET_X_VOLTS_PARAM].value) * params[SCALE_X_PARAM].value;
	outputs[Y_OUTPUT].value = (rescalef(ballPos.y, minY, maxY, maxVolt, minVolt) + params[OFFSET_Y_VOLTS_PARAM].value) * params[SCALE_Y_PARAM].value;//y is inverted because gui coords

	Vec newPos = ballPos.plus(ballVel.mult(params[SPEED_MULT_PARAM].value + inputs[SPEED_MULT_INPUT].value));
  	ballPos.x = clampf(newPos.x, minX, maxX);
  	ballPos.y = clampf(newPos.y, minY, maxY);
}

struct BouncyBallDisplay : Widget {
	BouncyBall *module;
	BouncyBallDisplay(){}

	void draw(NVGcontext *vg) override {
		//background
		nvgFillColor(vg, nvgRGB(20, 30, 33));
		nvgBeginPath(vg);
		nvgRect(vg, 0, 0, box.size.x, box.size.y);
		nvgFill(vg);
			
		//ball
		nvgFillColor(vg, nvgRGB(25, 150, 252));
		nvgStrokeColor(vg, nvgRGB(25, 150, 252));
		nvgStrokeWidth(vg, 2);
		nvgBeginPath(vg);
		nvgCircle(vg, module->ballPos.x, module->ballPos.y, module->ballRadius);
		nvgFill(vg);
		nvgStroke(vg);
	}
};

BouncyBallWidget::BouncyBallWidget() {
	BouncyBall *module = new BouncyBall();
	setModule(module);
	box.size = Vec(RACK_GRID_WIDTH*24, RACK_GRID_HEIGHT);

	SVGPanel *panel = new SVGPanel();
	panel->box.size = box.size;
	panel->setBackground(SVG::load(assetPlugin(plugin, "res/BouncyBall.svg")));
	addChild(panel);

	BouncyBallDisplay *display = new BouncyBallDisplay();
	display->module = module;
	display->box.pos = Vec(2, 40);
	display->box.size = Vec(box.size.x - 4, RACK_GRID_HEIGHT - 80);
	addChild(display);
	module->displayWidth = display->box.size.x;
	module->displayHeight = display->box.size.y;
	module->updateMinMax();
	module->resetBall();

	addChild(createScrew<Screw_J>(Vec(40, 20)));
	addChild(createScrew<Screw_W>(Vec(55, 20)));

	//top row
	addParam(createParam<JwTinyKnob>(Vec(140, 20), module, BouncyBall::SCALE_X_PARAM, 0.01, 1.0, 0.5));
	addParam(createParam<JwTinyKnob>(Vec(200, 20), module, BouncyBall::SCALE_Y_PARAM, 0.01, 1.0, 0.5));
	addParam(createParam<JwTinyKnob>(Vec(260, 20), module, BouncyBall::OFFSET_X_VOLTS_PARAM, -5.0, 5.0, 5.0));
	addParam(createParam<JwTinyKnob>(Vec(320, 20), module, BouncyBall::OFFSET_Y_VOLTS_PARAM, -5.0, 5.0, 5.0));
	
	//bottom row
	addInput(createInput<TinyPJ301MPort>(Vec(20, 360), module, BouncyBall::RESET_INPUT));
	addInput(createInput<TinyPJ301MPort>(Vec(60, 360), module, BouncyBall::BUMP_INPUT));
	
	addInput(createInput<TinyPJ301MPort>(Vec(90, 360), module, BouncyBall::VELOCITY_X_INPUT));
	addInput(createInput<TinyPJ301MPort>(Vec(120, 360), module, BouncyBall::VELOCITY_Y_INPUT));
	addInput(createInput<TinyPJ301MPort>(Vec(150, 360), module, BouncyBall::SPEED_MULT_INPUT));

	addParam(createParam<JwTinyKnob>(Vec(100, 360), module, BouncyBall::VELOCITY_X_PARAM, -3.0, 3.0, 0.25));
	addParam(createParam<JwTinyKnob>(Vec(130, 360), module, BouncyBall::VELOCITY_Y_PARAM, -3.0, 3.0, 0.5));
	addParam(createParam<JwTinyKnob>(Vec(165, 360), module, BouncyBall::SPEED_MULT_PARAM, 1.0, 20.0, 1.0));
	
	addOutput(createOutput<TinyPJ301MPort>(Vec(225, 360), module, BouncyBall::X_OUTPUT));
	addOutput(createOutput<TinyPJ301MPort>(Vec(250, 360), module, BouncyBall::Y_OUTPUT));
	addOutput(createOutput<TinyPJ301MPort>(Vec(280, 360), module, BouncyBall::NORTH_GATE_OUTPUT));
	addOutput(createOutput<TinyPJ301MPort>(Vec(300, 360), module, BouncyBall::EAST_GATE_OUTPUT));
	addOutput(createOutput<TinyPJ301MPort>(Vec(320, 360), module, BouncyBall::SOUTH_GATE_OUTPUT));
	addOutput(createOutput<TinyPJ301MPort>(Vec(340, 360), module, BouncyBall::WEST_GATE_OUTPUT));
}

