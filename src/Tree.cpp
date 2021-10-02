#include <string.h>
#include <algorithm>
#include "JWModules.hpp"
#include "JWResizableHandle.hpp"

#define RND_NUMS 25
struct Tree : Module {
	enum ParamIds {
		ANGLE_PARAM,
		HUE_PARAM,
		REDUCE_PARAM,
		LENGTH_PARAM,
		HEIGHT_PARAM,
		RND_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		ANGLE_INPUT,
		HUE_INPUT,
		REDUCE_INPUT,
		LENGTH_INPUT,
		HEIGHT_INPUT,
		RND_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	float rnd[RND_NUMS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	dsp::SchmittTrigger rndTrigger;
	float width = RACK_GRID_WIDTH*20;

	Tree() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(ANGLE_PARAM, 0.0, 90.0, 25.0, "Angle");
		configParam(HUE_PARAM, 0.0, 0.5, 0.1, "Color");
		configParam(REDUCE_PARAM, 0.1, 0.68, 0.66, "Reduce");
		configParam(LENGTH_PARAM, 10.0, 200.0, 50, "Length");
		configParam(HEIGHT_PARAM, 10.0, 250.0, 100, "Height");
		configParam(RND_PARAM, 0.0, 1.0, 0.0, "Jitter");
	}

	~Tree() {
	}

	void onReset() override {
		for(int i=0;i<RND_NUMS;i++){
			rnd[i] = 0;
		}
	}

	void onRandomize() override {
	}

	void generateRnd() {
		for(int i=0;i<RND_NUMS;i++){
			rnd[i] = (random::uniform() - 0.5) * 2;
		}
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		json_object_set_new(rootJ, "width", json_real(width));
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		json_t *widthJ = json_object_get(rootJ, "width");
		if (widthJ)
			width = json_number_value(widthJ);
	}

	void process(const ProcessArgs &args) override {
		if (rndTrigger.process(inputs[RND_INPUT].getVoltage())) {
			generateRnd();
		}
	}
};

struct TreeDisplay : LightWidget {
	Tree *module;
	float theta;
	TreeDisplay(){}

	void draw(const DrawArgs &args) override {
		nvgScissor(args.vg, RECT_ARGS(box));
		
		//background
		nvgFillColor(args.vg, nvgRGB(0, 0, 0));
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
		nvgFill(args.vg);

		float v = 2.5;
		if(module){
			float inputOffset = module->inputs[Tree::ANGLE_INPUT].isConnected() ? module->inputs[Tree::ANGLE_INPUT].getVoltage() : 0.0;
			v = clampfjw(module->params[Tree::ANGLE_PARAM].getValue()/9.0 + inputOffset, 0, 10.0);
		}
		float ang = rescalefjw(v, 0, 10, 0.0, 90.0);
		theta = nvgDegToRad(ang);

		float hue = 0.1;
		if(module){
			float inputOffset = module->inputs[Tree::HUE_INPUT].isConnected() ? module->inputs[Tree::HUE_INPUT].getVoltage() : 0.0;
			hue = clampfjw(module->params[Tree::HUE_PARAM].getValue() + inputOffset/10, 0, 1.0);
		}
		float reduce = 0.65;
		if(module){
			float inputOffset = module->inputs[Tree::REDUCE_INPUT].isConnected() ? module->inputs[Tree::REDUCE_INPUT].getVoltage() : 0.0;
			reduce = clampfjw(module->params[Tree::REDUCE_PARAM].getValue() + rescale(inputOffset,-5,5,0.05,0.33), 0.1, 0.66);
		}
		float length = 110.0;
		if(module){
			float inputOffset = module->inputs[Tree::LENGTH_INPUT].isConnected() ? module->inputs[Tree::LENGTH_INPUT].getVoltage() : 0.0;
			length = clampfjw(module->params[Tree::LENGTH_PARAM].getValue() + rescale(inputOffset,-5,5,5,100), 10.0, 200.0);
		}
		float height = 150.0;
		if(module){
			float inputOffset = module->inputs[Tree::HEIGHT_INPUT].isConnected() ? module->inputs[Tree::HEIGHT_INPUT].getVoltage() : 0.0;
			height = clampfjw(module->params[Tree::HEIGHT_PARAM].getValue() + rescale(inputOffset,-5,5,5,125), 10.0, 250.0);
		}
		//https://processing.org/examples/tree.html
		nvgTranslate(args.vg, box.size.x * 0.5, box.size.y);
		// nvgStrokeColor(args.vg, nvgRGB(255, 255, 255));
		// nvgStrokeColor(args.vg, nvgHSLA(0.1, 0.5, 1, 0xc0));
		nvgStrokeColor(args.vg, nvgHSLA(hue, 0.5, 0.5, 0xc0));
		int strokeW = 2;
		nvgStrokeWidth(args.vg, strokeW);
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, 0, 0);
		nvgLineTo(args.vg, 0, -height);
		nvgStroke(args.vg);
		nvgTranslate(args.vg, 0, -height);

