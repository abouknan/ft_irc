/* ************************************************************************** */
/*                                                                            */
/*   Server.cpp                                                               */
/*                                                                            */
/* ************************************************************************** */

#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "Message.hpp"
#include "MessageParser.hpp"
#include "Server.hpp"
#include "Utils.hpp"
#include "replies.hpp"

/* Set by the signal handler, read by the poll loop. sig_atomic_t is the only
   type the standard allows us to touch from a handler. */
static volatile sig_atomic_t	g_stop = 0;

static void	handleSignal(int)
{
	g_stop = 1;
}

Server::Server(int port, const std::string& password)
	: _port(port),
	  _password(password),
	  _listenFd(-1),
	  _createdAt(),
	  _pollFds(),
	  _clients(),
	  _channels(),
	  _pendingClose(),
	  _dispatcher()
{
	std::time_t	now = std::time(NULL);
	std::string	text(std::ctime(&now));

	/* ctime() ends with a newline that would break the numeric. */
	if (!text.empty() && text[text.size() - 1] == '\n')
		text.erase(text.size() - 1);
	_createdAt = text;
}

Server::~Server()
{
	std::map<int, Client*>::iterator			it = _clients.begin();
	std::map<std::string, Channel*>::iterator	ch = _channels.begin();

	while (it != _clients.end())
	{
		delete it->second;
		++it;
	}
	while (ch != _channels.end())
	{
		delete ch->second;
		++ch;
	}
	if (_listenFd >= 0)
		close(_listenFd);
}

