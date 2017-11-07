#include "rack.hpp"

using namespace rack;
extern Plugin *plugin;

struct SimpleClockWidget : ModuleWidget {
	SimpleClockWidget();
};

struct RMSWidget : ModuleWidget {
	RMSWidget();
};

struct XYPadWidget : ModuleWidget {
	XYPadWidget();
};

struct BouncyBallWidget : ModuleWidget {
	BouncyBallWidget();
};

struct QuantizerWidget : ModuleWidget {
	QuantizerWidget();
	enum Notes {
		NOTE_C, 
		NOTE_C_SHARP,
		NOTE_D,
		NOTE_D_SHARP,
		NOTE_E,
		NOTE_F,
		NOTE_F_SHARP,
		NOTE_G,
		NOTE_G_SHARP,
		NOTE_A,
		NOTE_A_SHARP,
		NOTE_B,
		NUM_NOTES
	};
	
	enum ScalesEnum {
		AEOLIAN,
		BLUES,
		CHROMATIC,
		DIATONIC_MINOR,
		DORIAN,
		HARMONIC_MINOR,
		INDIAN,
		LOCRIAN,
		LYDIAN,
		MAJOR,
		MELODIC_MINOR,
		MINOR,
		MIXOLYDIAN,
		NATURAL_MINOR,
		PENTATONIC,
		PHRYGIAN,
		TURKISH,
		NONE,
		NUM_SCALES
	};
};

struct FullScopeWidget : ModuleWidget {
	Panel *panel;
	Widget *rightHandle;
	TransparentWidget *display;
	FullScopeWidget();
	void step();
	json_t *toJson();
	void fromJson(json_t *rootJ);
};

struct GridSeqWidget : ModuleWidget {
	std::vector<ParamWidget*> seqKnobs;
	std::vector<ParamWidget*> gateButtons;
	GridSeqWidget();
	~GridSeqWidget(){ 
		seqKnobs.clear(); 
		gateButtons.clear(); 
	}
	Menu *createContextMenu();
};

struct CenteredLabel : Label {
	void draw(NVGcontext *vg) override {
		nvgTextAlign(vg, NVG_ALIGN_CENTER);
		nvgFillColor(vg, nvgRGB(25, 150, 252));
		nvgFontSize(vg, 12);
		nvgText(vg, box.pos.x, box.pos.y, text.c_str(), NULL);
	}
};

struct SmallWhiteKnob : RoundKnob {
	SmallWhiteKnob() {
		setSVG(SVG::load(assetPlugin(plugin, "res/SmallWhiteKnob.svg")));
	}
	CenteredLabel* linkedLabel = nullptr;
	
	void connectLabel(CenteredLabel* label) {
		linkedLabel = label;
		if (linkedLabel) {
			linkedLabel->text = formatCurrentValue();
		}
	}

	void onChange(EventChange &e) override {
		RoundKnob::onChange(e);
		if (linkedLabel) {
			linkedLabel->text = formatCurrentValue();
		}
	}

	virtual std::string formatCurrentValue() {
		return std::to_string(static_cast<unsigned int>(value));
	}
};

struct TinyBlackKnob : RoundKnob {
	TinyBlackKnob() {
		setSVG(SVG::load(assetGlobal("res/ComponentLibrary/RoundBlack.svg")));
		box.size = Vec(15, 15);
	}
};

struct TinyPJ301MPort : SVGPort {
	TinyPJ301MPort() {
		background->svg = SVG::load(assetPlugin(plugin, "res/TinyPJ301M.svg"));
		background->wrap();
		box.size = background->box.size;
	}
};

struct MyBlueValueLight : ColorLightWidget {
	MyBlueValueLight() {
		addColor(nvgRGB(25, 150, 252));
	}
};

struct NoteKnob : SmallWhiteKnob {
	std::string formatCurrentValue() override {
		switch(int(value)){
			case QuantizerWidget::NOTE_C:       return "C";
			case QuantizerWidget::NOTE_C_SHARP: return "C#";
			case QuantizerWidget::NOTE_D:       return "D";
			case QuantizerWidget::NOTE_D_SHARP: return "D#";
			case QuantizerWidget::NOTE_E:       return "E";
			case QuantizerWidget::NOTE_F:       return "F";
			case QuantizerWidget::NOTE_F_SHARP: return "F#";
			case QuantizerWidget::NOTE_G:       return "G";
			case QuantizerWidget::NOTE_G_SHARP: return "G#";
			case QuantizerWidget::NOTE_A:       return "A";
			case QuantizerWidget::NOTE_A_SHARP: return "A#";
			case QuantizerWidget::NOTE_B:       return "B";
			default: return "";
		}
	}
};

