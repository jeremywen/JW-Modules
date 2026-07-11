#include "JWModules.hpp"
#include <cmath>
#include <cstring>

// crawl.js inspired module: random points on a display, crawlers wander and
// trigger gates + pitch CV whenever they enter range of a point.

static constexpr int CRAWL_MAX_POINTS  = 64;
static constexpr int CRAWL_NUM_CRAWLERS = 4;

struct CrawlPoint {
	float x = 0.f;
	float y = 0.f;
};

struct CrawlCrawler {
	float x      = 0.f;
	float y      = 0.f;
	float origX  = 0.f;
	float origY  = 0.f;
	bool  connected[CRAWL_MAX_POINTS] = {};
	dsp::PulseGenerator gatePulse;
	float pitch       = 0.f;  // V/Oct from last entered point
	float xVolt       = 0.f;
	float yVolt       = 0.f;
	float nearestDist = 0.f;  // distance to nearest point (pixels)
	float displayX    = 0.f;  // smoothly interpolated position for rendering
	float displayY    = 0.f;
	float dirX        = 0.f;  // current movement direction (normalized)
	float dirY        = 0.f;
	int   dirCounter  = 0;    // frames until next direction change
};

// Crawler colors: orange, yellow, purple, blue
static const NVGcolor CRAWLER_COLORS[CRAWL_NUM_CRAWLERS] = {
	{{{1.f,  0.59f, 0.04f, 1.f}}},
	{{{1.f,  0.95f, 0.04f, 1.f}}},
	{{{0.56f,0.10f, 0.99f, 1.f}}},
	{{{0.10f,0.59f, 0.99f, 1.f}}}
};

struct Crawl : Module, QuantizeUtils {
	enum ParamIds {
		SPEED_PARAM,
		DISTANCE_PARAM,
		NUM_POINTS_PARAM,
		REGENERATE_PARAM,
		PITCH_RANGE_PARAM,
		CONSTRAIN_DIST_PARAM,
		COHERENCE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CLOCK_INPUT,
		RESET_INPUT,
		SPEED_CV_INPUT,
		DISTANCE_CV_INPUT,
		WANDER_CV_INPUT,
		COHERENCE_CV_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		GATE_OUTPUT,
		V_OCT_OUTPUT,
		X_OUTPUT,
		Y_OUTPUT,
		NEAREST_DIST_OUTPUT,  // poly 4ch — distance from each crawler to its nearest point
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	float displayWidth  = 0.f;
	float displayHeight = 0.f;

	CrawlPoint   points[CRAWL_MAX_POINTS];
	CrawlCrawler crawlers[CRAWL_NUM_CRAWLERS];

	float updateAccum    = 0.f;
	float updateInterval = 256.f;  // samples between physics ticks (~172 Hz at 44100)

	bool needsRegen = true;

	bool pitchQuantizeEnabled = false;
	int  pitchQuantizeRoot    = QuantizeUtils::NOTE_C;
	int  pitchQuantizeScale   = QuantizeUtils::NONE;

	dsp::SchmittTrigger regenTrigger;
	dsp::SchmittTrigger resetTrigger;
	dsp::SchmittTrigger clockTrigger;

	Crawl() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(SPEED_PARAM,         0.5f,  30.f,  3.f,   "Speed");
		configParam(DISTANCE_PARAM,      5.f,   200.f, 80.f,  "Connection distance", " px");
		configParam(NUM_POINTS_PARAM,    4.f,   (float)CRAWL_MAX_POINTS, 16.f, "Number of points");
		paramQuantities[NUM_POINTS_PARAM]->snapEnabled = true;
		configParam(REGENERATE_PARAM,    0.f,   1.f,   0.f,   "Regenerate points & crawlers");
		configParam(PITCH_RANGE_PARAM,   0.f,   5.f,   2.f,   "Pitch range", " V");
		configParam(CONSTRAIN_DIST_PARAM, 0.f,  500.f, 200.f, "Wander radius", " px");
		configParam(COHERENCE_PARAM,      0.f,  1.f,   0.7f, "Movement coherence");

