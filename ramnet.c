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

// this file contains generic function interfaces that would be useful in other projects
// and not only specific to rambot.

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include "ramnet.h"

// performs a dns lookup on input and returns an IP address
// returns NULL on failure.
char * dnslookup(const char *input)
{
	struct hostent *h = gethostbyname(input);
	struct in_addr a;
	if(h == NULL)
	{
		herror("gethostbyname");
	}
	if(h->h_addrtype == AF_INET)
	{
		printf("name: %s\n", h->h_name);
		while (*h->h_aliases)
		printf("alias: %s\n", *h->h_aliases++);
		while(*h->h_addr_list)
		{
			memmove((char *) &a, *h->h_addr_list++, sizeof(a));
			printf("address: %s\n", inet_ntoa(a));
			return inet_ntoa(a);
		}
	}

	return NULL;
}

// opens a TCP connection to hostname on port
// returns a socket file descriptor ready to use with send() and read() etc
int init_connection(const char *hostname, int port)
{
	int sock;
	struct sockaddr_in serv_addr;
	struct timeval timeout;
	sock = 0;

	char * host;
	host = dnslookup(hostname);
	if(host == NULL)
	{
		fprintf(stderr, "hostname lookup failure!\n");
		return -1;
	}
	printf("dnslookup: %s\n", host);

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock < 0)
	{
		printf("\n Socket creation error \n");
		return -1;
	}

	// timeout after 10 minutes
	timeout.tv_sec = 600;
	timeout.tv_usec = 0;

	if(setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0
	|| setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0)
	{
		printf("setsockopt failed\n");
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);

	// Convert IPv4 and IPv6 addresses from text to binary form
	if(inet_pton(AF_INET, host, &serv_addr.sin_addr) <= 0)
	{
		printf("\nInvalid address/ Address not supported \n");
		return -1;
	}

	if(connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		printf("\nConnection Failed \n");
		return -1;
	}
	return sock;
}

// note: because this uses strtok(), if we do something like:
// explode(&output, ":this:string", ":")
// then output[0] will contain "this" rather than "" as you might expect.
ssize_t explode(char ***output, const char *input, const char *tokens)
{
	// lazy sanity check of all function arguments
	if(tokens == NULL || input == NULL || output == NULL)
	{
		errno = EINVAL;
		return -1;
	}

	// declare variables used in this function
	size_t length;
	size_t needed;
	char *str;
	char *copy_input;
	char **new_output;
	char **tmp;

	*output = NULL;
	length = 0;

	// we need to make a copy of input that's safe to feed into strtok()
	copy_input = strdup(input);
	if(copy_input == NULL)
	{
		debugf("%s\n", "strdup() memory allocation failure.");
		return -1;
	}

	new_output = malloc(sizeof(*new_output));
	if(new_output == NULL)
	{
		debugf("%s\n", "malloc() memory allocation failure");
		free(copy_input);
		return -1;
	}

	// get the first token. this should always succeed.
	str = strtok(copy_input, tokens);
	if(str == NULL)
	{
		// this should never happen, ever.
		debugf("%s\n", "strtok() critical unexpected behavior.");
		free(copy_input);
		return -1;
	}

	// copy the first result into our list.
	new_output[length] = strdup(str);
	if(new_output[length] == NULL)
	{
		debugf("%s\n", "strdup() memory allocation failure.");
		free(copy_input);
		return -1;
	}

	length++;

	// see if we have any more results and process them.
	while(1)
	{
		// get the next result.
		str = strtok(NULL, tokens);

		// do we have any more results to handle?
		if(str == NULL)
		{
			debugf("found %zu tokens, breaking out of loop.\n", length);
			break;
		}

		// try to allocate memory to store another result.
		needed = sizeof(*new_output) * (length + 1);
		debugf("realloc() %zu bytes to new_output\n", needed);
		tmp = realloc(new_output, needed);
		if(tmp == NULL)
		{
			debugf("%s\n", "realloc() memory allocation failure");
			free(copy_input);
			return -1;
		}
		new_output = tmp;

		// try and copy the result into our list.
		new_output[length] = strdup(str);
		if(new_output[length] == NULL)
		{
			debugf("%s\n", "strdup() memory allocation failure.");
			free(copy_input);
			return -1;
		}

		length++;
	}

	*output = new_output;
	free(copy_input);
	return length;
}

