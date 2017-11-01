#include <string.h>
#include "JWModules.hpp"
#include "dsp/digital.hpp"

struct XYPad : Module {
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
		X_INV_OUTPUT,
		Y_INV_OUTPUT,
		GATE_OUTPUT,
		NUM_OUTPUTS
	};
	
	float minX = 0, minY = 0, maxX = 0, maxY = 0;
	float displayWidth = 0, displayHeight = 0;
	float totalBallSize = 12;
	float minVolt = -5, maxVolt = 5;
	
	XYPad() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS) {}
	void step();
	void initialize(){
		defaultPos();
	}

	json_t *toJson() {
		json_t *rootJ = json_object();
		json_object_set_new(rootJ, "xPos", json_real(params[X_POS_PARAM].value));
		json_object_set_new(rootJ, "yPos", json_real(params[Y_POS_PARAM].value));
		return rootJ;
	}

	void fromJson(json_t *rootJ) {
		json_t *xPosJ = json_object_get(rootJ, "xPos");
		json_t *yPosJ = json_object_get(rootJ, "yPos");
		if (xPosJ){ params[X_POS_PARAM].value = json_real_value(xPosJ); }
		if (yPosJ){ params[Y_POS_PARAM].value = json_real_value(yPosJ); }
	}

	void defaultPos() {
		params[XYPad::X_POS_PARAM].value = displayWidth / 2.0;
		params[XYPad::Y_POS_PARAM].value = displayHeight / 2.0;		
	}

	void updateMinMax(){
		minX = totalBallSize;
		minY = totalBallSize;
		maxX = displayWidth - totalBallSize;
		maxY = displayHeight - totalBallSize;

	}
};

void XYPad::step() {
	outputs[X_OUTPUT].value = rescalef(params[X_POS_PARAM].value, minX, maxX, minVolt, maxVolt) + params[OFFSET_X_VOLTS_PARAM].value;
	outputs[Y_OUTPUT].value = rescalef(params[Y_POS_PARAM].value, minY, maxY, maxVolt, minVolt) + params[OFFSET_Y_VOLTS_PARAM].value;//y is inverted because gui coords
	
	outputs[X_INV_OUTPUT].value = rescalef(params[X_POS_PARAM].value, minX, maxX, maxVolt, minVolt) + params[OFFSET_X_VOLTS_PARAM].value;
	outputs[Y_INV_OUTPUT].value = rescalef(params[Y_POS_PARAM].value, minY, maxY, minVolt, maxVolt) + params[OFFSET_Y_VOLTS_PARAM].value;//y is inverted because gui coords
	
	outputs[GATE_OUTPUT].value = rescalef(params[GATE_PARAM].value, 0.0, 1.0, 0.0, 10.0);
}

struct XYPadDisplay : Widget {
	XYPad *module;
	bool mouseDown = false;
	XYPadDisplay() {

	}

	void setMouseDown(bool down){
		mouseDown = down;
		module->params[XYPad::GATE_PARAM].value = down;
	}

	Widget *onMouseDown(Vec pos, int button){
		setMouseDown(true);
		return this;
	}

	Widget *onMouseUp(Vec pos, int button){
		setMouseDown(false);
		return this;
	}

	Widget *onMouseMove(Vec pos, Vec mouseRel){
		if(mouseDown){
			module->params[XYPad::X_POS_PARAM].value = clampf(pos.x, module->minX, module->maxX);
			module->params[XYPad::Y_POS_PARAM].value = clampf(pos.y, module->minY, module->maxY);
		}
		return this;
	}
	void onMouseEnter() {}
	void onMouseLeave() {}
	void onDragStart() {}
	void onDragEnd() {
		setMouseDown(false);
	}
	void onDragMove(Vec mouseRel) {
		if(mouseDown){
			float newX = module->params[XYPad::X_POS_PARAM].value + mouseRel.x;
			float newY = module->params[XYPad::Y_POS_PARAM].value + mouseRel.y;
			module->params[XYPad::X_POS_PARAM].value = clampf(newX, module->minX, module->maxX);
			module->params[XYPad::Y_POS_PARAM].value = clampf(newY, module->minY, module->maxY);
		}
	}

