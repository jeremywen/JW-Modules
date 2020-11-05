#include <string.h>
#include "JWModules.hpp"
#include "JWResizableHandle.hpp"

struct ThingThingBall {
	NVGcolor color;
};

struct ThingThing : Module {
	enum ParamIds {
		BALL_RAD_PARAM,
		ZOOM_MULT_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		BALL_RAD_INPUT,
		ZOOM_MULT_INPUT,
		ANG_INPUT,
		NUM_INPUTS = ANG_INPUT + 5
	};
	enum OutputIds {
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};
	
	ThingThingBall *balls = new ThingThingBall[5];
	float atten[5] = {1, 1, 1, 1, 1};
	// float atten[5] = {0.0, 0.25, 0.5, 0.75, 1};

	ThingThing() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(BALL_RAD_PARAM, 0.0, 30.0, 10.0, "Radius");
		configParam(ZOOM_MULT_PARAM, 1.0, 200.0, 20.0, "Length");
		balls[0].color = nvgRGB(255, 255, 255);//white
		balls[1].color = nvgRGB(255, 151, 9);//orange
		balls[2].color = nvgRGB(255, 243, 9);//yellow
		balls[3].color = nvgRGB(144, 26, 252);//purple
		balls[4].color = nvgRGB(25, 150, 252);//blue
	}
	~ThingThing() {
		delete [] balls;
	}

	void process(const ProcessArgs &args) override {};
	void onReset() override {}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {}
};

struct ThingThingDisplay : LightWidget {
	ThingThing *module;
	ThingThingDisplay(){}

	void draw(const DrawArgs &args) override {
		//background
		nvgFillColor(args.vg, nvgRGB(0, 0, 0));
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
		nvgFill(args.vg);

		if(module == NULL) return;

		float ballRadius = module->params[ThingThing::BALL_RAD_PARAM].getValue();
		if(module->inputs[ThingThing::BALL_RAD_INPUT].isConnected()){
			ballRadius += rescalefjw(module->inputs[ThingThing::BALL_RAD_INPUT].getVoltage(), -5.0, 5.0, 0.0, 30.0);
		}

		float zoom = module->params[ThingThing::ZOOM_MULT_PARAM].getValue();
		if(module->inputs[ThingThing::ZOOM_MULT_INPUT].isConnected()){
			zoom += rescalefjw(module->inputs[ThingThing::ZOOM_MULT_INPUT].getVoltage(), -5.0, 5.0, 1.0, 50.0);
		}

      float x[5];
      float y[5];
      float angle[5];

      for(int i=0; i<5; i++){
         angle[i] = i==0 ? 0 : (module->inputs[ThingThing::ANG_INPUT+i].getVoltage() + angle[i-1]) * module->atten[i];
			x[i] = i==0 ? 0 : sinf(rescalefjw(angle[i], -5, 5, -2*M_PI + M_PI/2.0f, 2*M_PI + M_PI/2.0f)) * zoom;
			y[i] = i==0 ? 0 : cosf(rescalefjw(angle[i], -5, 5, -2*M_PI + M_PI/2.0f, 2*M_PI + M_PI/2.0f)) * zoom;
      }

		/////////////////////// LINES ///////////////////////
		nvgSave(args.vg);
		nvgTranslate(args.vg, box.size.x * 0.5, box.size.y * 0.5);
		for(int i=0; i<5; i++){
			nvgTranslate(args.vg, x[i], y[i]);
			nvgStrokeColor(args.vg, nvgRGB(255, 255, 255));
			if(i>0){
				nvgStrokeWidth(args.vg, 1);
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg, 0, 0);
				nvgLineTo(args.vg, -x[i], -y[i]);
				nvgStroke(args.vg);
			}
		}
		nvgRestore(args.vg);

		/////////////////////// BALLS ///////////////////////
		nvgSave(args.vg);
		nvgTranslate(args.vg, box.size.x * 0.5, box.size.y * 0.5);
		for(int i=0; i<5; i++){
			nvgTranslate(args.vg, x[i], y[i]);
			nvgStrokeColor(args.vg, module->balls[i].color);
			nvgFillColor(args.vg, module->balls[i].color);
			nvgStrokeWidth(args.vg, 2);
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, 0, 0, ballRadius);
			nvgFill(args.vg);
			nvgStroke(args.vg);
		}
		nvgRestore(args.vg);
	}
};


struct ThingThingWidget : ModuleWidget {
	ThingThingWidget(ThingThing *module);
	ThingThingDisplay *display;
	BGPanel *panel;
	JWModuleResizeHandle *rightHandle;
	void step() override;
	json_t *toJson() override;
	void fromJson(json_t *rootJ) override;
};

ThingThingWidget::ThingThingWidget(ThingThing *module) {
	setModule(module);
	box.size = Vec(RACK_GRID_WIDTH*20, RACK_GRID_HEIGHT);

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

	display = new ThingThingDisplay();
	display->module = module;
	display->box.pos = Vec(0, 0);
	display->box.size = Vec(box.size.x, RACK_GRID_HEIGHT);
	addChild(display);

	addChild(createWidget<Screw_J>(Vec(265, 365)));
	addChild(createWidget<Screw_W>(Vec(280, 365)));

	for(int i=0; i<4; i++){
		addInput(createInput<TinyPJ301MPort>(Vec(5+(20*i), 360), module, ThingThing::ANG_INPUT+i+1));
	}
	
	addInput(createInput<TinyPJ301MPort>(Vec(140, 360), module, ThingThing::BALL_RAD_INPUT));
	addParam(createParam<JwTinyKnob>(Vec(155, 360), module, ThingThing::BALL_RAD_PARAM));

	addInput(createInput<TinyPJ301MPort>(Vec(190, 360), module, ThingThing::ZOOM_MULT_INPUT));
	addParam(createParam<JwTinyKnob>(Vec(205, 360), module, ThingThing::ZOOM_MULT_PARAM));
}
void ThingThingWidget::step() {
	panel->box.size = box.size;
	if (box.size.x < RACK_GRID_WIDTH * 20) box.size.x = RACK_GRID_WIDTH * 20;
	display->box.size = Vec(box.size.x, box.size.y);
	rightHandle->box.pos.x = box.size.x - rightHandle->box.size.x;
	ModuleWidget::step();
}

json_t *ThingThingWidget::toJson() {
	json_t *rootJ = ModuleWidget::toJson();
	json_object_set_new(rootJ, "width", json_real(box.size.x));
	return rootJ;
}

void ThingThingWidget::fromJson(json_t *rootJ) {
	ModuleWidget::fromJson(rootJ);
	json_t *widthJ = json_object_get(rootJ, "width");
	if (widthJ)	box.size.x = json_number_value(widthJ);
}
Model *modelThingThing = createModel<ThingThing, ThingThingWidget>("ThingThing");
