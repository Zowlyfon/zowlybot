/*-
 * zowlybot.c - A simple IRC bot written in C.
 * Copyright (C) 2016  Zowlyfon
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <curl/curl.h>

#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXDATASIZE 4096

void*
get_in_addr(struct sockaddr* sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

struct memory_struct {
    char* memory;
    size_t size;
};

int bot_connect (char* server, const char* port, int *sockfd);
int bot_setup   (int sockfd, char* nick_cmd, char* user_cmd, char* auth_cmd);
int bot_join    (int sockfd, char** channels, int nchannels);
int bot_send    (int sockfd, char *buf);
int bot_recv    (int sockfd, char *buf);
int bot_catch   (char* buf, char* call, char* catch);
int bot_listcmp (char* item, char** list, int n);
int bot_strlist (char* file, char*** list);
int bot_fileapp (char* file, char* string);
int bot_token   (char* del, char* string, char*** list);
int bot_furl    (char* url, char** data);
int bot_getpos  (char* buf, char** pos, char* callname, char* command);

uint64_t is_prime(uint64_t number);

static size_t bot_curl_callback(void* contents, size_t size, size_t nmemb,
    void* userp);

int
main(int argc, char* argv[])
{
    int sockfd, numbytes, temp_int;
    char buf[MAXDATASIZE];

    char* pos;
    char out[MAXDATASIZE];
    
    const char* port = "6667";
    
    char bot_name[] = "zowlybot";
    char owner_name[] = "Zowlyfon";
    char owner_host[] = ":Zowlyfon!zowlyfon@user/zowlyfon";
    char call_name[] = ":zb-";
    char* channel = argv[3];

    char* nick_cmd = (char*)malloc(sizeof(char) * 
        (strlen(bot_name) + strlen("NICK \r\n")));
    sprintf(nick_cmd, "NICK %s\r\n", bot_name);

    char* user_cmd = (char*)malloc(sizeof(char) *
        (strlen(bot_name) * 2 + strlen("USER  0 * : \r\n")));
    sprintf(user_cmd, "USER %s 0 * : %s\r\n", bot_name, bot_name);
    
    char* auth_cmd = (char*)malloc(sizeof(char) *
        (strlen(argv[2]) + strlen("PRIVMSG NICKSERV IDENTIFY \r\n")));
    sprintf(auth_cmd, "PRIVMSG NICKSERV IDENTIFY %s\r\n", argv[2]);

    char** channels;
    int nchannels = bot_strlist("channels.txt", &channels);

    char** memes;
    int nmemes = bot_strlist("memes.txt", &memes);

    char** banned;
    int nbanned = bot_strlist("ban.txt", &banned);

    char** ops;
    int nops = bot_strlist("ops.txt", &ops);

    char** buf_tok = NULL;
    int nbuftok = 0;

    char isgd[] = "https://is.gd/create.php?format=simple&url=";
    char* buf2 = NULL;
    char* compare = NULL;
    char* data = NULL;
    char* url = NULL;
    int i;

    char* endptr;
    uint64_t temp_llint;

    curl_global_init(CURL_GLOBAL_ALL);

    /* Make sure enough arguments are supplies */

    if (argc != 3) {
        fprintf(stderr, "usage: Server password\n");
        exit(1);
    }

    /* Connect to the IRC server */

    bot_connect(argv[1], port, &sockfd);
    bot_setup(sockfd, nick_cmd, user_cmd, auth_cmd);
    sleep(2);
    bot_join(sockfd, channels, nchannels);

    /* Program master loop */
    while (1) {
        sleep(1);
        buf[0] = 0;

        if ((numbytes = recv(sockfd, buf, MAXDATASIZE - 1, 0)) == -1) {
            perror("recv");
            exit(1);
        }

        if (numbytes > 0) {
            buf[numbytes] = '\0';
            printf("recv: %s\n", buf);

            /* Check for a PING */

            if (strlen(buf) > 3) {
                if (strncmp(buf, "PING", 4) == 0) {
                    out[0] = 0;
                    pos = strstr(buf, " ") + 1;
                    sprintf(out, "PONG %s\r\n", pos);
                
                    bot_send(sockfd, out);
                    continue;
                }
            }

            if (strlen(buf) > 19) {
                if (strncmp(buf, "ERROR :Closing link:", 20) == 0) {
                    close(sockfd);
                    bot_connect(argv[1], port, &sockfd);
                    bot_setup(sockfd, nick_cmd, user_cmd, auth_cmd);
                    bot_join(sockfd, channels, nchannels);
                    continue;
                }
            }

            /* Tokenise the buffer */

            for (i = 0; i < nbuftok; i++) {
                free(buf_tok[i]);
            }
            free(buf_tok);
            free(buf2);
            free(compare);

            buf2 = (char*)malloc(sizeof(char) * (strlen(buf) + 1));
            strcpy(buf2, buf);

            nbuftok = bot_token(" @!", buf2, &buf_tok);

            if (nbuftok < 6) {
                continue;
            }

            channel = buf_tok[4];

            if (strncmp(channel, bot_name, strlen(bot_name)) == 0) {
                channel = buf_tok[0] + 1;
            }
            
            compare = (char*)malloc(sizeof(char) * (strlen(buf_tok[2]) + 1));
            sprintf(compare, "%s\r\n", buf_tok[2]);

            if (bot_listcmp(compare, banned, nbanned) == 0) {
                printf("%s banned\n", buf_tok[2]);
                continue;
            }

            if (bot_listcmp(compare, ops, nops) == 0) {
                printf("%s op\n", buf_tok[2]);
            }

            if (strncmp(buf, "ERROR :Closing link:", 20) == 0) {
                close(sockfd);
                bot_connect(argv[1], port, &sockfd);
                bot_setup(sockfd, nick_cmd, user_cmd, auth_cmd);
                bot_join(sockfd, channels, nchannels);
                continue;
            }

            if (bot_catch(buf_tok[5], call_name, "die") == 0 &&
                    bot_listcmp(compare, ops, nops) == 0) {
                break;
            }

            if (bot_catch(buf_tok[5], call_name, "say") == 0 &&
                    bot_listcmp(compare, ops, nops) == 0) {
                out[0] = 0;
                pos = strstr(buf, "PRIVMSG");
                pos = strstr(pos, ":") + strlen(call_name) + 
                    strlen("say") + 1;
                
                sprintf(out, "PRIVMSG %s :%s\r\n", channel, pos);

                bot_send(sockfd, out);
                continue;
            }

            if (bot_catch(buf_tok[5], call_name, "command") == 0 &&
                    bot_listcmp(compare, ops, nops) == 0) {
                out[0] = 0;
                pos = strstr(buf, "PRIVMSG");
                pos = strstr(pos, ":") + strlen(call_name) + 
                    strlen("command") + 1;
                sprintf(out, "%s\r\n", pos);
                bot_send(sockfd, out);
                continue;
            }

            if (bot_catch(buf_tok[5], call_name, "join") == 0) {
                if (nbuftok < 7) {
                    continue;
                }
                out[0] = 0;
                sprintf(out, "JOIN %s\r\n", buf_tok[6]);
                bot_send(sockfd, out);
                continue;
            }

            if (bot_catch(buf_tok[5], call_name, "part") == 0) {
                out[0] = 0;
                sprintf(out, "PART %s\r\n", channel);
                bot_send(sockfd, out);
                continue;
            }

            if (bot_catch(buf_tok[5], call_name, "quit") == 0 &&
                    bot_listcmp(compare, ops, nops) == 0) {
                out[0] = 0;
                
                sprintf(out, "QUIT\r\n");
                bot_send(sockfd, out);
                continue;
            }

            if (bot_catch(buf_tok[5], call_name, "newmeme") == 0 &&
                    bot_listcmp(compare, ops, nops) == 0) {
                out[0] = 0;
                pos = strstr(buf, "PRIVMSG");
                pos = strstr(pos, ":") + strlen(call_name) +
                strlen("newmeme") + 1;
                
                if (bot_fileapp("memes.txt", pos) == 0) {
                    sprintf(out, "PRIVMSG %s :added: %s\r\n", channel, pos);
                    for (i = 0; i < nmemes; i++) {
                        free(memes[i]);
                    }
                    free(memes);
                    nmemes = bot_strlist("memes.txt", &memes);
                } else {
                    sprintf(out, "PRIVMSG %s :failed: %s\r\n", channel, pos);
                }

                bot_send(sockfd, out);
                continue;
            }
            
            if (bot_catch(buf_tok[5], call_name, "ban") == 0 &&
                    bot_listcmp(compare, ops, nops) == 0) {
                if (nbuftok < 7) {
                    continue;
                }
                
                out[0] = 0;
                
                if (bot_fileapp("ban.txt", buf_tok[6]) == 0) {
                    sprintf(out, "PRIVMSG %s :added: %s\r\n", 
                        channel, buf_tok[6]);
                    for (i = 0; i < nbanned; i++) {
                        free(banned[i]);
                    }
                    free(banned);
                    nbanned = bot_strlist("ban.txt", &banned);
                } else {
                    sprintf(out, "PRIVMSG %s :failed: %s\r\n", 
                        channel, buf_tok[6]);
                }

                bot_send(sockfd, out);
                continue;
            }

            if (bot_catch(buf_tok[5], call_name, "op") == 0 &&
                    strncmp(buf, owner_host, strlen(owner_host)) == 0) {
                if (nbuftok < 7) {
                    continue;
                }   

                out[0] = 0;
                
                if (bot_fileapp("ops.txt", buf_tok[6]) == 0) {
                    sprintf(out, "PRIVMSG %s :added: %s\r\n", 
                        channel, buf_tok[6]);
                    for (i = 0; i < nops; i++) {
                        free(ops[i]);
                    }
                    free(ops);
                    nops = bot_strlist("ops.txt", &ops);
                } else {
                    sprintf(out, "PRIVMSG %s :failed: %s\r\n", 
                        channel, buf_tok[6]);
                }

                bot_send(sockfd, out);
                continue;
            }

            if (bot_catch(buf_tok[5], call_name, "addchan") == 0 &&
                    bot_listcmp(compare, ops, nops) == 0) {
                if (nbuftok < 7) {
                    continue;
                }
                
                out[0] = 0;

                if (bot_fileapp("channels.txt", buf_tok[6]) == 0) {
                    sprintf(out, "PRIVMSG %s :added: %s\r\n", 
                        channel, buf_tok[6]);
                    for (i = 0; i < nchannels; i++) {
                        free(channels[i]);
                    }
                    free(channels);
                    nchannels = bot_strlist("channels.txt", &channels);
                } else {
                    sprintf(out, "PRIVMSG %s :failed: %s\r\n", 
                        channel, buf_tok[6]);
                }

                bot_send(sockfd, out);
            }

            if (bot_catch(buf_tok[5], call_name, "owner") == 0) {
                out[0] = 0;
                sprintf(out, "PRIVMSG %s %s\r\n", channel, owner_name);
                bot_send(sockfd, out);
                continue;
            }
            
            if (bot_catch(buf_tok[5], call_name, "url") == 0) {
                if (nbuftok < 7) {
                    continue;
                }
                
                out[0] = 0;
                pos = strstr(buf, "PRIVMSG");
                pos = strstr(pos, ":") + strlen(call_name) +
                    strlen("url") + 2;
                free(url);
                free(data);
                url = (char*)malloc(sizeof(char) * (
                    strlen(isgd) + (strlen(pos) - 2)));
                strncpy(url, isgd, strlen(isgd) + 1);
                strncat(url, pos, strlen(pos) - 2);
                printf("url: %s\n", url);
                bot_furl(url, &data);

                sprintf(out, "PRIVMSG %s :%s\r\n", channel, data);
                bot_send(sockfd, out);
                continue;
            }

            if (bot_catch(buf_tok[5], call_name, "mygit") == 0) {
                out[0] = 0;
                sprintf(out, 
                "PRIVMSG %s :https://github.com/Zowlyfon/zowlybot\r\n",
                channel);
                bot_send(sockfd, out);
                continue;
            }

            if (strstr(buf, "Thailand") != NULL &&
                    strstr(buf, "PRIVMSG") != NULL) {
                out[0] = 0;
                sprintf(out, "PRIVMSG %s :%s\r\n", channel,
                    "s/Thailand/Bang Cock/");
                bot_send(sockfd, out);
                continue;
            }
            
            if (bot_catch(buf_tok[5], call_name, "primality") == 0) {
                if (nbuftok < 7) {

                }

                out[0] = 0;
                
                temp_llint = strtoull(buf_tok[6], &endptr, 10);

                printf("temp_int: %d\n", temp_int);

                if (is_prime(temp_llint) == 0) {
                    sprintf(out, "PRIVMSG %s :%ld is NOT prime\r\n", 
                        channel, temp_llint);
                } else {
                    sprintf(out, "PRIVMSG %s :%ld MIGHT be prime\r\n", 
                        channel, temp_llint);
                }

                bot_send(sockfd, out);
                continue;
            }

            if (bot_catch(buf_tok[5], call_name, "random") == 0) {
                if (nbuftok < 7) {
                    continue;
                }
                
                out[0] = 0;
                temp_int = atoi(buf_tok[6]);
                if (temp_int > 0) {
                    temp_int = rand() % (temp_int + 1);
                }
                sprintf(out, "PRIVMSG %s :%d\r\n", channel, temp_int);
                bot_send(sockfd, out);
                continue;
            }

            if (bot_catch(buf_tok[5], call_name, "distro") == 0) {
                out[0] = 0;
                sprintf(out, "PRIVMSG %s :%s\r\n", channel, "freeBSD");
                bot_send(sockfd, out);
                continue;
            }
            
            if (bot_catch(buf_tok[5], call_name, "memes") == 0) {
                temp_int = rand() % nmemes;
                out[0] = 0;
                sprintf(out, "PRIVMSG %s :%s\r\n", channel, memes[temp_int]);
                bot_send(sockfd, out);
                continue;
            }

            if (bot_catch(buf_tok[5], call_name, "interject") == 0) {
                out[0] = 0;
                sprintf(out,
                "PRIVMSG %s :I'd just like to interject for a moment. What you're refering to as GNU/Linux, is in fact, Systemd/Linux, or as I've taken to calling it, Systemd plus Linux. GNU is not an operating system unto itself, but rather another free component of a fully functioning Systemd.\r\n",
                    channel);
                bot_send(sockfd, out);
                continue;
            }
        }
    }

    curl_global_cleanup();

    free(nick_cmd);
    free(user_cmd);
    free(auth_cmd);
    free(ops);
    free(banned);
    free(memes);
    close(sockfd);

    return 0;
}

