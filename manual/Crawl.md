
## Crawl

![crawl](./doc/crawl2.png)

Trigger gates and notes based on points crawling through stationary points.

Based on a javascript thing I made https://codepen.io/jeremywen/full/MemMEX

Double-click to add/remove points.

Knobs

* **Speed:** how fast the crawlers move
* **Connection Distance:** how close crawler have to be to connect to points
* **Number of Points:** number of stationary points
* **Pitch Range:** range of pitch
* **Wander Radius:** how far crawlers can wander off
* **Movement Coherence:** zero is drunk walking, 1 is linear movement

Button

* **Regen Points:** Regenerates a new set of points

Inputs

* **Clock:** triggers crawling
* **Reset:** regenerates points
* **Speed:** speed of movement
* **Distance:** how close crawler have to be to connect to points
* **Wander Radius:** how far crawlers can wander off
* **Movement Coherence:** zero is drunk walking, 1 is linear movement

Outputs

* **Gate:** poly out, gates on connection from crawler to stationary point
* **V/Oct:** poly out, based on pitch range and y axis
* **X Pos:** poly out, x positions of crawlers
* **Y Pos:** poly out, y positions of crawlers
* **Nearest Point Distance:** poly out, nearest distance of crawlers