	void draw(NVGcontext *vg) {
		float ballX = module->params[XYPad::X_POS_PARAM].value;
		float ballY = module->params[XYPad::Y_POS_PARAM].value;
		float invBallX = module->displayWidth-ballX;
		float invBallY = module->displayHeight-ballY;

		//background
		nvgFillColor(vg, nvgRGB(20, 30, 33));
		nvgBeginPath(vg);
		nvgRect(vg, 0, 0, box.size.x, box.size.y);
		nvgFill(vg);
			
		//INVERTED///////////////////////////////////
		NVGcolor invertedCol = nvgRGB(20, 50, 53);

		//horizontal line
		nvgStrokeColor(vg, invertedCol);
		nvgBeginPath(vg);
		nvgMoveTo(vg, 0, invBallY);
		nvgLineTo(vg, box.size.x, invBallY);
		nvgStroke(vg);
		
		//vertical line
		nvgStrokeColor(vg, invertedCol);
		nvgBeginPath(vg);
		nvgMoveTo(vg, invBallX, 0);
		nvgLineTo(vg, invBallX, box.size.y);
		nvgStroke(vg);
		
		//inv ball
		nvgFillColor(vg, invertedCol);
		nvgStrokeColor(vg, invertedCol);
		nvgStrokeWidth(vg, 2);
		nvgBeginPath(vg);
		nvgCircle(vg, module->displayWidth-ballX, module->displayHeight-ballY, 10);
		if(mouseDown)nvgFill(vg);
		nvgStroke(vg);
		
		//MAIN///////////////////////////////////

		//horizontal line
		nvgStrokeColor(vg, nvgRGB(255, 255, 255));
		nvgBeginPath(vg);
		nvgMoveTo(vg, 0, ballY);
		nvgLineTo(vg, box.size.x, ballY);
		nvgStroke(vg);
		
		//vertical line
		nvgStrokeColor(vg, nvgRGB(255, 255, 255));
		nvgBeginPath(vg);
		nvgMoveTo(vg, ballX, 0);
		nvgLineTo(vg, ballX, box.size.y);
		nvgStroke(vg);
		
		//ball
		nvgFillColor(vg, nvgRGB(25, 150, 252));
		nvgStrokeColor(vg, nvgRGB(25, 150, 252));
		nvgStrokeWidth(vg, 2);
		nvgBeginPath(vg);
		nvgCircle(vg, ballX, ballY, 10);
		if(mouseDown)nvgFill(vg);
		nvgStroke(vg);
	}
};

XYPadWidget::XYPadWidget() {
	XYPad *module = new XYPad();
	setModule(module);
	box.size = Vec(RACK_GRID_WIDTH*24, RACK_GRID_HEIGHT);

	{
		LightPanel *panel = new LightPanel();
		panel->box.size = box.size;
		addChild(panel);
	}

	{
		XYPadDisplay *display = new XYPadDisplay();
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
	titleLabel->text = "XY Pad";
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

	rack::Label* const xInvLabel = new rack::Label;
	xInvLabel->box.pos = Vec(290-7, 340);
	xInvLabel->text = "-X";
	addChild(xInvLabel);

	rack::Label* const yInvLabel = new rack::Label;
	yInvLabel->box.pos = Vec(310-7, 340);
	yInvLabel->text = "-Y";
	addChild(yInvLabel);

	rack::Label* const gLabel = new rack::Label;
	gLabel->box.pos = Vec(340-5, 340);
	gLabel->text = "G";
	addChild(gLabel);

	addParam(createParam<TinyBlackKnob>(Vec(120, 360), module, XYPad::OFFSET_X_VOLTS_PARAM, -5.0, 5.0, 0.0));
	addParam(createParam<TinyBlackKnob>(Vec(180, 360), module, XYPad::OFFSET_Y_VOLTS_PARAM, -5.0, 5.0, 0.0));
	addOutput(createOutput<TinyPJ301MPort>(Vec(240, 360), module, XYPad::X_OUTPUT));
	addOutput(createOutput<TinyPJ301MPort>(Vec(260, 360), module, XYPad::Y_OUTPUT));
	addOutput(createOutput<TinyPJ301MPort>(Vec(290, 360), module, XYPad::X_INV_OUTPUT));
	addOutput(createOutput<TinyPJ301MPort>(Vec(310, 360), module, XYPad::Y_INV_OUTPUT));
	addOutput(createOutput<TinyPJ301MPort>(Vec(340, 360), module, XYPad::GATE_OUTPUT));
}

