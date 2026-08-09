/* ************************************************************************** */
/*                                                                            */
/*   Server.hpp                                                               */
/*                                                                            */
/*   The whole server: one listening socket, one poll() call, every Client    */
/*   and every Channel.                                                       */
/*                                                                            */
/*   The subject is strict on two points and the design follows them:         */
/*                                                                            */
/*     - every descriptor is non blocking (fcntl F_SETFL O_NONBLOCK),         */
/*     - recv() and send() are only ever called on a descriptor that the      */
/*       single poll() just reported as ready.                                */
/*                                                                            */
/*   Nothing else in the project calls recv() or send(): handlers push text   */
/*   into Client::queue() and the loop below flushes it on POLLOUT.           */
/*                                                                            */
/*   Server owns every Client and every Channel and is the only place that    */
/*   deletes them.                                                            */
/*                                                                            */
/* ************************************************************************** */

#ifndef SERVER_HPP
# define SERVER_HPP

# include <map>
# include <poll.h>
# include <set>
# include <string>
# include <vector>

# include "Channel.hpp"
# include "Client.hpp"
# include "CommandDispatcher.hpp"

class Server
{
	private:

		int								_port;
		std::string						_password;
		int								_listenFd;
		std::string						_createdAt;

		/* Rebuilt from scratch before every poll(): one entry for the
		   listening socket, one per client. Cheap, and it makes it impossible
		   to leave a stale descriptor behind. */
		std::vector<struct pollfd>		_pollFds;

		std::map<int, Client*>			_clients;

		/* Keyed by the lower cased name, because "#Foo" and "#foo" are the
		   same channel in IRC. */
		std::map<std::string, Channel*>	_channels;

		std::set<int>					_pendingClose;
		CommandDispatcher				_dispatcher;

		Server(const Server& other);
		Server&	operator=(const Server& other);

		void	buildPollSet();
		void	acceptClient();
		void	readFrom(Client& client);
		void	writeTo(Client& client);
		void	handleLine(Client& client, const std::string& line);
		void	closePending();
		void	destroyClient(int fd);

	public:

		Server(int port, const std::string& password);
		~Server();

		/* socket / setsockopt / fcntl / bind / listen. Throws on failure. */
		void	setupListener();

		/* The single poll() loop. Returns when a signal asks us to stop. */
		void	run();

		const std::string&	getPassword() const;
		const std::string&	getCreatedAt() const;

		/* Case insensitive lookup, NULL when nobody uses that nick. */
		Client*				findClientByNick(const std::string& nick);

		/* Case insensitive lookup, NULL when the channel does not exist. */
		Channel*			findChannel(const std::string& name);

		/* Returns the existing channel or creates an empty one. */
		Channel*			getOrCreateChannel(const std::string& name);

		/* Called after a member left: an empty channel stops existing, along
		   with the modes and topic that were set on it. */
		void				dropChannelIfEmpty(Channel* channel);

		/* Sends 001-004 and the MOTD. Called once PASS, NICK and USER are in. */
		void				welcome(Client& client);

		/* Polite hang up: tells the channels the user is gone, queues a last
		   ERROR line, then lets the loop flush and close the socket. The
		   Client object stays alive until then, so callers may keep using the
		   reference for the rest of their function. */
		void				disconnect(Client& client, const std::string& reason);

		/* Removes the client from every channel and delivers `quitMessage`
		   once to each user that shared a channel with it. */
		void				broadcastQuit(Client& client, const std::string& quitMessage);

		/* Delivers `message` exactly once to every user sharing at least one
		   channel with `client`. Used by NICK, where two users in three
		   common channels must still see a single rename. */
		void				broadcastToPeers(Client& client, const std::string& message,
								bool includeSelf);
};

#endif
