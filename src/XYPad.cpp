#include <string.h>
#include "JWModules.hpp"

struct XYPad : Module {
	enum ParamIds {
		X_POS_PARAM,
		Y_POS_PARAM,
		GATE_PARAM,
		OFFSET_X_VOLTS_PARAM,
		OFFSET_Y_VOLTS_PARAM,
		SCALE_X_PARAM,
		SCALE_Y_PARAM,
		AUTO_PLAY_PARAM,
		PLAY_SPEED_PARAM,
		SPEED_MULT_PARAM,
		RND_SHAPES_PARAM,
		RND_VARIATION_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		PLAY_GATE_INPUT,
		PLAY_SPEED_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		X_OUTPUT,
		Y_OUTPUT,
		X_INV_OUTPUT,
		Y_INV_OUTPUT,
		GATE_OUTPUT,
		NUM_OUTPUTS
	};
	
	enum State {
		STATE_IDLE,
		STATE_RECORDING,
		STATE_AUTO_PLAYING,
		STATE_GATE_PLAYING
	};
	
	enum LightIds {
		AUTO_LIGHT,
		NUM_LIGHTS
	};

	enum Shapes {
		RND_SINE,
		RND_SQUARE,
		RND_RAMP,
		RND_LINE,
		RND_NOISE,
		RND_SINE_MOD,
		RND_SPIRAL,
		RND_STEPS,
		NUM_SHAPES
	};

	enum PlayModes {
		FWD_LOOP,
		BWD_LOOP,
		FWD_ONE_SHOT,
		BWD_ONE_SHOT,
		FWD_BWD_LOOP,
		BWD_FWD_LOOP,
		NUM_PLAY_MODES
	};

	float minX = 0, minY = 0, maxX = 0, maxY = 0;
	float displayWidth = 0, displayHeight = 0;
	float ballRadius = 10;
	float ballStrokeWidth = 2;
	float minVolt = -5, maxVolt = 5;
	float recordPhase = 0.0;
	float playbackPhase = 0.0;
	bool autoPlayOn = false;
	bool playingFwd = true;
	int state = STATE_IDLE;
	int curPlayMode = FWD_LOOP;
	int lastRandomShape = RND_STEPS;
	dsp::SchmittTrigger autoBtnTrigger;
	std::vector<Vec> points;
	long curPointIdx = 0;

