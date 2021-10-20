RamBot - a simple modular irc bot written in C
----------------------------------------------

Build Instructions
------------------
1) download this repository.
2) ensure you have a working C99 compiler, such as gcc
3) ensure you have POSIX - I have only tested this on Linux but we absolutely require a POSIX OS.
4) execute make
5) configure the bot with rambot.conf
6) run "rambotc" without any arguments - it should connect to an irc server, authenticate against NickServ, and join the channel you configured.
7) configure your action helpers to make the bot useful.

NOTE: For security reasons I would strongly suggest you setup this bot with it's own system user account. I use /home/rambot

Action Helpers
--------------
This bot comes with several action helper programs, located in the actions folder.
You can reference them to see how they work, and write your own.

The following is documentation for all the included action helpers.
These can be enabled and disabled in rambot.conf, and you can easily create custom ones as well.
You should review each actions/ helper code file, most of them are simple shell scripts or php cli files.

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

How the bot executes the action helper commands
-------------------------
The bot will run: ACTION_cmd 'usernick' 'channel' 'arguments'
For example, that means if someone types ".google my search query" into the irc chat, the bot will execute:

ACTION_google 'usernick' 'channel' 'my search query'

If there are no arguments the bot would instead run: ACTION_google 'usernick' 'channel'

You must define what ACTION_google is inside rambot.conf - see the included action helper inside "actions/google" file.

Return Status:

The bot expects the command to return status 1 if the response should output into the channel.
The bot expects the command to return status 2 if the response should output directly to the user that made the request.
All other exit status codes will cause the bot to completely discard and ignore the command results.

Encrypted IRC
-------------
This bot does not handle encrypted connections and probably never will.
If you need ssl/tls use a simple proxy, such as socat

socat TCP-LISTEN:6667,fork,reuseaddr OPENSSL:remote-server:7000,verify=0
