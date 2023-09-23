# NDIWrapper

This is a wrapper for [NDI SDK](https://ndi.video/tech/).

What it basically does is you can in your local network setup a ndi senders and receive them
at your computer (receiver) end. This lib makes it easy that if you setup lets say phones
to be NDI sender, you can connect to them all simultaneously by creating a ndi receiver for each one and receive
high quality lossless images.

With this lib you can send control messages to the ndi sender from receiver and vice versa.
* currently in this lib there is zoom, bounding box, aspect ratio and camera switch


## sampleapplication

The sample application includes a stress test where multiple ndi senders are created and one receiver
is created and the receiver switches between the senders.