/* This function sets up a socket to IRC */
int
bot_connect(char* server, const char* port, int *sockfd)
{    
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(server, port, &hints, &servinfo)) !=0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((*sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }
        if (connect(*sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(*sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr*)p->ai_addr),
        s, sizeof(s));

    printf("client:connecting to %s\n", s);

    freeaddrinfo(servinfo);
    return 0;
}

/* This function sets up a connection to IRC */
int
bot_setup(int sockfd, char* nick_cmd, char* user_cmd, char* auth_cmd)
{
    int n, pinged;
    char* pos;
    char out[MAXDATASIZE];
    char buf[MAXDATASIZE];

    bot_send(sockfd, nick_cmd);
    bot_send(sockfd, user_cmd);
    while (1) {
        if ((n = bot_recv(sockfd, buf)) > 0) {
            buf[n] = '\0';
            printf("recv: %s\n", buf);
            if (strncmp(buf, "PING", 4) == 0 && pinged == 0) {
                pos = strstr(buf, " ") + 1;
                out[0] = 0;
                sprintf(out, "PONG %s\r\n", pos);
                bot_send(sockfd, out);
                pinged = 1;
            }
            if (strstr(buf, "NickServ") != NULL) {
                bot_send(sockfd, auth_cmd);
                printf("Setup Finished.");
                return 0;
            }
        }
    }
    return 1;
}