struct ScaleKnob : SmallWhiteKnob {
	std::string formatCurrentValue() override {
		switch(int(value)){
			case QuantizerWidget::AEOLIAN:        return "Aeolian";
			case QuantizerWidget::BLUES:          return "Blues";
			case QuantizerWidget::CHROMATIC:      return "Chromat";
			case QuantizerWidget::DIATONIC_MINOR: return "Diat Min";
			case QuantizerWidget::DORIAN:         return "Dorian";
			case QuantizerWidget::HARMONIC_MINOR: return "Harm Min";
			case QuantizerWidget::INDIAN:         return "Indian";
			case QuantizerWidget::LOCRIAN:        return "Locrian";
			case QuantizerWidget::LYDIAN:         return "Lydian";
			case QuantizerWidget::MAJOR:          return "Major";
			case QuantizerWidget::MELODIC_MINOR:  return "Mel Min";
			case QuantizerWidget::MINOR:          return "Minor";
			case QuantizerWidget::MIXOLYDIAN:     return "Mixolyd";
			case QuantizerWidget::NATURAL_MINOR:  return "Nat Min";
			case QuantizerWidget::PENTATONIC:     return "Penta";
			case QuantizerWidget::PHRYGIAN:       return "Phrygian";
			case QuantizerWidget::TURKISH:        return "Turkish";
			case QuantizerWidget::NONE:           return "None";
			default: return "";
		}
	}
};

struct RightMoveButton : SVGSwitch, MomentarySwitch {
	RightMoveButton() {
		addFrame(SVG::load(assetPlugin(plugin, "res/RightButton.svg")));
		sw->wrap();
		box.size = sw->box.size;
	}
};

struct LeftMoveButton : SVGSwitch, MomentarySwitch {
	LeftMoveButton() {
		addFrame(SVG::load(assetPlugin(plugin, "res/LeftButton.svg")));
		sw->wrap();
		box.size = sw->box.size;
	}
};

struct DownMoveButton : SVGSwitch, MomentarySwitch {
	DownMoveButton() {
		addFrame(SVG::load(assetPlugin(plugin, "res/DownButton.svg")));
		sw->wrap();
		box.size = sw->box.size;
	}
};

struct UpMoveButton : SVGSwitch, MomentarySwitch {
	UpMoveButton() {
		addFrame(SVG::load(assetPlugin(plugin, "res/UpButton.svg")));
		sw->wrap();
		box.size = sw->box.size;
	}
};

struct RndMoveButton : SVGSwitch, MomentarySwitch {
	RndMoveButton() {
		addFrame(SVG::load(assetPlugin(plugin, "res/RndButton.svg")));
		sw->wrap();
		box.size = sw->box.size;
	}
};

struct RepMoveButton : SVGSwitch, MomentarySwitch {
	RepMoveButton() {
		addFrame(SVG::load(assetPlugin(plugin, "res/RepButton.svg")));
		sw->wrap();
		box.size = sw->box.size;
	}
};

struct ModuleResizeHandle : Widget {
	bool right = false;
	float dragX;
	Rect originalBox;
	ModuleResizeHandle() {
		box.size = Vec(RACK_GRID_WIDTH * 1, RACK_GRID_HEIGHT);
	}
	void onMouseDown(EventMouseDown &e) override {
		if (e.button == 0) {
			e.consumed = true;
			e.target = this;
		}
	}
	void onDragStart(EventDragStart &e) override {
		dragX = gRackWidget->lastMousePos.x;
		ModuleWidget *m = getAncestorOfType<ModuleWidget>();
		originalBox = m->box;
	}
	void onDragMove(EventDragMove &e) override {
		ModuleWidget *m = getAncestorOfType<ModuleWidget>();

		float newDragX = gRackWidget->lastMousePos.x;
		float deltaX = newDragX - dragX;

		Rect newBox = originalBox;
		const float minWidth = 12 * RACK_GRID_WIDTH;
		if (right) {
			newBox.size.x += deltaX;
			newBox.size.x = fmaxf(newBox.size.x, minWidth);
			newBox.size.x = roundf(newBox.size.x / RACK_GRID_WIDTH) * RACK_GRID_WIDTH;
		}
		else {
			newBox.size.x -= deltaX;
			newBox.size.x = fmaxf(newBox.size.x, minWidth);
			newBox.size.x = roundf(newBox.size.x / RACK_GRID_WIDTH) * RACK_GRID_WIDTH;
			newBox.pos.x = originalBox.pos.x + originalBox.size.x - newBox.size.x;
		}
		gRackWidget->requestModuleBox(m, newBox);
	}
};