ssize_t file_get_contents(char **output, const char *filename)
{
	// lazy sanity check of all function arguments
	if(output == NULL || filename == NULL)
	{
		errno = EINVAL;
		return -1;
	}

	// declare variables used in this function
	size_t cur_len;
	size_t length;
	size_t needed;
	char *new_output;
	char *tmp;
	FILE *fp;
	int c;

	// try to open filename for reading
	fp = fopen(filename, "r");
	if(fp == NULL)
	{
		return -1;
	}

	*output = NULL;
	cur_len = 0;

	// how big to make our initial malloc().
	// since we are going to be doubling this later, this should be an even number > 0.
	length = 2;

	// malloc() some space so we can get started.
	new_output = malloc(length);
	if(new_output == NULL)
	{
		fclose(fp);
		return -1;
	}

	// iterate over fp one character at a time until we reach the end
	while(1)
	{
		c = getc(fp);
		if(c == EOF)
		{
			break;
		}
		// do we need to realloc() bigger?
		if(cur_len + 1 >= length)
		{
			// whatever space we need, double it.
			needed = (length + 1) * 2;
			debugf("realloc: needed: [%zu] | cur_len: [%zu] | length: [%zu]\n", needed, cur_len, length);
			tmp = realloc(new_output, needed);
			if(tmp == NULL)
			{
				debugf("%s\n", "realloc() memory allocation failure");
				free(new_output);
				fclose(fp);
				return -1;
			}
			new_output = tmp;

			length = needed;
		}
		new_output[cur_len] = c;
		cur_len++;
	}
	fclose(fp);
	new_output[cur_len] = '\0';

	// realloc smaller so our result matches what we need
	tmp = realloc(new_output, cur_len + 1);
	if(tmp == NULL)
	{
		debugf("%s\n", "realloc() memory allocation failure");
		free(new_output);
		return -1;
	}
	new_output = tmp;

	*output = new_output;

	// return how many bytes we've read.
	return cur_len;
}

// test if filename is readable
// returns 1 on success, or 0 on failure
ssize_t is_readable(const char *filename)
{
	// lazy sanity check of all function arguments
	if(filename == NULL)
	{
		errno = EINVAL;
		return -1;
	}
	FILE *fp;
	fp = fopen(filename, "r");
	if(fp == NULL)
	{
		// error: file could not be opened for reading
		return 0;
	}
	fclose(fp);
	return 1;
}

/* string reversal */
char * reverse(const char *input)
{
	// lazy sanity check of all function arguments
	if(input == NULL)
	{
		errno = EINVAL;
		return NULL;
	}

	char *tmp;
	char *output;
	ssize_t y;
	ssize_t x;

	y = strlen(input);

	output = malloc(y + 1);
	if(output == NULL)
	{
		debugf("%s\n", "malloc() memory allocation failure.");
		errno = ENOMEM;
		return NULL;
	}

	x = 0;

	// skip the null byte at the end of input
	y--;

	// make sure we actually have something to work with
	if(y < 0)
	{
		output[0] = '\0';
		return output;
	}

	while(1)
	{
		output[x] = input[y];
		debugf("Loop: x = [%zd] | y = [%zd] | char at input[%zd] = [%c] | char at output[%zd] = [%c]\n", x, y, y, input[y], x, output[x]);
		x++;
		y--;
		if(y == -1)
		{
			output[x] = '\0';
			break;
		}
	}

	return output;
}

