# High-level design - Serialisation
"When I am working on a problem I never think about beauty. I think only how to solve the problem. But when I have finished, if the solution is not beautiful, I know it is wrong."

The serialiser should not know about packets from the protocol. It shouldnt care what the protocol is. It should just be given objects and then copy them into a buffer.


