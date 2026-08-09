/* ************************************************************************** */
/*                                                                            */
/*   Client.hpp                                                               */
/*                                                                            */
/*   One connected socket and everything we know about the user behind it.    */
/*                                                                            */
/*   A Client owns its file descriptor: closing happens in the destructor,    */
/*   so Server only has to `delete` the pointer to hang up.                   */
/*                                                                            */
/*   It also owns the two byte buffers that make the whole server work        */
/*   without ever blocking:                                                   */
/*     _inBuffer   bytes recv()'d but not yet a complete line                 */
/*     _outBuffer  bytes we want to send but that send() has not accepted yet */
/*                                                                            */
/* ************************************************************************** */

#ifndef CLIENT_HPP
# define CLIENT_HPP

# include <cstddef>
# include <string>

/* A client that keeps sending bytes without ever sending a newline would grow
   _inBuffer forever, so we cap it and drop the connection instead. */
# define CLIENT_IN_BUFFER_MAX 8192

class Client
{
	private:

		int			_fd;
		std::string	_nick;
		std::string	_user;
		std::string	_realname;
		std::string	_host;

		/* Registration is complete once the three of them are true. */
		bool		_passOk;
		bool		_nickSet;
		bool		_userSet;

		/* Set once the welcome burst (001-004) has been sent. */
		bool		_welcomed;

		/* The client is leaving: stop reading from it, finish flushing
		   _outBuffer, then Server closes it. */
		bool		_closing;

		std::string	_inBuffer;
		std::string	_outBuffer;

		/* A Client is tied to one fd; copying it would close that fd twice. */
		Client(const Client& other);
		Client&	operator=(const Client& other);

	public:

		explicit Client(int fd);
		~Client();

		int							getFd() const;
		const std::string&			getNick() const;
		const std::string&			getUser() const;
		const std::string&			getRealname() const;
		const std::string&			getHost() const;

		/* "nick!user@host": the source of every message we relay on this
		   client's behalf. */
		std::string					getPrefix() const;

		void						setNick(const std::string& nick);
		void						setUser(const std::string& user);
		void						setRealname(const std::string& realname);
		void						setHost(const std::string& host);

		bool						isPassOk() const;
		bool						isNickSet() const;
		bool						isUserSet() const;
		bool						isWelcomed() const;
		bool						isRegistered() const;
		bool						isClosing() const;

		void						setPassOk(bool value);
		void						setNickSet(bool value);
		void						setUserSet(bool value);
		void						setWelcomed(bool value);
		void						setClosing(bool value);

		/* Appends what recv() gave us. Returns false when the client has gone
		   past CLIENT_IN_BUFFER_MAX without a single newline. */
		bool						appendIn(const char* data, std::size_t n);

		/* Pops one complete line, CR and LF stripped. False when _inBuffer
		   does not hold a full line yet, which is the normal case for a
		   command split across several packets. */
		bool						takeLine(std::string& out);

		/* Nothing in the server ever calls send() directly: it queues here and
		   the poll loop flushes when the socket says it is writable. */
		void						queue(const std::string& message);
		bool						hasOut() const;
		const std::string&			outData() const;
		void						consumeOut(std::size_t n);
};

#endif
