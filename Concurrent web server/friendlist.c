/*
 * Name: Shirley(Shiyang) Li
 * UID:  u1160160
 * friendlist.c - a web-based friend-graph manager.
 *
 * Based on:
 *  tiny.c - A simple, iterative HTTP/1.0 Web server that uses the 
 *      GET method to serve static and dynamic content.
 *   Tiny Web server
 *   Dave O'Hallaron
 *   Carnegie Mellon University
 */
#include "csapp.h"
#include "dictionary.h"
#include "more_string.h"


static void *doit(void *cfd);
static dictionary_t *read_requesthdrs(rio_t *rp);
static void read_postquery(rio_t *rp, dictionary_t *headers, dictionary_t *d);
static void clienterror(int fd, char *cause, char *errnum, 
                        char *shortmsg, char *longmsg);
static void print_stringdictionary(dictionary_t *d);

//Four kinds of HTTP GET/PUT requests methods
static void friends_request(int fd, dictionary_t *query);
static void befriend_request(int fd, dictionary_t* query);
static void unfriend_request(int fd, dictionary_t* query);
static void introduce_request(int fd, dictionary_t* query);

dictionary_t* friends_dic;
pthread_mutex_t lock;

int main(int argc, char **argv) {
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  pthread_mutex_init(&lock, NULL);
  friends_dic =(dictionary_t*)make_dictionary(COMPARE_CASE_SENS,free);

  /* Don't kill the server if there's an error, because
     we want to survive errors due to a client. But we
     do want to report errors. */
  exit_on_error(0);

  /* Also, don't stop on broken connections: */
  Signal(SIGPIPE, SIG_IGN);

  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    if (connfd >= 0) {
      Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, 
                  port, MAXLINE, 0);
      printf("Accepted connection from (%s, %s)\n", hostname, port);

      pthread_t pt;
      int *cfd;
      cfd = malloc(sizeof(int));
      *cfd = connfd;
      Pthread_create(&pt, NULL, doit, cfd);
      Pthread_detach(pt);
    }
  }
}

/*
 * doit - handle HTTP request/response transactions
 */
void *doit(void *cfd)  {
  int fd = *(int *)cfd;
  free(cfd);
  char buf[MAXLINE], *method, *uri, *version;
  rio_t rio;
  dictionary_t *headers, *query;

  /* Read request line and headers */
  Rio_readinitb(&rio, fd);
  if (Rio_readlineb(&rio, buf, MAXLINE) <= 0)
    return;
  printf("%s", buf);
  
  if (!parse_request_line(buf, &method, &uri, &version)) {
    clienterror(fd, method, "400", "Bad Request",
                "Friendlist did not recognize the request");
  } else {
    if (strcasecmp(version, "HTTP/1.0")
        && strcasecmp(version, "HTTP/1.1")) {
      clienterror(fd, version, "501", "Not Implemented",
                  "Friendlist does not implement that version");
    } else if (strcasecmp(method, "GET")
               && strcasecmp(method, "POST")) {
      clienterror(fd, method, "501", "Not Implemented",
                  "Friendlist does not implement that method");
    } else {
      headers = read_requesthdrs(&rio);

      /* Parse all query arguments into a dictionary */
      query = make_dictionary(COMPARE_CASE_SENS, free);
      parse_uriquery(uri, query);
      if (!strcasecmp(method, "POST"))
        read_postquery(&rio, headers, query);

      /* For debugging, print the dictionary */
      print_stringdictionary(query);

      /* You'll want to handle different queries here,
         but the intial implementation always returns
         nothing: */
      if(starts_with("/friends", uri))
      {
        pthread_mutex_lock(&lock);
        friends_request(fd, query);
        pthread_mutex_unlock(&lock);
      }
      else if(starts_with("/befriend", uri))
      {
        pthread_mutex_lock(&lock);
        befriend_request(fd, query);
        pthread_mutex_unlock(&lock);
      }
      else if(starts_with("/unfriend", uri))
      {
        pthread_mutex_lock(&lock);
        unfriend_request(fd, query);
        pthread_mutex_unlock(&lock);
      }
      else if(starts_with("/introduce", uri))
      {
        introduce_request(fd, query);
      }

      /* Clean up */
      free_dictionary(query);
      free_dictionary(headers);
    }

    /* Clean up status line */
    free(method);
    free(uri);
    free(version);
  }
  Close(fd);
}

