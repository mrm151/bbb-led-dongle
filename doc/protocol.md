# A string-based protocol to be used with communication interfaces
There is no real need for this protocol in this project. The data being sent is not particularly critical, as it is just dealing with LEDs. However I thought it would be a good learning experience for me.

## The basis
As per the title, this protocol is a string based protocol. An example packet is shown below:

```
"!command,key0:value0,key1:value1,msg:0#1234"
```

Each packet is composed of:
* A preamble/identifier ("!")
* A command ("command")
* A list of key:value pairs which are compatible with the command
* A 16-bit msg number to identify the msg ("msg:0")
* A 16-bit ccitt CRC ("1234"), computed from the start of the packet up to the start of the CRC ("!" to "#")

The packet is base64 encoded.


The protocol operates as follows:
1. Packet received as base64 encoded bytes
2. Packet is decoded and parsed
    * If the packet is data, then construct an ACK and put it into the outgoing queue
        * Parse the data into a C struct
    * If the packet is an ACK, then we must have sent a packet earlier - get the packet from the outgoing queue an check its message number is equal to the ACKs. If yes, pop the packet from the outgoing queue.
    * Packets are only popped from the outgoing queue once an ACK is received OR five timeouts have occurred.
    * If timeout occurs, remove the packet from the queue.
    * If the packet is a NACK, keep whatever packet we have in the outgoing queue, and make sure we send it again
3. Send packets from the outgoing queue.


## Parsing
As described above, the parser has a few different functions.
