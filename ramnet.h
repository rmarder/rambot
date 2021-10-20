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

// this library conforms to ISO/IEC 9899:1999 (C99) and IEEE Std 1003.1-2008 (POSIX.1).
// primary development happened on Ubuntu GNU/Linux 20.04
// if you run this somewhere else, good luck. YMMV.

// the network stuff is not encrypted and probably never will be.
// if you need ssl/tls use a simple proxy, such as socat
// socat TCP-LISTEN:6667,fork,reuseaddr OPENSSL:remote-server:7000,verify=0

// this will print a lot of debug to stderr. set DEBUG 0 for release.
#define DEBUG 0
#define debugf(fmt, ...) do { if(DEBUG) fprintf(stderr, "DEBUG %s:%d:%s(): " fmt, __FILE__, __LINE__, __func__, __VA_ARGS__); } while(0)

extern ssize_t file_get_contents(char **output, const char *filename);
extern ssize_t explode(char ***list, const char *src, const char *tokens);
extern ssize_t is_readable(const char *filename);
extern char * reverse(const char *input);
extern char * rtrim(const char *input, const char *tokens);
extern char * ltrim(const char *input, const char *tokens);
extern char * trim(const char *input, const char *tokens);
extern char * strip(const char *input);
extern int init_connection(const char *hostname, int port);
extern char * dnslookup(const char *input);
extern char * read_line(int sock);
extern int write_line(int sock, const char *line);
extern char * parse_config(const char *filename, const char *search);
extern int is_shell_safe(const char *str);
