# Football
Football is a socket abstraction layer for building line-oriented socket services in the style of FTP, POP3, SMTP, etc.  It serves both IPv4 and IPv6, with TLS available for both (requires GNUTLS library).  Football also allows files to be treated as a connection.  Football is not multithreaded but does allow multiple services and multiple connections on each service.  It is written in C, but has helpful C++ wrappers.

Football also features Websocket support and a rudimentary HTTP server.  These connections use a separate port, and HTTP connections to the line socket will fail.  However, line connections to the HTTP port will work if greetings are enabled and the client greets correctly.  Secure line sessions may be established by connecting to the HTTPS port, TLS handshaking, then issuing the line greeting.

If provided with a directory to serve, football will serve files in response to HTTP and HTTPS requests.  If enabled, localized versions of the requested files are searched for; beyond that, no server-side processing or file compression is performed.

Websockets are an alternate to the line protocol.  A line of text in line-oriented mode (either command or response) is equivalent to one Websocket packet with the newline removed.  The type of connection is abstracted from the application code.

Football includes a primitive web server with enough function to serve a web applet to clients.

## Football vs. REST
As REST services are becoming ubiquitous and ever-easier to implement, wouldn't it make more sense to use REST?

In many scenarios, yes.  You should probably use REST if an application is:

1.	Web only.
2.	Transactional—that is, it generates a request and expects a response, without need for spontaneous notifications.

However, scenarios such as a chat application where communication ideally happens both ways in real time do not work well with REST or traditional http.  Websockets are an attempt to solve this, but provide only a rudimentary packet-exchange system, not a transaction framework.

Line-oriented Football was in place when the Websockets RFC was complete.  Translating lines to Websocket packets was an obvious extension, and initially done with separate software (wsgw, the Websocket gateway), allowing reuse of the application protocol and processing mechanism over the new transport layer.

On the simplicity of line-oriented protocols: Many older Internet protocols possess similar line-oriented mechanisms.  A friend once commented, "Every few years, people rediscover the power of the text file."  There is ease of understanding and the ability to read, explore, experiment and troubleshoot without special tools.  Line-oriented protocols have a similar simplicity, and while there are limitations to what they can carry, a little cleverness goes a long way.  Simplicity avoids the headaches of complicated encoding that make speaking a protocol difficult.  Football harnesses the power of the text protocol.

## Overview
The general use of football is:

1.	Initialize (if using TLS)
2.	Create a service
3.	Wait for events.
4.	Service events.
5.	If not shutting down, Goto 3.
6.	Destroy service.

## Football ANSI C API
### Initializing TLS
TLS support requires GNU TLS.  Your application must first call `gnutls_global_init()` to initialize the library.

Next you must call `fb_init_tls_support(char *path)` to provide the location of X509 certificates and key files.  If `path` is a directory, it must include a trailing slash; if not, the last directory is treated as a prefix for the filenames:

* `/etc/pianod-` becomes `/etc/pianod-x509-server-key.pem`.
* `/etc/pianod/` becomes `/etc/pianod/x509-server-key.pem`.
* `/etc` becomes `/etcx509-server-key.pem`.

The TLS files Football requires are:

* `x509-server-key.pem`
* `x509-server.pem`

See the [GNUTLS certtool documentation] for instructions on creating a certificate.

Lastly, set the `https_port` field in the service options when creating services.

[GNUTLS certtool documentation]: http://www.gnutls.org/manual/html_node/certtool-Invocation.html

### Creating a service

	struct fb_service_t *fb_create_service (FB_OPTIONS *options)

Creates a service on port(s) specified in the options.  For either type of port, Football attempts to create both a IPv4 and IPv6 socket.  Creation is successful if any of the 4 possible ports are created.  Options include:

