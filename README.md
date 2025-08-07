# bbb-led-dongle [work in progress]
This repo contains the source code for the dongle part of the bbb-led project.

The dongle is the NRF52840 Dongle seen [here](https://www.nordicsemi.com/-/media/Software-and-other-downloads/Product-Briefs/nRF52840-Dongle-product-brief.pdf).

The dongle is planned to receive commands over USB from a beaglebone black, and relay them over BLE to an STM32.

This will be done via the use of a software defined protocol as described in the doc folder ([protocol.md](doc/protocol.md)).