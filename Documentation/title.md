Title: pianod2 Documentation
Summary: pianod2 launch instructions, protocol, command set, and filter grammar.

pianod2 Documentation
=====================

`pianod2` is a music player as a UNIX daemon, supporting Pandora and local music collections.  The football socket library exposes both IPv4 and IPv6 sockets for control, which can be done via `nc`(1), `telnet`(1), or via a Websocket connection from a browser.  The included web client can connect via HTTP or HTTPS (if enabled/configured).

The package includes `piano`, a shell script to access and control the music server.  This allows control from other shell scripts, and provides a reference implementation for communicating with the daemon over the line protocol.

**Warning**: `pianod2` line protocol is incompatible with that of the original `pianod`.  And you might want to use the JSON protocol anyway.

This documentation is include with `pianod2` in Markdown format.  It is also available in [HTML].

[HTML]: http://deviousfish.com/pianod2/Documentation/pianod.html

