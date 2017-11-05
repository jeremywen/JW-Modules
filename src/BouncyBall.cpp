#include <string.h>
#include "JWModules.hpp"
#include "dsp/digital.hpp"

struct BouncyBall : Module {
	enum ParamIds {
		X_POS_PARAM,
		Y_POS_PARAM,
		GATE_PARAM,
		OFFSET_X_VOLTS_PARAM,
		OFFSET_Y_VOLTS_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		NUM_INPUTS
	};
	enum OutputIds {
		X_OUTPUT,
		Y_OUTPUT,
		GATE_OUTPUT,
		NUM_OUTPUTS
	};
	
	float minX = 0, minY = 0, maxX = 0, maxY = 0;
	float displayWidth = 0, displayHeight = 0;
	float totalBallSize = 12;
	float minVolt = -5, maxVolt = 5;
	PulseGenerator gatePulse;

	BouncyBall() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS) {}
	void step();
	void reset(){
		defaultPos();
	}

	json_t *toJson() {
		json_t *rootJ = json_object();
		return rootJ;
	}

	void fromJson(json_t *rootJ) {
	}

	void defaultPos() {
		params[BouncyBall::X_POS_PARAM].value = displayWidth / 2.0;
		params[BouncyBall::Y_POS_PARAM].value = displayHeight / 2.0;		
	}

	void updateMinMax(){
		minX = totalBallSize;
		minY = totalBallSize;
		maxX = displayWidth - totalBallSize;
		maxY = displayHeight - totalBallSize;

	}
};

void BouncyBall::step() {
	outputs[X_OUTPUT].value = rescalef(params[X_POS_PARAM].value, minX, maxX, minVolt, maxVolt) + params[OFFSET_X_VOLTS_PARAM].value;
	outputs[Y_OUTPUT].value = rescalef(params[Y_POS_PARAM].value, minY, maxY, maxVolt, minVolt) + params[OFFSET_Y_VOLTS_PARAM].value;//y is inverted because gui coords
	
	bool pulse = gatePulse.process(1.0 / engineGetSampleRate());
	outputs[GATE_OUTPUT].value = pulse ? 10.0 : 0.0;
}

struct BouncyBallDisplay : Widget {
	BouncyBall *module;
	Vec velocity;
	Vec gravity;
	Vec ballPos;
	float slowRate;
	int totalBallSize = 15;

	BouncyBallDisplay(){ init(); }

	void onMouseDown(EventMouseDown &e) override {
		init();
		module->params[BouncyBall::X_POS_PARAM].value = e.pos.x;
		module->params[BouncyBall::Y_POS_PARAM].value = e.pos.y;
	}

	void init(){
		velocity = Vec(randomf()*4-2, randomf()*4-2);
		gravity = Vec(0, randomf()*1);
		slowRate = 0.75;
	}

	void draw(NVGcontext *vg) override {
		ballPos.x = module->params[BouncyBall::X_POS_PARAM].value;
      	ballPos.y = module->params[BouncyBall::Y_POS_PARAM].value;
      	ballPos = ballPos.plus(velocity);
      	velocity = velocity.plus(gravity);

      	if(ballPos.x+totalBallSize > box.size.x || ballPos.x-totalBallSize < 0){
      		velocity.x *= -1;
      	}

      	if(ballPos.y+totalBallSize+2 > box.size.y){
      		// if(slowRate > 0.1){
	      		velocity.y *= -(slowRate);
	      		velocity.x *= slowRate;
	      		ballPos.y = box.size.y-totalBallSize;
	      		if(velocity.x < -0.02 || velocity.x > 0.02){
			  		printf("%f, %f\n", velocity.x, velocity.y);
					module->gatePulse.trigger(1e-3);
	      		}
	      		// slowRate *= 0.5;
      		// } else {
      		// 	velocity.x = 0;
      		// 	velocity.y = 0;
      		// }
      	}

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
		nvgCircle(vg, ballPos.x, ballPos.y, totalBallSize - 2/*stroke*/);
		nvgFill(vg);
		nvgStroke(vg);

		module->params[BouncyBall::X_POS_PARAM].value = ballPos.x;
		module->params[BouncyBall::Y_POS_PARAM].value = ballPos.y;
	}
};

BouncyBallWidget::BouncyBallWidget() {
	BouncyBall *module = new BouncyBall();
	setModule(module);
	box.size = Vec(RACK_GRID_WIDTH*24, RACK_GRID_HEIGHT);

	{
		LightPanel *panel = new LightPanel();
		panel->box.size = box.size;
		addChild(panel);
	}

	{
		BouncyBallDisplay *display = new BouncyBallDisplay();
		display->module = module;
		display->box.pos = Vec(2, 2);
		display->box.size = Vec(box.size.x - 4, RACK_GRID_HEIGHT - 40);
		addChild(display);

		module->displayWidth = display->box.size.x;
		module->displayHeight = display->box.size.y;
		module->updateMinMax();
		module->defaultPos();
	}

	rack::Label* const titleLabel = new rack::Label;
	titleLabel->box.pos = Vec(3, 350);
	titleLabel->text = "Bouncy Ball";
	addChild(titleLabel);

	////////////////////////////////////////////////////////////
	rack::Label* const xOffsetLabel = new rack::Label;
	xOffsetLabel->box.pos = Vec(120-20, 340);
	xOffsetLabel->text = "X OFST";
	addChild(xOffsetLabel);

	rack::Label* const yOffsetLabel = new rack::Label;
	yOffsetLabel->box.pos = Vec(180-20, 340);
	yOffsetLabel->text = "Y OFST";
	addChild(yOffsetLabel);

	rack::Label* const xLabel = new rack::Label;
	xLabel->box.pos = Vec(240-4, 340);
	xLabel->text = "X";
	addChild(xLabel);

	rack::Label* const yLabel = new rack::Label;
	yLabel->box.pos = Vec(260-4, 340);
	yLabel->text = "Y";
	addChild(yLabel);

	rack::Label* const gLabel = new rack::Label;
	gLabel->box.pos = Vec(340-5, 340);
	gLabel->text = "G";
	addChild(gLabel);

	addParam(createParam<TinyBlackKnob>(Vec(120, 360), module, BouncyBall::OFFSET_X_VOLTS_PARAM, -5.0, 5.0, 0.0));
	addParam(createParam<TinyBlackKnob>(Vec(180, 360), module, BouncyBall::OFFSET_Y_VOLTS_PARAM, -5.0, 5.0, 0.0));
	addOutput(createOutput<TinyPJ301MPort>(Vec(240, 360), module, BouncyBall::X_OUTPUT));
	addOutput(createOutput<TinyPJ301MPort>(Vec(260, 360), module, BouncyBall::Y_OUTPUT));
	addOutput(createOutput<TinyPJ301MPort>(Vec(340, 360), module, BouncyBall::GATE_OUTPUT));
}

