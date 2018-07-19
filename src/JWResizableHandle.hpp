#pragma once
#include "rack.hpp"

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
	
	void onMouseDown(EventMouseDown &e) override {
		if (e.button == 0) {
			e.consumed = true;
			e.target = this;
		}
	}

	void onDragStart(EventDragStart &e) override {
		dragX = gRackWidget->lastMousePos.x;
		dragY = gRackWidget->lastMousePos.y;
		ModuleWidget *m = getAncestorOfType<ModuleWidget>();
		originalBox = m->box;
	}

	void onDragMove(EventDragMove &e) override {
		ModuleWidget *m = getAncestorOfType<ModuleWidget>();

		float newDragX = gRackWidget->lastMousePos.x;
		float deltaX = newDragX - dragX;
		float newDragY = gRackWidget->lastMousePos.y;
		float deltaY = newDragY - dragY;

		Rect newBox = originalBox;
		
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

		gRackWidget->requestModuleBox(m, newBox);
	}
};