		configInput(CLOCK_INPUT,    "Clock (step on trigger)");
		configInput(RESET_INPUT,    "Reset / Regenerate");
		configInput(SPEED_CV_INPUT,    "Speed CV");
		configInput(DISTANCE_CV_INPUT, "Distance CV");
		configInput(WANDER_CV_INPUT,   "Wander radius CV");
		configInput(COHERENCE_CV_INPUT, "Movement coherence CV");

		configOutput(GATE_OUTPUT,         "Gate (poly 4ch)");
		configOutput(V_OCT_OUTPUT,        "V/Oct (poly 4ch)");
		configOutput(X_OUTPUT,            "X position (poly 4ch)");
		configOutput(Y_OUTPUT,            "Y position (poly 4ch)");
		configOutput(NEAREST_DIST_OUTPUT, "Nearest point distance (poly 4ch, 0–10V)");
	}

	void generate() {
		if (displayWidth <= 0.f || displayHeight <= 0.f) return;
		int n = (int)params[NUM_POINTS_PARAM].getValue();
		n = clampijw(n, 1, CRAWL_MAX_POINTS);

		for (int i = 0; i < n; i++) {
			points[i].x = random::uniform() * displayWidth;
			points[i].y = random::uniform() * displayHeight;
		}
		for (int j = 0; j < CRAWL_NUM_CRAWLERS; j++) {
			crawlers[j].origX = random::uniform() * displayWidth;
			crawlers[j].origY = random::uniform() * displayHeight;
			crawlers[j].x        = crawlers[j].origX;
			crawlers[j].y        = crawlers[j].origY;
			crawlers[j].displayX = crawlers[j].origX;
			crawlers[j].displayY = crawlers[j].origY;
			crawlers[j].dirX = 0.f;
			crawlers[j].dirY = 1.f;  // start moving up
			crawlers[j].dirCounter = 0;
			memset(crawlers[j].connected, 0, sizeof(crawlers[j].connected));
			crawlers[j].pitch = 0.f;
		}
		needsRegen = false;
	}

