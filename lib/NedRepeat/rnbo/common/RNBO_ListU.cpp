#include "RNBO_List.h"
#include "RNBO_Array.h"

#ifdef C74_UNIT_TESTS
#include <JuceHeader.h>
#include <stdexcept>

namespace ListTests {
	using namespace RNBO;
	class Tests : public UnitTest {
		public:
			Tests()
				: UnitTest("List Tests")
			{}
			void runTest() override {
				beginTest("listbase bounds");
				listbase<number> l = { 1.0 };
				expectDoesNotThrow(l[0]);
				expectThrowsType(l[1], std::out_of_range);
				expectThrowsType(l[1] = 20.0, std::out_of_range);
				expectThrowsType(l[100], std::out_of_range);

				//shift
				expectEquals(l.shift(), 1.0);
				expectThrowsType(l.shift(), std::out_of_range);

				//pop
				l.push(20.0);
				l.push(10.0);
				expectEquals(l.pop(), 10.0);
				expectEquals(l.pop(), 20.0);
				expectEquals(l.pop(), 0.0);
				expectEquals(l.pop(), 0.0);

                // array conversions
                RNBO::array<number, 4> a1 = { 2, 3, 4, 5 };
                list l2 = a1;
                list l3(a1);

                expectEquals(l2[3], 5.);
                expectEquals(l3[2], 4.);

                l2 = l2.concat(1);
                l2 = l2.concat(l3);

                expectEquals(l2[l2.length - 1], 5.);

                l2.fill(74., 2, 4);
                expectEquals(l2[1], 3.);
                expectEquals(l2[2], 74.);
                expectEquals(l2[3], 74.);
                expectEquals(l2[4], 1.);

                l2.fill(74);
                expectEquals(l2[0], 74.);
                expectEquals(l2[1], 74.);
                expectEquals(l2[4], 74.);
                expectEquals(l2[l2.length - 1], 74.);

                size_t l2length = l2.length;
                l2.unshift(1);
                expectEquals((size_t)l2.length, l2length + 1);
                expectEquals(l2[0], 1.);
                expectEquals(l2[1], 74.);

                l2.splice(3, 0, 23, 24);
                expectEquals((size_t)l2.length, l2length + 3);
                expectEquals(l2[2], 74.);
                expectEquals(l2[3], 23.);
                expectEquals(l2[4], 24.);
                expectEquals(l2[5], 74.);

                l2.splice(5, 1, 7);
                expectEquals((size_t)l2.length, l2length + 3);
                expectEquals(l2[5], 7.);

                list l4 = l2.slice(3, 5);
                expectEquals((size_t)l4.length, (size_t)2);
                expectEquals(l4[0], l2[3]);
                expectEquals(l4[1], l2[4]);

                list l5 = l2.slice(-3, -1);
                expectEquals((size_t)l5.length, (size_t)2);
                expectEquals(l5[0], l2[l2.length - 3]);
                expectEquals(l5[1], l2[l2.length - 2]);

                expect(l4.includes(l2[3]));
                expect(!l4.includes(l2[3], 1));

                expectEquals(l4.indexOf(l2[4]), 1);

                l4.reverse();
                expectEquals(l4[0], l2[4]);
                expectEquals(l4[1], l2[3]);

                l4.reserve(10);
                expectEquals((size_t)l4.length, (size_t)2);
                expectEquals(l4[0], l2[4]);
                expectEquals(l4[1], l2[3]);
			}
	};
	Tests tests;
}
#endif

