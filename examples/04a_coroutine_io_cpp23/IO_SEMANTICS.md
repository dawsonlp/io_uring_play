# I/O Semantics

This document defines the public byte-stream, EOF, error, and socket-flag
contracts for the C++23 runtime.

## Primitive transfers

`TcpStream::send()` represents one io_uring send operation. Success reports the
number of bytes accepted by that operation, which may be less than the supplied
buffer. It does not imply delivery to the peer. TCP sends use `MSG_NOSIGNAL` by
default so a closed peer is reported as an error rather than terminating the
process with `SIGPIPE`.

`TcpStream::receive()` represents one receive operation. A positive result is the
number of bytes received. Zero is an orderly peer shutdown and is the TCP EOF
signal. A reset is `Error::Code::ConnectionClosed`, not EOF.

File `read()` also uses a successful zero-byte result for EOF. File `write()` and
TCP `send()` return the bytes transferred by one operation and may be partial.

## Complete sends

`TcpStream::send_all()` repeatedly awaits `send()` until the entire buffer has
been accepted or an error occurs. It retries `Interrupted`. A failure can occur
after a prefix has already been sent; the current error type deliberately does
not report that partial-progress count. Cancellation is returned immediately.

## Error mapping

- `EINTR` -> `Interrupted`
- `EAGAIN` / `EWOULDBLOCK` -> `WouldBlock`
- `ECONNRESET` / `EPIPE` -> `ConnectionClosed`
- `ECANCELED` -> `Cancelled`
- Invalid public descriptors or arguments -> `InvalidArgument`

Primitive operations expose these results. Composed operations may retry only
errors explicitly documented by that operation.

## Listener and accepted sockets

Listeners are nonblocking and close-on-exec. Accept requests explicitly use
`SOCK_NONBLOCK | SOCK_CLOEXEC`; accepted sockets do not rely on flag inheritance.
Backlog must be positive. Binding port zero reports the kernel-assigned port.

IPv4 and IPv6 listeners are selected with `AddressFamily`. IPv6 listeners set
`IPV6_V6ONLY`, making address-family behavior explicit rather than host-default
dependent.

## Close policy

TCP close is synchronous ownership release through shared `DescriptorState`.
Actual descriptor closure is deferred until in-flight operations release their
references. A raw public async-close awaitable would violate this ownership model,
so `CloseAwaitable` is not part of the API.
