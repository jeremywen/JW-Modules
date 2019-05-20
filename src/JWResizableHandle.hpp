#pragma once
#include "rack0.hpp"

using namespace rack;

struct JWModuleResizeHandle : Widget {

	float minWidth;
	bool right = false;
	float dragX;
	float dragY;
	Rect originalBox;

	JWModuleResizeHandle(float _minWidth) {
		box.size = Vec(RACK_GRID_WIDTH * 1, RACK_GRID_HEIGHT);
		minWidth = _minWidth;
	}
	
	void onButton(const event::Button &e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			e.consume(this);
		}
	}

	void onDragStart(const event::DragStart &e) override {
		dragX = APP->scene->rack->mousePos.x;
		dragY = APP->scene->rack->mousePos.y;
		ModuleWidget *m = getAncestorOfType<ModuleWidget>();
		originalBox = m->box;
	}

	void onDragMove(const event::DragMove &e) override {
		ModuleWidget *mw = getAncestorOfType<ModuleWidget>();

		float newDragX = APP->scene->rack->mousePos.x;
		float deltaX = newDragX - dragX;
		float newDragY = APP->scene->rack->mousePos.y;
		float deltaY = newDragY - dragY;

		Rect newBox = originalBox;
		Rect oldBox = mw->box;

		// resize width
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

		// resize height
		newBox.size.y += deltaY;
		newBox.size.y = fmaxf(newBox.size.y, RACK_GRID_HEIGHT);
		newBox.size.y = roundf(newBox.size.y / RACK_GRID_HEIGHT) * RACK_GRID_HEIGHT;

		mw->box = newBox;
		if (!APP->scene->rack->requestModulePos(mw, newBox.pos)) {
			mw->box = oldBox;
		}
	}
};