		branch(args, length, reduce, 1, strokeW, hue);
		nvgResetScissor(args.vg);
	}

	void branch(const DrawArgs &args, float dist, float reduce, int count, int strokeW, float hue){
		dist *= reduce;
		if (dist > 2) {
			// DEBUG("count*0.001=%f",count*0.001);
			count++;
			float rnd1 = module ? module->rnd[count % RND_NUMS] : 0;
			float rnd2 = module ? module->rnd[count+1 % RND_NUMS] : 0;
			nvgStrokeColor(args.vg, nvgHSLA(hue * count * 0.5, 0.5, 0.5, 0xc0));

			nvgSave(args.vg);
			nvgRotate(args.vg, theta + rnd1);
			nvgStrokeWidth(args.vg, strokeW);
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, 0, 0);
			nvgLineTo(args.vg, 0, -dist);
			nvgStroke(args.vg);
			nvgTranslate(args.vg, 0, -dist);
			branch(args, dist, reduce, count, strokeW, hue);
			nvgRestore(args.vg);

			nvgSave(args.vg);
			nvgRotate(args.vg, -theta + rnd1);
			nvgStrokeWidth(args.vg, strokeW);
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, 0, 0);
			nvgLineTo(args.vg, 0, -dist);
			nvgStroke(args.vg);
			nvgTranslate(args.vg, 0, -dist);
			branch(args, dist, reduce, count, strokeW, hue);
			nvgRestore(args.vg);
		}
	}
};

struct TreeWidget : ModuleWidget { 
	TreeDisplay *display;
	BGPanel *panel;
	JWModuleResizeHandle *rightHandle;
	TreeWidget(Tree *module); 
	void step() override;
	void appendContextMenu(Menu *menu) override;
};

struct RandomizeButton : TinyButton {
	void onButton(const event::Button &e) override {
		TinyButton::onButton(e);
		if(e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT){
			TreeWidget *tw = this->getAncestorOfType<TreeWidget>();
			Tree *twm = dynamic_cast<Tree*>(tw->module);
			twm->generateRnd();
		}
	}
};

TreeWidget::TreeWidget(Tree *module) {
	setModule(module);
	box.size = Vec(module ? module->width : RACK_GRID_WIDTH*20, RACK_GRID_HEIGHT);

	{
		panel = new BGPanel(nvgRGB(0, 0, 0));
		panel->box.size = box.size;
		addChild(panel);
	}

	JWModuleResizeHandle *leftHandle = new JWModuleResizeHandle;
	JWModuleResizeHandle *rightHandle = new JWModuleResizeHandle;
	rightHandle->right = true;
	this->rightHandle = rightHandle;
	addChild(leftHandle);
	addChild(rightHandle);

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

	addInput(createInput<TinyPJ301MPort>(Vec(205, 360), module, Tree::RND_INPUT));
	addParam(createParam<RandomizeButton>(Vec(220, 360), module, Tree::RND_PARAM));

}

void TreeWidget::step() {
	panel->box.size = box.size;
	if (box.size.x < RACK_GRID_WIDTH * 20) box.size.x = RACK_GRID_WIDTH * 20;
	display->box.size = Vec(box.size.x, box.size.y);
	rightHandle->box.pos.x = box.size.x - rightHandle->box.size.x;
	
	Tree *tree = dynamic_cast<Tree*>(module);
	if(tree){
		tree->width = box.size.x;
	}
	ModuleWidget::step();
}

void TreeWidget::appendContextMenu(Menu *menu) {
}

Model *modelTree = createModel<Tree, TreeWidget>("Tree");
