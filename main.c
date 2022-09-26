/*
 * Copyright (C) 2021 Robert Alex Marder (ram@robertmarder.com)
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 * http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

#define CONFIG "rambot.conf"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include "ramnet.h"

char * config(const char *search)
{
	char *result;

	if(is_readable(CONFIG) != 1)
	{
		fprintf(stderr, "Error reading %s: %s\n", CONFIG, strerror(errno));
		exit(1);
	}

	result = parse_config(CONFIG, search);

	if(result == NULL)
	{
		fprintf(stderr, "Configuration Error: unable to read %s from %s\n", search, CONFIG);
		exit(1);
	}
	return result;
}

void do_action(int sock, const char *usernick, const char *channel, const char *action, const char *action_args)
{
	char *config_actions;
	char **actions;
	ssize_t actions_length;
	char **output;
	ssize_t output_length;
	char *action_current;
	char *action_current_cmd;
	char *result;
	char *tmp;
	FILE* fp;
	int cmd_status;
	int c;
	// irc protocol says we can't send anything longer than 512 in one go, so this works.
	char send[513];
	char *cmd_line;

	//printf("usernick: [%s]\n", usernick);
	//printf("action: [%s]\n", action);
	//printf("action_args: [%s]\n", action_args);

	// handle built in actions
	if(strcmp(action, "say") == 0 && action_args != NULL)
	{
		if(sprintf(send, "PRIVMSG %s :%s", channel, action_args) > 0 || sprintf(send, "NOTICE %s :%s", channel, action_args) > 0)
		{
			printf("SEND: [%s]\n", send);
			write_line(sock, send);
			return;
		}
	}

	// handle the user-defined actions.

	config_actions = config("ACTIONS");
	debugf("ACTIONS = [%s]\n", config_actions);

	// loop over all the actions
	actions_length = explode(&actions, config_actions, ",");
	if(actions_length == -1)
	{
		fprintf(stderr, "Error reading ACTIONS config: %s\n", strerror(errno));
		exit(1);
	}

	for(size_t i = 0; i < actions_length; i++)
	{
		action_current = strip(actions[i]);
		if(action_current == NULL)
		{
			debugf("%s\n", "Error: action_current strip() failed.");
			exit(1);
		}
		tmp = malloc(strlen(action_current) + 8);
		if(tmp == NULL)
		{
			debugf("%s\n", "Error: malloc() failed.");
			exit(1);
		}
		strcpy(tmp, "ACTION_");
		strcat(tmp, action_current);
		action_current_cmd = config(tmp);
		free(tmp);

		//printf("index: [%zu] | action_current: [%s] | action_current_cmd: [%s]\n", i, action_current, action_current_cmd);
		if(strcmp(action, action_current) == 0)
		{
			if(is_shell_safe(action_args) == 1)
			{
				tmp = malloc(strlen(action_current_cmd) + strlen(usernick) + strlen(channel) + strlen(action_args) + 20);
				if(tmp == NULL)
				{
					debugf("%s\n", "Error: malloc() failed.");
					exit(1);
				}
				if(strlen(action_args) > 0)
				{
					if(sprintf(tmp, "%s '%s' '%s' '%s'", action_current_cmd, usernick, channel, action_args) < 0)
					{
						debugf("%s\n", "sprintf() failure.");
						exit(1);
					}
				}
				else
				{
					if(sprintf(tmp, "%s '%s' '%s'", action_current_cmd, usernick, channel) < 0)
					{
						debugf("%s\n", "sprintf() failure.");
						exit(1);
					}
				}
				printf("EXEC: [%s]\n", tmp);
				fp = popen(tmp, "r");
				free(tmp);

				// hard limit of how much we will read from the command
				tmp = malloc(2000);
				for(int i = 0; i < 2000; i++)
				{
					c = fgetc(fp);
					if(c == EOF || i == 1999)
					{
						tmp[i] = '\0';
						break;
					}
					tmp[i] = c;
				}

				debugf("strlen: [%zu] | contains: [%s]\n", strlen(tmp), tmp);
				if(strlen(tmp) > 0)
				{
					cmd_line = strip(tmp);
					if(cmd_line == NULL)
					{
						debugf("%s\n", "strip() cmd_line failure");
						exit(1);
					}
					free(tmp);
					//printf("command line output: %s\n", cmd_line);

					cmd_status = pclose(fp);
					cmd_status = WEXITSTATUS(cmd_status);
					printf("EXEC: exit status: [%d]\n", cmd_status);

					if(cmd_status > 0 && cmd_status < 4 && strlen(cmd_line) > 0)
					{
						// status 1 means output to channel, status 2 means output to user directly, status 3 means output using NOTICE to user
						// we ignore all other statuses

						output_length = explode(&output, cmd_line, "\n");
						if(output_length == -1)
						{
							debugf("%s\n", "Error explode() cmd_line output");
							exit(1);
						}
						else
						{
							for(size_t i = 0; i < output_length; i++)
							{
								if(cmd_status == 1)
								{
									if(sprintf(send, "PRIVMSG %s :%s", channel, output[i]) < 0)
									{
										debugf("%s\n", "sprintf() failure.");
										exit(1);
									}
								}
								if(cmd_status == 2)
								{
									if(sprintf(send, "PRIVMSG %s :%s", usernick, output[i]) < 0)
									{
										debugf("%s\n", "sprintf() failure.");
										exit(1);
									}
								}
								if(cmd_status == 3)
								{
									if(sprintf(send, "NOTICE %s :%s", usernick, output[i]) < 0)
									{
										debugf("%s\n", "sprintf() failure.");
										exit(1);
									}
								}
								printf("SEND: [%s]\n", send);
								write_line(sock, send);
							}
							for(size_t i = 0; i < output_length; i++)
							{
								free(output[i]);
							}
							free(output);
						}
					}
					else
					{
						printf("%s\n", "EXEC: command had no output");
					}
					free(cmd_line);
				}
				else
				{
					free(tmp);
					cmd_status = pclose(fp);
					cmd_status = WEXITSTATUS(cmd_status);
					printf("EXEC: exit status: [%d]\n", cmd_status);
					printf("%s\n", "EXEC: command had no output");
				}
			}
			else
			{
				printf("%s\n", "shell arguments are NOT safe.");
			}
		}
		free(action_current);
		free(action_current_cmd);
	}

	// free list
	for(size_t i = 0; i < actions_length; i++)
	{
		free(actions[i]);
	}
	free(actions);
	free(config_actions);
}

// process http:// and https:// links appearing in chat
void do_http(int sock, const char *usernick, const char *channel, const char *chat)
{
	char *tmp1;
	char *tmp2;
	char **parts;
	ssize_t parts_length;

	// we need to isolate the http:// or https:// url from chat, and pass it and it alone to do_action()

	// identify what kind of url we have
	// we don't really have to be this specific, but better safe than sorry.
	if(strstr(chat, "http://") != NULL)
	{
		tmp1 = strdup(strstr(chat, "http://"));
	}
	else if(strstr(chat, "https://") != NULL)
	{
		tmp1 = strdup(strstr(chat, "https://"));
	}
	if(tmp1 == NULL)
	{
		debugf("%s\n", "Error: malloc() failure.");
		exit(1);
	}

	//printf("url so far: [%s]\n", tmp1);

	// explode the line we just read into parts.
	parts_length = explode(&parts, tmp1, " ");
	free(tmp1);
	if(parts_length == -1)
	{
		fprintf(stderr, "ERROR: failed exploding parts: %s\n", strerror(errno));
		exit(1);
	}
	// grab the first part and strip it
	tmp2 = strip(parts[0]);
	if(tmp2 == NULL)
	{
		debugf("%s\n", "part0 strip() failure.");
		exit(1);
	}
	for(size_t i = 0; i < parts_length; i++)
	{
		free(parts[i]);
	}
	free(parts);

	debugf("url extracted: [%s]\n", tmp2);
	do_action(sock, usernick, channel, "http", tmp2);
	free(tmp2);
}

int main()
{
	char *tmp;
	char *line;
	char *chat;
	char **parts;
	ssize_t parts_length;
	char *part0;
	char *part1;
	char **command;
	ssize_t command_length;
	char *action;
	char *action_args;
	char *usernick;
	size_t line_length;
	int sock;
	unsigned long loop;

	char *config_host;
	char *config_port;
	char *config_user;
	char *config_password;
	char *config_channel;

	config_host = config("HOST");
	config_port = config("PORT");
	config_user = config("USER");
	config_password = config("PASSWORD");
	config_channel = config("CHANNEL");

	sock = init_connection(config_host, atoi(config_port));

	loop = 0;
	// enter the main program loop
	while(1)
	{
		printf("LOOP: [%lu]\n", loop);

		if(loop < 20)
		{
			if(loop == 0)
			{
				line_length = strlen(config_user) + 1; // dynamic part
				line_length = line_length + 5; // add our static part
				line = malloc(line_length);
				if(line == NULL)
				{
					debugf("%s\n", "malloc() memory allocation failure.");
					exit(1);
				}
				if(sprintf(line, "NICK %s", config_user) < 0)
				{
					debugf("%s\n", "sprintf() failure.");
					exit(1);
				}
				printf("SEND: [%s]\n", line);
				write_line(sock, line);
				goto loop_end0;
			}
			if(loop == 1)
			{
				line_length = 5 * (strlen(config_user) + 1); // dynamic part
				line_length = line_length + 11; // add our static part
				line = malloc(line_length);
				if(line == NULL)
				{
					debugf("%s\n", "malloc() memory allocation failure.");
					exit(1);
				}
				if(sprintf(line, "USER %s %s %s %s :%s", config_user, config_user, config_user, config_user, config_user) < 0)
				{
					debugf("%s\n", "sprintf() failure.");
					exit(1);
				}
				printf("SEND: [%s]\n", line);
				write_line(sock, line);
				goto loop_end0;
			}
			// these next 2 below are an ugly hack.
			// ideally we would read the status codes the server is sending
			// and respond at the proper time.
			// instead, we just guess when the server should be ready to recieve
			// our commands to IDENTIFY and JOIN
			// if we send them too early the server may ignore them, however
			// we don't want to delay them too long either.
			if(loop == 18)
			{
				line_length = strlen(config_password) + 1; // dynamic part
				line_length = line_length + 27; // add our static part
				line = malloc(line_length);
				if(line == NULL)
				{
					debugf("%s\n", "malloc() memory allocation failure.");
					exit(1);
				}
				if(sprintf(line, "PRIVMSG NickServ :IDENTIFY %s", config_password) < 0)
				{
					debugf("%s\n", "sprintf() failure.");
					exit(1);
				}
				printf("SEND: [%s]\n", line);
				write_line(sock, line);
				goto loop_end0;
			}
			if(loop == 19)
			{
				line_length = strlen(config_channel) + 1; // dynamic part
				line_length = line_length + 5; // add our static part
				line = malloc(line_length);
				if(line == NULL)
				{
					debugf("%s\n", "malloc() memory allocation failure.");
					exit(1);
				}
				if(sprintf(line, "JOIN %s", config_channel) < 0)
				{
					debugf("%s\n", "sprintf() failure.");
					exit(1);
				}
				printf("SEND: [%s]\n", line);
				write_line(sock, line);
				goto loop_end0;
			}
		}

		// read a line from our socket
		line = read_line(sock);
		if(line == NULL)
		{
			fprintf(stderr, "%s\n", "ERROR: failed reading line from socket");
			exit(1);
		}

		if(strcmp(line, "") == 0)
		{
			debugf("%s\n", "READ: got empty line, skipping...");
			goto loop_end0;
		}

		printf("READ: [%s]\n", line);

		// explode the line we just read into parts.
		parts_length = explode(&parts, line, ":");
		//printf("parts_length: [%zd]\n", parts_length);
		if(parts_length == -1)
		{
			fprintf(stderr, "ERROR: failed exploding parts: %s\n", strerror(errno));
			exit(1);
		}

		// grab the first part and strip it
		part0 = strip(parts[0]);
		if(part0 == NULL)
		{
			debugf("%s\n", "part0 strip() failure.");
			exit(1);
		}
		//printf("PART0: %s\n", part0);

		// handle PING / PONG
		if(strcmp(part0, "PING") == 0)
		{
			tmp = strdup(strstr(line, ":"));
			if(tmp == NULL)
			{
				debugf("%s\n", "strdup() memory allocation failure.");
				exit(1);
			}
			free(line);

			// this cuts off the ":" that strstr matched on.
			line = ltrim(tmp, ":");
			if(line == NULL)
			{
				debugf("%s\n", "ltrim() failure.");
				exit(1);
			}
			free(tmp);

			// strip off any whitespace
			tmp = strip(line);
			if(tmp == NULL)
			{
				debugf("%s\n", "strip() failure.");
				exit(1);
			}
			free(line);

			// tmp should now contain what we need to answer PONG with.

			line_length = strlen(tmp) + 1; // dynamic part
			line_length = line_length + 6; // add our static part
			line = malloc(line_length);
			if(line == NULL)
			{
				debugf("%s\n", "malloc() memory allocation failure.");
				exit(1);
			}
			if(sprintf(line, "PONG :%s", tmp) < 0)
			{
				debugf("%s\n", "sprintf() failure.");
				exit(1);
			}
			free(tmp);
			printf("SEND: [%s]\n", line);
			write_line(sock, line);
			goto loop_end1;
		}

		// finally, handle chat messages
		if(strstr(part0, "PRIVMSG") != NULL || strstr(part0, "NOTICE") != NULL)
		{
			debugf("%s\n", "PRIVMSG or NOTICE detected!");
			// grab the second part and strip it
			// since we have PRIVMSG or NOTICE, we can be assured that part1 exists.
			part1 = strip(parts[1]);
			if(part1 == NULL)
			{
				debugf("%s\n", "part1 strip() failure.");
				exit(1);
			}
			//printf("PART1: %s\n", part1);

			// grab the usernick of whomever is talking
			tmp = reverse(part0);
			usernick = reverse(strstr(tmp, "!"));
			free(tmp);
			tmp = rtrim(usernick, "!");
			free(usernick);
			usernick = tmp;

			// this is safe to do here, even though it looks dangerous.
			chat = strdup(strstr(line + 1, ":") + 1);
			if(chat == NULL)
			{
				debugf("%s\n", "Error: strdup() chat failure.");
				exit(1);
			}
			printf("usernick: [%s] said [%s]\n", usernick, chat);

			if(part1[0] == '.')
			{
				debugf("%s\n", "found a command for the bot!");
				if(strstr(part1, " ") == NULL)
				{
					debugf("%s\n", "no arguments for the command found.");
					action = ltrim(part1, ".");
					if(action == NULL)
					{
						debugf("%s\n", "action ltrim() failure.");
						exit(1);
					}

					do_action(sock, usernick, config_channel, action, "");
					free(action);
				}
				if(strstr(part1, " ") != NULL)
				{
					debugf("%s\n", "found arguments for the command.");

					// explode part1.
					command_length = explode(&command, part1, " ");
					if(command_length == -1)
					{
						fprintf(stderr, "ERROR: failed exploding command: %s\n", strerror(errno));
						exit(1);
					}

					// grab the first part and strip it
					tmp = strip(command[0]);
					if(tmp == NULL)
					{
						debugf("%s\n", "tmp (action) strip() failure.");
						exit(1);
					}

					// free command
					for(size_t i = 0; i < command_length; i++)
					{
						free(command[i]);
					}
					free(command);

					action = ltrim(tmp, ".");
					if(action == NULL)
					{
						debugf("%s\n", "action ltrim() failure.");
						exit(1);
					}
					free(tmp);

					// get the arguments
					action_args = strip(strstr(part1, " "));
					if(action_args == NULL)
					{
						debugf("%s\n", "action_args strip() failure.");
						exit(1);
					}

					do_action(sock, usernick, config_channel, action, action_args);
					free(action);
					free(action_args);
				}
			}
			if(strstr(chat, "http:") != NULL || strstr(chat, "https:") != NULL)
			{
				// handle http links in chat
				debugf("http link found in [%s]!\n", chat);
				do_http(sock, usernick, config_channel, chat);
			}
			free(part1);
			free(usernick);
			free(chat);
		}

		loop_end1:
		for(size_t i = 0; i < parts_length; i++)
		{
			free(parts[i]);
		}
		free(parts);
		free(part0);

		loop_end0:
		free(line);
		loop++;
	}

	free(config_host);
	free(config_port);
	free(config_user);
	free(config_password);
	free(config_channel);

	return 0;
}
