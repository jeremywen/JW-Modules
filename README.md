# JW-Modules

`	           d(-_-)b        Useful, Musical, and Fun     ¯\_(ツ)_/¯`

# [Nightly Builds](https://github.com/jeremywen/JW-Modules/releases/tag/Nightly)

Modules for [VCV Rack](https://vcvrack.com/) by [Jeremy Wentworth](https://linktr.ee/jeremyw)

[Official downloads](https://metamodule.info/downloads) for the 4MS Metamodule | My Metamodule [repo and releases](https://github.com/jeremywen/JWModulesMeta/releases)

Add JW-Modules through [the vcv library](https://library.vcvrack.com/?query=&brand=JW-Modules&tag=&license=).

Support me on [Patreon](https://www.patreon.com/jeremywen) or make a one time donation with [Paypal](https://www.paypal.me/jeremywen)

![All](./doc/all-img22.gif)

----
## Table of Contents:

[1Pattern](./manual/1Pattern.md) | 
[8Seq](./manual/8Seq.md) | 
[AbcdSeq](./manual/AbcdSeq.md) | 
[Add5](./manual/Add5.md) | 
[Arrange](./manual/Arrange.md) | 
[Bouncy Balls](./manual/BouncyBalls.md) | 
[Buffer](./manual/Buffer.md) | 
[Crawl](./manual/Crawl.md) | 
[D1v1de](./manual/D1v1de.md) | 
[DivSeq](./manual/DivSeq.md) | 
[FM16SEQ](./manual/FM16Seq.md) | 
[FM4DICE](./manual/FM4Dice.md) | 
[Fract](./manual/Fract.md) | 
[FullScope](./manual/Fullscope.md) | 
[Grains](./manual/Grains.md) | 
[GridSeq](./manual/Gridseq.md) | 
[MinMax](./manual/Minmax.md) | 
[NoteSeq](./manual/NoteSeq.md) | 
[NoteSeq16](./manual/NoteSeq16.md) | 
[NoteSeqFu](./manual/NoteSeqFu.md) | 
[Patterns](./manual/Patterns.md) | 
[Pres1t](./manual/Pres1t.md) | 
[Quantizer](./manual/Quantizer.md) | 
[RandomSound](./manual/RandomSound.md) | 
[SampleGrid](./manual/SampleGrid.md) | 
[ShiftRegRnd](./manual/ShiftRegRnd.md) | 
[SimpleClock](./manual/SimpleClock.md) | 
[StereoSwitch](./manual/StereoSwitch.md) | 
[StereoSwitchInv](./manual/StereoSwitchInv.md) | 
[Str1ker](./manual/Str1ker.md) | 
[Subtract5](./manual/Subtract5.md) | 
[Thing Thing](./manual/ThingThing.md) | 
[Timer](./manual/Timer.md) | 
[Tree](./manual/Tree.md) | 
[Trigs](./manual/Trigs.md) | 
[Trigs128](./manual/Trigs128.md) | 
[WavHead](./manual/WavhHead.md) | 
[XY-Pad](./manual/XYPad.md)

**Make sure to always see if there are options or functions inside the right click menus of all modules**

**Seed inputs allow you to send in a cv value, hit random, and always get the same output for that value**

## My Cable Colors in order below: orange, yellow, purple, blue
```
  "cableColors": [
    "#ff9709",
    "#fff309",
    "#901afc",
    "#1996fc"
  ]

```

## Other goofy crap

![goofy](https://user-images.githubusercontent.com/408586/34752974-6934555a-f583-11e7-8fd1-bc378e455839.gif)

## Building

Compile Rack from source, following the instructions at https://github.com/VCVRack/Rack.

Check out JW-Modules into the `plugins/` directory

Then run:

	make

To make plugin zip:

	make dist

SEE ALSO [Dev FAQ for VCV Rack](https://github.com/jeremywen/JW-Modules/wiki/Dev-FAQ-for-VCV-Rack)

Library Update ticket https://github.com/VCVRack/library/issues/467
