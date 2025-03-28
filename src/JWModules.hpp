#pragma once
#include "rack.hpp"
#include "QuantizeUtils.cpp"
#define RIGHT_ARROW "▸"

using namespace rack;
extern Plugin *pluginInstance;

static constexpr int blackKeys[20] = 
               { 1,  3,  6,  8, 10, 
				13, 15, 18, 20, 22,
				25, 27, 30, 32, 34,
				37, 39, 42, 44, 46 };

inline bool isBlackKey(int indexFromBottom){
	return std::find(std::begin(blackKeys), std::end(blackKeys), indexFromBottom) != std::end(blackKeys);
}


struct InputsOverrideItem : MenuItem {
	QuantizeUtils *quantizeUtils;
	void onAction(const event::Action &e) override {
		quantizeUtils->inputsOverride = !quantizeUtils->inputsOverride;
	}
	void step() override {
		rightText = quantizeUtils->inputsOverride ? "✔" : "";
		MenuItem::step();
	}
};

////////////////////////////////////////////// PANELS //////////////////////////////////////////////

struct BGPanel : Widget {
	Widget *panelBorder;
	NVGcolor color;
	NVGcolor darkColor;

	BGPanel(NVGcolor _color, NVGcolor _darkColor) {
		panelBorder = new PanelBorder;
		color = _color;
		darkColor = _darkColor;
		addChild(panelBorder);
	}

	void step() override {
		panelBorder->box.size = box.size;
		Widget::step();
	}

	void draw(const DrawArgs &args) override {
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0.0, 0.0, box.size.x, box.size.y);
		nvgFillColor(args.vg, settings::preferDarkPanels ? darkColor : color);
		nvgFill(args.vg);
		Widget::draw(args);
	}
};

struct BlackDisplay : LightWidget {
	BlackDisplay(){}
	void draw(const DrawArgs &args) override {
		nvgFillColor(args.vg, nvgRGB(0, 0, 0));
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
		nvgFill(args.vg);
	}
};

////////////////////////////////////////////// LABELS //////////////////////////////////////////////

struct SmallWhiteKnob;

struct CenteredLabel : Widget {
	int fontSize;
	std::string text = "";
	SmallWhiteKnob *knob = nullptr;
	CenteredLabel(int _fontSize = 12) {
		fontSize = _fontSize;
		box.size.y = 100;
		box.size.x = 100;
	}
	void updateText();
	void draw(const DrawArgs &args) override {
		Widget::draw(args);
		updateText();
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER);
		nvgFillColor(args.vg, nvgRGB(25, 150, 252));
		nvgFontSize(args.vg, fontSize);
		nvgText(args.vg, box.pos.x, box.pos.y, text.c_str(), NULL);
	}
};

////////////////////////////////////////////// KNOBS //////////////////////////////////////////////

struct SmallWhiteKnob : SvgKnob {
	CenteredLabel* linkedLabel = NULL;
	Module* linkedModule = NULL;

	SmallWhiteKnob() {
		minAngle = -0.83 * M_PI;
		maxAngle = 0.83 * M_PI;
		shadow->opacity = 0;
		setSVG(APP->window->loadSvg(asset::plugin(pluginInstance, "res/SmallWhiteKnob.svg")));
	}
	
	void connectLabel(CenteredLabel* label, Module* module) {
		linkedLabel = label;
		linkedModule = module;
		if (linkedModule && linkedLabel) {
			linkedLabel->text = formatCurrentValue();
			linkedLabel->knob = this;
		}
	}

	virtual std::string formatCurrentValue() {
		if(getParamQuantity() != NULL){
			return std::to_string(static_cast<unsigned int>(getParamQuantity()->getDisplayValue()));
		}
		return "";
	}
};

struct NoteKnob : SmallWhiteKnob {
	QuantizeUtils *quantizeUtils;
	NoteKnob(){
		snap = true;
	}
	std::string formatCurrentValue() override {
		if(getParamQuantity() != NULL){
			return quantizeUtils->noteName(int(getParamQuantity()->getDisplayValue()));
		}
		return "";
	}
};

struct ScaleKnob : SmallWhiteKnob {
	QuantizeUtils *quantizeUtils;
	ScaleKnob(){
		snap = true;
	}
	std::string formatCurrentValue() override {
		if(getParamQuantity() != NULL){
			return quantizeUtils->scaleName(int(getParamQuantity()->getDisplayValue()));
		}
		return "";
	}
};

