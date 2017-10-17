#include "JWModules.hpp"


Plugin *plugin;

void init(rack::Plugin *p) {
	plugin = p;
	plugin->slug = "JW-Modules";
	plugin->name = "JW-Modules";
	plugin->homepageUrl = "https://github.com/jeremywen/JW-Modules";
	createModel<SimpleClockWidget>(plugin, "SimpleClock", "SimpleClock");
}
