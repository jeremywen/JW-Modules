#include <string.h>
#include "JWModules.hpp"


#define BUFFER_SIZE 512

struct MinMax : Module {
	enum ParamIds {
		TIME_PARAM,
		TRIG_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		X_INPUT,
		Y_INPUT,
		TRIG_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		NUM_OUTPUTS
	};

	float bufferX[BUFFER_SIZE] = {};
	float bufferY[BUFFER_SIZE] = {};
	int bufferIndex = 0;
	float frameIndex = 0;

	dsp::SchmittTrigger sumTrigger;
	dsp::SchmittTrigger extTrigger;
	bool lissajous = false;
	dsp::SchmittTrigger resetTrigger;

	MinMax() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS) {
		configParam(TIME_PARAM, -6.0, -16.0, -14.0);		
	}
	void process(const ProcessArgs &args) override;

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		json_object_set_new(rootJ, "lissajous", json_integer((int) lissajous));
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		json_t *sumJ = json_object_get(rootJ, "lissajous");
		if (sumJ)
			lissajous = json_integer_value(sumJ);

	}

	void onReset() override {
		lissajous = false;
	}
};


void MinMax::process(const ProcessArgs &args) {
	// Compute time
	float deltaTime = powf(2.0, params[TIME_PARAM].getValue());
	int frameCount = (int)ceilf(deltaTime * args.sampleRate);

	// Add frame to buffer
	if (bufferIndex < BUFFER_SIZE) {
		if (++frameIndex > frameCount) {
			frameIndex = 0;
			bufferX[bufferIndex] = inputs[X_INPUT].getVoltage();
			bufferY[bufferIndex] = inputs[Y_INPUT].getVoltage();
			bufferIndex++;
		}
	}

	// Are we waiting on the next trigger?
	if (bufferIndex >= BUFFER_SIZE) {
		// Trigger immediately if external but nothing plugged in, or in Lissajous mode
		if (lissajous) {
			bufferIndex = 0;
			frameIndex = 0;
			return;
		}

		// Reset the Schmitt trigger so we don't trigger immediately if the input is high
		if (frameIndex == 0) {
			resetTrigger.reset();
		}
		frameIndex++;

		// Must go below 0.1V to trigger
		// resetTrigger.setThresholds(params[TRIG_PARAM].getValue() - 0.1, params[TRIG_PARAM].getValue());
		float gate = inputs[X_INPUT].getVoltage();

		// Reset if triggered
		float holdTime = 0.1;
		if (resetTrigger.process(gate) || (frameIndex >= args.sampleRate * holdTime)) {
			bufferIndex = 0; frameIndex = 0; return;
		}

		// Reset if we've waited too long
		if (frameIndex >= args.sampleRate * holdTime) {
			bufferIndex = 0; frameIndex = 0; return;
		}
	}
}


struct MinMaxDisplay : TransparentWidget {
	MinMax *module;
	int frame = 0;
	std::shared_ptr<Font> font;

	struct Stats {
		float vrms, vpp, vmin = 0, vmax = 0;
		void calculate(float *values) {
			vrms = 0.0;
			vmax = -INFINITY;
			vmin = INFINITY;
			for (int i = 0; i < BUFFER_SIZE; i++) {
				float v = values[i];
				vrms += v*v;
				vmax = fmaxf(vmax, v);
				vmin = fminf(vmin, v);
			}
			vrms = sqrtf(vrms / BUFFER_SIZE);
			vpp = vmax - vmin;
		}
	};
	Stats statsX, statsY;

	MinMaxDisplay() {
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/DejaVuSansMono.ttf"));
	}

	void drawStats(const DrawArgs &args, Vec pos, const char *title, Stats *stats) {
		nvgFontSize(args.vg, 24);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -2);

		nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0xff, 0x80));
		char text[128];
		snprintf(text, sizeof(text), "%5.2f", stats->vmin);
		nvgText(args.vg, pos.x + 10, pos.y + 28, text, NULL);
		snprintf(text, sizeof(text), "%5.2f", stats->vmax);
		nvgText(args.vg, pos.x + 10, pos.y + 78, text, NULL);
	}

	void draw(const DrawArgs &args) override {
		if(module == NULL) return;
		// Calculate and draw stats
		if (++frame >= 4) {
			frame = 0;
			statsX.calculate(module->bufferX);
			statsY.calculate(module->bufferY);
		}
		drawStats(args, Vec(0, 20), "X", &statsX);
	}
};


struct MinMaxWidget : ModuleWidget { 
	MinMaxWidget(MinMax *module); 
};

MinMaxWidget::MinMaxWidget(MinMax *module) {
		setModule(module);
	box.size = Vec(RACK_GRID_WIDTH*6, RACK_GRID_HEIGHT);

	{
		SVGPanel *panel = new SVGPanel();
		panel->box.size = box.size;
		panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/MinMax.svg")));
		addChild(panel);
	}

	addChild(createWidget<Screw_J>(Vec(16, 2)));
	addChild(createWidget<Screw_J>(Vec(16, 365)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 2)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 365)));

	CenteredLabel* const titleLabel = new CenteredLabel(16);
	titleLabel->box.pos = Vec(22, 15);
	titleLabel->text = "MinMax";
	addChild(titleLabel);

	{
		MinMaxDisplay *display = new MinMaxDisplay();
		display->module = module;
		display->box.pos = Vec(0, 44);
		display->box.size = Vec(box.size.x, 140);
		addChild(display);
	}

	CenteredLabel* const minLabel = new CenteredLabel(12);
	minLabel->box.pos = Vec(22, 35);
	minLabel->text = "Min";
	addChild(minLabel);

	CenteredLabel* const maxLabel = new CenteredLabel(12);
	maxLabel->box.pos = Vec(22, 60);
	maxLabel->text = "Max";
	addChild(maxLabel);

	CenteredLabel* const timeLabel = new CenteredLabel(12);
	timeLabel->box.pos = Vec(22, 101);
	timeLabel->text = "Time";
	addChild(timeLabel);

	CenteredLabel* const inLabel = new CenteredLabel(12);
	inLabel->box.pos = Vec(23, 132);
	inLabel->text = "Input";
	addChild(inLabel);

	addParam(createParam<SmallWhiteKnob>(Vec(32, 209), module, MinMax::TIME_PARAM));
	addInput(createInput<PJ301MPort>(Vec(33, 275), module, MinMax::X_INPUT));
}

Model *modelMinMax = createModel<MinMax, MinMaxWidget>("MinMax");