struct JwSmallSnapKnob : SmallWhiteKnob {
	JwSmallSnapKnob() {
		snap = true;
	}
};

struct JwTinyKnob : SvgKnob {
	JwTinyKnob() {
		minAngle = -0.83 * M_PI;
		maxAngle = 0.83 * M_PI;
		setSVG(APP->window->loadSvg(asset::plugin(pluginInstance, "res/TinyWhiteKnob.svg")));
	}
};

struct JwTinyGrayKnob : SvgKnob {
	JwTinyGrayKnob() {
		minAngle = -0.83 * M_PI;
		maxAngle = 0.83 * M_PI;
		setSVG(APP->window->loadSvg(asset::plugin(pluginInstance, "res/TinyWhiteGrayKnob.svg")));
	}
};

struct JwTinyGraySnapKnob : JwTinyGrayKnob {
	JwTinyGraySnapKnob() {
		snap = true;
	}
};

struct BPMPartKnob : JwSmallSnapKnob {	
	BPMPartKnob(){} 
};
inline void CenteredLabel::updateText() {
	if (knob != nullptr && knob->linkedModule) {
		this->text = knob->formatCurrentValue();
	}
}
////////////////////////////////////////////// SWITCHES //////////////////////////////////////////////

struct JwHorizontalSwitch : SVGSwitch {
	JwHorizontalSwitch() {
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Switch_Horizontal_0.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Switch_Horizontal_1.svg")));
	}
};

struct JwVerticalSwitch : SVGSwitch {
	JwVerticalSwitch() {
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Switch_Vertical_0.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Switch_Vertical_1.svg")));
	}
};

struct BowlSwitch : SVGSwitch {
	BowlSwitch() {
		shadow->opacity = 0;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Bowl-no-food.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Bowl-food.svg")));
	}
};

////////////////////////////////////////////// PORTS //////////////////////////////////////////////

struct TinyPJ301MPort : SvgPort {
	TinyPJ301MPort() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/TinyPJ301M.svg")));
	}
};

struct Orange_TinyPJ301MPort : SvgPort {
	Orange_TinyPJ301MPort() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/TinyPJ301M_orange.svg")));
	}
};

struct Yellow_TinyPJ301MPort : SvgPort {
	Yellow_TinyPJ301MPort() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/TinyPJ301M_yellow.svg")));
	}
};

struct Purple_TinyPJ301MPort : SvgPort {
	Purple_TinyPJ301MPort() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/TinyPJ301M_purple.svg")));
	}
};

struct Blue_TinyPJ301MPort : SvgPort {
	Blue_TinyPJ301MPort() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/TinyPJ301M_blue.svg")));
	}
};

struct White_TinyPJ301MPort : SvgPort {
	White_TinyPJ301MPort() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/TinyPJ301M_white.svg")));
	}
};

////////////////////////////////////////////// LIGHTS //////////////////////////////////////////////

struct MyBlueValueLight : ModuleLightWidget {
	MyBlueValueLight() {
		this->bgColor = nvgRGBA(0x55, 0x55, 0x55, 0xff);
		this->addBaseColor(nvgRGB(25, 150, 252));
	}
};

struct MyYellowValueLight : ModuleLightWidget {
	MyYellowValueLight() {
		this->bgColor = nvgRGBA(0x55, 0x55, 0x55, 0xff);
		addBaseColor(nvgRGB(255, 243, 9));
	}
};

struct MyOrangeValueLight : ModuleLightWidget {
	MyOrangeValueLight() {
		this->bgColor = nvgRGBA(0x55, 0x55, 0x55, 0xff);
		addBaseColor(nvgRGB(255, 151, 9));
	}
};

struct MyGreenValueLight : ModuleLightWidget {
	MyGreenValueLight() {
		this->bgColor = nvgRGBA(0x55, 0x55, 0x55, 0xff);
		addBaseColor(nvgRGB(0, 200, 0));
	}
};

struct MyRedValueLight : ModuleLightWidget {
	MyRedValueLight() {
		this->bgColor = nvgRGBA(0x55, 0x55, 0x55, 0xff);
		addBaseColor(nvgRGB(200, 0, 0));
	}
};

