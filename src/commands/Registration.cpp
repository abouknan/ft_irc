/* ************************************************************************** */
/*                                                                            */
/*   Registration.cpp                                                         */
/*                                                                            */
/*   CAP, PASS, NICK, USER, QUIT, PING, PONG.                                 */
/*                                                                            */
/*   These are the only commands a client may send before it is registered.   */
/*   Registration is complete when PASS, NICK and USER have all been          */
/*   accepted; the welcome burst is sent exactly once, by tryRegister().      */
/*                                                                            */
/* ************************************************************************** */

#include "Client.hpp"
#include "Message.hpp"
#include "Server.hpp"
#include "Utils.hpp"
#include "commands.hpp"
#include "replies.hpp"

/* Called after every PASS, NICK and USER. Does nothing until the three of
   them are in, so the client may send them in any order. */
static void	tryRegister(Server& srv, Client& c)
{
	if (c.isWelcomed() || !c.isNickSet() || !c.isUserSet())
		return ;
	/* The nick and the user are known but no valid PASS ever arrived: this is
	   where a client that skipped it is turned away. */
	if (!c.isPassOk())
	{
		c.queue(errPasswdMismatch(c.getNick()));
		srv.disconnect(c, "Password required");
		return ;
	}
	srv.welcome(c);
}

/* Modern clients open with "CAP LS" and wait for an answer before sending
   NICK and USER. We support no capability at all, so we answer with an empty
   list and let them carry on instead of leaving them hanging. */
void	cmdCap(Server& srv, Client& c, const Message& m)
{
	const std::string&	subcommand = m.arg(0);

	(void)srv;
	if (subcommand == "LS" || subcommand == "LIST")
		c.queue(":" SERVER_NAME " CAP * " + subcommand + " :\r\n");
	else if (subcommand == "REQ")
		c.queue(":" SERVER_NAME " CAP * NAK :" + m.arg(1) + "\r\n");
	/* "END" needs no answer, and neither does anything we do not know. */
}

void	cmdPass(Server& srv, Client& c, const Message& m)
{
	if (c.isRegistered())
	{
		c.queue(errAlreadyRegistered(c.getNick()));
		return ;
	}
	if (m.argc() < 1 || m.arg(0).empty())
	{
		c.queue(errNeedMoreParams(c.getNick(), "PASS"));
		return ;
	}
	if (m.arg(0) != srv.getPassword())
	{
		c.queue(errPasswdMismatch(c.getNick()));
		srv.disconnect(c, "Password incorrect");
		return ;
	}
	c.setPassOk(true);
}

void	cmdNick(Server& srv, Client& c, const Message& m)
{
	const std::string&	wanted = m.arg(0);
	Client*				owner;
	std::string			previous;

	if (m.argc() < 1 || wanted.empty())
	{
		c.queue(errNoNicknameGiven(c.getNick()));
		return ;
	}
	if (!isValidNick(wanted))
	{
		c.queue(errErroneusNickname(c.getNick(), wanted));
		return ;
	}
	owner = srv.findClientByNick(wanted);
	if (owner != NULL && owner != &c)
	{
		c.queue(errNicknameInUse(c.getNick(), wanted));
		return ;
	}

	previous = c.getNick();
	if (previous == wanted)
		return ;
	c.setNick(wanted);
	c.setNickSet(true);

	/* A rename after registration has to be announced, otherwise every other
	   client keeps showing the old name. The prefix is built after the change
	   so the source of the message is the new nick, as the RFC wants. */
	if (c.isWelcomed() && !previous.empty())
		srv.broadcastToPeers(c, ":" + previous + "!" + c.getUser() + "@"
			+ c.getHost() + " NICK :" + wanted + "\r\n", true);
	tryRegister(srv, c);
}

void	cmdUser(Server& srv, Client& c, const Message& m)
{
	if (c.isRegistered())
	{
		c.queue(errAlreadyRegistered(c.getNick()));
		return ;
	}
	/* USER <username> <hostname> <servername> <realname> */
	if (m.argc() < 4)
	{
		c.queue(errNeedMoreParams(c.getNick(), "USER"));
		return ;
	}
	/* The hostname and servername a client sends about itself are not
	   trusted by any server: we keep the address we saw on accept(). */
	c.setUser(m.arg(0));
	c.setRealname(m.arg(3));
	c.setUserSet(true);
	tryRegister(srv, c);
}

void	cmdQuit(Server& srv, Client& c, const Message& m)
{
	std::string	reason = m.arg(0);

	if (reason.empty())
		reason = "Client quit";
	srv.disconnect(c, "Quit: " + reason);
}

void	cmdPing(Server& srv, Client& c, const Message& m)
{
	(void)srv;
	if (m.argc() < 1 || m.arg(0).empty())
	{
		c.queue(errNeedMoreParams(c.getNick(), "PING"));
		return ;
	}
	c.queue(":" SERVER_NAME " PONG " SERVER_NAME " :" + m.arg(0) + "\r\n");
}

/* We never send PING ourselves, so an incoming PONG is simply proof that the
   client is alive. Nothing to answer. */
void	cmdPong(Server& srv, Client& c, const Message& m)
{
	(void)srv;
	(void)c;
	(void)m;
}
