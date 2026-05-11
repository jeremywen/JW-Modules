//
//  RNBO_MidiStreamParserU.cpp
//  RNBOUnitTests
//
//  Created by Stefan Brunner on 21.05.25.
//

#ifdef C74_UNIT_TESTS
#include <JuceHeader.h>
#include <vector>
#include <deque>

#include "RNBO_MidiStreamParser.h"

namespace MidiStreamParserTests {
	using namespace RNBO;
	class Tests : public UnitTest {
		public:
			Tests()
				: UnitTest("MidiStreamParser Tests")
			{}
			void runTest() override {
				beginTest ("Basic");

				std::deque<std::vector<int>> msg; //int requires less casting in expectEquals
				auto fill = [&msg](const uint8_t * b, size_t c) {
					std::vector<int> m;
					for (size_t i = 0; i < c; i++) {
						m.push_back(static_cast<int>(b[i]));
					}
					msg.push_back(m);
				};
				MidiStreamParser parser;

				auto popf = [&msg]() -> std::vector<int> {
					if (msg.size() == 0) {
						throw new std::runtime_error("no message");
					}
					auto m = msg.front();
					msg.pop_front();
					return m;
				};

				//note on
				parser.process(0x91, fill);
				expectEquals(msg.size(), (size_t)0);
				parser.process(0x1, fill);
				expectEquals(msg.size(), (size_t)0);
				parser.process(0x2, fill);
				expectEquals(msg.size(), (size_t)1);

				{
					auto m = popf();
					expectEquals(m[0], 0x91);
					expectEquals(m[1], 0x1);
					expectEquals(m[2], 0x2);
				}

				//note on with realtime message
				msg.clear();
				parser.process(0x91, fill);
				expectEquals(msg.size(), (size_t)0);
				parser.process(0xF8, fill);
				expectEquals(msg.size(), (size_t)1);
				parser.process(0x1, fill);
				expectEquals(msg.size(), (size_t)1);
				parser.process(0x2, fill);
				expectEquals(msg.size(), (size_t)2);

				{
					auto m = popf();
					expectEquals(m[0], 0xF8);
				}

				{
					auto m = popf();
					expectEquals(m[0], 0x91);
					expectEquals(m[1], 0x1);
					expectEquals(m[2], 0x2);
				}

				// program change
				parser.process(0xC0, fill);
				expectEquals(msg.size(), (size_t)0);
				parser.process(0x1, fill);
				expectEquals(msg.size(), (size_t)1);
				{
					auto m = popf();
					expectEquals(m[0], 0xC0);
					expectEquals(m[1], 0x1);
				}

				beginTest ("Running Status");
				msg.clear();

				//note on
				parser.process(0x91, fill);
				expectEquals(msg.size(), (size_t)0);
				parser.process(0x1, fill);
				expectEquals(msg.size(), (size_t)0);
				parser.process(0x2, fill);
				expectEquals(msg.size(), (size_t)1);
				parser.process(0x3, fill);
				expectEquals(msg.size(), (size_t)1);
				parser.process(0x4, fill);
				expectEquals(msg.size(), (size_t)2, "running status note on");

				{
					auto m = popf();
					expectEquals(m[0], 0x91);
					expectEquals(m[1], 0x1);
					expectEquals(m[2], 0x2);
				}

				{
					auto m = popf();
					expectEquals(m[0], 0x91);
					expectEquals(m[1], 0x3);
					expectEquals(m[2], 0x4);
				}

				//CC
				parser.process(0xB2, fill);
				expectEquals(msg.size(), (size_t)0);
				parser.process(0x11, fill);
				expectEquals(msg.size(), (size_t)0);
				parser.process(0x12, fill);
				expectEquals(msg.size(), (size_t)1);
				parser.process(0x13, fill);
				expectEquals(msg.size(), (size_t)1);
				parser.process(0x14, fill);
				expectEquals(msg.size(), (size_t)2, "running status CC");

				{
					auto m = popf();
					expectEquals(m[0], 0xB2);
					expectEquals(m[1], 0x11);
					expectEquals(m[2], 0x12);
				}

				{
					auto m = popf();
					expectEquals(m[0], 0xB2);
					expectEquals(m[1], 0x13);
					expectEquals(m[2], 0x14);
				}

				//error recovery
				parser.process(0xB2, fill);
				expectEquals(msg.size(), (size_t)0);
				parser.process(0x11, fill);
				expectEquals(msg.size(), (size_t)0);
				parser.process(0x12, fill);
				expectEquals(msg.size(), (size_t)1);
				//partial cc message, then note
				parser.process(0x14, fill);
				expectEquals(msg.size(), (size_t)1);
				parser.process(0x91, fill);
				expectEquals(msg.size(), (size_t)1);
				parser.process(0x1, fill);
				expectEquals(msg.size(), (size_t)1);
				parser.process(0x2, fill);
				expectEquals(msg.size(), (size_t)2);

				{
					auto m = popf();
					expectEquals(m[0], 0xB2);
					expectEquals(m[1], 0x11);
					expectEquals(m[2], 0x12);
				}

				{
					auto m = popf();
					expectEquals(m[0], 0x91);
					expectEquals(m[1], 0x1);
					expectEquals(m[2], 0x2);
				}

				//program change
				msg.clear();
				parser.process(0xC0, fill);
				expectEquals(msg.size(), (size_t)0);
				parser.process(0x1, fill);
				expectEquals(msg.size(), (size_t)1);
				parser.process(0x2, fill);
				expectEquals(msg.size(), (size_t)2, "program change");

				{
					auto m = popf();
					expectEquals(m[0], 0xC0);
					expectEquals(m[1], 0x1);
				}

				{
					auto m = popf();
					expectEquals(m[0], 0xC0);
					expectEquals(m[1], 0x2);
				}


				beginTest ("Sysex");

				msg.clear();
				parser.process(0xF0, fill);
				expectEquals(msg.size(), (size_t)0);
				parser.process(0x1, fill);
				expectEquals(msg.size(), (size_t)0);
				parser.process(0x2, fill);
				expectEquals(msg.size(), (size_t)1);
				parser.process(0xF7, fill);
				expectEquals(msg.size(), (size_t)2);

				{
					auto m = popf();
					expectEquals(m.size(), (size_t)3);
					expectEquals(m[0], 0xF0);
					expectEquals(m[1], 0x1);
					expectEquals(m[2], 0x2);
				}
				{
					auto m = popf();
					expectEquals(m.size(), (size_t)1);
					expectEquals(m[0], 0xF7);
				}

				msg.clear();
				parser.process(0xF0, fill);
				expectEquals(msg.size(), (size_t)0);
				parser.process(0x1, fill);
				expectEquals(msg.size(), (size_t)0);
				parser.process(0x2, fill);
				expectEquals(msg.size(), (size_t)1);
				parser.process(0x3, fill);
				expectEquals(msg.size(), (size_t)1);
				parser.process(0xF7, fill);
				expectEquals(msg.size(), (size_t)2);

				{
					auto m = popf();
					expectEquals(m.size(), (size_t)3);
					expectEquals(m[0], 0xF0);
					expectEquals(m[1], 0x1);
					expectEquals(m[2], 0x2);
				}
				{
					auto m = popf();
					expectEquals(m.size(), (size_t)2);
					expectEquals(m[0], 0x3);
					expectEquals(m[1], 0xF7);
				}

				msg.clear();
				parser.process(0xF0, fill);
				expectEquals(msg.size(), (size_t)0);
				parser.process(0x1, fill);
				expectEquals(msg.size(), (size_t)0);
				parser.process(0x2, fill);
				expectEquals(msg.size(), (size_t)1);
				parser.process(0x3, fill);
				expectEquals(msg.size(), (size_t)1);
				parser.process(0x4, fill);
				expectEquals(msg.size(), (size_t)1);
				parser.process(0xF7, fill);
				expectEquals(msg.size(), (size_t)2);

				{
					auto m = popf();
					expectEquals(m.size(), (size_t)3);
					expectEquals(m[0], 0xF0);
					expectEquals(m[1], 0x1);
					expectEquals(m[2], 0x2);
				}
				{
					auto m = popf();
					expectEquals(m.size(), (size_t)3);
					expectEquals(m[0], 0x3);
					expectEquals(m[1], 0x4);
					expectEquals(m[2], 0xF7);
				}

				msg.clear();
				parser.process(0xF0, fill);
				expectEquals(msg.size(), (size_t)0);
				parser.process(0x1, fill);
				expectEquals(msg.size(), (size_t)0);
				parser.process(0x2, fill);
				expectEquals(msg.size(), (size_t)1);
				parser.process(0x3, fill);
				expectEquals(msg.size(), (size_t)1);
				parser.process(0x4, fill);
				expectEquals(msg.size(), (size_t)1);
				parser.process(0x5, fill);
				expectEquals(msg.size(), (size_t)2);
				parser.process(0xF7, fill);
				expectEquals(msg.size(), (size_t)3);

				{
					auto m = popf();
					expectEquals(m.size(), (size_t)3);
					expectEquals(m[0], 0xF0);
					expectEquals(m[1], 0x1);
					expectEquals(m[2], 0x2);
				}
				{
					auto m = popf();
					expectEquals(m.size(), (size_t)3);
					expectEquals(m[0], 0x3);
					expectEquals(m[1], 0x4);
					expectEquals(m[2], 0x5);
				}
				{
					auto m = popf();
					expectEquals(m.size(), (size_t)1);
					expectEquals(m[0], 0xF7);
				}


				beginTest ("Sysex with Realtime");
				msg.clear();
				parser.process(0xF0, fill);
				expectEquals(msg.size(), (size_t)0);
				parser.process(0x1, fill);
				expectEquals(msg.size(), (size_t)0);
				parser.process(0xF8, fill);
				expectEquals(msg.size(), (size_t)1);
				parser.process(0x2, fill);
				expectEquals(msg.size(), (size_t)2);
				parser.process(0x3, fill);
				expectEquals(msg.size(), (size_t)2);
				parser.process(0x4, fill);
				expectEquals(msg.size(), (size_t)2);
				parser.process(0xF7, fill);
				expectEquals(msg.size(), (size_t)3);

				{
					auto m = popf();
					expectEquals(m.size(), (size_t)1);
					expectEquals(m[0], 0xF8);
				}
				{
					auto m = popf();
					expectEquals(m.size(), (size_t)3);
					expectEquals(m[0], 0xF0);
					expectEquals(m[1], 0x1);
					expectEquals(m[2], 0x2);
				}
				{
					auto m = popf();
					expectEquals(m.size(), (size_t)3);
					expectEquals(m[0], 0x3);
					expectEquals(m[1], 0x4);
					expectEquals(m[2], 0xF7);
				}

				msg.clear();
				parser.process(0xF0, fill);
				expectEquals(msg.size(), (size_t)0);
				parser.process(0x1, fill);
				expectEquals(msg.size(), (size_t)0);
				parser.process(0x2, fill);
				expectEquals(msg.size(), (size_t)1);
				parser.process(0xFA, fill);
				expectEquals(msg.size(), (size_t)2);
				parser.process(0x3, fill);
				expectEquals(msg.size(), (size_t)2);
				parser.process(0x4, fill);
				expectEquals(msg.size(), (size_t)2);
				parser.process(0xF7, fill);
				expectEquals(msg.size(), (size_t)3);

				{
					auto m = popf();
					expectEquals(m.size(), (size_t)3);
					expectEquals(m[0], 0xF0);
					expectEquals(m[1], 0x1);
					expectEquals(m[2], 0x2);
				}
				{
					auto m = popf();
					expectEquals(m.size(), (size_t)1);
					expectEquals(m[0], 0xFA);
				}
				{
					auto m = popf();
					expectEquals(m.size(), (size_t)3);
					expectEquals(m[0], 0x3);
					expectEquals(m[1], 0x4);
					expectEquals(m[2], 0xF7);
				}

				//note on after sysex
				msg.clear();
				parser.process(0x91, fill);
				expectEquals(msg.size(), (size_t)0);
				parser.process(0x1, fill);
				expectEquals(msg.size(), (size_t)0);
				parser.process(0x2, fill);
				expectEquals(msg.size(), (size_t)1);

				{
					auto m = popf();
					expectEquals(m[0], 0x91);
					expectEquals(m[1], 0x1);
					expectEquals(m[2], 0x2);
				}

				beginTest ("Sysex error Recovery");

				//two sysex bytes and then note
				msg.clear();
				parser.process(0xF0, fill);
				expectEquals(msg.size(), (size_t)0);
				parser.process(0x1, fill);
				expectEquals(msg.size(), (size_t)0);
				parser.process(0x91, fill);
				expectEquals(msg.size(), (size_t)0);
				parser.process(0x1, fill);
				expectEquals(msg.size(), (size_t)0);
				parser.process(0x2, fill);
				expectEquals(msg.size(), (size_t)1);

				{
					auto m = popf();
					expectEquals(m[0], 0x91);
					expectEquals(m[1], 0x1);
					expectEquals(m[2], 0x2);
				}

				//partial sysex message and then note
				msg.clear();
				parser.process(0xF0, fill);
				expectEquals(msg.size(), (size_t)0);
				parser.process(0x1, fill);
				expectEquals(msg.size(), (size_t)0);
				parser.process(0x2, fill);
				expectEquals(msg.size(), (size_t)1);
				parser.process(0x91, fill);
				expectEquals(msg.size(), (size_t)1);
				parser.process(0x1, fill);
				expectEquals(msg.size(), (size_t)1);
				parser.process(0x2, fill);
				expectEquals(msg.size(), (size_t)2);

				{
					auto m = popf();
					expectEquals(m.size(), (size_t)3);
					expectEquals(m[0], 0xF0);
					expectEquals(m[1], 0x1);
					expectEquals(m[2], 0x2);
				}

				{
					auto m = popf();
					expectEquals(m[0], 0x91);
					expectEquals(m[1], 0x1);
					expectEquals(m[2], 0x2);
				}

				// program change after sysex
				msg.clear();
				parser.process(0xC0, fill);
				expectEquals(msg.size(), (size_t)0);
				parser.process(0x1, fill);
				expectEquals(msg.size(), (size_t)1);
				{
					auto m = popf();
					expectEquals(m[0], 0xC0);
					expectEquals(m[1], 0x1);
				}

				//1 sysex byte and then program change, sysex should be ignored
				msg.clear();
				parser.process(0xF0, fill);
				expectEquals(msg.size(), (size_t)0);
				parser.process(0xC0, fill);
				expectEquals(msg.size(), (size_t)0);
				parser.process(0x1, fill);
				expectEquals(msg.size(), (size_t)1);

				{
					auto m = popf();
					expectEquals(m[0], 0xC0);
					expectEquals(m[1], 0x1);
				}

				//two sysex bytes and then program change, sysex should be ignored
				msg.clear();
				parser.process(0xF0, fill);
				expectEquals(msg.size(), (size_t)0);
				parser.process(0x1, fill);
				expectEquals(msg.size(), (size_t)0);
				parser.process(0xC0, fill);
				expectEquals(msg.size(), (size_t)0);
				parser.process(0x1, fill);
				expectEquals(msg.size(), (size_t)1);

				{
					auto m = popf();
					expectEquals(m[0], 0xC0);
					expectEquals(m[1], 0x1);
				}

				//3 sysex bytes and then program change, should get 1 sysex message
				msg.clear();
				parser.process(0xF0, fill);
				expectEquals(msg.size(), (size_t)0);
				parser.process(0x1, fill);
				expectEquals(msg.size(), (size_t)0);
				parser.process(0x2, fill);
				expectEquals(msg.size(), (size_t)1);
				parser.process(0xC0, fill);
				expectEquals(msg.size(), (size_t)1);
				parser.process(0x1, fill);
				expectEquals(msg.size(), (size_t)2);

				{
					auto m = popf();
					expectEquals(m.size(), (size_t)3);
					expectEquals(m[0], 0xF0);
					expectEquals(m[1], 0x1);
					expectEquals(m[2], 0x2);
				}

				{
					auto m = popf();
					expectEquals(m[0], 0xC0);
					expectEquals(m[1], 0x1);
				}

			}
	};

	Tests tests;
}
#endif
