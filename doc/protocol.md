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

## Initialisation
To initialise an instance of the protocol, you must call the `protocol_init` function.

`protocol_init`


## Parsing
```
parser_ret_t parse(protocol_ctx_t ctx)
```

As described above, the parser has a few different functions.

### Receiving data
When receiving data, the parser needs to construct an ack to go into the outgoing queue and parse data received into a C struct so that the user can interact with it.

Detailed process:
* Parse packet
* Obtain msg number
* Create ACK with msg number
* Add ack to outgoing queue
* Put received packet params into context for user access

### Receiving ACK
When receiving an ACK, the parser needs to check the outgoing queue to see if there is a packet with a matching message number, and pop it.

Process:
* Parse packet = ACK
* obtain msg number
* compare msg number with front of queue
* pop front of queue
* deallocate packet

### Receiving NACK
When receiving a NACK, the parser needs to resend the packet with the matching msg number

### Timeout
One additional function is the timeout - ACKs and NACKs are responses which give an idea that the packet is parsable, but data is incorrect. What happens if we get no response?

This shouldn't be handled by the parsing function.

