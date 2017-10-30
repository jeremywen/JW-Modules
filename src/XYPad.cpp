#include <string.h>
#include "JWModules.hpp"
#include "dsp/digital.hpp"

struct XYPad : Module {
	enum ParamIds {
		X_POS_PARAM,
		Y_POS_PARAM,
		GATE_PARAM,
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

	XYPad() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS) {}
	void step();

	void initialize() {
	}
};

void XYPad::step() {
	outputs[X_OUTPUT].value = rescalef(params[X_POS_PARAM].value, 0.0, 300, 0.0, 10.0);
	outputs[Y_OUTPUT].value = rescalef(params[Y_POS_PARAM].value, 0.0, 300, 10.0, 0.0);
	outputs[GATE_OUTPUT].value = rescalef(params[GATE_PARAM].value, 0.0, 1, 0, 10.0);
}

struct XYPadDisplay : Widget {
	XYPad *module;
	bool mouseDown = false;
	float minX = 5, minY = 5, maxX = 290, maxY = 370;
	XYPadDisplay() {}

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
		if(mouseDown 
			 && pos.x > minX && pos.x < maxX
			 && pos.y > minY && pos.y < maxY ){
			module->params[XYPad::X_POS_PARAM].value = pos.x;
			module->params[XYPad::Y_POS_PARAM].value = pos.y;
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
			if(newX > minX && newX < maxX){
				module->params[XYPad::X_POS_PARAM].value = newX;
			}
			float newY = module->params[XYPad::Y_POS_PARAM].value + mouseRel.y;
			if(newY > minY && newY < maxY){
				module->params[XYPad::Y_POS_PARAM].value = newY;
			}
		}

	}

	void draw(NVGcontext *vg) {
		float ballX = module->params[XYPad::X_POS_PARAM].value;
		float ballY = module->params[XYPad::Y_POS_PARAM].value;

		nvgFillColor(vg, nvgRGB(25, 150, 252));
		nvgStrokeColor(vg, nvgRGB(25, 150, 252));
		nvgStrokeWidth(vg, 2);
		
		// nvgBeginPath(vg);
		// nvgRect(vg, 20, 20, maxY, box.size.y-40);
		// nvgStroke(vg);
		
		nvgBeginPath(vg);
		nvgCircle(vg, ballX, ballY, 10);
		if(mouseDown)nvgFill(vg);
		nvgStroke(vg);
	}
};

XYPadWidget::XYPadWidget() {
	XYPad *module = new XYPad();
	setModule(module);
	box.size = Vec(RACK_GRID_WIDTH*20, RACK_GRID_HEIGHT);

	{
		LightPanel *panel = new LightPanel();
		panel->backgroundColor = nvgRGB(30, 40, 43);
		panel->box.size = box.size;
		addChild(panel);
	}

	{
		XYPadDisplay *display = new XYPadDisplay();
		display->module = module;
		display->box.pos = Vec(0, 0);
		display->box.size = Vec(box.size.x, RACK_GRID_HEIGHT);
		addChild(display);
	}

	addOutput(createOutput<TinyPJ301MPort>(Vec(box.size.x - 60, 360), module, XYPad::X_OUTPUT));
	addOutput(createOutput<TinyPJ301MPort>(Vec(box.size.x - 40, 360), module, XYPad::Y_OUTPUT));
	addOutput(createOutput<TinyPJ301MPort>(Vec(box.size.x - 20, 360), module, XYPad::GATE_OUTPUT));
}