/* trim tokens from the end of a string. */
char * rtrim(const char *input, const char *tokens)
{
	// lazy sanity check of all function arguments
	if(input == NULL || tokens == NULL)
	{
		errno = EINVAL;
		return NULL;
	}

	char *output;
	char *tmp;

	// copy input into output
	output = strdup(input);
	if(output == NULL)
	{
		debugf("%s\n", "strdup() memory allocation failure.");
		return NULL;
	}

	int trim = 0;
	int shorten = 0;

	// skip over the null byte string terminator, no point testing that for tokens.
	int x = strlen(output) - 1;

	// make sure we actually have something to work with
	if(x < 0)
	{
		output[0] = '\0';
		return output;
	}

	// iterate from the end of the string going backwards
	// replacing trimed characters with null bytes, truncating the string as we go.
	while(1)
	{
		// check if any of the tokens are in this character position.
		for(int i = 0; i < strlen(tokens); i++)
		{
			if(output[x] == tokens[i])
			{
				debugf("Loop: x = [%d] | i = [%d] | char at output[%d] = [%c] matched char at tokens[%d] = [%c]\n", x, i, x, output[x], i, tokens[i]);
				trim = 1;
				shorten = 1;
			}
		}

		// none of the tokens matched.
		if(trim == 0) { break; }

		// one of our tokens matched, overwrite it with null byte.
		output[x] = '\0';

		// reset trim flag
		trim = 0;

		x--;

		// no more characters left
		if(x < 0) { break; }
	}

	if(shorten == 1)
	{
		debugf("%s\n", "characters were trimmed, shortening the string memory.");
		tmp = realloc(output, strlen(output) + 1);
		if(tmp == NULL)
		{
			debugf("%s\n", "realloc() memory allocation failure");
			free(output);
			return NULL;
		}
		output = tmp;
	}

	return output;
}

/* trim tokens from the start of a string. */
char * ltrim(const char *input, const char *tokens)
{
	// lazy sanity check of all function arguments
	if(input == NULL || tokens == NULL)
	{
		errno = EINVAL;
		return NULL;
	}

	char *output;
	char *tmp1;
	char *tmp2;
	size_t length = strlen(input) + 1;

	// reverse input, feed it into rtrim, and reverse the result.
	tmp1 = reverse(input);
	tmp2 = rtrim(tmp1, tokens);
	free(tmp1);
	output = reverse(tmp2);
	free(tmp2);
	return output;
}

/* trim tokens from both ends of a string. */
char * trim(const char *input, const char *tokens)
{
	// lazy sanity check of all function arguments
	if(input == NULL || tokens == NULL)
	{
		errno = EINVAL;
		return NULL;
	}

	char *output;
	char *tmp;
	size_t length = strlen(input) + 1;

	// pass into rtrim() and ltrim()
	tmp = rtrim(input, tokens);
	output = ltrim(tmp, tokens);
	free(tmp);

	return output;
}

/* strip whitespace from both ends of a string. */
char * strip(const char *input)
{
	// lazy sanity check of all function arguments
	if(input == NULL)
	{
		errno = EINVAL;
		return NULL;
	}

	char *output;

	// pass into trim, using whitespaces for tokens
	output = trim(input, " \n\r\t\f\v");

	return output;
}

int write_line(int sock, const char *line)
{
	size_t length;
	size_t i;
	char buf[2];

	i = 0;
	length = strlen(line);
	strcpy(buf, "0"); // sanity check - populate with dummy data

	while(1)
	{
		buf[0] = line[i];
		if(write(sock, buf, 1) < 1)
		{
			debugf("%s\n", "socket failure. write failed.");
			return 0;
		}
		i++;
		if(line[i] == '\0') { break; }
	}

	if(write(sock, "\r\n", 2) < 1)
	{
		debugf("%s\n", "socket failure. write failed.");
		return 0;
	}

	return 1;
}

char * read_line(int sock)
{
	int c;
	char buf[2];
	char *output;
	char *tmp;
	size_t length;
	size_t cur_len;
	size_t needed;

	cur_len = 0;

	// how big to make our initial malloc().
	// since we are going to be doubling this later, this should be an even number > 0.
	length = 2;

	output = malloc(length);

	while(1)
	{
		// read 1 byte.
		// make sure we actually get our 1 byte too.
		if(read(sock, buf, 1) != 1)
		{
			debugf("%s\n", "socket failure. read failed.");
			free(output);
			return NULL;
		}

		// convert buf to int.
		c = buf[0];

		// are we at the end of the line?
		if(c == '\n' || c == '\r')
		{
			// yes we are
			output[cur_len] = '\0';
			break;
		}

		// do we need to realloc() ?
		if(cur_len + 1 >= length)
		{
			// whatever space we need, double it.
			needed = (length + 1) * 2;
			debugf("realloc: needed: [%zu] | cur_len: [%zu] | length: [%zu]\n", needed, cur_len, length);
			tmp = realloc(output, needed);
			if(tmp == NULL)
			{
				debugf("%s\n", "realloc() memory allocation failure");
				free(output);
				return NULL;
			}
			output = tmp;
			length = needed;
		}

		// store our 1 character
		output[cur_len] = c;
		cur_len++;
	}

	needed = strlen(output) + 1;
	debugf("shortening the string memory to [%zu]\n", needed);
	tmp = realloc(output, needed);
	if(tmp == NULL)
	{
		debugf("%s\n", "realloc() memory allocation failure");
		free(output);
		return NULL;
	}
	output = tmp;

	return output;
}

