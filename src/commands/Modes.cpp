/* ************************************************************************** */
/*                                                                            */
/*   Modes.cpp                                                                */
/*                                                                            */
/*   MODE, and the five channel modes the subject requires:                   */
/*                                                                            */
/*     +i / -i   invite only                                                  */
/*     +t / -t   only operators may change the topic                          */
/*     +k / -k   channel key, takes the key as argument when set              */
/*     +o / -o   operator privilege, takes a nick as argument                 */
/*     +l / -l   member limit, takes a number as argument when set            */
/*                                                                            */
/*   A single MODE may carry several flags, "MODE #x +itk-l secret", so the    */
/*   flags are walked one by one and their arguments consumed in order. Only  */
/*   what really changed is echoed back to the channel.                       */
/*                                                                            */
/* ************************************************************************** */

#include <cstdlib>

#include "Channel.hpp"
#include "Client.hpp"
#include "Message.hpp"
#include "Server.hpp"
#include "Utils.hpp"
#include "commands.hpp"
#include "replies.hpp"

/* Builds the confirmation as we go: "+it-k" plus the arguments in order.
   The sign is only written when it changes, the way real servers do it. */
static void	recordChange(std::string& modes, std::string& arguments,
			char& shownSign, char sign, char mode, const std::string& parameter)
{
	if (shownSign != sign)
	{
		shownSign = sign;
		modes += sign;
	}
	modes += mode;
	if (!parameter.empty())
		arguments += " " + parameter;
}

/* A limit must be a plain positive number; anything else is ignored rather
   than turned into a nonsensical channel. */
static bool	parseLimit(const std::string& text, std::size_t& out)
{
	std::size_t	i = 0;

	if (text.empty() || text.size() > 9)
		return (false);
	while (i < text.size())
	{
		if (text[i] < '0' || text[i] > '9')
			return (false);
		++i;
	}
	out = static_cast<std::size_t>(std::atol(text.c_str()));
	return (out > 0);
}

static void	applyKey(Client& c, Channel& channel, const Message& m,
			std::size_t& next, char sign, std::string& modes,
			std::string& arguments, char& shownSign)
{
	if (sign == '-')
	{
		/* Some clients send the key back when removing it: accept and drop. */
		if (next < m.argc())
			++next;
		channel.clearKey();
		recordChange(modes, arguments, shownSign, sign, 'k', "");
		return ;
	}
	if (next >= m.argc() || m.arg(next).empty())
	{
		c.queue(errNeedMoreParams(c.getNick(), "MODE"));
		return ;
	}
	channel.setKey(m.arg(next));
	recordChange(modes, arguments, shownSign, sign, 'k', m.arg(next));
	++next;
}

static void	applyLimit(Client& c, Channel& channel, const Message& m,
			std::size_t& next, char sign, std::string& modes,
			std::string& arguments, char& shownSign)
{
	std::size_t	limit = 0;

	if (sign == '-')
	{
		channel.clearLimit();
		recordChange(modes, arguments, shownSign, sign, 'l', "");
		return ;
	}
	if (next >= m.argc())
	{
		c.queue(errNeedMoreParams(c.getNick(), "MODE"));
		return ;
	}
	if (parseLimit(m.arg(next), limit))
	{
		channel.setLimit(limit);
		recordChange(modes, arguments, shownSign, sign, 'l', m.arg(next));
	}
	++next;
}

static void	applyOperator(Server& srv, Client& c, Channel& channel,
			const Message& m, std::size_t& next, char sign, std::string& modes,
			std::string& arguments, char& shownSign)
{
	Client*	member;

	if (next >= m.argc() || m.arg(next).empty())
	{
		c.queue(errNeedMoreParams(c.getNick(), "MODE"));
		return ;
	}
	member = srv.findClientByNick(m.arg(next));
	if (member == NULL || !channel.isMember(*member))
	{
		c.queue(errUserNotInChannel(c.getNick(), m.arg(next), channel.getName()));
		++next;
		return ;
	}
	if (sign == '+')
		channel.addOperator(*member);
	else
		channel.removeOperator(*member);
	recordChange(modes, arguments, shownSign, sign, 'o', member->getNick());
	++next;
}

static void	applyModes(Server& srv, Client& c, Channel& channel, const Message& m)
{
	const std::string&	spec = m.arg(1);
	std::size_t			next = 2;		/* first parameter after the flags */
	char				sign = '+';		/* a flag with no sign means '+' */
	char				shownSign = 0;
	std::string			modes;
	std::string			arguments;
	std::size_t			i;

	for (i = 0; i < spec.size(); ++i)
	{
		char	mode = spec[i];

		if (mode == '+' || mode == '-')
		{
			sign = mode;
			continue ;
		}
		if (mode == 'i')
		{
			channel.setInviteOnly(sign == '+');
			recordChange(modes, arguments, shownSign, sign, 'i', "");
		}
		else if (mode == 't')
		{
			channel.setTopicLocked(sign == '+');
			recordChange(modes, arguments, shownSign, sign, 't', "");
		}
		else if (mode == 'k')
			applyKey(c, channel, m, next, sign, modes, arguments, shownSign);
		else if (mode == 'l')
			applyLimit(c, channel, m, next, sign, modes, arguments, shownSign);
		else if (mode == 'o')
			applyOperator(srv, c, channel, m, next, sign, modes, arguments,
				shownSign);
		else
			c.queue(errUnknownMode(c.getNick(), mode));
	}
	/* Nothing actually changed: stay quiet instead of sending an empty MODE. */
	if (modes.empty())
		return ;
	channel.broadcast(":" + c.getPrefix() + " MODE " + channel.getName() + " "
		+ modes + arguments + "\r\n", NULL);
}

void	cmdMode(Server& srv, Client& c, const Message& m)
{
	Channel*	channel;
	std::string	shown;

	if (m.argc() < 1 || m.arg(0).empty())
	{
		c.queue(errNeedMoreParams(c.getNick(), "MODE"));
		return ;
	}

	/* Clients ask for their own user modes right after connecting. We have no
	   user mode to offer, so we answer an empty set and move on. */
	if (m.arg(0)[0] != '#' && m.arg(0)[0] != '&')
	{
		if (toIrcLower(m.arg(0)) == toIrcLower(c.getNick()))
			c.queue(":" SERVER_NAME " 221 " + c.getNick() + " +\r\n");
		else
			c.queue(errNoSuchNick(c.getNick(), m.arg(0)));
		return ;
	}

	channel = srv.findChannel(m.arg(0));
	if (channel == NULL)
	{
		c.queue(errNoSuchChannel(c.getNick(), m.arg(0)));
		return ;
	}

	/* "MODE #chan" is a question. The key and the limit are only part of the
	   answer for a member: they must not leak to the outside. */
	if (m.argc() < 2)
	{
		shown = channel->modeString();
		if (channel->isMember(c) && !channel->modeArguments().empty())
			shown += " " + channel->modeArguments();
		c.queue(rplChannelModeIs(c.getNick(), channel->getName(), shown));
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
	applyModes(srv, c, *channel, m);
}
