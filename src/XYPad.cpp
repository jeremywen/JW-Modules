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
		LOOP_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		PLAYBACK_INPUT,
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
	float repeatLight = 0.0;
	bool loopModeOn = false;
	bool playing = false;
	SchmittTrigger loopBtnTrigger;
	SchmittTrigger playbackTrigger;
	std::vector<Vec> points;
	int curPointIdx = 0;

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

	bool insideBox(float x, float y){
		return x <= maxX && x >= 0 && y <= maxY && y >=0;
	}

	void updatePos(float x, float y){
		if(!playing){
			points.push_back(Vec(x, y));
		}
		params[XYPad::X_POS_PARAM].value = clampf(x, minX, maxX);
		params[XYPad::Y_POS_PARAM].value = clampf(y, minY, maxY);
	}

	void updateMinMax(){
		minX = totalBallSize;
		minY = totalBallSize;
		maxX = displayWidth - totalBallSize;
		maxY = displayHeight - totalBallSize;

	}

	void playback(){
		if(playing && points.size() > 0){ 
			params[XYPad::X_POS_PARAM].value = points[curPointIdx].x;
			params[XYPad::Y_POS_PARAM].value = points[curPointIdx].y;
			if(curPointIdx < points.size()){
				curPointIdx++;
				params[XYPad::GATE_PARAM].value = true;
			} else {
				params[XYPad::GATE_PARAM].value = false;
				curPointIdx = 0;
			}
		}
	}

	void startPlayback(){
		printf("start\n");
		curPointIdx = 0;
		playing = true;
	}

	void stopPlayback(bool clearPoints){
		printf("stop\n");
		curPointIdx = 0;
		playing = false;
		params[XYPad::GATE_PARAM].value = false;
		if(clearPoints){ points.clear(); }
	}

};

void XYPad::step() {
	if (loopBtnTrigger.process(params[LOOP_PARAM].value)) {
		loopModeOn = !loopModeOn;
		if(!loopModeOn){ //stop on loop off
			stopPlayback(false);
		}
	}
	repeatLight = loopModeOn ? 1.0 : 0.0;

	// if (inputs[PLAYBACK_INPUT].active) {
	// 	if (playbackTrigger.process(inputs[PLAYBACK_INPUT].value)) {
	// 	} else {
	// 	}
	// }

	outputs[X_OUTPUT].value = rescalef(params[X_POS_PARAM].value, minX, maxX, minVolt, maxVolt) + params[OFFSET_X_VOLTS_PARAM].value;
	outputs[Y_OUTPUT].value = rescalef(params[Y_POS_PARAM].value, minY, maxY, maxVolt, minVolt) + params[OFFSET_Y_VOLTS_PARAM].value;//y is inverted because gui coords
	
	outputs[X_INV_OUTPUT].value = rescalef(params[X_POS_PARAM].value, minX, maxX, maxVolt, minVolt) + params[OFFSET_X_VOLTS_PARAM].value;
	outputs[Y_INV_OUTPUT].value = rescalef(params[Y_POS_PARAM].value, minY, maxY, minVolt, maxVolt) + params[OFFSET_Y_VOLTS_PARAM].value;//y is inverted because gui coords
	
	outputs[GATE_OUTPUT].value = rescalef(params[GATE_PARAM].value, 0.0, 1.0, 0.0, 10.0);
}

struct XYPadDisplay : Widget {
	XYPad *module;
	bool mouseDown = false;
	bool playAfterEnabled = true;//TODO

	XYPadDisplay() {}

	void setMouseDown(bool down){
		mouseDown = down;
		module->params[XYPad::GATE_PARAM].value = mouseDown;
		if(module->loopModeOn){
			if(down && module->playing){
				//stop playing and clear points on new recording
				module->stopPlayback(true);
			} else if(!down && !module->playing) {
				// done dragging mouse
				module->startPlayback();
			}
		}
	}

	Widget *onMouseDown(Vec pos, int button){setMouseDown(true); return this; }
	Widget *onMouseUp(Vec pos, int button){setMouseDown(false); return this; }
	void onDragEnd(){ setMouseDown(false); }
	void onMouseEnter() {}
	void onMouseLeave() {}
	void onDragStart() {}

	Widget *onMouseMove(Vec pos, Vec mouseRel){
		if(mouseDown && module->insideBox(pos.x, pos.y)){ //onDragMove handles outside
			// printf("onMouseMove\n");
			module->updatePos(pos.x, pos.y);
		}
		return this;
	}
	void onDragMove(Vec mouseRel) {
		if(mouseDown){
			float newX = module->params[XYPad::X_POS_PARAM].value + mouseRel.x;
			float newY = module->params[XYPad::Y_POS_PARAM].value + mouseRel.y;
			if(!module->insideBox(newX, newY)){ //onMouseMove handles inside
				// printf("onDragMove %f %f\n", newX, newY);
				module->updatePos(newX, newY);
			}
		}
	}

	void draw(NVGcontext *vg) {
		module->playback();

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
	rack::Label* const loopLabel = new rack::Label;
	loopLabel->box.pos = Vec(93-20, 340);
	loopLabel->text = "Loop";
	addChild(loopLabel);

	rack::Label* const xOffsetLabel = new rack::Label;
	xOffsetLabel->box.pos = Vec(145-20, 340);
	xOffsetLabel->text = "X OFST";
	addChild(xOffsetLabel);

	rack::Label* const yOffsetLabel = new rack::Label;
	yOffsetLabel->box.pos = Vec(200-20, 340);
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

	// addInput(createInput<TinyPJ301MPort>(Vec(70, 363), module, XYPad::PLAYBACK_INPUT));

	addParam(createParam<LEDButton>(Vec(85, 358), module, XYPad::LOOP_PARAM, 0.0, 1.0, 0.0));
	addChild(createValueLight<SmallLight<MyBlueValueLight>>(Vec(85+5, 358+5), &module->repeatLight));

	addParam(createParam<TinyBlackKnob>(Vec(145, 360), module, XYPad::OFFSET_X_VOLTS_PARAM, -5.0, 5.0, 0.0));
	addParam(createParam<TinyBlackKnob>(Vec(200, 360), module, XYPad::OFFSET_Y_VOLTS_PARAM, -5.0, 5.0, 0.0));
	addOutput(createOutput<TinyPJ301MPort>(Vec(240, 360), module, XYPad::X_OUTPUT));
	addOutput(createOutput<TinyPJ301MPort>(Vec(260, 360), module, XYPad::Y_OUTPUT));
	addOutput(createOutput<TinyPJ301MPort>(Vec(290, 360), module, XYPad::X_INV_OUTPUT));
	addOutput(createOutput<TinyPJ301MPort>(Vec(310, 360), module, XYPad::Y_INV_OUTPUT));
	addOutput(createOutput<TinyPJ301MPort>(Vec(340, 360), module, XYPad::GATE_OUTPUT));
}

