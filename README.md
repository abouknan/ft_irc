*This project has been created as part of the 42 curriculum by abouknan, msabir, &lt;login3&gt;.*

# ft_irc

## Description

`ircserv` is an IRC server written from scratch in C++ 98. It speaks enough of
the RFC 1459 / 2812 protocol for a real IRC client to connect to it, register,
join channels, talk in public and in private, and for channel operators to
administrate their channels.

The whole server is a **single process, a single thread and a single `poll()`
call**. There is no fork, no thread and no blocking read anywhere: every socket
is put in non-blocking mode with `fcntl(fd, F_SETFL, O_NONBLOCK)`, and `recv()`
or `send()` are only ever called on a descriptor that `poll()` has just
reported as ready.

Two consequences drive the whole design:

* **Data arrives in pieces.** A single `recv()` can return half a command, two
  commands, or a command and half of the next one. Every client therefore owns
  an input buffer, and a command is only executed once a complete `\n`
  terminated line has been reassembled.
* **Writes can be refused.** `send()` may accept only part of what we hand it,
  so nothing in the project calls `send()` directly. Commands push text into
  `Client::queue()`, and the main loop flushes each output buffer when `poll()`
  says the socket is writable.

### Implemented features

| Area | Commands |
|---|---|
| Registration | `PASS`, `NICK`, `USER`, `CAP` (answered, no capability offered) |
| Connection | `PING`, `PONG`, `QUIT` |
| Messaging | `PRIVMSG`, `NOTICE` (to a user or to a channel, several targets allowed) |
| Channels | `JOIN`, `PART`, `TOPIC`, `NAMES`, `INVITE`, `KICK` |
| Operators | `MODE` with `+i`, `+t`, `+k`, `+o`, `+l` and their `-` counterparts |

Errors are reported with the standard numeric replies (`401`, `403`, `404`,
`421`, `431`, `432`, `433`, `441`, `442`, `443`, `451`, `461`, `462`, `464`,
`471`, `472`, `473`, `475`, `476`, `482`).

As the subject requires, there is **no IRC client** and **no server to server
communication** in this project.

## Instructions

### Build

```sh
make            # builds ./ircserv
make clean      # removes the object files
make fclean     # removes the object files and the binary
make re         # fclean + make
```

The code compiles with `c++ -Wall -Wextra -Werror -std=c++98` and uses no
external library.

### Run

```sh
./ircserv <port> <password>
```

* `port` — the TCP port to listen on, 1 to 65535.
* `password` — the password every client must send with `PASS` before it can
  register.

Example:

```sh
./ircserv 6667 mypassword
```

### Connect

Reference client: **irssi**.

```sh
irssi
/connect 127.0.0.1 6667 mypassword
/join #42
/msg #42 hello
```

Any other client works the same way, and so does raw `netcat`:

```sh
nc -C 127.0.0.1 6667
PASS mypassword
NICK alice
USER alice 0 * :Alice
JOIN #42
PRIVMSG #42 :hello
```

## Project layout

```
include/                  every header
    Server.hpp            the listening socket, the poll loop, all the clients
    Client.hpp            one connection: its identity and its two byte buffers
    Channel.hpp           membership, modes, topic
    Message.hpp           one parsed IRC line
    MessageParser.hpp     text -> Message, and nothing else
    CommandDispatcher.hpp command name -> handler
    commands.hpp          the signature every handler shares
    replies.hpp           every numeric reply, in one place
    Utils.hpp             small shared helpers

src/
    main.cpp              argument checking, then Server::run()
    Server.cpp            accept / recv / send around the single poll()
    Client.cpp            line reassembly and output queueing
    Channel.cpp           channel state, no protocol logic
    Message.cpp
    MessageParser.cpp
    CommandDispatcher.cpp
    Utils.cpp
    commands/
        Registration.cpp  CAP PASS NICK USER QUIT PING PONG
        Messaging.cpp     PRIVMSG NOTICE
        Channels.cpp      JOIN PART TOPIC KICK INVITE NAMES
        Modes.cpp         MODE
```

## Technical choices

* **`poll()` rather than `epoll()` or `select()`.** The subject allows any
  equivalent, but `poll()` is the one it names, it exists identically on Linux
  and macOS so the project builds on both, and unlike `select()` it has no
  `FD_SETSIZE` limit. The set of watched descriptors is rebuilt from scratch
  before every call, which is cheap at this scale and makes it impossible to
  leave a stale descriptor behind.
* **Channel stores, handlers decide.** `Channel` only holds state and answers
  questions such as `isOperator()` or `isFull()`. Whether an action is allowed,
  and which numeric answers a refusal, lives entirely in the command handlers.
  Keeping the rules of the protocol in one place avoids two versions of the
  same rule drifting apart.
* **Handlers never touch the socket.** They only call `Client::queue()`. That
  is what guarantees the "no `send()` outside `poll()`" rule holds no matter
  which command is added later.
* **Closing is deferred.** `Server::disconnect()` does not close the socket. It
  announces the departure, queues a last `ERROR` line and marks the client;
  the main loop finishes flushing the output buffer and only then destroys the
  `Client`. Without this, the last numeric a rejected client is sent (`464`,
  for instance) would be lost.
* **Case insensitivity.** Nicknames and channel names are compared through
  `toIrcLower()`, which implements the RFC 1459 rule where `[`, `]` and `\`
  are the upper case forms of `{`, `}` and `|`.

## Resources

* RFC 1459 — Internet Relay Chat Protocol: https://datatracker.ietf.org/doc/html/rfc1459
* RFC 2812 — IRC Client Protocol: https://datatracker.ietf.org/doc/html/rfc2812
* Modern IRC client protocol documentation: https://modern.ircdocs.horse/
* IRC numeric replies reference: https://defs.ircdocs.horse/defs/numerics.html
* Beej's Guide to Network Programming: https://beej.us/guide/bgnet/
* `man 2 poll`, `man 2 socket`, `man 2 accept`, `man 2 recv`, `man 2 send`,
  `man 2 fcntl`

### Use of AI

AI (Claude) was used on this project for the following, and only the
following:

* **Reorganising the repository.** Headers were scattered between the root,
  `channel/` and `include/`; they were regrouped under `include/` and the
  sources under `src/`, and a `Makefile` with proper dependency tracking was
  written.
* **Completing the command layer.** The dispatcher and the parser already
  existed; the handlers behind them (`PASS`, `NICK`, `USER`, `JOIN`, `MODE`,
  `KICK`, ...) and the numeric replies in `replies.hpp` were written with AI
  assistance from the RFC.
* **Fixing the `Channel` class**, which did not compile: an undeclared `op`
  variable, a `,`-with-`continue` expression that is not valid C++, and a
  missing include of `Client.hpp`.
* **Reviewing the event loop** for the cases that matter in this subject:
  partial `recv()`, partial `send()`, and destroying a client without leaving
  a dangling pointer in a channel.

Every part of the result was read, tested and is understood by the team. The
protocol decisions were checked against RFC 1459 / 2812 rather than taken on
trust, and the server was tested with a reference client and with raw `nc`,
including the packet splitting scenario described in the subject.