* `line_port`: The line-oriented port, or 0 to disable.
* `http_port`: The HTTP port, or 0 to disable.
* `https_port`: The HTTP secure port, or 0 to disable.
* `transfer_only`: Do not fail if no ports are created, as the service will be used for transfers.
* `greeting_mode`: controls line-oriented greetings, which allow both line and Websocket traffic to share a port by accepting a "greeting" instead of the usual GET on the HTTP port.
	* `FB_GREETING_OFF`: The line port session starts on connection, and does not use greetings. Greeting the HTTP port is a protocol error.
	* `FB_GREETING_REQUIRED`: The line port waits for a required greeting before starting the session.  Greeting the HTTP port triggers a line session.
	* `FB_GREETING_FALLBACK`: Like `REQUIRED`, but invalid input on the either port initiates a line session.
	* `FB_GREETING_ALLOW`: The line port session starts on connection, but greetings are filtered out of the line protocol. Greeting the HTTP port triggers a line session, other invalid input is a protocol error.
* `greeting`: The greeting text.  Default is "HELO".  To accommodate future enhancements allowing port sharing, it is suggested clients greet with with a requested service name: `HELO pianod`.
* `name`: The name of the service, presently only required in URLs if set.  Future enhancements to Football could allow multiple services to share a port, using 'name' in greeting and URLS to disambiguate.
* `queue_size`: the new connection queue size.  Football regularly accepts new connections, so unless your application is expected to poll infrequently, a small number is fine.
* `context_size`: the size of a per-connection context created for each connection.  Context is created acceptance and destroyed at closure; it is entirely for use by your application.
* `serve_directory`: A path to a directory containing HTML files to be served.
* `locale_directory`: Path to append to serve_directory when looking for localizations.
* `internationalization`: selects internationalization modes via one of the following values:
    * `FB_INTERNATIONALIZATION_NONE`: Do not internationalize.
    * `FB_INTERNATIONALIZATION_EXTENSION`: Look for localizations by appending language to filenames.
    * `FB_INTERNATIONALIZATION_DIRECTORY`: Look for localizations by inserting language, as a directory, between the serve_directory/locale_directory and the HTTP requested path/filename.