void	Server::setupListener()
{
	struct sockaddr_in	address;
	int					reuse = 1;

	_listenFd = socket(AF_INET, SOCK_STREAM, 0);
	if (_listenFd < 0)
		throw std::runtime_error("cannot create the listening socket");

	/* Without SO_REUSEADDR the port stays blocked for a couple of minutes
	   after a restart, which makes testing painful. */
	if (setsockopt(_listenFd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
		throw std::runtime_error("setsockopt failed");
	if (fcntl(_listenFd, F_SETFL, O_NONBLOCK) < 0)
		throw std::runtime_error("cannot set the socket non blocking");

	std::memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(static_cast<uint16_t>(_port));

	if (bind(_listenFd, reinterpret_cast<struct sockaddr*>(&address),
			sizeof(address)) < 0)
		throw std::runtime_error("bind failed (port already in use?)");
	if (listen(_listenFd, SOMAXCONN) < 0)
		throw std::runtime_error("listen failed");

	std::cout << SERVER_NAME << " listening on port " << _port << std::endl;
}

const std::string&	Server::getPassword() const
{
	return (_password);
}

const std::string&	Server::getCreatedAt() const
{
	return (_createdAt);
}

/* -------------------------------------------------------------------------- */
/*  Lookups                                                                    */
/* -------------------------------------------------------------------------- */

Client*	Server::findClientByNick(const std::string& nick)
{
	std::string						wanted = toIrcLower(nick);
	std::map<int, Client*>::iterator	it = _clients.begin();

	while (it != _clients.end())
	{
		if (!it->second->isClosing()
			&& toIrcLower(it->second->getNick()) == wanted)
			return (it->second);
		++it;
	}
	return (NULL);
}

Channel*	Server::findChannel(const std::string& name)
{
	std::map<std::string, Channel*>::iterator	it = _channels.find(toIrcLower(name));

	if (it == _channels.end())
		return (NULL);
	return (it->second);
}

Channel*	Server::getOrCreateChannel(const std::string& name)
{
	Channel*	channel = findChannel(name);

	if (channel != NULL)
		return (channel);
	channel = new Channel(name);
	_channels.insert(std::make_pair(toIrcLower(name), channel));
	return (channel);
}

void	Server::dropChannelIfEmpty(Channel* channel)
{
	if (channel == NULL || !channel->isEmpty())
		return ;
	_channels.erase(toIrcLower(channel->getName()));
	delete channel;
}

/* -------------------------------------------------------------------------- */
/*  Registration and disconnection                                             */
/* -------------------------------------------------------------------------- */

void	Server::welcome(Client& client)
{
	const std::string&	nick = client.getNick();

	client.setWelcomed(true);
	client.queue(rplWelcome(nick, client.getPrefix()));
	client.queue(rplYourHost(nick));
	client.queue(rplCreated(nick, _createdAt));
	client.queue(rplMyInfo(nick));
	client.queue(rplMotdStart(nick));
	client.queue(rplMotd(nick, "Welcome to a 42 ft_irc server."));
	client.queue(rplEndOfMotd(nick));
	std::cout << "[+] " << client.getPrefix() << " registered" << std::endl;
}

void	Server::broadcastQuit(Client& client, const std::string& quitMessage)
{
	std::set<Client*>							notify;
	std::vector<Channel*>						emptied;
	std::map<std::string, Channel*>::iterator	it = _channels.begin();

	while (it != _channels.end())
	{
		Channel*	channel = it->second;

		++it;
		if (!channel->isMember(client))
		{
			channel->removeInvite(client);
			continue ;
		}
		channel->removeMember(client);
		/* Collect instead of sending right away: a user sharing two channels
		   with us must still receive exactly one QUIT. */
		notify.insert(channel->getMembers().begin(), channel->getMembers().end());
		if (channel->isEmpty())
			emptied.push_back(channel);
	}

	/* An empty message means "just leave the channels quietly", which is what
	   we want for a client that never finished registering. */
	if (!quitMessage.empty())
	{
		for (std::set<Client*>::iterator target = notify.begin();
			 target != notify.end(); ++target)
			(*target)->queue(quitMessage);
	}

	for (std::vector<Channel*>::iterator dead = emptied.begin();
		 dead != emptied.end(); ++dead)
		dropChannelIfEmpty(*dead);
}

void	Server::broadcastToPeers(Client& client, const std::string& message,
			bool includeSelf)
{
	std::set<Client*>							notify;
	std::map<std::string, Channel*>::iterator	it = _channels.begin();

	while (it != _channels.end())
	{
		if (it->second->isMember(client))
			notify.insert(it->second->getMembers().begin(),
				it->second->getMembers().end());
		++it;
	}
	notify.erase(&client);
	if (includeSelf)
		notify.insert(&client);
	for (std::set<Client*>::iterator target = notify.begin();
		 target != notify.end(); ++target)
		(*target)->queue(message);
}

void	Server::disconnect(Client& client, const std::string& reason)
{
	if (client.isClosing())
		return ;
	if (client.isWelcomed())
		broadcastQuit(client, ":" + client.getPrefix() + " QUIT :" + reason + "\r\n");
	else
		broadcastQuit(client, "");
	client.queue(errorLine(reason));
	/* From here on nothing may be queued any more. The loop keeps the socket
	   open until _outBuffer is flushed, then closes it. */
	client.setClosing(true);
}

void	Server::destroyClient(int fd)
{
	std::map<int, Client*>::iterator	it = _clients.find(fd);
	std::string							quit;

	if (it == _clients.end())
		return ;

	Client*	client = it->second;

	/* disconnect() already announced a clean QUIT. A socket that simply died
	   never went through it, so the announcement happens here instead. */
	if (client->isWelcomed() && !client->isClosing())
		quit = ":" + client->getPrefix() + " QUIT :Connection closed\r\n";

	/* Also removes the Client from every channel, so no channel is left with
	   a pointer to the object we are about to delete. */
	broadcastQuit(*client, quit);

	std::cout << "[-] client " << fd << " disconnected" << std::endl;
	delete client;
	_clients.erase(it);
}

void	Server::closePending()
{
	std::set<int>::iterator	it = _pendingClose.begin();

	while (it != _pendingClose.end())
	{
		destroyClient(*it);
		++it;
	}
	_pendingClose.clear();
}

/* -------------------------------------------------------------------------- */
/*  poll() plumbing                                                            */
/* -------------------------------------------------------------------------- */

void	Server::buildPollSet()
{
	struct pollfd						entry;
	std::map<int, Client*>::iterator		it = _clients.begin();

	_pollFds.clear();

	std::memset(&entry, 0, sizeof(entry));
	entry.fd = _listenFd;
	entry.events = POLLIN;
	_pollFds.push_back(entry);

	while (it != _clients.end())
	{
		Client*	client = it->second;

		std::memset(&entry, 0, sizeof(entry));
		entry.fd = client->getFd();
		/* A client on its way out is not read from any more, we only wait for
		   its last bytes to leave. */
		if (!client->isClosing())
			entry.events |= POLLIN;
		if (client->hasOut())
			entry.events |= POLLOUT;
		if (entry.events != 0)
			_pollFds.push_back(entry);
		++it;
	}
}

void	Server::acceptClient()
{
	struct sockaddr_in	address;
	socklen_t			length = sizeof(address);
	char				host[INET_ADDRSTRLEN];
	int					fd;

	std::memset(&address, 0, sizeof(address));
	fd = accept(_listenFd, reinterpret_cast<struct sockaddr*>(&address), &length);
	if (fd < 0)
		return ;
	if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
	{
		close(fd);
		return ;
	}

	std::memset(host, 0, sizeof(host));
	if (inet_ntop(AF_INET, &address.sin_addr, host, sizeof(host)) == NULL)
		std::strcpy(host, "unknown");

	Client*	client = new Client(fd);

	client->setHost(std::string(host));
	_clients.insert(std::make_pair(fd, client));
	std::cout << "[+] client " << fd << " connected from " << host << std::endl;
}

void	Server::readFrom(Client& client)
{
	char		buffer[4096];
	ssize_t		received;
	std::string	line;

	received = recv(client.getFd(), buffer, sizeof(buffer), 0);
	/* 0 means the peer closed, negative means the socket is broken: poll()
	   told us it was readable, so there is no EAGAIN case to handle here. */
	if (received <= 0)
	{
		_pendingClose.insert(client.getFd());
		return ;
	}
	if (!client.appendIn(buffer, static_cast<std::size_t>(received)))
	{
		disconnect(client, "Input line too long");
		return ;
	}
	/* One recv() can carry several commands, half a command, or both. Only
	   complete lines come out of takeLine(). */
	while (!client.isClosing() && client.takeLine(line))
		handleLine(client, line);
}

void	Server::writeTo(Client& client)
{
	ssize_t	sent;

	if (!client.hasOut())
		return ;
	sent = send(client.getFd(), client.outData().c_str(), client.outData().size(), 0);
	if (sent <= 0)
	{
		_pendingClose.insert(client.getFd());
		return ;
	}
	/* send() is free to accept only part of the buffer: keep the rest for the
	   next time poll() says the socket is writable. */
	client.consumeOut(static_cast<std::size_t>(sent));
}

void	Server::handleLine(Client& client, const std::string& line)
{
	Message	message = MessageParser::parse(line);

	_dispatcher.dispatch(*this, client, message);
}

/* -------------------------------------------------------------------------- */
/*  Main loop                                                                  */
/* -------------------------------------------------------------------------- */

void	Server::run()
{
	std::signal(SIGINT, handleSignal);
	std::signal(SIGTERM, handleSignal);
	std::signal(SIGQUIT, handleSignal);

	while (!g_stop)
	{
		buildPollSet();

		int	ready = poll(&_pollFds[0], _pollFds.size(), -1);

		if (ready < 0)
		{
			/* A signal interrupted the wait: loop again and let the while
			   condition notice that we have to stop. */
			if (errno == EINTR)
				continue ;
			break ;
		}

		for (std::size_t i = 0; i < _pollFds.size(); ++i)
		{
			short	events = _pollFds[i].revents;
			int		fd = _pollFds[i].fd;

			if (events == 0)
				continue ;
			if (fd == _listenFd)
			{
				if (events & POLLIN)
					acceptClient();
				continue ;
			}

			std::map<int, Client*>::iterator	it = _clients.find(fd);

			if (it == _clients.end())
				continue ;

			Client&	client = *it->second;

			if (events & (POLLERR | POLLHUP | POLLNVAL))
			{
				_pendingClose.insert(fd);
				continue ;
			}
			if (events & POLLIN)
				readFrom(client);
			/* readFrom() may have scheduled this very client for closing. */
			if (_pendingClose.count(fd) != 0)
				continue ;
			if (events & POLLOUT)
				writeTo(client);
		}

		/* A client that asked to leave and has nothing left to send can go. */
		for (std::map<int, Client*>::iterator it = _clients.begin();
			 it != _clients.end(); ++it)
		{
			if (it->second->isClosing() && !it->second->hasOut())
				_pendingClose.insert(it->first);
		}
		closePending();
	}
	std::cout << "\nshutting down" << std::endl;
}
