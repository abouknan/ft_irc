/* ************************************************************************** */
/*                                                                            */
/*   Utils.cpp                                                                */
/*                                                                            */
/* ************************************************************************** */

#include <sstream>

#include "Utils.hpp"

std::string	toIrcLower(const std::string& text)
{
	std::string	out(text);
	std::size_t	i = 0;

	while (i < out.size())
	{
		char	c = out[i];

		if (c >= 'A' && c <= 'Z')
			out[i] = static_cast<char>(c + ('a' - 'A'));
		else if (c == '[')
			out[i] = '{';
		else if (c == ']')
			out[i] = '}';
		else if (c == '\\')
			out[i] = '|';
		++i;
	}
	return (out);
}

std::vector<std::string>	split(const std::string& text, char separator)
{
	std::vector<std::string>	out;
	std::string					current;
	std::size_t					i = 0;

	while (i < text.size())
	{
		if (text[i] == separator)
		{
			if (!current.empty())
				out.push_back(current);
			current.clear();
		}
		else
			current += text[i];
		++i;
	}
	if (!current.empty())
		out.push_back(current);
	return (out);
}

std::string	toString(long value)
{
	std::ostringstream	stream;

	stream << value;
	return (stream.str());
}

/* Letters, digits and the "special" characters the RFC allows in a nick. */
static bool	isNickChar(char c, bool first)
{
	if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
		return (true);
	if (c == '[' || c == ']' || c == '\\' || c == '`'
		|| c == '_' || c == '^' || c == '{' || c == '|' || c == '}')
		return (true);
	if (first)
		return (false);
	return ((c >= '0' && c <= '9') || c == '-');
}

bool	isValidNick(const std::string& nick)
{
	std::size_t	i;

	/* RFC 2812 guarantees at least 9 characters; modern servers allow more and
	   advertise the real limit, so 30 keeps every usual client happy. */
	if (nick.empty() || nick.size() > 30)
		return (false);
	if (!isNickChar(nick[0], true))
		return (false);
	i = 1;
	while (i < nick.size())
	{
		if (!isNickChar(nick[i], false))
			return (false);
		++i;
	}
	return (true);
}

bool	isValidChannelName(const std::string& name)
{
	std::size_t	i;

	if (name.size() < 2 || name.size() > 50)
		return (false);
	if (name[0] != '#' && name[0] != '&')
		return (false);
	i = 1;
	while (i < name.size())
	{
		char	c = name[i];

		if (c == ' ' || c == ',' || c == 7 || c == '\0')
			return (false);
		++i;
	}
	return (true);
}