	XYPad() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(RND_SHAPES_PARAM, 0.0, 1.0, 0.0);
		configParam(RND_VARIATION_PARAM, 0.0, 1.0, 0.0);
		configParam(SCALE_X_PARAM, 0.01, 1.0, 0.5);
		configParam(SCALE_Y_PARAM, 0.01, 1.0, 0.5);
		configParam(OFFSET_X_VOLTS_PARAM, -5.0, 5.0, 5.0);
		configParam(OFFSET_Y_VOLTS_PARAM, -5.0, 5.0, 5.0);
		configParam(AUTO_PLAY_PARAM, 0.0, 1.0, 0.0);
		configParam(PLAY_SPEED_PARAM, 0.0, 10.0, 1.0);
		configParam(SPEED_MULT_PARAM, 1.0, 100.0, 1.0);
	}

	void process(const ProcessArgs &args) override;

	void onReset() override {
	    setState(STATE_IDLE);
	    points.clear();
	    defaultPos();
	}

	void onRandomize() override {
		randomizeShape();
	}

	void randomizeShape(){
		makeShape((lastRandomShape+1)%NUM_SHAPES);
	}

	void makeShape(int shape){
		lastRandomShape = shape;
		int stateBefore = state;
	    setState(STATE_IDLE);
	    points.clear();
	    switch(shape){
			case RND_SINE: {
				    float twoPi = 2.0*M_PI;
				    float cycles = 1 + int(random::uniform() * 13);
				    bool inside = true;
				    for(float i=0; i<twoPi * cycles; i+=M_PI/displayWidth*cycles){
				    	float x = rescalefjw(i, 0, twoPi*cycles, minX, maxX);
				    	float y = rescalefjw(sin(i), -1, 1, minY, maxY);
						inside = isInView(x, y);
				        if(inside)addPoint(x, y);
			    	}
			    }
				break;
			case RND_SQUARE: {
				    float twoPi = 2.0*M_PI;
				    float cycles = 1 + int(random::uniform() * 13);
				    bool inside = true;
				    for(float i=0; i<twoPi * cycles; i+=M_PI/displayWidth*cycles){
				    	float x = rescalefjw(i, 0, twoPi*cycles, minX, maxX);
				    	float y = rescalefjw(sin(i)<0, 0, 1, minY, maxY);
						inside = isInView(x, y);
				        if(inside)addPoint(x, y);
			    	}
			    }
				break;
			case RND_RAMP: {
				    float lastY = maxY;
				    float rate = random::uniform();
				    bool inside = true;
				    for(int i=0; i<5000 && inside; i+=2){
				    	float x = minX + i;
				    	lastY -= powf(2, powf(x*0.005, 2)) * rate;
				    	float y = lastY;
						inside = isInView(x, y);
				        if(inside)addPoint(x, y);
			    	}
			    }
				break;
			case RND_LINE: {
				    float startHeight = (random::uniform() * maxY * 0.5) + (maxY * 0.25);
				    float rate = random::uniform() - 0.5;
				    bool inside = true;
				    for(int i=0; i<5000 && inside; i+=2){
				    	float x = minX + i;
				    	float y = startHeight + rate * x;
						inside = isInView(x, y);
				        if(inside)addPoint(x, y);
			    	}
			    }
				break;
			case RND_NOISE: {
				    float midHeight = maxY / 2.0;
				    float amp = midHeight * 0.9;
				    bool inside = true;
				    for(int i=0; i<5000 && inside; i+=2){
				    	float x = minX + i;
				    	float y = (random::uniform()*2-1) * amp + midHeight;
						inside = isInView(x, y);
				        if(inside)addPoint(x, y);
			    	}
			    }
				break;
			case RND_SINE_MOD: {
				    float midHeight = maxY / 2.0;
				    float amp = midHeight * 0.90 * 0.50;
				    float rate = random::uniform() * 0.1;
				    float rateAdder = random::uniform() * 0.001;
				    float ampAdder = random::uniform() * 0.25;
				    bool inside = true;
				    for(int i=0; i<5000 && inside; i+=2){
				    	float x = minX + i;
				    	float y = sin(i*rate) * amp + (maxY / 2.0);
						inside = isInView(x, y);
				        if(inside)addPoint(x, y);
				        rate+=rateAdder;
				        amp+=ampAdder;
			    	}
			    }
				break;
			case RND_SPIRAL: {
				    float curX = maxX / 2.0;
				    float curY = maxY / 2.0;
				    float radius = 5;
				    float rate = 1 + (random::uniform()*0.1);
				    bool inside = true;
				    for(int i=0; i<5000 && inside; i+=2){
				    	float x = curX + sin(i/10.0) * radius;
				    	float y = curY + cos(i/10.0) * radius;
						inside = isInView(x, y);
				        if(inside)addPoint(x, y);
				        radius*=rate;
				    }
				}
			    break;
			case RND_STEPS: {
				    float x = maxX * 0.5;
				    float y = maxY * 0.5;
				    enum stateEnum { ST_RIGHT, ST_LEFT, ST_UP, ST_DOWN };
				    int squSt = ST_RIGHT;
				    int stepsBeforeStateChange = 5 * int(random::uniform()*5+1);
				    bool inside = true;
				    for(int i=0; i<5000 && inside; i+=2){
				    	if(squSt == ST_RIGHT && x < maxX){
				    		x++;
				    	} else if(squSt == ST_LEFT && x > minX){
				    		x--;
				    	} else if(squSt == ST_UP && y > minY){
				    		y--;
				    	} else if(squSt == ST_DOWN && y < maxY){
				    		y++;
				    	}
				    	if(i % stepsBeforeStateChange == 0){
				    		squSt = int(random::uniform() * 4);
				    	}
						inside = isInView(x, y);
				        if(inside)addPoint(x, y);
			    	}
			    }
				break;
	    }
		setCurrentPos(points[0].x, points[0].y);
		setState(stateBefore);
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		json_object_set_new(rootJ, "lastRandomShape", json_integer(lastRandomShape));
		json_object_set_new(rootJ, "curPlayMode", json_integer(curPlayMode));
		json_object_set_new(rootJ, "autoPlayOn", json_boolean(autoPlayOn));
		json_object_set_new(rootJ, "xPos", json_real(params[X_POS_PARAM].getValue()));
		json_object_set_new(rootJ, "yPos", json_real(params[Y_POS_PARAM].getValue()));

		json_t *pointsArr = json_array();
		for(Vec pt : points){
			json_t *posArr = json_array();
			json_array_append(posArr, json_real(pt.x));
			json_array_append(posArr, json_real(pt.y));
			json_array_append(pointsArr, posArr);
		}
		json_object_set_new(rootJ, "points", pointsArr);
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		lastRandomShape = json_integer_value(json_object_get(rootJ, "lastRandomShape"));
		curPlayMode = json_integer_value(json_object_get(rootJ, "curPlayMode"));

		json_t *xPosJ = json_object_get(rootJ, "xPos");
		json_t *yPosJ = json_object_get(rootJ, "yPos");
		setCurrentPos(json_real_value(xPosJ), json_real_value(yPosJ));

		json_t *array = json_object_get(rootJ, "points");
		if(array){
			size_t index;
			json_t *value;
			json_array_foreach(array, index, value) {
				float x = json_real_value(json_array_get(value, 0));
				float y = json_real_value(json_array_get(value, 1));
				addPoint(x, y);
			}
		}

		json_t *autoPlayOnJ = json_object_get(rootJ, "autoPlayOn");
		if (autoPlayOnJ){
			autoPlayOn = json_is_true(autoPlayOnJ);
		}
		lights[AUTO_LIGHT].value = autoPlayOn ? 1.0 : 0.0;
		params[AUTO_PLAY_PARAM].setValue(autoPlayOn ? 1 : 0);
		if(autoPlayOn){setState(STATE_AUTO_PLAYING);}
	}

	void defaultPos() {
		params[XYPad::X_POS_PARAM].setValue(displayWidth / 2.0);
		params[XYPad::Y_POS_PARAM].setValue(displayHeight / 2.0);		
	}

	void setMouseDown(const Vec &pos, bool down){
		if(down){
			setCurrentPos(pos.x, pos.y);
			setState(STATE_RECORDING);
		} else {
			if(autoPlayOn && !inputs[PLAY_GATE_INPUT].isConnected()){ //no auto play if wire connected to play in
				setState(STATE_AUTO_PLAYING);
			} else {
				setState(STATE_IDLE);
			}
		}
	}

	void setCurrentPos(float x, float y){
		params[X_POS_PARAM].setValue(clampfjw(x, minX, maxX));
		params[Y_POS_PARAM].setValue(clampfjw(y, minY, maxY));
	}

	bool isInView(float x, float y){
		return x >= minX && x <= maxX && y >= minY && y <= maxY;
	}

	void addPoint(float x, float y){
		points.push_back(Vec(x, y));
	}

	void updateMinMax(){
		float distToMid = ballRadius + ballStrokeWidth;
		minX = distToMid;
		minY = distToMid;
		maxX = displayWidth - distToMid;
		maxY = displayHeight - distToMid;
	}

	bool isStatePlaying() {
		return state == STATE_GATE_PLAYING || state == STATE_AUTO_PLAYING;
	}

	void playback(){
		if(isStatePlaying() && points.size() > 0){
			params[X_POS_PARAM].setValue(points[curPointIdx].x);
			params[Y_POS_PARAM].setValue(points[curPointIdx].y);

			if(curPlayMode == FWD_LOOP || curPlayMode == FWD_ONE_SHOT){
				playingFwd = true;
			} else if(curPlayMode == BWD_LOOP || curPlayMode == BWD_ONE_SHOT){
				playingFwd = false;
			}

			curPointIdx += playingFwd ? 1 : -1;
			if(curPointIdx >= 0 && curPointIdx < long(points.size())){
				params[GATE_PARAM].setValue(true); //keep gate on
			} else {
				params[GATE_PARAM].setValue(false);

				if(curPlayMode == FWD_LOOP){
					curPointIdx = 0;
				} else if(curPlayMode == BWD_LOOP){
					curPointIdx = points.size() - 1;
				} else if(curPlayMode == FWD_ONE_SHOT || curPlayMode == BWD_ONE_SHOT){
					setState(STATE_IDLE);//done playing
					curPointIdx = playingFwd ? points.size() - 1 : 0;
				} else if(curPlayMode == FWD_BWD_LOOP || curPlayMode == BWD_FWD_LOOP){
					playingFwd = !playingFwd; //go the other way now
					curPointIdx = playingFwd ? 0 : points.size() - 1;
				}
			}
		}
	}

	void setState(int newState){
		switch(newState){		
			case STATE_IDLE:
				curPointIdx = 0;
				params[GATE_PARAM].setValue(false);		
				break;
			case STATE_RECORDING:
				points.clear();
				curPointIdx = 0;
				params[GATE_PARAM].setValue(true);
				break;
			case STATE_AUTO_PLAYING:
				params[GATE_PARAM].setValue(true);
				break;
			case STATE_GATE_PLAYING:
				params[GATE_PARAM].setValue(true);
				break;
		}
		if(isStatePlaying()){
			if(curPlayMode == FWD_LOOP || curPlayMode == FWD_ONE_SHOT){
				curPointIdx = 0;
			} else if(curPlayMode == BWD_LOOP || curPlayMode == BWD_ONE_SHOT){
				curPointIdx = points.size() - 1;
			}
		}
		state = newState;
	}

};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void XYPad::process(const ProcessArgs &args) {
	if (autoBtnTrigger.process(params[AUTO_PLAY_PARAM].getValue())) {
		autoPlayOn = !autoPlayOn;
		if(autoPlayOn){ 
			if(!isStatePlaying()){
				setState(STATE_AUTO_PLAYING);
			}
		} else {
			//stop when auto turned off
			setState(STATE_IDLE);
		}
	}
	lights[AUTO_LIGHT].value = autoPlayOn ? 1.0 : 0.0;

	if (inputs[PLAY_GATE_INPUT].isConnected()) {
		params[AUTO_PLAY_PARAM].setValue(0); //disable autoplay if wire connected to play gate
		autoPlayOn = false; //disable autoplay if wire connected to play gate

		if (inputs[PLAY_GATE_INPUT].getVoltage() >= 1.0) {
			if(!isStatePlaying() && state != STATE_RECORDING){
				setState(STATE_GATE_PLAYING);
			}
		} else {
			if(isStatePlaying()){
				setState(STATE_IDLE);
			}
		}
	} else if(state == STATE_GATE_PLAYING){//wire removed while playing
		setState(STATE_IDLE);
	}

	if(state == STATE_RECORDING){
		float recordClockTime = 50;
		recordPhase += recordClockTime / args.sampleRate;
		if (recordPhase >= 1.0) {
			recordPhase -= 1.0;
			addPoint(params[X_POS_PARAM].getValue(), params[Y_POS_PARAM].getValue());
		}

	} else if(isStatePlaying()){
		float playSpeedTotal = clampfjw(inputs[PLAY_SPEED_INPUT].getVoltage() + params[PLAY_SPEED_PARAM].getValue(), 0, 20);
		float playbackClockTime = rescalefjw(playSpeedTotal, 0, 20, 1, 500 * params[SPEED_MULT_PARAM].getValue());
		playbackPhase += playbackClockTime / args.sampleRate;
		if (playbackPhase >= 1.0) {
			playbackPhase -= 1.0;
			playback();
		}
	}

	float xOut = rescalefjw(params[X_POS_PARAM].getValue(), minX, maxX, minVolt, maxVolt);
	float yOut = rescalefjw(params[Y_POS_PARAM].getValue(), minY, maxY, maxVolt, minVolt); //y is inverted because gui coords
	outputs[X_OUTPUT].setVoltage((xOut + params[OFFSET_X_VOLTS_PARAM].getValue()) * params[SCALE_X_PARAM].getValue());
	outputs[Y_OUTPUT].setVoltage((yOut + params[OFFSET_Y_VOLTS_PARAM].getValue()) * params[SCALE_Y_PARAM].getValue());
	
	float xInvOut = rescalefjw(params[X_POS_PARAM].getValue(), minX, maxX, maxVolt, minVolt);
	float yInvOut = rescalefjw(params[Y_POS_PARAM].getValue(), minY, maxY, minVolt, maxVolt); //y is inverted because gui coords
	outputs[X_INV_OUTPUT].setVoltage((xInvOut + params[OFFSET_X_VOLTS_PARAM].getValue()) * params[SCALE_X_PARAM].getValue());
	outputs[Y_INV_OUTPUT].setVoltage((yInvOut + params[OFFSET_Y_VOLTS_PARAM].getValue()) * params[SCALE_Y_PARAM].getValue());
	
	outputs[GATE_OUTPUT].setVoltage(params[GATE_PARAM].getValue() * 10);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct XYPadDisplay : Widget {
	XYPad *module;
	XYPadDisplay() {}
	float initX = 0;
	float initY = 0;
	float dragX = 0;
	float dragY = 0;

	void onButton(const event::Button &e) override {
		if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
			if(e.action == GLFW_PRESS){
				e.consume(this);
				// e.target = this;
				initX = e.pos.x;
				initY = e.pos.y;
				module->setMouseDown(e.pos, true);
			} else {
				module->setMouseDown(e.pos, false);
			}		
		}
	}

	void onDragStart(const event::DragStart &e) override {
		dragX = APP->scene->rack->mousePos.x;
		dragY = APP->scene->rack->mousePos.y;
	}

	void onDragEnd(const event::DragEnd &e) override { 
		module->setMouseDown(Vec(0,0), false); 
		// gDraggedWidget = NULL;
	}

	void onDragMove(const event::DragMove &e) override {
		if(module->state == XYPad::STATE_RECORDING){
			float newDragX = APP->scene->rack->mousePos.x;
			float newDragY = APP->scene->rack->mousePos.y;
			module->setCurrentPos(initX+(newDragX-dragX), initY+(newDragY-dragY));
		}
	}

	void draw(const DrawArgs &args) override {
		//background
		nvgFillColor(args.vg, nvgRGB(20, 30, 33));
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
		nvgFill(args.vg);

		if(module == NULL) return;
			
		float ballX = module->params[XYPad::X_POS_PARAM].getValue();
		float ballY = module->params[XYPad::Y_POS_PARAM].getValue();
		float invBallX = module->displayWidth-ballX;
		float invBallY = module->displayHeight-ballY;

		//INVERTED///////////////////////////////////
		NVGcolor invertedColor = nvgRGB(20, 50, 53);
		NVGcolor ballColor = nvgRGB(25, 150, 252);

		//horizontal line
		nvgStrokeColor(args.vg, invertedColor);
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, 0, invBallY);
		nvgLineTo(args.vg, box.size.x, invBallY);
		nvgStroke(args.vg);
		
		//vertical line
		nvgStrokeColor(args.vg, invertedColor);
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, invBallX, 0);
		nvgLineTo(args.vg, invBallX, box.size.y);
		nvgStroke(args.vg);
		
		//inv ball
		nvgFillColor(args.vg, invertedColor);
		nvgStrokeColor(args.vg, invertedColor);
		nvgStrokeWidth(args.vg, module->ballStrokeWidth);
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, module->displayWidth-ballX, module->displayHeight-ballY, module->ballRadius);
		if(module->params[XYPad::GATE_PARAM].getValue())nvgFill(args.vg);
		nvgStroke(args.vg);
		
		//POINTS///////////////////////////////////
		if(module->points.size() > 0){
			nvgStrokeColor(args.vg, ballColor);
			nvgStrokeWidth(args.vg, 2);
			nvgBeginPath(args.vg);
			long lastI = module->points.size() - 1;
			for (long i = lastI; i>=0 && i<long(module->points.size()); i--) {
				if(i == lastI){ 
					nvgMoveTo(args.vg, module->points[i].x, module->points[i].y); 
				} else {
					nvgLineTo(args.vg, module->points[i].x, module->points[i].y); 
				}
			}
			nvgStroke(args.vg);
		}


		//MAIN///////////////////////////////////

		//horizontal line
		nvgStrokeColor(args.vg, nvgRGB(255, 255, 255));
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, 0, ballY);
		nvgLineTo(args.vg, box.size.x, ballY);
		nvgStroke(args.vg);
		
		//vertical line
		nvgStrokeColor(args.vg, nvgRGB(255, 255, 255));
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, ballX, 0);
		nvgLineTo(args.vg, ballX, box.size.y);
		nvgStroke(args.vg);
		
		//ball
		nvgFillColor(args.vg, ballColor);
		nvgStrokeColor(args.vg, ballColor);
		nvgStrokeWidth(args.vg, module->ballStrokeWidth);
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, ballX, ballY, module->ballRadius);
		if(module->params[XYPad::GATE_PARAM].getValue())nvgFill(args.vg);
		nvgStroke(args.vg);
	}
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct XYPadWidget : ModuleWidget {
	XYPadWidget(XYPad *module);
	void appendContextMenu(Menu *menu) override;
};

