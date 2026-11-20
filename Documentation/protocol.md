 `pianod2` Protocols [protocols]
--------------------

`pianod2` supports 2 protocols:

- A text-based protocol in the traditions of FTP, SMTP, POP, and other UNIX services.  Use port 4445.
- A more modern-style, JSON protocol.  Connect to port 4446 and issue the greeting `"HELO pianod json\n"`.
- Either of these can be used over TLS by using port 4447 and issuing the greeting after the TLS handshake.
- To use JSON over a websocket connection, include the query parameter `protocol=json` when opening the websocket connection to port 4446 or 4447.

These procedures select the *response* protocol.  Commands/requests can be issued in either form at any time.

For clarity, "**commands**" refers to command-line style directives, while "**request**" refers to JSON style.

Common elements of the protocols are:

TCP/IP Line Framing
: For both text and JSON protocols over plain-old TCP, commands/requests are submitted as a line of text terminated by a newline.  Newlines are also key in framing responses.

WebSocket Framing
: When using WebSockets, each line is instead a WebSocket packet, for both commands/requests and responses.  Newlines are omitted from WebSocket responses.

Mingling of spontaneous information and replies
: pianod transmits both spontaneous information (such as playback status changes and current track information) and replies to clients.  Clients must be able to separate the spontaneous from the replies.  The protocols have been designed to allow this.

Reply & Ordering Guarantee
: Furthermore, a command or request is guaranteed to receive a reply, and replies will arrive in the order in which requests were sent.  Thus, a client may submit successive requests without waiting for replies, and replies can be handled (typically dispatched to callback functions) as they are received.

No Guarantee of Responsiveness
: There is no guarantee how quickly the response will be provided.  Clients should avoid using timeouts, or set them sufficiently high that normal delayed responses will not trigger them.  Delays occur:

    - around song transitions & crossfades
    - preparing responses to complicated queries
    - if other connected users/sessions are submitting requests
    - if Internet music services are slow in responding due to network or server congestion
    - if pianod is on the Internet and being probed heavily for exploits


