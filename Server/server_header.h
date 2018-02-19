/*
 * Name: Kushagra Gupta
 * */

#include <iostream>
#include <string.h>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <vector>
#include <sys/stat.h> // for getting the stats of the file(binary)
#include <math.h>
#include <fstream>
#include <pthread.h>
#include <tuple>
#include <map>
#include <set>
#include <mutex>
#include <regex>
#include <ctime>

using namespace std;

#define SOCKET int
#define BUFFSIZE 1024

#ifndef min
    #define min(a,b) ((a) < (b) ? (a) : (b))
#endif

struct server_data{
	string serv_ip;
	int serv_port;
	string repo_file;
	string cli_info_file;
	string rel_path;
};

struct connection_data{
	string conn_ip;
	int conn_port;
	int confd;
};

struct client_data_struct{
	string client_alias;
	string client_ip;
	string client_port;
	string client_download_port;
};

struct repo_file_structure{
	string file_name;
	string path;
	string author_alias;
};

/* ALIAS:CLIENT_IP:CLIENT_PORT:CLIENT_DOWNLOAD_PORT*/
extern string server_root_global;
extern string DELIMETER;
extern map<string, tuple<string, string, string> > active_client;
extern vector<struct repo_file_structure> repo;

/* Function Declerations */

void err_sys(const char* x);
void* thread_proc(void *arg);