struct RandomShapeButton : TinyButton {
	void onButton(const event::Button &e) override {
		if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
			if(e.action == GLFW_PRESS){
				TinyButton::onButton(e);
				XYPadWidget *xyw = this->getAncestorOfType<XYPadWidget>();
				XYPad *xyPad = dynamic_cast<XYPad*>(xyw->module);
				xyPad->randomizeShape();
			}
		}
	}
};

struct RandomVariationButton : TinyButton {
	void onButton(const event::Button &e) override {
		if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
			if(e.action == GLFW_PRESS){
				TinyButton::onButton(e);
				XYPadWidget *xyw = this->getAncestorOfType<XYPadWidget>();
				XYPad *xyPad = dynamic_cast<XYPad*>(xyw->module);
				xyPad->makeShape(xyPad->lastRandomShape);
			}
		}
	}
};

XYPadWidget::XYPadWidget(XYPad *module) {
		setModule(module);
	box.size = Vec(RACK_GRID_WIDTH*24, RACK_GRID_HEIGHT);

	SVGPanel *panel = new SVGPanel();
	panel->box.size = box.size;
	panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/XYPad.svg")));
	addChild(panel);

	XYPadDisplay *display = new XYPadDisplay();
	display->module = module;
	display->box.pos = Vec(2, 40);
	display->box.size = Vec(box.size.x - 4, RACK_GRID_HEIGHT - 80);
	addChild(display);

	if(module != NULL){
		module->displayWidth = display->box.size.x;
		module->displayHeight = display->box.size.y;
		module->updateMinMax();
		module->defaultPos();
	}

	addChild(createWidget<Screw_J>(Vec(40, 20)));
	addChild(createWidget<Screw_W>(Vec(55, 20)));
	addParam(createParam<RandomShapeButton>(Vec(90, 20), module, XYPad::RND_SHAPES_PARAM));
	addParam(createParam<RandomVariationButton>(Vec(105, 20), module, XYPad::RND_VARIATION_PARAM));
	addParam(createParam<JwTinyKnob>(Vec(140, 20), module, XYPad::SCALE_X_PARAM));
	addParam(createParam<JwTinyKnob>(Vec(200, 20), module, XYPad::SCALE_Y_PARAM));
	addParam(createParam<JwTinyKnob>(Vec(260, 20), module, XYPad::OFFSET_X_VOLTS_PARAM));
	addParam(createParam<JwTinyKnob>(Vec(320, 20), module, XYPad::OFFSET_Y_VOLTS_PARAM));

	////////////////////////////////////////////////////////////

	addInput(createInput<TinyPJ301MPort>(Vec(25, 360), module, XYPad::PLAY_GATE_INPUT));
	addParam(createParam<TinyButton>(Vec(71, 360), module, XYPad::AUTO_PLAY_PARAM));
	addChild(createLight<SmallLight<MyBlueValueLight>>(Vec(71+3.75, 360+3.75), module, XYPad::AUTO_LIGHT));
	addInput(createInput<TinyPJ301MPort>(Vec(110, 360), module, XYPad::PLAY_SPEED_INPUT));
	addParam(createParam<JwTinyKnob>(Vec(130, 360), module, XYPad::PLAY_SPEED_PARAM));
	addParam(createParam<JwTinyKnob>(Vec(157, 360), module, XYPad::SPEED_MULT_PARAM));
	addOutput(createOutput<TinyPJ301MPort>(Vec(195, 360), module, XYPad::X_OUTPUT));
	addOutput(createOutput<TinyPJ301MPort>(Vec(220, 360), module, XYPad::Y_OUTPUT));
	addOutput(createOutput<TinyPJ301MPort>(Vec(255, 360), module, XYPad::X_INV_OUTPUT));
	addOutput(createOutput<TinyPJ301MPort>(Vec(280, 360), module, XYPad::Y_INV_OUTPUT));
	addOutput(createOutput<TinyPJ301MPort>(Vec(320, 360), module, XYPad::GATE_OUTPUT));
}