/* generic config file parser. sample config file follows:
# this is a comment in the config file that will be ignored
# we also properly handle commented out entries
# KEY = value
FIRST_KEY = VALUE
ANOTHER_KEY = another value

# KEY can be any sequence of characters except whitespace or =
# each KEY must be unique, and not contain parts of other KEY definitions.
KEY = bad choice of name
ANOTHER_KEY = undefined behavior

# any arbitrary whitespace around KEY and VALUE will be ignored and removed.
# any error processing, or if KEY is not found, we return NULL
# processing of VALUE stops when a newline is found, so this is wrong:
MULTILINE_KEY = this is what will be found
this will be ignored and not included. high risk of causing a parser error.

# values containing direct matches to KEY is undefined behavior.
# for this reason, it is recommended to use verbose descriptive all caps 
# for key names so you won't accidentially make this error.
first key = don't put another key here
another key = undefined behavior

# if KEY appears multiple times, the first match will always be used.
KEY = this value will be used
KEY = this value will not be used

# both of these have undefined behavior, don't do it.
KEY
KEY = 

# this crazyness is correct, but for god sakes don't do strange things like this.
# parse_config("filename", "this is MY,STRANGE|KEY;") will work on the following:
this is MY,STRANGE|KEY;= anything
*/
char * parse_config(const char *filename, const char *search)
{
	// lazy sanity check of all function arguments
	if(filename == NULL || search == NULL)
	{
		errno = EINVAL;
		return NULL;
	}

	if(is_readable(filename) != 1)
	{
		debugf("Error reading %s: %s\n", filename, strerror(errno));
		return NULL;
	}
	char *tmp1;
	char *tmp2;
	char *result = NULL;

	char *config_raw;
	char **config;
	ssize_t config_length;

	config_length = file_get_contents(&config_raw, filename);
	if(config_length == -1)
	{
		debugf("ERROR: config file_get_contents failed for %s\n", filename);
		return NULL;
	}

	config_length = explode(&config, config_raw, "\n");
	if(config_length == -1)
	{
		debugf("ERROR: config explode lines failed for %s\n", config_raw);
		return NULL;
	}
	free(config_raw);

	// loop over each line looking for search
	for(size_t i = 0; i < config_length; i++)
	{
		debugf("[%zu] => [%s]\n", i, config[i]);

		// skip processing if this line is a comment.
		if(config[i][0] == '#')
		{
			continue;
		}

		if(strstr(config[i], search) != NULL)
		{
			// found it!
			tmp1 = strdup(strstr(config[i], "="));
			debugf("Found: %s\n", tmp1);

			// this cuts off the "=" that strstr matched on.
			tmp2 = ltrim(tmp1, "=");
			if(tmp2 == NULL)
			{
				debugf("ltrim [%s] from [%s] failed.", "=", tmp1);
				return NULL;
			}
			free(tmp1);

			// strip off any whitespace
			tmp1 = strip(tmp2);
			if(tmp1 == NULL)
			{
				debugf("ltrim strip from [%s] failed.", tmp2);
				return NULL;
			}
			free(tmp2);

			result = tmp1;
			break;
		}
	}

	// free config
	for(size_t i = 0; i < config_length; i++)
	{
		free(config[i]);
	}
	free(config);

	return result;
}

// this function returns 1 if the str is safe, 0 otherwise.
// safe means safe to pass into the shell as a single quoted 'command argument.'
// this means ascii characters 32 through 126, excluding 39 (the single quote itself)
int is_shell_safe(const char *str)
{
	size_t length = strlen(str) - 1;
	size_t i = 0;
	while(1)
	{
		if(str[i] == '\0')
		{
			break;
		}
		if(str[i] > 31 && str[i] < 127 && str[i] != 39)
		{
			i++;
			continue;
		}
		return 0;
	}
	return 1;
}
