/*	
	Name: Kushagra Gupta
 *	### How we'll call the client
 * 	./executable client_alias client_ip client_port server_ip server_port downloading_port client_root
*/


#include <iostream>
#include <string.h>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <cstdlib>
#include <netdb.h>
#include <errno.h>
#include <vector>
#include <fstream>
#include <pthread.h>
#include <fcntl.h>
#include <mutex>

using namespace std;

#define SOCKET int
#define BUFFSIZE 1024

#ifndef min
    #define min(a,b) ((a) < (b) ? (a) : (b))
#endif

struct client_data{
	string client_alias = "";
	string client_ip = "";
	int client_port = 0;
	string server_ip = "";
	int server_port = 0;
	int cli_download_port = 0;
	char *client_root;
};

struct search_results{
	string filename;
	string relative_path;
	string client_alias;
	string client_ip;
	string client_rpc_port;
	string client_download_port;
};

struct connection_data{
	string conn_ip;
	int conn_port;
	int confd;
	char * client_root;
};

struct getResults{
	string filename;
	string relative_path_client;
	string cServer_ip;
	string cServer_downPort;
	string output_file;
	char * client_root;
};

struct rpcResults{
	string rpc_server_ip;
	string rpc_server_port;
	string command;
};

extern string client_root_global;
extern vector<struct search_results> search_list;
extern string DELIMETER;
extern int search_results_len;

void err_sys(const char* x);
