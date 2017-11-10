#include <string.h>
#include "JWModules.hpp"
#include "dsp/digital.hpp"

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
		RND_RAMP,
		RND_LINE,
		RND_SINE,
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
	float totalBallSize = 12;
	float minVolt = -5, maxVolt = 5;
	float recordPhase = 0.0;
	float playbackPhase = 0.0;
	bool autoPlayOn = false;
	bool playingFwd = true;
	int state = STATE_IDLE;
	int curPlayMode = FWD_LOOP;
	SchmittTrigger autoBtnTrigger;
	std::vector<Vec> points;
	long curPointIdx = 0;

	XYPad() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {}
	void step();

	void reset(){
	    setState(STATE_IDLE);
	    points.clear();
	    defaultPos();
	}

	void randomize(){
		makeShape(int(randomf() * NUM_SHAPES));
	}

	void makeShape(int shape){
		int stateBefore = state;
	    setState(STATE_IDLE);
	    points.clear();
	    switch(shape){
			case RND_RAMP: {
				    float lastY = maxY;
				    float rate = randomf();
				    bool inside = true;
				    for(int i=0; i<5000 && inside; i++){
				    	float x = minX + i;
				    	lastY -= powf(2, powf(x*0.005, 2)) * rate;
				    	float y = lastY;
						inside = isInView(x, y);
				        if(inside)addPoint(x, y);
			    	}
			    }
				break;
			case RND_LINE: {
				    float startHeight = (randomf() * maxY * 0.5) + (maxY * 0.25);
				    float rate = randomf() - 0.5;
				    bool inside = true;
				    for(int i=0; i<5000 && inside; i++){
				    	float x = minX + i;
				    	float y = startHeight + rate * x;
						inside = isInView(x, y);
				        if(inside)addPoint(x, y);
			    	}
			    }
				break;
			case RND_SINE: {
				    float midHeight = maxY / 2.0;
				    float amp = midHeight * 0.90;
				    float rate = randomf() * 0.1;
				    bool inside = true;
				    for(int i=0; i<5000 && inside; i++){
				    	float x = minX + i;
				    	float y = sin(i*rate) * amp + (maxY / 2.0);
						inside = isInView(x, y);
				        if(inside)addPoint(x, y);
			    	}
			    }
				break;
			case RND_SINE_MOD: {
				    float midHeight = maxY / 2.0;
				    float amp = midHeight * 0.50;
				    float rate = randomf() * 0.1;
				    float rateAdder = randomf() * 0.001;
				    float ampAdder = randomf() * 0.5;
				    bool inside = true;
				    for(int i=0; i<5000 && inside; i++){
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
				    float radius = 10;
				    float rate = randomf() * 0.5;
				    bool inside = true;
				    for(int i=0; i<5000 && inside; i++){
				    	float x = curX + sin(i/10.0) * radius;
				    	float y = curY + cos(i/10.0) * radius;
						inside = isInView(x, y);
				        if(inside)addPoint(x, y);
				        radius+=rate;
				        rate+=0.005;
				    }
				}
			    break;
			case RND_STEPS: {
				    float x = maxX * 0.5;
				    float y = maxY * 0.5;
				    enum stateEnum { ST_RIGHT, ST_LEFT, ST_UP, ST_DOWN };
				    int squSt = ST_RIGHT;
				    int stepsBeforeStateChange = 10 + randomf()*30;
				    bool inside = true;
				    for(int i=0; i<5000 && inside; i++){
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
				    		squSt = int(randomf() * 4);
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

	json_t *toJson() {
		json_t *rootJ = json_object();
		json_object_set_new(rootJ, "autoPlayOn", json_boolean(autoPlayOn));
		json_object_set_new(rootJ, "xPos", json_real(params[X_POS_PARAM].value));
		json_object_set_new(rootJ, "yPos", json_real(params[Y_POS_PARAM].value));

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

	void fromJson(json_t *rootJ) {
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
		params[AUTO_PLAY_PARAM].value = autoPlayOn ? 1 : 0;
		if(autoPlayOn){setState(STATE_AUTO_PLAYING);}
	}

	void defaultPos() {
		params[XYPad::X_POS_PARAM].value = displayWidth / 2.0;
		params[XYPad::Y_POS_PARAM].value = displayHeight / 2.0;		
	}

	void setMouseDown(const Vec &pos, bool down){
		if(down){
			setCurrentPos(pos.x, pos.y);
			setState(STATE_RECORDING);
		} else {
			if(autoPlayOn && !inputs[PLAY_GATE_INPUT].active){ //no auto play if wire connected to play in
				setState(STATE_AUTO_PLAYING);
			} else {
				setState(STATE_IDLE);
			}
		}
	}

	void setCurrentPos(float x, float y){
		params[X_POS_PARAM].value = clampf(x, minX, maxX);
		params[Y_POS_PARAM].value = clampf(y, minY, maxY);
	}

	bool isInView(float x, float y){
		return x >= minX && x <= maxX && y >= minY && y <= maxY;
	}

	void addPoint(float x, float y){
		points.push_back(Vec(x, y));
	}

	void updateMinMax(){
		minX = totalBallSize;
		minY = totalBallSize;
		maxX = displayWidth - totalBallSize;
		maxY = displayHeight - totalBallSize;

	}

	bool isStatePlaying() {
		return state == STATE_GATE_PLAYING || state == STATE_AUTO_PLAYING;
	}

	void playback(){
		if(isStatePlaying() && points.size() > 0){ 
			params[X_POS_PARAM].value = points[curPointIdx].x;
			params[Y_POS_PARAM].value = points[curPointIdx].y;
			curPointIdx += playingFwd ? 1 : -1;
			if(curPointIdx >= 0 && curPointIdx < long(points.size())){
				params[GATE_PARAM].value = true; //keep gate on
			} else {
				params[GATE_PARAM].value = false;

				if(curPlayMode == FWD_LOOP){
					curPointIdx = 0;
				} else if(curPlayMode == BWD_LOOP){
					curPointIdx = points.size() - 1;
				} else if(curPlayMode == FWD_ONE_SHOT || curPlayMode == BWD_ONE_SHOT){
					setState(STATE_IDLE);
					return; //done playing
				} else if(curPlayMode == FWD_BWD_LOOP || curPlayMode == BWD_FWD_LOOP){
					playingFwd = !playingFwd; //go the other way now
TODO FIX THIS so it isnt jumpy
				}
			}
		}
	}

	void setState(int newState){
		switch(newState){		
			case STATE_IDLE:
				curPointIdx = 0;
				params[GATE_PARAM].value = false;		
				break;
			case STATE_RECORDING:
				points.clear();
				curPointIdx = 0;
				params[GATE_PARAM].value = true;
				break;
			case STATE_AUTO_PLAYING:
				params[GATE_PARAM].value = true;
				break;
			case STATE_GATE_PLAYING:
				params[GATE_PARAM].value = true;
				break;
		}
		if(isStatePlaying()){
			if(curPlayMode == FWD_LOOP || curPlayMode == FWD_ONE_SHOT){
				playingFwd = true;
				curPointIdx = 0;
			} else if(curPlayMode == BWD_LOOP || curPlayMode == BWD_ONE_SHOT){
				playingFwd = false;
				curPointIdx = points.size() - 1;
			}
		}
		state = newState;
	}

};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void XYPad::step() {
	if (autoBtnTrigger.process(params[AUTO_PLAY_PARAM].value)) {
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

	if (inputs[PLAY_GATE_INPUT].active) {
		params[AUTO_PLAY_PARAM].value = 0; //disable autoplay if wire connected to play gate
		autoPlayOn = false; //disable autoplay if wire connected to play gate

		if (inputs[PLAY_GATE_INPUT].value >= 1.0) {
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
		recordPhase += recordClockTime / engineGetSampleRate();
		if (recordPhase >= 1.0) {
			recordPhase -= 1.0;
			addPoint(params[X_POS_PARAM].value, params[Y_POS_PARAM].value);
		}

	} else if(isStatePlaying()){
		float playSpeedTotal = clampf(inputs[PLAY_SPEED_INPUT].value + params[PLAY_SPEED_PARAM].value, 0, 10);
		float playbackClockTime = rescalef(playSpeedTotal, 0, 10, 1, 100 * params[SPEED_MULT_PARAM].value);
		playbackPhase += playbackClockTime / engineGetSampleRate();
		if (playbackPhase >= 1.0) {
			playbackPhase -= 1.0;
			playback();
		}
	}

	float xOut = rescalef(params[X_POS_PARAM].value, minX, maxX, minVolt, maxVolt);
	float yOut = rescalef(params[Y_POS_PARAM].value, minY, maxY, maxVolt, minVolt); //y is inverted because gui coords
	outputs[X_OUTPUT].value = (xOut + params[OFFSET_X_VOLTS_PARAM].value) * params[SCALE_X_PARAM].value;
	outputs[Y_OUTPUT].value = (yOut + params[OFFSET_Y_VOLTS_PARAM].value) * params[SCALE_Y_PARAM].value;
	
	float xInvOut = rescalef(params[X_POS_PARAM].value, minX, maxX, maxVolt, minVolt);
	float yInvOut = rescalef(params[Y_POS_PARAM].value, minY, maxY, minVolt, maxVolt); //y is inverted because gui coords
	outputs[X_INV_OUTPUT].value = (xInvOut + params[OFFSET_X_VOLTS_PARAM].value) * params[SCALE_X_PARAM].value;
	outputs[Y_INV_OUTPUT].value = (yInvOut + params[OFFSET_Y_VOLTS_PARAM].value) * params[SCALE_Y_PARAM].value;
	
	outputs[GATE_OUTPUT].value = params[GATE_PARAM].value * 10;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct XYPadDisplay : Widget {
	XYPad *module;
	XYPadDisplay() {}
	float initX = 0;
	float initY = 0;
	float dragX = 0;
	float dragY = 0;

	void onMouseDown(EventMouseDown &e) override { 
		if (e.button == 0) {
			e.consumed = true;
			e.target = this;

			initX = e.pos.x;
			initY = e.pos.y;
			module->setMouseDown(e.pos, true);
		}
	}
	
	void onMouseMove(EventMouseMove &e) override {
	}

	void onMouseUp(EventMouseUp &e) override { 
		if(e.button==0)module->setMouseDown(e.pos, false);
	}

	void onDragStart(EventDragStart &e) override {
		dragX = gRackWidget->lastMousePos.x;
		dragY = gRackWidget->lastMousePos.y;
	}

	void onDragEnd(EventDragEnd &e) override { 
		module->setMouseDown(Vec(0,0), false); 
		gDraggedWidget = NULL;
	}

	void onDragMove(EventDragMove &e) override {
		if(module->state == XYPad::STATE_RECORDING){
			float newDragX = gRackWidget->lastMousePos.x;
			float newDragY = gRackWidget->lastMousePos.y;
			module->setCurrentPos(initX+(newDragX-dragX), initY+(newDragY-dragY));
		}
	}

	void draw(NVGcontext *vg) override {
		float ballX = module->params[XYPad::X_POS_PARAM].value;
		float ballY = module->params[XYPad::Y_POS_PARAM].value;
		float invBallX = module->displayWidth-ballX;
		float invBallY = module->displayHeight-ballY;

		//background
		nvgFillColor(vg, nvgRGB(20, 30, 33));
		nvgBeginPath(vg);
		nvgRect(vg, 0, 0, box.size.x, box.size.y);
		nvgFill(vg);
			
		//INVERTED///////////////////////////////////
		NVGcolor invertedColor = nvgRGB(20, 50, 53);
		NVGcolor ballColor = nvgRGB(25, 150, 252);

		//horizontal line
		nvgStrokeColor(vg, invertedColor);
		nvgBeginPath(vg);
		nvgMoveTo(vg, 0, invBallY);
		nvgLineTo(vg, box.size.x, invBallY);
		nvgStroke(vg);
		
		//vertical line
		nvgStrokeColor(vg, invertedColor);
		nvgBeginPath(vg);
		nvgMoveTo(vg, invBallX, 0);
		nvgLineTo(vg, invBallX, box.size.y);
		nvgStroke(vg);
		
		//inv ball
		nvgFillColor(vg, invertedColor);
		nvgStrokeColor(vg, invertedColor);
		nvgStrokeWidth(vg, 2);
		nvgBeginPath(vg);
		nvgCircle(vg, module->displayWidth-ballX, module->displayHeight-ballY, 10);
		if(module->params[XYPad::GATE_PARAM].value)nvgFill(vg);
		nvgStroke(vg);
		
		//POINTS///////////////////////////////////
		if(module->points.size() > 0){
			nvgStrokeColor(vg, ballColor);
			nvgStrokeWidth(vg, 2);
			nvgBeginPath(vg);
			long lastI = module->points.size() - 1;
			for (long i = lastI; i>=0 && i<long(module->points.size()); i--) {
				if(i == lastI){ 
					nvgMoveTo(vg, module->points[i].x, module->points[i].y); 
				} else {
					nvgLineTo(vg, module->points[i].x, module->points[i].y); 
				}
			}
			nvgStroke(vg);
		}


		//MAIN///////////////////////////////////

		//horizontal line
		nvgStrokeColor(vg, nvgRGB(255, 255, 255));
		nvgBeginPath(vg);
		nvgMoveTo(vg, 0, ballY);
		nvgLineTo(vg, box.size.x, ballY);
		nvgStroke(vg);
		
		//vertical line
		nvgStrokeColor(vg, nvgRGB(255, 255, 255));
		nvgBeginPath(vg);
		nvgMoveTo(vg, ballX, 0);
		nvgLineTo(vg, ballX, box.size.y);
		nvgStroke(vg);
		
		//ball
		nvgFillColor(vg, ballColor);
		nvgStrokeColor(vg, ballColor);
		nvgStrokeWidth(vg, 2);
		nvgBeginPath(vg);
		nvgCircle(vg, ballX, ballY, 10);
		if(module->params[XYPad::GATE_PARAM].value)nvgFill(vg);
		nvgStroke(vg);
	}
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

XYPadWidget::XYPadWidget() {
	XYPad *module = new XYPad();
	setModule(module);
	box.size = Vec(RACK_GRID_WIDTH*24, RACK_GRID_HEIGHT);

	{
		LightPanel *panel = new LightPanel();
		panel->box.size = box.size;
		addChild(panel);
	}
	{
		XYPadDisplay *display = new XYPadDisplay();
		display->module = module;
		display->box.pos = Vec(2, 40);
		display->box.size = Vec(box.size.x - 4, RACK_GRID_HEIGHT - 80);
		addChild(display);

		module->displayWidth = display->box.size.x;
		module->displayHeight = display->box.size.y;
		module->updateMinMax();
		module->defaultPos();
	}

	////////////////////////////////////////////////////////////
	CenteredLabel* const titleLabel = new CenteredLabel;
	titleLabel->box.pos = Vec(12, 12);
	titleLabel->text = "XY Pad";
	addChild(titleLabel);

	rack::Label* const xScaleLabel = new rack::Label;
	xScaleLabel->box.pos = Vec(130-20, 2);
	xScaleLabel->text = "X Scale";
	addChild(xScaleLabel);

	rack::Label* const yScaleLabel = new rack::Label;
	yScaleLabel->box.pos = Vec(190-20, 2);
	yScaleLabel->text = "Y Scale";
	addChild(yScaleLabel);

	rack::Label* const xOffsetLabel = new rack::Label;
	xOffsetLabel->box.pos = Vec(250-20, 2);
	xOffsetLabel->text = "X Offset";
	addChild(xOffsetLabel);

	rack::Label* const yOffsetLabel = new rack::Label;
	yOffsetLabel->box.pos = Vec(310-20, 2);
	yOffsetLabel->text = "Y Offset";
	addChild(yOffsetLabel);

	addParam(createParam<TinyBlackKnob>(Vec(130, 20), module, XYPad::SCALE_X_PARAM, 0.01, 1.0, 0.5));
	addParam(createParam<TinyBlackKnob>(Vec(190, 20), module, XYPad::SCALE_Y_PARAM, 0.01, 1.0, 0.5));
	addParam(createParam<TinyBlackKnob>(Vec(250, 20), module, XYPad::OFFSET_X_VOLTS_PARAM, -5.0, 5.0, 5.0));
	addParam(createParam<TinyBlackKnob>(Vec(310, 20), module, XYPad::OFFSET_Y_VOLTS_PARAM, -5.0, 5.0, 5.0));

	////////////////////////////////////////////////////////////
	rack::Label* const trigLabel = new rack::Label;
	trigLabel->box.pos = Vec(5, 340);
	trigLabel->text = "Gate In";
	addChild(trigLabel);

	rack::Label* const autoLabel = new rack::Label;
	autoLabel->box.pos = Vec(78-20, 340);
	autoLabel->text = "Auto";
	addChild(autoLabel);

	rack::Label* const speedLabel = new rack::Label;
	speedLabel->box.pos = Vec(122-20, 340);
	speedLabel->text = "Speed";
	addChild(speedLabel);

	rack::Label* const speedMultLabel = new rack::Label;
	speedMultLabel->box.pos = Vec(145, 340);
	speedMultLabel->text = "Mult";
	addChild(speedMultLabel);

	rack::Label* const xLabel = new rack::Label;
	xLabel->box.pos = Vec(195-4, 340);
	xLabel->text = "X";
	addChild(xLabel);

	rack::Label* const yLabel = new rack::Label;
	yLabel->box.pos = Vec(220-4, 340);
	yLabel->text = "Y";
	addChild(yLabel);

	rack::Label* const xInvLabel = new rack::Label;
	xInvLabel->box.pos = Vec(255-7, 340);
	xInvLabel->text = "-X";
	addChild(xInvLabel);

	rack::Label* const yInvLabel = new rack::Label;
	yInvLabel->box.pos = Vec(280-7, 340);
	yInvLabel->text = "-Y";
	addChild(yInvLabel);

	rack::Label* const gLabel = new rack::Label;
	gLabel->box.pos = Vec(300-5, 340);
	gLabel->text = "Gate Out";
	addChild(gLabel);

	addInput(createInput<TinyPJ301MPort>(Vec(25, 360), module, XYPad::PLAY_GATE_INPUT));

	addParam(createParam<LEDButton>(Vec(70, 358), module, XYPad::AUTO_PLAY_PARAM, 0.0, 1.0, 0.0));
	addChild(createLight<SmallLight<MyBlueValueLight>>(Vec(70+5.5, 358+5.5), module, XYPad::AUTO_LIGHT));

	addInput(createInput<TinyPJ301MPort>(Vec(110, 360), module, XYPad::PLAY_SPEED_INPUT));
	addParam(createParam<TinyBlackKnob>(Vec(130, 360), module, XYPad::PLAY_SPEED_PARAM, 0.0, 10.0, 5.0));
	addParam(createParam<TinyBlackKnob>(Vec(157, 360), module, XYPad::SPEED_MULT_PARAM, 1.0, 100.0, 1.0));

	addOutput(createOutput<TinyPJ301MPort>(Vec(195, 360), module, XYPad::X_OUTPUT));
	addOutput(createOutput<TinyPJ301MPort>(Vec(220, 360), module, XYPad::Y_OUTPUT));
	addOutput(createOutput<TinyPJ301MPort>(Vec(255, 360), module, XYPad::X_INV_OUTPUT));
	addOutput(createOutput<TinyPJ301MPort>(Vec(280, 360), module, XYPad::Y_INV_OUTPUT));
	addOutput(createOutput<TinyPJ301MPort>(Vec(320, 360), module, XYPad::GATE_OUTPUT));
}

struct PlayModeItem : MenuItem {
	XYPad *xyPad;
	int mode;
	void onAction(EventAction &e) override {
		xyPad->curPlayMode = mode;
	}
	void step() override {
		rightText = (xyPad->curPlayMode == mode) ? "✔" : "";
	}
};

struct ShapeMenuItem : MenuItem {
	XYPad *xyPad;
	int shape = -1;
	void onAction(EventAction &e) override {
		xyPad->makeShape(shape);
	}
};

Menu *XYPadWidget::createContextMenu() {
	Menu *menu = ModuleWidget::createContextMenu();

{	
	MenuLabel *spacerLabel = new MenuLabel();
	menu->pushChild(spacerLabel);
}
	XYPad *xyPad = dynamic_cast<XYPad*>(module);
	assert(xyPad);

	{
		PlayModeItem *item = new PlayModeItem();
		item->text = "Forward Loop";
		item->xyPad = xyPad;
		item->mode = XYPad::FWD_LOOP;
		menu->pushChild(item);
	}
	{
		PlayModeItem *item = new PlayModeItem();
		item->text = "Backward Loop";
		item->xyPad = xyPad;
		item->mode = XYPad::BWD_LOOP;
		menu->pushChild(item);
	}
	{
		PlayModeItem *item = new PlayModeItem();
		item->text = "Forward One-Shot";
		item->xyPad = xyPad;
		item->mode = XYPad::FWD_ONE_SHOT;
		menu->pushChild(item);
	}
	{
		PlayModeItem *item = new PlayModeItem();
		item->text = "Backward One-Shot";
		item->xyPad = xyPad;
		item->mode = XYPad::BWD_ONE_SHOT;
		menu->pushChild(item);
	}
	{
		PlayModeItem *item = new PlayModeItem();
		item->text = "Forward-Backward Loop";
		item->xyPad = xyPad;
		item->mode = XYPad::FWD_BWD_LOOP;
		menu->pushChild(item);
	}
	{
		PlayModeItem *item = new PlayModeItem();
		item->text = "Backward-Forward Loop";
		item->xyPad = xyPad;
		item->mode = XYPad::BWD_FWD_LOOP;
		menu->pushChild(item);
	}
	{	
		MenuLabel *spacerLabel = new MenuLabel();
		menu->pushChild(spacerLabel);
	}
	{
		ShapeMenuItem *item = new ShapeMenuItem();
		item->text = "Random Ramp";
		item->xyPad = xyPad;
		item->shape = XYPad::RND_RAMP;
		menu->pushChild(item);
	}
	{
		ShapeMenuItem *item = new ShapeMenuItem();
		item->text = "Random Line";
		item->xyPad = xyPad;
		item->shape = XYPad::RND_LINE;
		menu->pushChild(item);
	}
	{
		ShapeMenuItem *item = new ShapeMenuItem();
		item->text = "Random Sine";
		item->xyPad = xyPad;
		item->shape = XYPad::RND_SINE;
		menu->pushChild(item);
	}
	{
		ShapeMenuItem *item = new ShapeMenuItem();
		item->text = "Random Sine Mod";
		item->xyPad = xyPad;
		item->shape = XYPad::RND_SINE_MOD;
		menu->pushChild(item);
	}
	{
		ShapeMenuItem *item = new ShapeMenuItem();
		item->text = "Random Spiral";
		item->xyPad = xyPad;
		item->shape = XYPad::RND_SPIRAL;
		menu->pushChild(item);	
	}
	{
		ShapeMenuItem *item = new ShapeMenuItem();
		item->text = "Random Steps";
		item->xyPad = xyPad;
		item->shape = XYPad::RND_STEPS;
		menu->pushChild(item);	
	}

	return menu;
}