	void tickCrawlers() {
		float speed        = params[SPEED_PARAM].getValue();
		float distance     = params[DISTANCE_PARAM].getValue();
		float constrainDst = params[CONSTRAIN_DIST_PARAM].getValue();
		float pitchRange   = params[PITCH_RANGE_PARAM].getValue();
		float coherence    = params[COHERENCE_PARAM].getValue();
		int   n            = clampijw((int)params[NUM_POINTS_PARAM].getValue(), 1, CRAWL_MAX_POINTS);

		if (inputs[SPEED_CV_INPUT].isConnected()) {
			speed = clampfjw(speed + inputs[SPEED_CV_INPUT].getVoltage() * 3.f, 0.1f, 60.f);
		}
		if (inputs[DISTANCE_CV_INPUT].isConnected()) {
			distance = clampfjw(distance + inputs[DISTANCE_CV_INPUT].getVoltage() * 20.f, 1.f, 300.f);
		}
		if (inputs[WANDER_CV_INPUT].isConnected()) {
			constrainDst = clampfjw(constrainDst + inputs[WANDER_CV_INPUT].getVoltage() * 50.f, 0.f, 1000.f);
		}
		if (inputs[COHERENCE_CV_INPUT].isConnected()) {
			coherence = clampfjw(coherence + inputs[COHERENCE_CV_INPUT].getVoltage() * 0.1f, 0.f, 1.f);
		}

		for (int j = 0; j < CRAWL_NUM_CRAWLERS; j++) {
			CrawlCrawler &c = crawlers[j];
			c.nearestDist = 1e9f;

			// Drunk walk: maintain a direction for several frames, then change it
			// Low coherence = change direction frequently (more randomness)
			// High coherence = maintain direction longer (directional movement)
			c.dirCounter--;
			if (c.dirCounter <= 0 || random::uniform() > coherence) {
				// Pick new random direction
				float angle = random::uniform() * 2.f * M_PI;
				c.dirX = std::cos(angle);
				c.dirY = std::sin(angle);
				// Duration in ticks until next change (inverse of coherence randomness)
				int maxFrames = 10 + (int)(coherence * 50.f);
				c.dirCounter = random::uniform() * maxFrames + 1;
			}

			// Apply movement in current direction with slight jitter
			float jitter = (1.f - coherence) * 0.5f;
			float jx = c.dirX + (random::uniform() * 2.f - 1.f) * jitter;
			float jy = c.dirY + (random::uniform() * 2.f - 1.f) * jitter;
			float jlen = std::sqrt(jx * jx + jy * jy);
			if (jlen > 0.001f) { jx /= jlen; jy /= jlen; }

			float nx = c.x + jx * speed;
			float ny = c.y + jy * speed;
			c.x = clampfjw(nx, c.origX - constrainDst, c.origX + constrainDst);
			c.y = clampfjw(ny, c.origY - constrainDst, c.origY + constrainDst);
			c.x = clampfjw(c.x, 0.f, displayWidth);
			c.y = clampfjw(c.y, 0.f, displayHeight);

			// Update CV voltages from position
			c.xVolt = rescalefjw(c.x, 0.f, displayWidth,  -5.f,  5.f);
			c.yVolt = rescalefjw(c.y, 0.f, displayHeight,  5.f, -5.f); // invert: top = +5V

			// Check connections — trigger gate on newly entered point
			float bestNewDist = 1e9f;
			int   bestNewIdx  = -1;
			for (int i = 0; i < n; i++) {
				float dx   = points[i].x - c.x;
				float dy   = points[i].y - c.y;
				float d    = std::sqrt(dx * dx + dy * dy);
				bool  wasConn = c.connected[i];
				bool  nowConn = d < distance;
				c.connected[i] = nowConn;
				if (d < c.nearestDist) c.nearestDist = d;
				if (nowConn && !wasConn && d < bestNewDist) {
					bestNewDist = d;
					bestNewIdx  = i;
				}
			}
			if (bestNewIdx >= 0) {
				c.gatePulse.trigger(0.01f);
				// Pitch: top of display = +pitchRange V, bottom = -pitchRange V
				float rawPitch = rescalefjw(points[bestNewIdx].y, 0.f, displayHeight,
				                             pitchRange, -pitchRange);
				c.pitch = pitchQuantizeEnabled
				        ? closestVoltageInScale(rawPitch, pitchQuantizeRoot, pitchQuantizeScale)
				        : rawPitch;
			}
		}
	}

