/* ************************************************************************** */
/*                                                                            */
/*   Channels.cpp                                                             */
/*                                                                            */
/*   JOIN, PART, TOPIC, KICK, INVITE, NAMES.                                  */
/*                                                                            */
/*   Channel itself only stores state; every rule of who may do what and      */
/*   which numeric answers a refusal lives here.                              */
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

void	sendNames(Client& c, Channel& channel)
{
	c.queue(rplNamReply(c.getNick(), channel.getName(), channel.namesList()));
	c.queue(rplEndOfNames(c.getNick(), channel.getName()));
}

/* RPL_TOPIC when there is one, RPL_NOTOPIC when there is not: a client that
   joins expects exactly one of the two. */
static void	sendTopic(Client& c, Channel& channel)
{
	if (channel.hasTopic())
		c.queue(rplTopic(c.getNick(), channel.getName(), channel.getTopic()));
	else
		c.queue(rplNoTopic(c.getNick(), channel.getName()));
}

static void	joinOne(Server& srv, Client& c, const std::string& name,
			const std::string& key)
{
	Channel*	channel;
	bool		created;

	if (!isValidChannelName(name))
	{
		c.queue(errBadChanMask(c.getNick(), name));
		return ;
	}
	channel = srv.findChannel(name);
	created = (channel == NULL);
	if (created)
		channel = srv.getOrCreateChannel(name);
	/* Joining twice is not an error, there is simply nothing to do. */
	if (channel->isMember(c))
		return ;

	/* A channel that did not exist a line ago has no mode to check. */
	if (!created)
	{
		if (channel->isInviteOnly() && !channel->isInvited(c))
		{
			c.queue(errInviteOnlyChan(c.getNick(), channel->getName()));
			return ;
		}
		if (channel->hasKey() && channel->getKey() != key)
		{
			c.queue(errBadChannelKey(c.getNick(), channel->getName()));
			return ;
		}
		if (channel->isFull())
		{
			c.queue(errChannelIsFull(c.getNick(), channel->getName()));
			return ;
		}
	}

	channel->addMember(c);
	/* Whoever opens a channel runs it. */
	if (created)
		channel->addOperator(c);

	/* Sent to the whole channel, the new member included: that echo is how a
	   client knows its own JOIN succeeded. */
	channel->broadcast(":" + c.getPrefix() + " JOIN " + channel->getName()
		+ "\r\n", NULL);
	sendTopic(c, *channel);
	sendNames(c, *channel);
}

void	cmdJoin(Server& srv, Client& c, const Message& m)
{
	std::vector<std::string>	names;
	std::vector<std::string>	keys;
	std::size_t					i;

	if (m.argc() < 1 || m.arg(0).empty())
	{
		c.queue(errNeedMoreParams(c.getNick(), "JOIN"));
		return ;
	}
	/* "JOIN #a,#b key" pairs the nth channel with the nth key. */
	names = split(m.arg(0), ',');
	keys = split(m.arg(1), ',');
	for (i = 0; i < names.size(); ++i)
		joinOne(srv, c, names[i], i < keys.size() ? keys[i] : std::string());
}

void	cmdPart(Server& srv, Client& c, const Message& m)
{
	std::vector<std::string>	names;
	std::string					reason;
	std::size_t					i;

	if (m.argc() < 1 || m.arg(0).empty())
	{
		c.queue(errNeedMoreParams(c.getNick(), "PART"));
		return ;
	}
	reason = m.arg(1);
	names = split(m.arg(0), ',');
	for (i = 0; i < names.size(); ++i)
	{
		Channel*	channel = srv.findChannel(names[i]);
		std::string	line;

		if (channel == NULL)
		{
			c.queue(errNoSuchChannel(c.getNick(), names[i]));
			continue ;
		}
		if (!channel->isMember(c))
		{
			c.queue(errNotOnChannel(c.getNick(), channel->getName()));
			continue ;
		}
		line = ":" + c.getPrefix() + " PART " + channel->getName();
		if (!reason.empty())
			line += " :" + reason;
		line += "\r\n";
		/* Broadcast before removing, so the leaver sees its own PART. */
		channel->broadcast(line, NULL);
		channel->removeMember(c);
		srv.dropChannelIfEmpty(channel);
	}
}