/* This sends the initial join command */
int
bot_join(int sockfd, char** channels, int nchannels)
{
    char* join_cmd;
    int i;
    for (i = 0; i < nchannels; i++) {
        join_cmd = (char*)malloc(sizeof(char) * (
            strlen(channels[i]) + strlen("JOIN \r\n") + 1));
        sprintf(join_cmd, "JOIN %s\r\n", channels[i]);
        bot_send(sockfd, join_cmd);
    }
    
    free(join_cmd);
    return 0;
}

/* This is a wrapper around send */
int
bot_send(int sockfd, char* buf)
{
    int n;
    if ((n = send(sockfd, buf, strlen(buf), 0)) == -1) {
        perror("send");
        return 1;
    }
    printf("send: %s\n", buf);
    return n;
}

/* This is a wrapper around recv */
int
bot_recv(int sockfd, char* buf) 
{
    int n;
    if ((n = recv(sockfd, buf, MAXDATASIZE, 0)) == -1) {
        perror("recv");
        exit(1);
    }
    return n;
}

/* This function compares a string against the start of a message */
int
bot_catch(char* buf, char* call, char* catch) 
{
    char* call_catch = (char*)malloc(sizeof(char*) * (
        strlen(call) + strlen(catch) + 1));
    strncpy(call_catch, call, strlen(call));
    strncat(call_catch, catch, strlen(catch));
    call_catch[strlen(call) + strlen(catch)] = '\0';

    if (strncmp(buf, call_catch, strlen(call_catch)) == 0) {
        return 0;
        free(call_catch);
    }

    free(call_catch);
    return 1;
}