/*
 * read_requesthdrs - read HTTP request headers
 */
dictionary_t *read_requesthdrs(rio_t *rp) {
  char buf[MAXLINE];
  dictionary_t *d = make_dictionary(COMPARE_CASE_INSENS, free);

  Rio_readlineb(rp, buf, MAXLINE);
  printf("%s", buf);
  while(strcmp(buf, "\r\n")) {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
    parse_header_line(buf, d);
  }
  
  return d;
}

void read_postquery(rio_t *rp, dictionary_t *headers, dictionary_t *dest) {
  char *len_str, *type, *buffer;
  int len;
  
  len_str = dictionary_get(headers, "Content-Length");
  len = (len_str ? atoi(len_str) : 0);

  type = dictionary_get(headers, "Content-Type");
  
  buffer = malloc(len+1);
  Rio_readnb(rp, buffer, len);
  buffer[len] = 0;

  if (!strcasecmp(type, "application/x-www-form-urlencoded")) {
    parse_query(buffer, dest);
  }

  free(buffer);
}

static char *ok_header(size_t len, const char *content_type) {
  char *len_str, *header;
  
  header = append_strings("HTTP/1.0 200 OK\r\n",
                          "Server: Friendlist Web Server\r\n",
                          "Connection: close\r\n",
                          "Content-length: ", len_str = to_string(len), "\r\n",
                          "Content-type: ", content_type, "\r\n\r\n",
                          NULL);
  free(len_str);

  return header;
}

static void friends_request(int fd, dictionary_t *query) {
  size_t len;
  char *body, *header;

  if(dictionary_count(query) != 1)
  {
    clienterror(fd, "GET", "400", "Bad Request",
                "requires at least one user");
  }

  const char *q_user_val = dictionary_get(query, "user");
  if(q_user_val == NULL)
  {
    clienterror(fd, "GET", "400", "Bad Request",
                "no value for the key, user, in query");
  }

  body = strdup("");
  dictionary_t *f_user_val = dictionary_get(friends_dic, q_user_val);
  if(f_user_val != NULL)
  {
    const char **friends_list = dictionary_keys(f_user_val);
    body = join_strings(friends_list, '\n');
  }

  len = strlen(body);

  /* Send response headers to client */
  header = ok_header(len, "text/html; charset=utf-8");
  Rio_writen(fd, header, strlen(header));
  printf("Response headers:\n");
  printf("%s", header);

  free(header);

  /* Send response body to client */
  Rio_writen(fd, body, len);

  free(body);
}

static void befriend_request(int fd, dictionary_t *query) {
  size_t len;
  char *body, *header;

  if(query == NULL)
  {
    clienterror(fd, "GET", "400", "Bad Request",
                "empty query");
  }

  if(dictionary_count(query) != 2)
  {
    clienterror(fd, "GET", "400", "Bad Request",
                "query is not enough");
  }

  const char *q_user_val = dictionary_get(query, "user");
  if(q_user_val == NULL)
  {
    dictionary_t* q_user = make_dictionary(COMPARE_CASE_SENS, free);
    dictionary_set(friends_dic, q_user_val, q_user);
  }

  dictionary_t* f_user_val = dictionary_get(friends_dic, q_user_val);
  if(f_user_val == NULL)
  {
    f_user_val = make_dictionary(COMPARE_CASE_SENS, free);
    dictionary_set(friends_dic, q_user_val, f_user_val);
  }

  char** friends_list = split_string((char*)dictionary_get(query, "friends"), '\n');
  if(friends_list == NULL)
  {
    clienterror(fd, "GET", "400", "Bad Request",
                "No friends");
  }

  int i;
  for(i = 0; friends_list[i] != NULL; i++)
  {
    if(strcmp(friends_list[i], q_user_val) != 0)
    {
      dictionary_t* us_f = dictionary_get(friends_dic, q_user_val);
      if(us_f == NULL)
      {
        us_f = make_dictionary(COMPARE_CASE_SENS, free);
        dictionary_set(friends_dic, q_user_val, us_f);
      }

      if(dictionary_get(us_f, friends_list[i]) == NULL)
      {
        dictionary_set(us_f, friends_list[i], NULL);
      }

      dictionary_t* friends_list_f = dictionary_get(friends_dic, friends_list[i]);
      if(friends_list_f == NULL)
      {
        friends_list_f = make_dictionary(COMPARE_CASE_SENS, free);
        dictionary_set(friends_dic, friends_list[i], friends_list_f);
      }
      if(dictionary_get(friends_list_f, q_user_val) == NULL)
      {
        dictionary_set(friends_list_f, q_user_val, NULL);
      }
      free(friends_list[i]);
    }
  }

  f_user_val = dictionary_get(friends_dic, q_user_val);
  const char** user_names = dictionary_keys(f_user_val);
  body = strdup("");
  body = join_strings(user_names, '\n');

  len = strlen(body);

  /* Send response headers to client */
  header = ok_header(len, "text/html; charset=utf-8");
  Rio_writen(fd, header, strlen(header));
  printf("Response headers:\n");
  printf("%s", header);

  free(header);

  /* Send response body to client */
  Rio_writen(fd, body, len);

  free(body);
}

