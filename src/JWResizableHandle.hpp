#pragma once
#include "rack0.hpp"

using namespace rack;

struct JWModuleResizeHandle : Widget {
	bool right = false;
	Vec dragPos;
	Rect originalBox;

	JWModuleResizeHandle() {
		box.size = Vec(RACK_GRID_WIDTH * 1, RACK_GRID_HEIGHT);
	}

	void onDragStart(const event::DragStart &e) override {
printf("onDragStart\n");
		if (e.button != GLFW_MOUSE_BUTTON_LEFT)
			return;

		dragPos = APP->scene->rack->mousePos;
		ModuleWidget *mw = getAncestorOfType<ModuleWidget>();
		assert(mw);
		originalBox = mw->box;
	}

	void onDragMove(const event::DragMove &e) override {
printf("onDragMove\n");
		ModuleWidget *mw = getAncestorOfType<ModuleWidget>();
		assert(mw);

		Vec newDragPos = APP->scene->rack->mousePos;
		float deltaX = newDragPos.x - dragPos.x;

		Rect newBox = originalBox;
		Rect oldBox = mw->box;
		const float minWidth = 3 * RACK_GRID_WIDTH;
		if (right) {
			newBox.size.x += deltaX;
			newBox.size.x = std::fmax(newBox.size.x, minWidth);
			newBox.size.x = std::round(newBox.size.x / RACK_GRID_WIDTH) * RACK_GRID_WIDTH;
		}
		else {
			newBox.size.x -= deltaX;
			newBox.size.x = std::fmax(newBox.size.x, minWidth);
			newBox.size.x = std::round(newBox.size.x / RACK_GRID_WIDTH) * RACK_GRID_WIDTH;
			newBox.pos.x = originalBox.pos.x + originalBox.size.x - newBox.size.x;
		}

		// Set box and test whether it's valid
		mw->box = newBox;
		if (!APP->scene->rack->requestModulePos(mw, newBox.pos)) {
			mw->box = oldBox;
		}
	}
};
