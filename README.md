# esp32-x32-remote
A simple remote unit for Behringer X32 / Midas M32 mixer capable to control fader and mute of each channel and showing values/feedback on a 1602 display.
It uses OSC protocol via UDP to set and get information.

The project is still on early stages, but it's already working.
It needs futher improvements and optimization, i will include a todo as soon as possible.
At the moment it only works using ethernet, I plan to add wifi support as well very soon.
Also comments and advices are always welcome!

Components needed:
* 1x ESP32
* 1x Rotary Push Encoder
* 1x LCD 1602
* 1x Ethernet Shield ENC28J60

IP addresses are as following:
* Controller IP: 10.0.1.2
* Mixer IP: 10.0.1.1

They could be changed using the menu.

The code still needs A LOT of optimization, I wrote it as fast as possible in order to make it work.

Please be kind!