static void unfriend_request(int fd, dictionary_t *query) {
  size_t len;
  char *body, *header;

  if(dictionary_count(query)!=2)
  {
    clienterror(fd, "GET", "400", "Bad Request",
                "not enough query arguments");
  }

  const char* user = dictionary_get(query, "user");
  if(user == NULL)
  {
    clienterror(fd, "GET", "400", "Bad Request",
                "user is invalid");
  }
  
  dictionary_t* f_user_val = dictionary_get(friends_dic, user);
  if(f_user_val == NULL)
  {
    clienterror(fd, "GET", "400", "Bad Request",
                "no user");
  }

  char** friends_list = split_string((char*)dictionary_get(query, "friends"), '\n');
  if(friends_list == NULL)
  {
    clienterror(fd, "GET", "400", "Bad Request",
                "no friends");
  }

  int i;
  for(i=0; friends_list[i] != NULL; i++)
  {
    dictionary_remove(f_user_val, friends_list[i]);
    dictionary_t* fr_dic = dictionary_get(friends_dic, friends_list[i]);
    if(fr_dic != NULL)
	  {
	    dictionary_remove(fr_dic, user);
	  }
  }
  
  f_user_val = dictionary_get(friends_dic, user);
  const char** user_names = dictionary_keys(f_user_val);
  body = join_strings(user_names, '\n');

  len = strlen(body);

  /* Send response headers to client */
  header = ok_header(len, "text/html; charset=utf-8");
  Rio_writen(fd, header, strlen(header));
  printf("Response headers:\n");
  printf("%s", header);

  free(header);

  /* Send response body to client */
  Rio_writen(fd, body, len);

  free(body);
}

