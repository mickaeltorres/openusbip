# openusbip
An USBIP host implementation for OpenBSD/ugen(4)

This permits an OpenBSD computer to share USB devices directly connected
to it, to another computer over the network using the USBIP protocol.

Only devices without a specific driver can be shared (i.e. only ugen attached).

This has been currently tested with a linux client.

This is still a work in progress, many things are yet to be fixed
e.g. currently it only permit to share ugen0.