void	cmdTopic(Server& srv, Client& c, const Message& m)
{
	Channel*	channel;

	if (m.argc() < 1 || m.arg(0).empty())
	{
		c.queue(errNeedMoreParams(c.getNick(), "TOPIC"));
		return ;
	}
	channel = srv.findChannel(m.arg(0));
	if (channel == NULL)
	{
		c.queue(errNoSuchChannel(c.getNick(), m.arg(0)));
		return ;
	}
	if (!channel->isMember(c))
	{
		c.queue(errNotOnChannel(c.getNick(), channel->getName()));
		return ;
	}
	/* No second parameter at all: this is a question, not an order. */
	if (m.argc() < 2)
	{
		sendTopic(c, *channel);
		return ;
	}
	if (channel->isTopicLocked() && !channel->isOperator(c))
	{
		c.queue(errChanOPrivsNeeded(c.getNick(), channel->getName()));
		return ;
	}
	/* "TOPIC #chan :" with an empty trailing clears the topic. */
	channel->setTopic(m.arg(1));
	channel->broadcast(":" + c.getPrefix() + " TOPIC " + channel->getName()
		+ " :" + m.arg(1) + "\r\n", NULL);
}

void	cmdKick(Server& srv, Client& c, const Message& m)
{
	Channel*					channel;
	std::vector<std::string>	targets;
	std::string					comment;
	std::size_t					i;

	if (m.argc() < 2)
	{
		c.queue(errNeedMoreParams(c.getNick(), "KICK"));
		return ;
	}
	channel = srv.findChannel(m.arg(0));
	if (channel == NULL)
	{
		c.queue(errNoSuchChannel(c.getNick(), m.arg(0)));
		return ;
	}
	if (!channel->isMember(c))
	{
		c.queue(errNotOnChannel(c.getNick(), channel->getName()));
		return ;
	}
	if (!channel->isOperator(c))
	{
		c.queue(errChanOPrivsNeeded(c.getNick(), channel->getName()));
		return ;
	}
	/* Without a reason the RFC uses the kicker's nick as the comment. */
	comment = m.arg(2);
	if (comment.empty())
		comment = c.getNick();

	targets = split(m.arg(1), ',');
	for (i = 0; i < targets.size(); ++i)
	{
		Client*	victim = srv.findClientByNick(targets[i]);

		if (victim == NULL || !channel->isMember(*victim))
		{
			c.queue(errUserNotInChannel(c.getNick(), targets[i],
				channel->getName()));
			continue ;
		}
		channel->broadcast(":" + c.getPrefix() + " KICK " + channel->getName()
			+ " " + victim->getNick() + " :" + comment + "\r\n", NULL);
		channel->removeMember(*victim);
	}
	/* Only once the loop is over: dropping the channel earlier would delete
	   the object the next iteration still needs. */
	srv.dropChannelIfEmpty(channel);
}

void	cmdInvite(Server& srv, Client& c, const Message& m)
{
	Client*		target;
	Channel*	channel;

	if (m.argc() < 2)
	{
		c.queue(errNeedMoreParams(c.getNick(), "INVITE"));
		return ;
	}
	target = srv.findClientByNick(m.arg(0));
	if (target == NULL)
	{
		c.queue(errNoSuchNick(c.getNick(), m.arg(0)));
		return ;
	}
	channel = srv.findChannel(m.arg(1));
	if (channel == NULL)
	{
		c.queue(errNoSuchChannel(c.getNick(), m.arg(1)));
		return ;
	}
	if (!channel->isMember(c))
	{
		c.queue(errNotOnChannel(c.getNick(), channel->getName()));
		return ;
	}
	/* On a +i channel the invite is the only way in, so it is an operator
	   privilege. Anywhere else any member may invite. */
	if (channel->isInviteOnly() && !channel->isOperator(c))
	{
		c.queue(errChanOPrivsNeeded(c.getNick(), channel->getName()));
		return ;
	}
	if (channel->isMember(*target))
	{
		c.queue(errUserOnChannel(c.getNick(), target->getNick(),
			channel->getName()));
		return ;
	}
	channel->addInvite(*target);
	c.queue(rplInviting(c.getNick(), target->getNick(), channel->getName()));
	target->queue(":" + c.getPrefix() + " INVITE " + target->getNick()
		+ " :" + channel->getName() + "\r\n");
}

void	cmdNames(Server& srv, Client& c, const Message& m)
{
	std::vector<std::string>	names;
	std::size_t					i;

	/* Listing every channel of the network is not part of the subject, so a
	   bare NAMES just closes the list. */
	if (m.argc() < 1 || m.arg(0).empty())
	{
		c.queue(rplEndOfNames(c.getNick(), "*"));
		return ;
	}
	names = split(m.arg(0), ',');
	for (i = 0; i < names.size(); ++i)
	{
		Channel*	channel = srv.findChannel(names[i]);

		if (channel == NULL)
			c.queue(rplEndOfNames(c.getNick(), names[i]));
		else
			sendNames(c, *channel);
	}
}
