# pebble_watch

This is a simple watchface I have written. Most of the code comes directly from the tutorials (<https://developer.pebble.com/tutorials/watchface-tutorial/part1/>).

## Features:

* Shows the time
* Shows the day of the week and date
* Shows battery level (bar at the top)
* Shows the percentage of the day (left bar)
* Shows the percentage of the hour (right bar)
* Shows your step count
* Shows the percentage of your average step count (bar behind step count)
* A red bar will appear under the time if you lose bluetooth connectivity, as well as vibrating.

I do plan on adding other features at some point (excluding weather). But for now it is more about making this more efficient and learning about the API.

## Building & Running:

To run you need the Pebble SDK installed <https://developer.pebble.com/sdk/>

You can build/run in one of two ways:

1. Run `make` (after updating your IP address in the `Makefile`)
2. Manually run: `pebble build` and `pebble install --phone <ip address>`