	void process(const ProcessArgs &args) override {
		if (needsRegen) {
			generate();
		}

		if (regenTrigger.process(params[REGENERATE_PARAM].getValue())) {
			generate();
		}
		if (resetTrigger.process(inputs[RESET_INPUT].getVoltage())) {
			generate();
		}

		// Advance crawlers either every updateInterval samples, or on each clock trigger
		bool clockConnected = inputs[CLOCK_INPUT].isConnected();
		if (clockConnected) {
			if (clockTrigger.process(inputs[CLOCK_INPUT].getVoltage())) {
				tickCrawlers();
			}
		} else {
			updateAccum += 1.f;
			if (updateAccum >= updateInterval) {
				updateAccum = 0.f;
				tickCrawlers();
			}
		}

		// Write outputs
		outputs[GATE_OUTPUT].setChannels(CRAWL_NUM_CRAWLERS);
		outputs[V_OCT_OUTPUT].setChannels(CRAWL_NUM_CRAWLERS);
		outputs[X_OUTPUT].setChannels(CRAWL_NUM_CRAWLERS);
		outputs[Y_OUTPUT].setChannels(CRAWL_NUM_CRAWLERS);
		outputs[NEAREST_DIST_OUTPUT].setChannels(CRAWL_NUM_CRAWLERS);

		// Scale nearestDist: 0px → 0V, display diagonal → 10V
		float diag = std::sqrt(displayWidth * displayWidth + displayHeight * displayHeight);
		if (diag < 1.f) diag = 1.f;

		// Lerp display positions toward logical positions (~25ms time constant)
		float lr = args.sampleTime * 40.f;
		for (int j = 0; j < CRAWL_NUM_CRAWLERS; j++) {
			crawlers[j].displayX += (crawlers[j].x - crawlers[j].displayX) * lr;
			crawlers[j].displayY += (crawlers[j].y - crawlers[j].displayY) * lr;
		}

		for (int j = 0; j < CRAWL_NUM_CRAWLERS; j++) {
			outputs[GATE_OUTPUT].setVoltage(
				crawlers[j].gatePulse.process(args.sampleTime) ? 10.f : 0.f, j);
			outputs[V_OCT_OUTPUT].setVoltage(crawlers[j].pitch,  j);
			outputs[X_OUTPUT].setVoltage(crawlers[j].xVolt, j);
			outputs[Y_OUTPUT].setVoltage(crawlers[j].yVolt, j);
			float nd = (crawlers[j].nearestDist >= 1e8f) ? 0.f : crawlers[j].nearestDist;
			outputs[NEAREST_DIST_OUTPUT].setVoltage(clampfjw(nd / diag * 75.f, 0.f, 10.f), j);
		}
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		// Points
		json_t *pointsJ = json_array();
		int n = clampijw((int)params[NUM_POINTS_PARAM].getValue(), 1, CRAWL_MAX_POINTS);
		for (int i = 0; i < n; i++) {
			json_t *pJ = json_object();
			json_object_set_new(pJ, "x", json_real(points[i].x));
			json_object_set_new(pJ, "y", json_real(points[i].y));
			json_array_append_new(pointsJ, pJ);
		}
		json_object_set_new(rootJ, "points", pointsJ);
		// Crawler origins
		json_t *crawlersJ = json_array();
		for (int j = 0; j < CRAWL_NUM_CRAWLERS; j++) {
			json_t *cJ = json_object();
			json_object_set_new(cJ, "origX", json_real(crawlers[j].origX));
			json_object_set_new(cJ, "origY", json_real(crawlers[j].origY));
			json_array_append_new(crawlersJ, cJ);
		}
		json_object_set_new(rootJ, "crawlers", crawlersJ);
		json_object_set_new(rootJ, "pitchQuantizeEnabled", json_boolean(pitchQuantizeEnabled));
		json_object_set_new(rootJ, "pitchQuantizeRoot",    json_integer(pitchQuantizeRoot));
		json_object_set_new(rootJ, "pitchQuantizeScale",   json_integer(pitchQuantizeScale));
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		json_t *pointsJ = json_object_get(rootJ, "points");
		if (pointsJ) {
			int n = (int)json_array_size(pointsJ);
			n = clampijw(n, 0, CRAWL_MAX_POINTS);
			for (int i = 0; i < n; i++) {
				json_t *pJ = json_array_get(pointsJ, i);
				if (!pJ) continue;
				json_t *xJ = json_object_get(pJ, "x");
				json_t *yJ = json_object_get(pJ, "y");
				if (xJ) points[i].x = (float)json_real_value(xJ);
				if (yJ) points[i].y = (float)json_real_value(yJ);
			}
		}
		json_t *crawlersJ = json_object_get(rootJ, "crawlers");
		if (crawlersJ) {
			int n = (int)json_array_size(crawlersJ);
			n = clampijw(n, 0, CRAWL_NUM_CRAWLERS);
			for (int j = 0; j < n; j++) {
				json_t *cJ = json_array_get(crawlersJ, j);
				if (!cJ) continue;
				json_t *oxJ = json_object_get(cJ, "origX");
				json_t *oyJ = json_object_get(cJ, "origY");
				if (oxJ) crawlers[j].origX = (float)json_real_value(oxJ);
				if (oyJ) crawlers[j].origY = (float)json_real_value(oyJ);
				crawlers[j].x = crawlers[j].origX;
				crawlers[j].y = crawlers[j].origY;
			}
		}
		needsRegen = false;
		json_t *pqeJ = json_object_get(rootJ, "pitchQuantizeEnabled");
		if (pqeJ) pitchQuantizeEnabled = json_is_true(pqeJ);
		json_t *pqrJ = json_object_get(rootJ, "pitchQuantizeRoot");
		if (pqrJ) pitchQuantizeRoot = clampijw((int)json_integer_value(pqrJ), 0, QuantizeUtils::NUM_NOTES - 1);
		json_t *pqsJ = json_object_get(rootJ, "pitchQuantizeScale");
		if (pqsJ) pitchQuantizeScale = clampijw((int)json_integer_value(pqsJ), 0, QuantizeUtils::NUM_SCALES - 1);
	}