/* This function compares an array of strings against a single string */
int
bot_listcmp(char* item, char** list, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        if (strncmp(item, list[i], strlen(list[i])) == 0) {
            return 0;
        }
    }
    return 1;
}

/* This function reads a file line by line into an array of strings */
int
bot_strlist(char* file, char*** list)
{
    int i = 0;
    FILE* fp;
    char* line = NULL;
    size_t len = 0;
    ssize_t read;

    fp = fopen(file, "r");
    if (fp == NULL) {
        perror("File Open");
        return 1;
    }

    while ((read = getline(&line, &len, fp)) != -1) {
        i++;
    }

    *list = (char**)malloc(sizeof(char*) * i);
    i = 0;
    rewind(fp);

    while ((read = getline(&line, &len, fp)) != -1) {
        (*list)[i] = (char*)malloc(sizeof(char) * read);
        strncpy((*list)[i], line, strlen(line));
        i++;
    }

    fclose(fp);
    free(line);

    return i;
}

/* This function appends a string to the end of a file */
int
bot_fileapp(char* file, char* string)
{
    FILE* fp;
    fp = fopen(file, "a");
    if (fp == NULL) {
        perror("File Open");
        return 1;
    }

    fprintf(fp, "%s", string);
    fclose(fp);

    return 0;
}

