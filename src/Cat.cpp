#include "JWModules.hpp"

struct Cat : Module {
	enum ParamIds {
		BOWL_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		NUM_INPUTS
	};
	enum OutputIds {
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	int catY = 0;
	bool goingDown = true;

	Cat() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(BOWL_PARAM, 0.0, 1.0, 0.0, "CLICK BOWL TO FEED THE CAT!");
	}
	
};

struct CatWidget : ModuleWidget {
	CatWidget(Cat *module);
	Widget* widgetToMove;
	Widget* hairballs[10];
	void step() override;
};

void CatWidget::step() {
	if(module != NULL){
		ModuleWidget::step();
		Cat *cat = dynamic_cast<Cat*>(module);
		widgetToMove->box.pos.y = cat->catY;

		
		if(cat->goingDown){
			cat->catY+=2;
			if(cat->catY > 250){
				cat->goingDown = false;
			}
		} else {
			cat->catY-=2;
			if(cat->catY < 15){
				cat->goingDown = true;
			}
		}

		for(int i=0; i<10; i++){
			if(hairballs[i]->box.pos.y > box.size.y*1.5 && !bool(cat->params[Cat::BOWL_PARAM].getValue())){
				hairballs[i]->box.pos.y = widgetToMove->box.pos.y;
			} else {
				hairballs[i]->box.pos.y += random::uniform()*10;
			}
		}
	}
};

CatWidget::CatWidget(Cat *module) {
	setModule(module);
	box.size = Vec(RACK_GRID_WIDTH*4, RACK_GRID_HEIGHT);

	BGPanel *panel = new BGPanel(nvgRGB(230, 230, 230), nvgRGB(51, 51, 51));
	panel->box.size = box.size;
	addChild(panel);

	widgetToMove = createWidget<CatScrew>(Vec(5, 250));
	addChild(widgetToMove);
	addChild(createWidget<Screw_J>(Vec(16, 2)));
	addChild(createWidget<Screw_J>(Vec(16, 365)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 2)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 365)));

	addParam(createParam<BowlSwitch>(Vec(5, 300), module, Cat::BOWL_PARAM));

	if(module != NULL){
		for(int i=0; i<10; i++){
			hairballs[i] = createWidget<HairballScrew>(Vec(random::uniform()*7, widgetToMove->box.pos.y));
			addChild(hairballs[i]);
		}
	}
}

Model *modelCat = createModel<Cat, CatWidget>("0Cat");