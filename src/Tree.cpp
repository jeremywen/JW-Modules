#include <string.h>
#include <algorithm>
#include "JWModules.hpp"

struct Tree : Module {
	enum ParamIds {
		ANGLE_PARAM,
		HUE_PARAM,
		REDUCE_PARAM,
		LENGTH_PARAM,
		HEIGHT_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		ANGLE_INPUT,
		HUE_INPUT,
		REDUCE_INPUT,
		LENGTH_INPUT,
		HEIGHT_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	Tree() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(ANGLE_PARAM, 0.0, 90.0, 10.0, "Angle");
		configParam(HUE_PARAM, 0.0, 1.0, 0.59, "Color");
		configParam(REDUCE_PARAM, 0.1, 0.68, 0.66, "Reduce");
		configParam(LENGTH_PARAM, 10.0, 200.0, 130, "Length");
		configParam(HEIGHT_PARAM, 10.0, 250.0, 120, "Height");
	}

	~Tree() {
	}

	void onRandomize() override {
	}

	void process(const ProcessArgs &args) override {
	}
};

struct TreeDisplay : TransparentWidget {
	Tree *module;
	float theta;
	TreeDisplay(){}

	//https://processing.org/examples/tree.html
	void draw(const DrawArgs &args) override {
		//background
		nvgFillColor(args.vg, nvgRGB(0, 0, 0));
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
		nvgFill(args.vg);

		nvgStrokeColor(args.vg, nvgRGB(255, 255, 255));
		float v = 5;
		if(module){
			float inputOffset = module->inputs[Tree::ANGLE_INPUT].isConnected() ? module->inputs[Tree::ANGLE_INPUT].getVoltage() : 0.0;
			v = clampfjw(module->params[Tree::ANGLE_PARAM].getValue()/9.0 + inputOffset, 0, 10.0);
		}
		float ang = rescalefjw(v, 0, 10, 0.0, 90.0);
		theta = nvgDegToRad(ang);

		float hue = 0.0;
		if(module){
			float inputOffset = module->inputs[Tree::HUE_INPUT].isConnected() ? module->inputs[Tree::HUE_INPUT].getVoltage() : 0.0;
			hue = clampfjw(module->params[Tree::HUE_PARAM].getValue() + inputOffset/10, 0, 1.0);
		}
		float reduce = 0.45;
		if(module){
			float inputOffset = module->inputs[Tree::REDUCE_INPUT].isConnected() ? module->inputs[Tree::REDUCE_INPUT].getVoltage() : 0.0;
			reduce = clampfjw(module->params[Tree::REDUCE_PARAM].getValue() + rescale(inputOffset,-5,5,0.05,0.33), 0.1, 0.66);
		}
		float length = 0.45;
		if(module){
			float inputOffset = module->inputs[Tree::LENGTH_INPUT].isConnected() ? module->inputs[Tree::LENGTH_INPUT].getVoltage() : 0.0;
			length = clampfjw(module->params[Tree::LENGTH_PARAM].getValue() + rescale(inputOffset,-5,5,5,100), 10.0, 200.0);
		}
		float height = 120.0;
		if(module){
			float inputOffset = module->inputs[Tree::HEIGHT_INPUT].isConnected() ? module->inputs[Tree::HEIGHT_INPUT].getVoltage() : 0.0;
			height = clampfjw(module->params[Tree::HEIGHT_PARAM].getValue() + rescale(inputOffset,-5,5,5,125), 10.0, 250.0);
		}
		nvgTranslate(args.vg, box.size.x * 0.5, box.size.y);
		nvgStrokeColor(args.vg, nvgHSLA(hue, 0.5, 0.5, 0xc0));
		nvgStrokeWidth(args.vg, 1);
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, 0, 0);
		nvgLineTo(args.vg, 0, -height);
		nvgStroke(args.vg);

		nvgTranslate(args.vg, 0, -height);

		branch(args, length, reduce);
	}

	void branch(const DrawArgs &args, float dist, float reduce){
		dist *= reduce;
		if (dist > 2) {
			nvgSave(args.vg);
			nvgRotate(args.vg, theta);
			nvgStrokeWidth(args.vg, 1);
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, 0, 0);
			nvgLineTo(args.vg, 0, -dist);
			nvgStroke(args.vg);
			nvgTranslate(args.vg, 0, -dist);
			branch(args, dist, reduce);
			nvgRestore(args.vg);

			nvgSave(args.vg);
			nvgRotate(args.vg, -theta);
			nvgStrokeWidth(args.vg, 1);
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, 0, 0);
			nvgLineTo(args.vg, 0, -dist);
			nvgStroke(args.vg);
			nvgTranslate(args.vg, 0, -dist);
			branch(args, dist, reduce);
			nvgRestore(args.vg);
		}
	}
};

struct TreeWidget : ModuleWidget { 
	TreeWidget(Tree *module); 
	TreeDisplay *display;
	void appendContextMenu(Menu *menu) override;
};

TreeWidget::TreeWidget(Tree *module) {
	setModule(module);
	box.size = Vec(RACK_GRID_WIDTH*20, RACK_GRID_HEIGHT);

	{
		TreeDisplay *display = new TreeDisplay();
		display->module = module;
		display->box.pos = Vec(0, 0);
		display->box.size = Vec(box.size.x, box.size.y);
		addChild(display);
		this->display = display;
	}

	addChild(createWidget<Screw_J>(Vec(265, 365)));
	addChild(createWidget<Screw_W>(Vec(280, 365)));

	addInput(createInput<TinyPJ301MPort>(Vec(5, 360), module, Tree::ANGLE_INPUT));
	addParam(createParam<JwTinyKnob>(Vec(20, 360), module, Tree::ANGLE_PARAM));

	addInput(createInput<TinyPJ301MPort>(Vec(45, 360), module, Tree::LENGTH_INPUT));
	addParam(createParam<JwTinyKnob>(Vec(60, 360), module, Tree::LENGTH_PARAM));

	addInput(createInput<TinyPJ301MPort>(Vec(85, 360), module, Tree::HEIGHT_INPUT));
	addParam(createParam<JwTinyKnob>(Vec(100, 360), module, Tree::HEIGHT_PARAM));

	addInput(createInput<TinyPJ301MPort>(Vec(125, 360), module, Tree::REDUCE_INPUT));
	addParam(createParam<JwTinyKnob>(Vec(140, 360), module, Tree::REDUCE_PARAM));

	addInput(createInput<TinyPJ301MPort>(Vec(165, 360), module, Tree::HUE_INPUT));
	addParam(createParam<JwTinyKnob>(Vec(180, 360), module, Tree::HUE_PARAM));

}

void TreeWidget::appendContextMenu(Menu *menu) {
}

Model *modelTree = createModel<Tree, TreeWidget>("Tree");
