# My Pebble Watch

This is a simple watchface I have written. Most of the code comes directly from the tutorials (<https://developer.pebble.com/tutorials/watchface-tutorial/part1/>).

## Features:

* Shows the time
* Shows the day of the week and date
* Shows battery level
* Shows the percentage of the day (blue ring)
* Shows the percentage of the hour (dark pink ring)
* Shows your step count (bottom left of the display)
* Shows the percentage of your average step count (bottom right of the display)
* The entire background changes to red if you lose bluetooth connectivity, as well as vibrating.

I do plan on adding other features at some point (excluding weather). But for now it is more about making this more efficient and learning about the API.

## Building & Running:

To run you need the Pebble SDK installed <https://developer.pebble.com/sdk/>

You can build/run in one of two ways:

1. Run `make` (after updating your IP address in the `Makefile`)
2. Manually run: `pebble build` and `pebble install --phone <ip address>`

## Notes:

The font is a free font that I downloaded from: <http://www.dafont.com/blocked.font>, I believe that I can use it for any purpose. So I used it here.

## Description in Store:

This watchface displays the following information:

* Current time
* Current Date
* Current day
* Battery percentage
* Bluetooth Connectivity
* Steps taken
* How many steps you should have taken by now
* Your average steps for the day

The rings from the centre are:

1. How far you are from your current average (centre is perfect, to the left is behind and to the right is ahead)
2. Minute hand
3. Hour hand (using 24 hour time)
4. Steps taken as a percentage of your average steps (and the little piece is where you should be)

The entire background changes to red when the bluetooth connection is lost.

The values:
Top:
* Left: date with day below
* Right: battery percentage

Bottom:
* Left: current steps with what you should have above
* Right: Percentage of your average, with the daily average above