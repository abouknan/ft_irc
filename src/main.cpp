/* ************************************************************************** */
/*                                                                            */
/*   main.cpp                                                                 */
/*                                                                            */
/*   ./ircserv <port> <password>                                              */
/*                                                                            */
/* ************************************************************************** */

#include <csignal>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "Server.hpp"

static int	parsePort(const std::string& text)
{
	std::istringstream	stream(text);
	int					port = 0;

	/* eof() makes sure the whole argument was consumed, so "6667abc" and
	   "66 67" are rejected instead of silently becoming 6667 and 66. */
	if (!(stream >> port) || !stream.eof())
		throw std::runtime_error("port must be a number");
	if (port < 1 || port > 65535)
		throw std::runtime_error("port must be between 1 and 65535");
	return (port);
}

int	main(int argc, char** argv)
{
	if (argc != 3)
	{
		std::cerr << "usage: " << argv[0] << " <port> <password>" << std::endl;
		return (1);
	}

	/* Writing to a socket the peer already closed raises SIGPIPE, whose
	   default action is to kill the process. The server must never die from a
	   client hanging up, so the signal is ignored and send() reports the
	   error through its return value instead. */
	std::signal(SIGPIPE, SIG_IGN);

	try
	{
		int			port = parsePort(argv[1]);
		std::string	password(argv[2]);

		if (password.empty())
			throw std::runtime_error("password must not be empty");

		Server	server(port, password);

		server.setupListener();
		server.run();
	}
	catch (const std::exception& error)
	{
		std::cerr << "error: " << error.what() << std::endl;
		return (1);
	}
	return (0);
}
