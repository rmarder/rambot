RamBot - A Simple Modular Scriptable IRC Bot
----------------------------------------------

Setup Instructions
------------------
1) download this repository.
2) ensure you have a working C99 compiler, such as gcc. you'll want make too.
3) ensure you have a POSIX compliant operating system
 - I have only tested this on Ubuntu 20.04 but this should work on any Linux/BSD/Unix.
4) execute "make" - RamBot itself has no external library dependencies at all.
5) configure HOST, PORT, USER, PASSWORD, and CHANNEL in rambot.conf
6) run "./rambotc" without any arguments - it should connect to an irc server, authenticate against NickServ, and join the channel you configured.
7) test the builtin action by typing ".say hello world" into your irc chat - the bot should say "hello world" into the irc channel.
8) configure your action helpers in rambot.conf and in the actions folder to make the bot do useful things!

NOTE: For security reasons I would strongly suggest you setup this bot with it's own system user account. I use /home/rambot

Action Helpers
--------------
This bot comes with several action helper programs, located in the actions folder.
You can reference them to see how they work, and write your own.

The following is documentation for all the included action helpers.
These can be enabled and disabled in rambot.conf, and you can easily create custom ones as well.
You should review each actions/ helper code file, most of them are simple shell scripts or php cli files and very easy for the layman to read and understand. Any executable can be used as an action helper if it follows RamBot's guidelines.

1) weather feature:

.weather placeName - prints the current weather at placeName

.weather save placeName - have the weather feature remember where your nick is located. Creates new database entry or updates existing location.

.weather - use after weather save placeName to show your own weather.

2) google feature

.google query - do a google search (the helper actually uses duckduckgo lite), return the summary of the first result.

3) ping feature

.ping 1.2.3.4 - pings the specified ip address.

.ping example.com - pings the specified host.

4) traceroute feature

.traceroute 1.2.3.4 - performs a traceroute to the specified ip address. result is returned via PM.

.traceroute example.com - performs a traceroute to the specicied host. result is returned via PM.

5) fortune teller

.fortune - retrieves and prints a random fortune.

.fortune category - retrieves and prints random fortune from the database specified by category.

6) calculator

.calc expression - evaluates expression and calculates result using bc.

7) insult feature

.insult user - send an insult to the user.

8) stock feature

.stock ticker - retrieves current stock price information.

9) help feature

.help - returns this help documentation via PM.

How RamBot Executes Action Helpers
-------------------------
The bot will run: ACTION_cmd 'usernick' 'channel' 'arguments'

The bot will send nothing on standard input (stdin), and expect a response on standard output (stdout).

For example, that means if someone types ".google my search query" into the irc chat, the bot will execute:

ACTION_google 'usernick' 'channel' 'my search query'

If there are no arguments the bot would instead run: ACTION_google 'usernick' 'channel'

You must define what ACTION_google is inside rambot.conf 

The included action helper inside the "actions/google" file will perform a web search and output the first result to stdout, returning exit status 1.

Return Status Special Meaning:

The bot expects the command to return status 1 if the response should output into the channel for everyone to see.

The bot expects the command to return status 2 if the response should output directly to the user that made the request in a private message.

All other program return exit status codes will cause the bot to completely discard and ignore the command results. You should pay special attention to this, as it is convention for most commands to return exit status 0 on success.

Important Note:
RamBot is a single threaded program. That means, the bot will only handle one request at a time.
This also means, if you define an action helper that runs for a very long time, RamBot will not respond to any other commands during that time.

To ensure this situation doesn't happen by accident, you should use something such as timeout(1) from GNU coreutils to run the action helpers.

You can reconfigure the action helpers while the bot is running, you don't need to restart it. RamBot will read and parse rambot.conf every single time an action is requested.


Special Actions
--------------
RamBot has 2 special actions.

1) echo server

.say any text

This is a basic echo function - useful to test RamBot is working correctly if you have no actions configured and working yet.

2) http:// and https:// link handler

.http http://anything

.http https://anything

This is a web site URL handler action.

Note that the only builtin part of this, is that RamBot will scan every single chat line for url links to websites and internally translate the request to this format.

For example, if somebody writes this into the channel:

"I really like https://github.com/rmarder/rambot/ you should try it out!"

RamBot will extract the URL out and translate that into a request of this format:

.http https://github.com/rmarder/rambot/

From that point onwards the http action must be defined, and behaves, just like any other external action as defined in rambot.conf.

In rambot.conf if you have http listed in ACTIONS and ACTION_http set then RamBot will execute:

ACTION_http 'usernick' 'channel' 'https://github.com/rmarder/rambot/'

The included actions/http script will extract and return web page titles, which is the most obvious value of this feature.


Shell Execution and Locale / Unicode Notes:
-------
For security, RamBot has been designed to pass the action commands to the shell in a safe manner. However, this also means it will only accept arguments to action commands that are valid US-ASCII characters greater than code 31 and less than code 127, and because the command arguments are always single quoted, we exclude that character (ASCII code 39).

RamBot assumes your shell and the IRC server you are connected to are all using a US-ASCII compatible character encoding locale, such as the default POSIX locale or something like UTF-8 or ISO 8859-1 which are all compatible with US-ASCII. If your local system, or the IRC server you connect with, are using a character encoding that is not compatible, such as UTF-16 or UTF-32, then RamBot won't work as expected.

Encrypted IRC and IPv6 Support
----------------
This bot does not handle encrypted connections and probably never will.
If you need ssl/tls use a simple proxy, such as socat

socat TCP-LISTEN:6667,fork,reuseaddr OPENSSL:remote-server:7000,verify=0

If you need IPv6 support you can do something similar. RamBot itself only communicates on IPv4.

Daemonizing RamBot
--------------
echo '/usr/bin/sudo -u rambot screen -d -m /home/rambot/rambot.sh' >> /etc/rc.local

Adjust if your system doesn't have an /etc/rc.local or equivalent - writing an initd or systemd script is left as an exercise to the reader.

Note that on connection faults, RamBot will terminate and not try to automatically reconnect. So having a helper script to automatically restart RamBot when that happens is very useful.

RamBot will output lots of interesting information to it's console. If you run it inside screen, then run "screen -rd" to see what RamBot is doing.

RamBot has been very carefully written and checked against valgrind to ensure absolutely no memory leaks, and is used by the author for many years connected to a real IRC server where people use it every day. To the authors knowledge RamBot has never unexpectedly segfaulted or experienced any buffer overflows, nor does it contain any known security problems.
