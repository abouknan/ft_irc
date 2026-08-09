/* ************************************************************************** */
/*                                                                            */
/*   Messaging.cpp                                                            */
/*                                                                            */
/*   PRIVMSG and NOTICE.                                                      */
/*                                                                            */
/*   They do exactly the same job with one difference the RFC insists on:     */
/*   a NOTICE must never trigger an automatic reply, error numerics           */
/*   included. That is what stops two bots from answering each other forever. */
/*                                                                            */
/* ************************************************************************** */

#include <vector>

#include "Channel.hpp"
#include "Client.hpp"
#include "Message.hpp"
#include "Server.hpp"
#include "Utils.hpp"
#include "commands.hpp"
#include "replies.hpp"

static bool	isChannelTarget(const std::string& target)
{
	return (!target.empty() && (target[0] == '#' || target[0] == '&'));
}

static void	deliverToChannel(Server& srv, Client& c, const std::string& target,
			const std::string& line, bool quiet)
{
	Channel*	channel = srv.findChannel(target);

	if (channel == NULL)
	{
		if (!quiet)
			c.queue(errNoSuchChannel(c.getNick(), target));
		return ;
	}
	/* Only members may talk to a channel, otherwise anybody could shout into
	   any room without ever joining it. */
	if (!channel->isMember(c))
	{
		if (!quiet)
			c.queue(errCannotSendToChan(c.getNick(), channel->getName()));
		return ;
	}
	/* Everyone but the author: a client already shows what it just typed. */
	channel->broadcast(line, &c);
}

static void	deliverToUser(Server& srv, Client& c, const std::string& target,
			const std::string& line, bool quiet)
{
	Client*	receiver = srv.findClientByNick(target);

	if (receiver == NULL)
	{
		if (!quiet)
			c.queue(errNoSuchNick(c.getNick(), target));
		return ;
	}
	receiver->queue(line);
}

static void	deliver(Server& srv, Client& c, const Message& m,
			const std::string& command)
{
	bool						quiet = (command == "NOTICE");
	std::vector<std::string>	targets;
	std::size_t					i;

	if (m.argc() < 1 || m.arg(0).empty())
	{
		if (!quiet)
			c.queue(errNoRecipient(c.getNick(), command));
		return ;
	}
	if (m.argc() < 2 || m.arg(1).empty())
	{
		if (!quiet)
			c.queue(errNoTextToSend(c.getNick()));
		return ;
	}

	/* "PRIVMSG #a,#b,carol :hi" is one command with three destinations. */
	targets = split(m.arg(0), ',');
	for (i = 0; i < targets.size(); ++i)
	{
		std::string	line = ":" + c.getPrefix() + " " + command + " "
			+ targets[i] + " :" + m.arg(1) + "\r\n";

		if (isChannelTarget(targets[i]))
			deliverToChannel(srv, c, targets[i], line, quiet);
		else
			deliverToUser(srv, c, targets[i], line, quiet);
	}
}

void	cmdPrivmsg(Server& srv, Client& c, const Message& m)
{
	deliver(srv, c, m, "PRIVMSG");
}

void	cmdNotice(Server& srv, Client& c, const Message& m)
{
	deliver(srv, c, m, "NOTICE");
}