static void introduce_request(int fd, dictionary_t *query) {
  size_t len;
  char *body, *header;

  if(dictionary_count(query)!=4)
  {
    clienterror(fd, "GET", "400", "Bad Request",
                "query is not enough");
  }

  char* host = dictionary_get(query, "host");
  char* port = dictionary_get(query, "port");
  const char *user = dictionary_get(query, "user");
  const char *friend = dictionary_get(query, "friend");  

  if(!(user && friend && host && port))
  {
    clienterror(fd, "GET", "400", "Bad Request",
                "query is not enough");
  }

  body = strdup("");

  char buffer[MAXBUF];
  int open_cfd = Open_clientfd(host, port);
  sprintf(buffer, "GET /friends?user=%s HTTP/1.1\r\n\r\n", query_encode(friend));
  Rio_writen(open_cfd, buffer, strlen(buffer));
  Shutdown(open_cfd, SHUT_WR);

  char rbuffer[MAXLINE];
  rio_t rio;
  Rio_readinitb(&rio, open_cfd);
  
  if(Rio_readlineb(&rio, rbuffer, MAXLINE) <= 0)
  {
    clienterror(fd, "GET", "400", "Bad Request",
                "cannot read");
  }

  char *status, *protocol, *description;
  
  if(parse_status_line(rbuffer, &protocol, &status, &description))
  {
    
    if(strcasecmp(status, "200"))
    {
	    clienterror(fd, "GET", "400", "Bad Request",
                "status is not right");
    }
    else if(strcasecmp(protocol, "HTTP/1.0") && strcasecmp(protocol, "HTTP/1.1"))
	  {
	    clienterror(fd, "GET", "400", "Bad Request",
                "wrong version");
	  }
    else
	  {
	    dictionary_t *hdrs = read_requesthdrs(&rio);
	    char *length = dictionary_get(hdrs, "Content-length");
      int lg = 0;
      if(length)
      {
        lg = atoi(length);
      }
	    char bufr[lg];
	    if(lg <=0)
	    {
	      clienterror(fd, "GET", "400", "Bad Request",
                "no message");
	    }
	    else
	    {
	      Rio_readnb(&rio, bufr, lg);
	      bufr[lg] = 0;
	      pthread_mutex_lock(&lock);
	      
	      dictionary_t* f_user_val = dictionary_get(friends_dic, user);
        if(f_user_val == NULL)
		    {
		      f_user_val = make_dictionary(COMPARE_CASE_SENS,NULL);
		      dictionary_set(friends_dic, user, f_user_val);
		    }
	      char** friends_list = split_string(bufr, '\n');

	      int i;
		    for(i = 0; friends_list[i] != NULL; i++)
		    {
		      if(strcmp(friends_list[i], user) != 0)
          {
		        dictionary_t* new_f_user_val = dictionary_get(friends_dic, user);
		        if(new_f_user_val == NULL)
			      {
			        new_f_user_val = make_dictionary(COMPARE_CASE_SENS, free);
			        dictionary_set(friends_dic, user, new_f_user_val);
			      }

		        if(dictionary_get(new_f_user_val, friends_list[i])==NULL)
            {
			        dictionary_set(new_f_user_val, friends_list[i], NULL);
            }

		        dictionary_t* n_friends = dictionary_get(friends_dic, friends_list[i]);
		        if(n_friends == NULL)
			      {
			        n_friends = make_dictionary(COMPARE_CASE_SENS, free);
			        dictionary_set(friends_dic, friends_list[i], n_friends);
			      } 

		        if(dictionary_get(friends_dic,user) == NULL)
            {
			        dictionary_set(friends_dic, user, NULL);
            }
		        free(friends_list[i]);
          }
		    }
		
	      free(friends_list);
	      const char** list = dictionary_keys(f_user_val);
	      body = join_strings(list, '\n');
	      pthread_mutex_unlock(&lock);
        len = strlen(body);
	      
        /* Send response headers to client */
	      header = ok_header(len, "text/http; charset=utf-8");
	      Rio_writen(fd, header, strlen(header));
	      printf("Response headers:\n");
	      printf("%s", header);

	      free(header);

	      /* Send response body to client */
	      Rio_writen(fd, body, len);
	      free(body);
	    }
	  }
    free(protocol);
    free(status);
    free(description);
  }
  else
  {
    clienterror(fd, "GET", "400", "Bad Request",
                "status errror");
  }
  Close(open_cfd);
}

/*
 * clienterror - returns an error message to the client
 */
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg) {
  size_t len;
  char *header, *body, *len_str;

  body = append_strings("<html><title>Friendlist Error</title>",
                        "<body bgcolor=""ffffff"">\r\n",
                        errnum, " ", shortmsg,
                        "<p>", longmsg, ": ", cause,
                        "<hr><em>Friendlist Server</em>\r\n",
                        NULL);
  len = strlen(body);

  /* Print the HTTP response */
  header = append_strings("HTTP/1.0 ", errnum, " ", shortmsg, "\r\n",
                          "Content-type: text/html; charset=utf-8\r\n",
                          "Content-length: ", len_str = to_string(len), "\r\n\r\n",
                          NULL);
  free(len_str);
  
  Rio_writen(fd, header, strlen(header));
  Rio_writen(fd, body, len);

  free(header);
  free(body);
}

static void print_stringdictionary(dictionary_t *d) {
  int i, count;

  count = dictionary_count(d);
  for (i = 0; i < count; i++) {
    printf("%s=%s\n",
           dictionary_key(d, i),
           (const char *)dictionary_value(d, i));
  }
  printf("\n");
}
