My simple implementation of HTTP proxy server.

# Details
* Kqueue is used for asynchronous writing and reading messages from sockets.
* Server supports HTTP caching (only ETag).
* Every client will be disconnected from server after 10 minutes doing nothing.
* Multithreading added.

# System requirements
* Operating system: FreeBSD or MacOS