	void onReset() override {
		needsRegen = true;
	}
};

///////////////////////////////////////////////////////////////////////////////
// DISPLAY
///////////////////////////////////////////////////////////////////////////////

struct CrawlDisplay : LightWidget {
	Crawl *module = nullptr;
	Vec lastButtonPos = Vec(0, 0);

	CrawlDisplay() {}

	void handleDoubleClick(Vec pos) {
		if (!module) return;
		int n = clampijw((int)module->params[Crawl::NUM_POINTS_PARAM].getValue(),
		                  1, CRAWL_MAX_POINTS);

		// Find closest point within hit radius
		static constexpr float HIT_RADIUS = 10.f;
		int   closest     = -1;
		float closestDist = HIT_RADIUS;
		for (int i = 0; i < n; i++) {
			float dx = module->points[i].x - pos.x;
			float dy = module->points[i].y - pos.y;
			float d  = std::sqrt(dx * dx + dy * dy);
			if (d < closestDist) { closestDist = d; closest = i; }
		}

		if (closest >= 0 && n > 1) {
			// Remove: swap with last point, decrement count
			module->points[closest] = module->points[n - 1];
			module->params[Crawl::NUM_POINTS_PARAM].setValue((float)(n - 1));
			// Clear stale connection state
			for (int j = 0; j < CRAWL_NUM_CRAWLERS; j++)
				memset(module->crawlers[j].connected, 0, sizeof(module->crawlers[j].connected));
		} else if (closest < 0 && n < CRAWL_MAX_POINTS) {
			// Add: place new point at click position
			module->points[n].x = pos.x;
			module->points[n].y = pos.y;
			module->params[Crawl::NUM_POINTS_PARAM].setValue((float)(n + 1));
		}
	}

