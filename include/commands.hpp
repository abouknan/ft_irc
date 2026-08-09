/* ************************************************************************** */
/*                                                                            */
/*   commands.hpp                                                             */
/*                                                                            */
/*   Every command handler has the exact same signature, which is what lets   */
/*   CommandDispatcher keep them all in one std::map of plain function        */
/*   pointers: no virtual, no allocation, C++98 friendly.                     */
/*                                                                            */
/*     void cmdXxx(Server& srv, Client& c, const Message& m);                 */
/*                                                                            */
/*   Contract every handler respects:                                         */
/*     - it is only reached by a client allowed to run it, the dispatcher     */
/*       already refused unregistered clients,                                */
/*     - it NEVER calls send(). It pushes into Client::queue() and the poll   */
/*       loop drains it on POLLOUT,                                           */
/*     - it never deletes a Client. It calls Server::disconnect().            */
/*                                                                            */
/* ************************************************************************** */

#ifndef COMMANDS_HPP
# define COMMANDS_HPP

class Server;
class Client;
class Channel;
struct Message;

/* ---- Registration and connection ----------------------------------------- */

void	cmdPass(Server& srv, Client& c, const Message& m);
void	cmdNick(Server& srv, Client& c, const Message& m);
void	cmdUser(Server& srv, Client& c, const Message& m);
void	cmdQuit(Server& srv, Client& c, const Message& m);
void	cmdPing(Server& srv, Client& c, const Message& m);
void	cmdPong(Server& srv, Client& c, const Message& m);
void	cmdCap(Server& srv, Client& c, const Message& m);

/* ---- Messaging ----------------------------------------------------------- */

void	cmdPrivmsg(Server& srv, Client& c, const Message& m);
void	cmdNotice(Server& srv, Client& c, const Message& m);

/* ---- Channels ------------------------------------------------------------ */

void	cmdJoin(Server& srv, Client& c, const Message& m);
void	cmdPart(Server& srv, Client& c, const Message& m);
void	cmdTopic(Server& srv, Client& c, const Message& m);
void	cmdKick(Server& srv, Client& c, const Message& m);
void	cmdInvite(Server& srv, Client& c, const Message& m);
void	cmdNames(Server& srv, Client& c, const Message& m);
void	cmdMode(Server& srv, Client& c, const Message& m);

/* Shared by JOIN and NAMES: sends RPL_NAMREPLY then RPL_ENDOFNAMES. */
void	sendNames(Client& c, Channel& channel);

#endif