////////////////////////////////////////////// BUTTONS //////////////////////////////////////////////

struct RightMoveButton : SVGSwitch {
	RightMoveButton() {
		momentary = true;
		shadow->opacity = 0;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/RightButton.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/RightButtonDown.svg")));
	}
};

struct LeftMoveButton : SVGSwitch {
	LeftMoveButton() {
		momentary = true;
		shadow->opacity = 0;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/LeftButton.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/LeftButtonDown.svg")));
	}
};

struct DownMoveButton : SVGSwitch {
	DownMoveButton() {
		momentary = true;
		shadow->opacity = 0;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DownButton.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DownButtonDown.svg")));
	}
};

struct UpMoveButton : SVGSwitch {
	UpMoveButton() {
		momentary = true;
		shadow->opacity = 0;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/UpButton.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/UpButtonDown.svg")));
	}
};

struct RndMoveButton : SVGSwitch {
	RndMoveButton() {
		momentary = true;
		shadow->opacity = 0;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/RndButton.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/RndButtonDown.svg")));
	}
};

struct RepMoveButton : SVGSwitch {
	RepMoveButton() {
		momentary = true;
		shadow->opacity = 0;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/RepButton.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/RepButtonDown.svg")));
	}
};

struct TinyButton : SVGSwitch {
	TinyButton() {
		momentary = true;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/TinyButtonUp.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/TinyButtonDown.svg")));
	}
};

struct SmallButton : SVGSwitch {
	SmallButton() {
		momentary = true;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/SmallButtonUp.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/SmallButtonDown.svg")));
	}
};

////////////////////////////////////////////// SCREWS //////////////////////////////////////////////

struct Snowflake : SVGWidget {
	Snowflake() {
		setSVG(APP->window->loadSvg(asset::plugin(pluginInstance, "res/SnowFlake.svg")));
	}
};

struct WavHeadLogo : SVGWidget {
	WavHeadLogo() {
		setSVG(APP->window->loadSvg(asset::plugin(pluginInstance, "res/WavHeadSmall.svg")));
	}
};

struct Screw_J : SVGScrew {
	Screw_J() {
		sw->setSVG(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Screw_J.svg")));
		box.size = sw->box.size;
	}
};

struct Screw_W : SVGScrew {
	Screw_W() {
		sw->setSVG(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Screw_W.svg")));
		box.size = sw->box.size;
	}
};

struct CatScrew : SVGWidget {
	CatScrew() {
		setSVG(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Cat.svg")));
	}
};

struct HairballScrew : SVGWidget {
	HairballScrew() {
		setSVG(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Hairball.svg")));
	}
};

////////////////////////////////////////////// WIDGETS //////////////////////////////////////////////

extern Model *modelAdd5;
extern Model *modelAbcdSeq;
extern Model *modelBouncyBalls;
extern Model *modelSimpleClock;
extern Model *modelStr1ker;
extern Model *modelD1v1de;
extern Model *modelPres1t;
extern Model *modelMinMax;
extern Model *modelQuantizer;
extern Model *modelNoteSeq;
extern Model *modelNoteSeqFu;
extern Model *modelNoteSeq16;
extern Model *modelOnePattern;
extern Model *modelPatterns;
extern Model *modelWavHead;
extern Model *modelXYPad;
extern Model *modelFullScope;
extern Model *modelGridSeq;
extern Model *modelEightSeq;
extern Model *modelDivSeq;
extern Model *modelThingThing;
extern Model *modelCat;
extern Model *modelTree;
extern Model *modelTrigs;
extern Model *modelBlankPanel1hp;
extern Model *modelBlankPanelSmall;
extern Model *modelBlankPanelMedium;
extern Model *modelBlankPanelLarge;
extern Model *modelCoolBreeze;
extern Model *modelPete;
extern Model *modelTimer;

inline int clampijw(int x, int minimum, int maximum) {
	return clamp(x, minimum, maximum);
}
inline float clampfjw(float x, float minimum, float maximum) {
	return fminf(fmaxf(x, minimum), maximum);
}
inline float rescalefjw(float x, float xMin, float xMax, float yMin, float yMax) {
	return yMin + (x - xMin) / (xMax - xMin) * (yMax - yMin);
}