	void onButton(const event::Button &e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			lastButtonPos = e.pos;
			e.consume(this);
		}
		Widget::onButton(e);
	}

	void onDoubleClick(const event::DoubleClick &e) override {
		handleDoubleClick(lastButtonPos);
		e.consume(this);
	}

	void drawLayer(const DrawArgs &args, int layer) override {
		// Black background always
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
		nvgFillColor(args.vg, nvgRGB(0, 0, 0));
		nvgFill(args.vg);

		if (layer != 1) {
			Widget::drawLayer(args, layer);
			return;
		}

		nvgScissor(args.vg, 0, 0, box.size.x, box.size.y);

		if (module == nullptr) {
			// Preview: draw some placeholder dots
			nvgFillColor(args.vg, nvgRGBAf(1.f, 1.f, 1.f, 0.5f));
			for (int i = 0; i < 12; i++) {
				float px = (float)(i * 17 + 10) * (box.size.x / 220.f);
				float py = box.size.y * (0.2f + 0.6f * ((i * 37 + 13) % 100) / 100.f);
				nvgBeginPath(args.vg);
				nvgCircle(args.vg, px, py, 3.f);
				nvgFill(args.vg);
			}
			Widget::drawLayer(args, layer);
			return;
		}

		int n = clampijw((int)module->params[Crawl::NUM_POINTS_PARAM].getValue(), 1, CRAWL_MAX_POINTS);
		float distance = module->params[Crawl::DISTANCE_PARAM].getValue();

		// Draw connection lines (thin, semi-transparent)
		for (int j = 0; j < CRAWL_NUM_CRAWLERS; j++) {
			CrawlCrawler &c = module->crawlers[j];
			NVGcolor col    = CRAWLER_COLORS[j];
			nvgStrokeColor(args.vg, nvgRGBAf(col.r, col.g, col.b, 0.25f));
			nvgStrokeWidth(args.vg, 1.0f);
			for (int i = 0; i < n; i++) {
				if (c.connected[i]) {
					nvgBeginPath(args.vg);
					nvgMoveTo(args.vg, c.displayX, c.displayY);
					nvgLineTo(args.vg, module->points[i].x, module->points[i].y);
					nvgStroke(args.vg);
				}
			}
		}

		// Draw distance rings around crawlers (subtle)
		for (int j = 0; j < CRAWL_NUM_CRAWLERS; j++) {
			CrawlCrawler &c = module->crawlers[j];
			NVGcolor col = CRAWLER_COLORS[j];
			nvgStrokeColor(args.vg, nvgRGBAf(col.r, col.g, col.b, 0.08f));
			nvgStrokeWidth(args.vg, 1.0f);
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, c.displayX, c.displayY, distance);
			nvgStroke(args.vg);
		}

		// Draw fixed points
		for (int i = 0; i < n; i++) {
			// Check if any crawler is connected to this point
			bool anyConn = false;
			for (int j = 0; j < CRAWL_NUM_CRAWLERS; j++) {
				if (module->crawlers[j].connected[i]) { anyConn = true; break; }
			}
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, module->points[i].x, module->points[i].y, 3.f);
			if (anyConn) {
				nvgFillColor(args.vg, nvgRGBf(1.f, 1.f, 1.f));
			} else {
				nvgFillColor(args.vg, nvgRGBAf(1.f, 1.f, 1.f, 0.45f));
			}
			nvgFill(args.vg);
		}

		// Draw crawler origins (small cross) — fixed, no interpolation needed
		for (int j = 0; j < CRAWL_NUM_CRAWLERS; j++) {
			CrawlCrawler &c = module->crawlers[j];
			NVGcolor col = CRAWLER_COLORS[j];
			nvgStrokeColor(args.vg, nvgRGBAf(col.r, col.g, col.b, 0.35f));
			nvgStrokeWidth(args.vg, 1.0f);
			float ox = c.origX, oy = c.origY;
			float cs = 4.f;
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, ox - cs, oy); nvgLineTo(args.vg, ox + cs, oy);
			nvgStroke(args.vg);
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, ox, oy - cs); nvgLineTo(args.vg, ox, oy + cs);
			nvgStroke(args.vg);
		}

		// Draw crawlers (filled colored circles) — use interpolated display position
		for (int j = 0; j < CRAWL_NUM_CRAWLERS; j++) {
			CrawlCrawler &c = module->crawlers[j];
			NVGcolor col = CRAWLER_COLORS[j];
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, c.displayX, c.displayY, 6.f);
			nvgFillColor(args.vg, col);
			nvgFill(args.vg);
			nvgStrokeColor(args.vg, nvgRGBf(1.f, 1.f, 1.f));
			nvgStrokeWidth(args.vg, 1.0f);
			nvgStroke(args.vg);
		}

		Widget::drawLayer(args, layer);
	}
};

///////////////////////////////////////////////////////////////////////////////
// WIDGET
///////////////////////////////////////////////////////////////////////////////

