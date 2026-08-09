/* ************************************************************************** */
/*                                                                            */
/*   Client.cpp                                                               */
/*                                                                            */
/* ************************************************************************** */

#include <unistd.h>

#include "Client.hpp"

Client::Client(int fd)
	: _fd(fd),
	  _nick(),
	  _user(),
	  _realname(),
	  _host(),
	  _passOk(false),
	  _nickSet(false),
	  _userSet(false),
	  _welcomed(false),
	  _closing(false),
	  _inBuffer(),
	  _outBuffer()
{
}

Client::~Client()
{
	if (_fd >= 0)
		close(_fd);
}

int	Client::getFd() const
{
	return (_fd);
}

const std::string&	Client::getNick() const
{
	return (_nick);
}

const std::string&	Client::getUser() const
{
	return (_user);
}

const std::string&	Client::getRealname() const
{
	return (_realname);
}

const std::string&	Client::getHost() const
{
	return (_host);
}

std::string	Client::getPrefix() const
{
	return (_nick + "!" + _user + "@" + _host);
}

void	Client::setNick(const std::string& nick)
{
	_nick = nick;
}

void	Client::setUser(const std::string& user)
{
	_user = user;
}

void	Client::setRealname(const std::string& realname)
{
	_realname = realname;
}

void	Client::setHost(const std::string& host)
{
	_host = host;
}

bool	Client::isPassOk() const
{
	return (_passOk);
}

bool	Client::isNickSet() const
{
	return (_nickSet);
}

bool	Client::isUserSet() const
{
	return (_userSet);
}

bool	Client::isWelcomed() const
{
	return (_welcomed);
}

bool	Client::isRegistered() const
{
	return (_passOk && _nickSet && _userSet);
}

bool	Client::isClosing() const
{
	return (_closing);
}

void	Client::setPassOk(bool value)
{
	_passOk = value;
}

void	Client::setNickSet(bool value)
{
	_nickSet = value;
}

void	Client::setUserSet(bool value)
{
	_userSet = value;
}

void	Client::setWelcomed(bool value)
{
	_welcomed = value;
}

void	Client::setClosing(bool value)
{
	_closing = value;
}

bool	Client::appendIn(const char* data, std::size_t n)
{
	_inBuffer.append(data, n);
	if (_inBuffer.size() > CLIENT_IN_BUFFER_MAX
		&& _inBuffer.find('\n') == std::string::npos)
		return (false);
	return (true);
}

bool	Client::takeLine(std::string& out)
{
	std::string::size_type	pos = _inBuffer.find('\n');
	std::size_t				length;

	if (pos == std::string::npos)
		return (false);
	length = pos;
	/* The RFC terminator is CRLF, but a raw `nc` user only sends LF. */
	if (length > 0 && _inBuffer[length - 1] == '\r')
		--length;
	out = _inBuffer.substr(0, length);
	_inBuffer.erase(0, pos + 1);
	return (true);
}

void	Client::queue(const std::string& message)
{
	/* A client on its way out must not receive anything else. */
	if (_closing)
		return ;
	_outBuffer.append(message);
}

bool	Client::hasOut() const
{
	return (!_outBuffer.empty());
}

const std::string&	Client::outData() const
{
	return (_outBuffer);
}

void	Client::consumeOut(std::size_t n)
{
	_outBuffer.erase(0, n);
}
