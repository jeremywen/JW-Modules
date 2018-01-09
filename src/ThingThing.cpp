#include <string.h>
#include "JWModules.hpp"
#include "dsp/digital.hpp"

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

	ThingThing() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		balls[0].color = nvgRGB(255, 255, 255);//white
		balls[1].color = nvgRGB(255, 151, 9);//orange
		balls[2].color = nvgRGB(255, 243, 9);//yellow
		balls[3].color = nvgRGB(144, 26, 252);//purple
		balls[4].color = nvgRGB(25, 150, 252);//blue
	}
	~ThingThing() {
		delete [] balls;
	}

	void step() override {};
	void reset() override {}

	json_t *toJson() override {
		json_t *rootJ = json_object();
		return rootJ;
	}

	void fromJson(json_t *rootJ) override {}
};

struct ThingThingDisplay : Widget {
	ThingThing *module;
	ThingThingDisplay(){}

	void draw(NVGcontext *vg) override {
		//background
		nvgFillColor(vg, nvgRGB(20, 30, 33));
		nvgBeginPath(vg);
		nvgRect(vg, 0, 0, box.size.x, box.size.y);
		nvgFill(vg);

		float ballRadius = module->params[ThingThing::BALL_RAD_PARAM].value;
		if(module->inputs[ThingThing::BALL_RAD_INPUT].active){
			ballRadius += rescalef(module->inputs[ThingThing::BALL_RAD_INPUT].value, -5.0, 5.0, 0.0, 30.0);
		}

		float zoom = module->params[ThingThing::ZOOM_MULT_PARAM].value;
		if(module->inputs[ThingThing::ZOOM_MULT_INPUT].active){
			ballRadius += rescalef(module->inputs[ThingThing::ZOOM_MULT_INPUT].value, -5.0, 5.0, 1.0, 50.0);
		}

		/////////////////////// LINES ///////////////////////
		nvgSave(vg);
		nvgTranslate(vg, box.size.x * 0.5, box.size.y * 0.5);
		for(int i=0; i<5; i++){
			float angle = module->inputs[ThingThing::ANG_INPUT+i].value * module->atten[i];
			float x = i==0 ? 0 : sinf(rescalef(angle, -5, 5, -2*M_PI, 2*M_PI)) * zoom;
			float y = i==0 ? 0 : cosf(rescalef(angle, -5, 5, -2*M_PI, 2*M_PI)) * zoom;
			nvgTranslate(vg, x, y);
			nvgStrokeColor(vg, nvgRGB(255, 255, 255));
			if(i>0){
				nvgStrokeWidth(vg, 1);
				nvgBeginPath(vg);
				nvgMoveTo(vg, 0, 0);
				nvgLineTo(vg, -x, -y);
				nvgStroke(vg);
			}
		}
		nvgRestore(vg);

		/////////////////////// BALLS ///////////////////////
		nvgSave(vg);
		nvgTranslate(vg, box.size.x * 0.5, box.size.y * 0.5);
		for(int i=0; i<5; i++){
			float angle = module->inputs[ThingThing::ANG_INPUT+i].value * module->atten[i];
			float x = i==0 ? 0 : sinf(rescalef(angle, -5, 5, -2*M_PI, 2*M_PI)) * zoom;
			float y = i==0 ? 0 : cosf(rescalef(angle, -5, 5, -2*M_PI, 2*M_PI)) * zoom;
			nvgTranslate(vg, x, y);
			nvgStrokeColor(vg, module->balls[i].color);
			nvgFillColor(vg, module->balls[i].color);
			nvgStrokeWidth(vg, 2);
			nvgBeginPath(vg);
			nvgCircle(vg, 0, 0, ballRadius);
			nvgFill(vg);
			nvgStroke(vg);
		}
		nvgRestore(vg);
	}
};


ThingThingWidget::ThingThingWidget() {
	ThingThing *module = new ThingThing();
	setModule(module);
	box.size = Vec(RACK_GRID_WIDTH*20, RACK_GRID_HEIGHT);

	SVGPanel *panel = new SVGPanel();
	panel->box.size = box.size;
	panel->setBackground(SVG::load(assetPlugin(plugin, "res/ThingThing.svg")));
	addChild(panel);

	ThingThingDisplay *display = new ThingThingDisplay();
	display->module = module;
	display->box.pos = Vec(0, 2);
	display->box.size = Vec(box.size.x, RACK_GRID_HEIGHT);
	addChild(display);

	addChild(createScrew<Screw_J>(Vec(265, 365)));
	addChild(createScrew<Screw_W>(Vec(280, 365)));

	for(int i=0; i<4; i++){
		addInput(createInput<TinyPJ301MPort>(Vec(5+(20*i), 360), module, ThingThing::ANG_INPUT+i+1));
	}
	
	addInput(createInput<TinyPJ301MPort>(Vec(140, 360), module, ThingThing::BALL_RAD_INPUT));
	addParam(createParam<JwTinyKnob>(Vec(155, 360), module, ThingThing::BALL_RAD_PARAM, 0.0, 30.0, 10.0));

	addInput(createInput<TinyPJ301MPort>(Vec(190, 360), module, ThingThing::ZOOM_MULT_INPUT));
	addParam(createParam<JwTinyKnob>(Vec(205, 360), module, ThingThing::ZOOM_MULT_PARAM, 1.0, 200.0, 20.0));
}
