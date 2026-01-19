umsg

library for embedded messaging
c++11, single header, no dependencies no dynamic memory allocation

Protocol Frame format:
|version | msg_id | msg_hash|   len   |     payload    | 
| 1 byte | 1 byte | 4 bytes | 2 bytes | max_size bytes |


wire packet format:
|encode(|Frame | crc32) | delimiter | 

objets:

Framer, handle the framing and deframing of messages using cobs. 
uses crc32 to validate integrity of the frame encoded in the message.
provides callback mechanism to notify when a complete message is received.
provides a method to create a packet from a given frame. does not know anithing about the frame but its size.

Router, responsible for create a frame in network byte order from a serialized message object.
also responsible to provide a member function callback to the framer to receive complete frames.
when a complete frame is received the router will parse the frame, validate the frame content (version, hash)
and then dispatch the deseralized message to the registered message handler callback.
Provides a method to register message handlers for specific message ids.

Node: high level object that combines a framer and a router. templated on a io object 
that provides read and write methods to an arbitrary transport layer (serial, tcp/ip, etc).
Also templated on a max message size to allocate the necessary buffers statically.