struct CrawlWidget : ModuleWidget {
	BGPanel *panel = nullptr;

	CrawlWidget(Crawl *module) {
		setModule(module);
		box.size = Vec(RACK_GRID_WIDTH * 20, RACK_GRID_HEIGHT);

#ifdef METAMODULE
		setPanel(createPanel(
			asset::plugin(pluginInstance, "res/Crawl.svg"),
			asset::plugin(pluginInstance, "res/Crawl.svg")
		));
#else
		panel = new BGPanel(nvgRGB(10, 10, 18), nvgRGB(6, 6, 12));
		panel->box.size = box.size;
		addChild(panel);
#endif

		// ---- Display (top area, full width) ----
		CrawlDisplay *display = new CrawlDisplay();
		display->module   = module;
		display->box.pos  = Vec(2.f, 14.f);
		display->box.size = Vec(box.size.x - 4.f, 268.f);
		addChild(display);

		if (module != nullptr) {
			module->displayWidth  = display->box.size.x;
			module->displayHeight = display->box.size.y;
			module->generate();
		}

		// ---- Screws ----
		addChild(createWidget<Screw_J>(Vec(2,   2)));
		addChild(createWidget<Screw_W>(Vec(box.size.x - 14, 2)));
		addChild(createWidget<Screw_J>(Vec(2,   365)));
		addChild(createWidget<Screw_W>(Vec(box.size.x - 14, 365)));

		// ---- Knobs (single row below display) ----
		// 6 knobs + 1 button evenly across the panel
		float ky   = 292.f;
		float kx   = 6.f;
		float kStep = 44.f;
		addParam(createParam<SmallWhiteKnob>(Vec(kx + 0 * kStep, ky), module, Crawl::SPEED_PARAM));
		addParam(createParam<SmallWhiteKnob>(Vec(kx + 1 * kStep, ky), module, Crawl::DISTANCE_PARAM));
		addParam(createParam<SmallWhiteKnob>(Vec(kx + 2 * kStep, ky), module, Crawl::NUM_POINTS_PARAM));
		addParam(createParam<SmallWhiteKnob>(Vec(kx + 3 * kStep, ky), module, Crawl::PITCH_RANGE_PARAM));
		addParam(createParam<SmallWhiteKnob>(Vec(kx + 4 * kStep, ky), module, Crawl::CONSTRAIN_DIST_PARAM));
		addParam(createParam<SmallWhiteKnob>(Vec(kx + 5 * kStep, ky), module, Crawl::COHERENCE_PARAM));
		addParam(createParam<TinyButton>(Vec(kx + 6 * kStep + 2.f, ky + 8.f), module, Crawl::REGENERATE_PARAM));

		// ---- Inputs then Outputs (single port row) ----
		float py  = 344.f;
		float px  = 6.f;
		float pStep = 22.f;
		// Inputs
		addInput(createInput<TinyPJ301MPort>(Vec(px + 0 * pStep, py), module, Crawl::CLOCK_INPUT));
		addInput(createInput<TinyPJ301MPort>(Vec(px + 1 * pStep, py), module, Crawl::RESET_INPUT));
		addInput(createInput<TinyPJ301MPort>(Vec(px + 2 * pStep, py), module, Crawl::SPEED_CV_INPUT));
		addInput(createInput<TinyPJ301MPort>(Vec(px + 3 * pStep, py), module, Crawl::DISTANCE_CV_INPUT));
		addInput(createInput<TinyPJ301MPort>(Vec(px + 4 * pStep, py), module, Crawl::WANDER_CV_INPUT));
		addInput(createInput<TinyPJ301MPort>(Vec(px + 5 * pStep, py), module, Crawl::COHERENCE_CV_INPUT));
		// Outputs (with a small gap after inputs)
		float ox = px + 6 * pStep + 16.f;
		addOutput(createOutput<TinyPJ301MPort>(Vec(ox + 0 * pStep, py), module, Crawl::GATE_OUTPUT));
		addOutput(createOutput<TinyPJ301MPort>(Vec(ox + 1 * pStep, py), module, Crawl::V_OCT_OUTPUT));
		addOutput(createOutput<TinyPJ301MPort>(Vec(ox + 2 * pStep, py), module, Crawl::X_OUTPUT));
		addOutput(createOutput<TinyPJ301MPort>(Vec(ox + 3 * pStep, py), module, Crawl::Y_OUTPUT));
		addOutput(createOutput<TinyPJ301MPort>(Vec(ox + 4 * pStep, py), module, Crawl::NEAREST_DIST_OUTPUT));
	}