struct PlayModeItem : MenuItem {
	XYPad *xyPad;
	int mode;
	void onAction(const event::Action &e) override {
		xyPad->curPlayMode = mode;
		xyPad->setState(XYPad::STATE_AUTO_PLAYING);
	}
	void step() override {
		rightText = (xyPad->curPlayMode == mode) ? "✔" : "";
		MenuItem::step();
	}
};

struct ShapeMenuItem : MenuItem {
	XYPad *xyPad;
	int shape = -1;
	void onAction(const event::Action &e) override {
		xyPad->makeShape(shape);
	}
};

void XYPadWidget::appendContextMenu(Menu *menu) {
	{	
		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);
	}
	XYPad *xyPad = dynamic_cast<XYPad*>(module);
	assert(xyPad);

	{
		PlayModeItem *item = new PlayModeItem();
		item->text = "Forward Loop";
		item->xyPad = xyPad;
		item->mode = XYPad::FWD_LOOP;
		menu->addChild(item);
	}
	{
		PlayModeItem *item = new PlayModeItem();
		item->text = "Backward Loop";
		item->xyPad = xyPad;
		item->mode = XYPad::BWD_LOOP;
		menu->addChild(item);
	}
	{
		PlayModeItem *item = new PlayModeItem();
		item->text = "Forward One-Shot";
		item->xyPad = xyPad;
		item->mode = XYPad::FWD_ONE_SHOT;
		menu->addChild(item);
	}
	{
		PlayModeItem *item = new PlayModeItem();
		item->text = "Backward One-Shot";
		item->xyPad = xyPad;
		item->mode = XYPad::BWD_ONE_SHOT;
		menu->addChild(item);
	}
	{
		PlayModeItem *item = new PlayModeItem();
		item->text = "Forward-Backward Loop";
		item->xyPad = xyPad;
		item->mode = XYPad::FWD_BWD_LOOP;
		menu->addChild(item);
	}
	{
		PlayModeItem *item = new PlayModeItem();
		item->text = "Backward-Forward Loop";
		item->xyPad = xyPad;
		item->mode = XYPad::BWD_FWD_LOOP;
		menu->addChild(item);
	}
}

Model *modelXYPad = createModel<XYPad, XYPadWidget>("XYPad");