/* This function splits a string into an 
 * array of smaller strings by a token string. 
 */
int
bot_token(char* del, char* string, char*** list)
{
    char* token;
    char* string2 = strdup(string);
    int i = 0;

    while ((token = strsep(&string2, del)) != NULL) {
        i++;
    }

    *list = (char**)malloc(sizeof(char*) * i);
    i = 0;

    while ((token = strsep(&string, del)) != NULL) {
        (*list)[i] = (char*)malloc(sizeof(char) * (strlen(token) + 1));
        strcpy((*list)[i], token);
        i++;
    }

    free(token);
    free(string2);
    return i;
}

int
bot_furl(char* url, char** data)
{
    CURL *ch;
    CURLcode res;

    struct memory_struct chunk;

    chunk.memory = malloc(1);
    chunk.size = 0;

    ch = curl_easy_init();
    curl_easy_setopt(ch, CURLOPT_URL, url);
    curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, bot_curl_callback);
    curl_easy_setopt(ch, CURLOPT_WRITEDATA, (void*)&chunk);
    curl_easy_setopt(ch, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    res = curl_easy_perform(ch);

    if (res != CURLE_OK) {
        perror("Curl Error");
        return -1;
    } else {
        *data = (char*)malloc(chunk.size);
        sprintf(*data, "%s", chunk.memory);
    }

    curl_easy_cleanup(ch);
    free(chunk.memory);
    return chunk.size;
}

int
bot_getpos(char* buf, char** pos, char* callname, char* command)
{
    *pos = strstr(buf, "PRIVMSG");
    *pos = strstr(buf, ":") + strlen(callname) + strlen(command) + 2;
    return 0;
}

static size_t
bot_curl_callback(void* contents, size_t size, size_t nmemb, void* userp)
{
    size_t realsize = size * nmemb;
    struct memory_struct *mem = (struct memory_struct *)userp;

    mem->memory = realloc(mem->memory, mem->size + realsize + 1);
    
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

uint64_t
is_prime(uint64_t number)
{
    if (number <= 3) {
        return number;
    }
    if (number % 2 == 0) {
        return 0;
    }
    if (number % 3 == 0) {
        return 0;
    }
    uint64_t i;
    for (i = 5; i * i <= number; i = i + 6) {
        if (number % i == 0 || number % (i + 2) == 0) {
            return 0;
        }
    }
    return number;
}


