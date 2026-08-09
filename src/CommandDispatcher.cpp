/* ************************************************************************** */
/*                                                                            */
/*   CommandDispatcher.cpp                                                    */
/*                                                                            */
/* ************************************************************************** */

#include "Client.hpp"
#include "CommandDispatcher.hpp"
#include "Message.hpp"
#include "Server.hpp"
#include "commands.hpp"
#include "replies.hpp"

CommandDispatcher::CommandDispatcher() : _table()
{
	bind("CAP", &cmdCap);
	bind("PASS", &cmdPass);
	bind("NICK", &cmdNick);
	bind("USER", &cmdUser);
	bind("QUIT", &cmdQuit);
	bind("PING", &cmdPing);
	bind("PONG", &cmdPong);

	bind("PRIVMSG", &cmdPrivmsg);
	bind("NOTICE", &cmdNotice);

	bind("JOIN", &cmdJoin);
	bind("PART", &cmdPart);
	bind("TOPIC", &cmdTopic);
	bind("MODE", &cmdMode);
	bind("KICK", &cmdKick);
	bind("INVITE", &cmdInvite);
	bind("NAMES", &cmdNames);
}

CommandDispatcher::~CommandDispatcher()
{
}

void	CommandDispatcher::bind(const std::string& name, Handler handler)
{
	_table[name] = handler;
}

bool	CommandDispatcher::allowedBeforeRegistration(const std::string& command)
{
	static const char* const	allowed[] = { "CAP", "PASS", "NICK", "USER",
											  "QUIT", "PING", "PONG", 0 };
	std::size_t					i = 0;

	while (allowed[i] != 0)
	{
		if (command == allowed[i])
			return (true);
		++i;
	}
	return (false);
}

void	CommandDispatcher::dispatch(Server& srv, Client& c, const Message& m)
{
	/* An empty line is legal on the wire and must produce no error at all. */
	if (m.empty())
		return ;

	std::map<std::string, Handler>::const_iterator	it = _table.find(m.command);

	if (it == _table.end())
	{
		c.queue(errUnknownCommand(c.getNick(), m.command));
		return ;
	}
	if (!c.isRegistered() && !allowedBeforeRegistration(m.command))
	{
		c.queue(errNotRegistered(c.getNick()));
		return ;
	}
	it->second(srv, c, m);
}