* `parent`: A parent fb_service_t structure.  When in use, connections are transferred to the appropriate service based on URL (http://localhost/servicename/) or greeting option (“HELO servicename”).  Greeting and serve directory are inherited from the parent if not set.

### The event model
After initializing and setting up a service, an application will enter a run loop.

	FB_EVENT *fb_wait ();	/* Wait indefinitely */
	FB_EVENT *fb_poll ();	/* Check without waiting */
	FB_EVENT *fb_poll_until (time_t untilwhen); /* Wait until this time */
	FB_EVENT *fb_poll_with_timeout (double timeout); /* Amount of time */

Most applications will simply call one of these, depending on the nature of the application, but it is possible to mix the calls as needed.  These functions *should always* return an event (a timeout being an event too); a NULL response indicates a Football failure.

`FB_EVENT` is a structure exposing:

* `type`—the type of event
* `socket`—To identify your own sockets registered with Football
* `context`—A pointer to the connections context, if you requested
* `command`—statements and greetings received
* `argc`—the number of greeting parameters received
* `argv`—greeting parameters parsed into an argv-style array
* `param_count`—the number of HTTP query parameters received
* `param_names`—the names of the HTTP query parameters, URL decoded
* `param_values`—the values of the HTTP query parameters, URL decoded
* `connection`—a Football connection handle
* `service`—the service that owns the connection

Other things in `FB_EVENT` are private.

### Event types
#### New connections
A new connection creates an `FB_EVENT_CONNECT` event.  Application code can rely on:

* `type`
* `context`, which is initialized with all zero bytes.
* `service` and `connection`
* `param_names` and `param_values` are set, the connection is from a websocket.  `param_names[0]` will indicate connection type (HTTP/HTTPS) and `param_values[0]` will indicate the connected service name.
* If `param_names` is NULL but `argv` is populated, the connection greeted with is line-oriented protocol.
* If both are NULL, the connection is from a file or to an ungreeted port.

It is safe to use the connection at this point.

To close the connection at some later time, use:

	fb_close_connection (struct fb_connection_t *connection)

Note you should avoid closing connections twice.  In debug mode, Football will assert() if you do.  However, the possibility of a race condition exists if a connection is dropped at the other end.  Football will handle this correctly when debugging is disabled (NDEBUG is defined).

#### Command received
When a command has been received, Football parses it and returns `FB_EVENT_INPUT` with the command in both raw and argv-arranged forms.  Application code can rely on:

* `type`
* `context`
* `service` and `connection`
* `command`, `argv`, and `argc`

#### Connection closure
`FB_EVENT_CLOSE` occurs when the connection is closing.  It is still safe to write to the connection—however, there is no guarantee the connection is still open, as it may have have closed from the other end.  Writes to the connection in response to `FB_EVENT_CLOSE` are not guaranteed to complete.  The event includes:

* `type`
* `context`
* `service` and `connection`

#### Timeouts
`FB_EVENT_TIMEOUT` occurs with the Football polling calls except for `fb_wait`.  The event includes only the type.

#### Interruptions
`FB_EVENT_INTERRUPT` indicates `fb_interrupt()` was invoked.  Successive `fb_interrupt` invokations do not queue, and there is no guarantee that the FB_EVENT_INTERRUPT will be the next event returned, only that one will occur in the (near) future.  A signal is required to interrupt a poll; indeed, this mechanism is primarily for signal handlers to force return prior to a timeout.  

#### Service Termination
When service closure is requested, new connections are no longer accepted, all open connections will be closed and flushed, and finally a FB_EVENT_STOPPED event delivered.  The event includes:

* `type`
* `service`

The function `fb_services_are_open()` can be used to determine when the last connection has closed.

#### Transferring Connections
A connection may be transferred from one service to another:

	bool fb_transfer (connection, service)

The return value is true if successful.  No events are generated.

### User socket events

Documentation forthcoming.


### Writing to a connection
Football offers both directed and broadcast messaging.  Writing to a connection guarantees that Football will deliver it, so long as the other end does not close the connection and `FB_EVENT_CLOSE` has not yet been delivered for that connection.  However, Football offers no facilities for verifying deliver; if the other end closes the connection or the network goes down, pending writes are silently dropped.

For simplicity, Football uses `printf`(3)-style formatting, and the write functions are named accordingly:

	int fb_fprintf (void *thing, const char *format, ...);
	int fb_vfprintf (void *thing, const char *format, va_list parameters);
	int fb_bfprintf (void *thing, const char *format, ...);
	int fb_bvfprintf (void *thing, const char *format, va_list parameters);

`fb_fprintf` and `fb_vfprintf` correspond to the non-Football equivalents of the standard library `fprintf` and `vfprintf`, but accept the polymorphic `thing` (either a service, connection, or event) instead of a stream.  If `thing` is a service, the message is broadcast to all open connections on that service; if `thing` is a connection or event, the message is directed.

The "b" variations broadcast to a service, even if a connection or event is specified.


### Iterating Connections
To access all the connections on a service, create an iterator using the service returned from `fb_create_service()`.

	struct fb_iterator_t * fb_new_iterator (struct fb_service_t *);

The iterator is then used to walk through the open connections:

	FB_EVENT *fb_iterate_next (struct fb_iterator_t *);

The event includes:

* `type`, which will be either `FB_EVENT_ITERATOR` if the connection is open, or `FB_EVENT_ITERATOR_CLOSE` if closure has been initiated.
* `context`
* `service` and `connection`

On completion, you must release resources allocated by the iterator:

	void fb_destroy_iterator (fb_iterator_t *);

For example:

	struct fb_iterator_t *it = fb_new_iterator (service);
	if (it) {
		FB_EVENT *event;
		while (event = fb_iterate_next (it)) {
			/* Do something with this connection */
		}
		fb_destroy_iterator (it);
	}

## Football C++ API
The C++ API is rather different from the standard C API.

In C++, you will specialize `Football::Connection` to store your context and provide handlers for events such as new connections.  You will then specialize `Football::Service`, using your connection type as a template parameter:

	class MyConnection : Football::Connection () {
		bool authenticated = false; // Application’s variable
		virtual void newConnection();
		virtual void connectionClose();
	}
	class MyService : public Football::Service<MyConnection> {
		public Settings appSettings; // Application’s data
		MyService (const FB_SERVICE_OPTIONS &options,
		           const Settings &settings) : Service (option) {
			appSettings = settings;
		}
	}

After setting up the types, you will instantiate your service and add interpreters to it:

	FB_SERVICE_OPTIONS options;
	memset (options, 0, sizeof (options));
	options.http_port = 6809; /* e? */
	// … set up remaining options
	MyService svc = new MyService (options);
	svc.addCommands (new UserInterpreter);
	svc.addCommands (new AdminInterpreter);

The main run loop will now use the Arena, which manages all the Football services:

	while (Football::Arena::ready()) {
		// …Do your application things…
		if (Football::Arena::pollWithTimeout (1.0)) {
			// …Do after-input handling…
		} else {
			// …Do timeout handling…
		}
	}

### Event handlers
Implement handlers by providing your own methods.  All handlers have void return values; default implementations reply to the connection with a sane message:

`Football::Connection` event handlers and overrides:

* `newConnection (FB_EVENT *)`
* `connectionClose (FB_EVENT *)`
* `inputReceived (FB_EVENT *)`

`Football::Service` overrides:

* `serviceShutdown()`—This is called when the service has completed shutdown; there are no connections left by the time this is called.  The default implementation does nothing.

### Writing to the connection or service
There are methods to write to a connection or service on both the corresponding objects.  Methods include:

* `printf`—See football API documentation for details on this and other printf variants.  Instead of `fb_fprintf (destination, “format string %s\n”, value)`, you will `destination.printf (“format string%s\n”, value)`
* `vprintf`
* `bprintf`
* `bvprintf`
* `connection << string`—for example: `destination << “format string “ << value << “\n”`

### Connection/Event accessors (“Getters”)

* `const char *command (void)`—to get the raw command line

### Action methods
To manipulate the connection, use the methods provided by Football::Connection:

* `close()`
* `acceptInput (bool enable)`—enables or disables reading from the connection.
* `bool Connection::transfer (ServiceBase *newservice, bool invokeNewConnectionHandler)`—Transfers the connection to another service.  If invokeNewConnectionHandler, the newConnection() is invoked with a null event pointer after the transfer.

Actions on Football::Service:

* `close()`—This initiates closure of a service.  This disables listeners and initiates closure of all open connections.  When complete, `serviceShutdown()` is called.  When the last service has completed closing, `Football::Arena::ready()` returns `false` and your runloop will exit.
* `newConnectionFromFile (name)`—Creates a simulated connection that  reads from the specified file.  Writes to the connection are discarded.

## Command Parser
Earlier versions of Football included a command line parser, and the C++ wrapper included a disptacher that directed commands to related interpreter modules.  In `pianod`, Football's home project, this was superceded by Parsnip in r343.  See earlier versions of `pianod` if you need the parser.

## Localization
When internationalization is enabled in the service options, the HTTP server will try to find language-specific versions of files based on languages specified in the HTTP request.  It looks at all languages requested, in user preference.  If none of these are available, it tries again with truncated language identifiers (en_US, for example, becomes simply en).  If these variations are also not found, the unlocalized version of the file (or a 404 if not found) is served.

## Logging
Football's internal logger sends "real" errors (failure within Football) to standard error.  This does not include "expected" errors such as a connection terminating unexpectedly.

Other events use categories.  Each event has 1 or more categories associated with it.  By default, only "real" errors are logged; additional categories can be logged with:

	void fb_set_logging (category_bitmask, log_function)

The `category_bitmask` indicates the categories to log.  Multiply-categorized events are logged if _any_ category matches.  See `fb_service.h` for current bitmask values.  Note connection, TLS and HTTP error categories log connection failures such as unexpected close, bad TLS handshake, or invalid HTTP request.

The `log_function` parameter is normally NULL, but if Football's internal log function does not meet your needs, you can supply your own function here.  The function prototype varies depending on whether `NDEBUG` is set (as used by `assert(3)`).

Without NDEBUG:
	log_function (char *file, int *line, char *function,
	              int category, char *format, ...)

With NDEBUG:
	log_function (int category, char *format, ...)

When using a custom log function, all events are passed to it regardless of the categories set by `fb_set_logging`.  `format` is printf-style, with parameters to follow.
