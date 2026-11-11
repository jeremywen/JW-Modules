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

[1Pattern](/manual/1pattern.md) | 
[8Seq](/manual/8seq.md) | 
[AbcdSeq](/manual/abcdseq.md) | 
[Add5](/manual/add5.md) | 
[Arrange](/manual/arrange.md) | 
[Bouncy Balls](/manual/bouncyballs.md) | 
[Buffer](/manual/buffer.md) | 
[Crawl](/manual/crawl.md) | 
[D1v1de](/manual/d1v1de.md) | 
[DivSeq](/manual/divseq.md) | 
[FM16SEQ](/manual/fm16seq.md) | 
[FM4DICE](/manual/fm4dice.md) | 
[Fract](/manual/fract.md) | 
[FullScope](/manual/fullscope.md) | 
[Grains](/manual/grains.md) | 
[GridSeq](/manual/gridseq.md) | 
[MinMax](/manual/minmax.md) | 
[NoteSeq](/manual/noteSeq.md) | 
[NoteSeq16](/manual/noteSeq16.md) | 
[NoteSeqFu](/manual/noteSeqFu.md) | 
[Patterns](/manual/patterns.md) | 
[Pres1t](/manual/pres1t.md) | 
[Quantizer](/manual/quantizer.md) | 
[RandomSound](/manual/randomsound.md) | 
[SampleGrid](/manual/samplegrid.md) | 
[ShiftRegRnd](/manual/shiftregrnd.md) | 
[SimpleClock](/manual/simpleclock.md) | 
[StereoSwitch](/manual/stereoswitch.md) | 
[StereoSwitchInv](/manual/stereoswitchinv.md) | 
[Str1ker](/manual/str1ker.md) | 
[Subtract5](/manual/subtract5.md) | 
[Thing Thing](/manual/thingthing.md) | 
[Timer](/manual/timer.md) | 
[Tree](/manual/tree.md) | 
[Trigs](/manual/trigs.md) | 
[Trigs128](/manual/trigs128.md) | 
[WavHead](/manual/wavhead.md) | 
[XY-Pad](/manual/xypad)

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