	struct RegenItem : MenuItem {
		Crawl *module;
		void onAction(const event::Action &e) override { module->needsRegen = true; }
	};

	struct QuantizeEnabledItem : MenuItem {
		Crawl *module;
		void onAction(const event::Action &e) override {
			if (module) module->pitchQuantizeEnabled = !module->pitchQuantizeEnabled;
		}
		void step() override {
			rightText = CHECKMARK(module && module->pitchQuantizeEnabled);
			MenuItem::step();
		}
	};

	struct RootNoteValueItem : MenuItem {
		Crawl *module; int note;
		void onAction(const event::Action &e) override {
			if (module) module->pitchQuantizeRoot = note;
		}
		void step() override {
			rightText = CHECKMARK(module && module->pitchQuantizeRoot == note);
			MenuItem::step();
		}
	};
	struct RootNoteItem : MenuItem {
		Crawl *module;
		Menu *createChildMenu() override {
			Menu *m = new Menu;
			if (!module) return m;
			for (int i = 0; i < QuantizeUtils::NUM_NOTES; i++) {
				RootNoteValueItem *item = new RootNoteValueItem;
				item->text = module->noteName(i);
				item->module = module; item->note = i;
				m->addChild(item);
			}
			return m;
		}
	};

	struct ScaleValueItem : MenuItem {
		Crawl *module; int scale;
		void onAction(const event::Action &e) override {
			if (module) module->pitchQuantizeScale = scale;
		}
		void step() override {
			rightText = CHECKMARK(module && module->pitchQuantizeScale == scale);
			MenuItem::step();
		}
	};
	struct ScaleItem : MenuItem {
		Crawl *module;
		Menu *createChildMenu() override {
			Menu *m = new Menu;
			if (!module) return m;
			for (int i = 0; i < QuantizeUtils::NUM_SCALES; i++) {
				ScaleValueItem *item = new ScaleValueItem;
				item->text = module->scaleName(i);
				item->module = module; item->scale = i;
				m->addChild(item);
			}
			return m;
		}
	};

	void appendContextMenu(Menu *menu) override {
		Crawl *m = dynamic_cast<Crawl *>(module);
		if (!m) return;
		menu->addChild(new MenuSeparator());

		RegenItem *regenItem = createMenuItem<RegenItem>("Regenerate points & crawlers");
		regenItem->module = m;
		menu->addChild(regenItem);

		menu->addChild(new MenuSeparator());

		QuantizeEnabledItem *qe = new QuantizeEnabledItem;
		qe->text = "Enable pitch quantization"; qe->module = m;
		menu->addChild(qe);

		RootNoteItem *rootItem = new RootNoteItem;
		rootItem->text = "Root note";
		rootItem->rightText = m->noteName(m->pitchQuantizeRoot) + " " + RIGHT_ARROW;
		rootItem->module = m;
		menu->addChild(rootItem);

		ScaleItem *scaleItem = new ScaleItem;
		scaleItem->text = "Scale";
		scaleItem->rightText = m->scaleName(m->pitchQuantizeScale) + " " + RIGHT_ARROW;
		scaleItem->module = m;
		menu->addChild(scaleItem);
	}
};

Model *modelCrawl = createModel<Crawl, CrawlWidget>("Crawl");
