/**
  * Name: Kushagra Gupta
**/

#include "server_header.h"

mutex client_file_lock;
mutex repo_file_lock;
mutex log_file_lock;

server_data serv_info;

string DELIMETER = "@#@";
string server_root_global = "";

map<string, tuple<string, string, string> > active_client;
vector<struct repo_file_structure> repo;

repo_file_structure* repofile_iterator; 



/***************************************************************************************************/

void logServer(const char * log){
    ofstream server_log_file;

    time_t result = time(NULL);

    /* Local time by default gives newline */
    string time = string(asctime(localtime(&result)));
    size_t index = time.find('\n');
    time = time.substr(0,index);

    string logger = time + " : " + string(log) + "\n";
    
    log_file_lock.lock();

    string log_file_name = server_root_global + '/' +  "repo.log";
    server_log_file.open(log_file_name.c_str(), ofstream::app);

    if(server_log_file.good())
        server_log_file << logger;
    else
         cout << "Error opening server log file";

    server_log_file.close();

    log_file_lock.unlock();
}

/*************************************************************************************************/

void err_sys(const char* x) 
{ 
    /* perror() prints descriptive error message to stderr*/
    logServer(x);
    perror(x); 
    exit(1); 
}

/*************************************************************************************************/
string receiveData(SOCKET confd){
    vector<char> recv_buffer(BUFFSIZE);
    int n;
    string rec;

    do{
        if(( n = recv(confd, recv_buffer.data(), recv_buffer.size(), 0)) == -1)
            err_sys("Error receiving from server");
        rec.append(recv_buffer.cbegin(), recv_buffer.cend());
    }while(n == BUFFSIZE);

    size_t index;
    if((index = rec.find('\0')) != string::npos){
        rec = rec.substr(0, index);
    }
    return rec;
}


/****************************************************************************************************/

void sendData(SOCKET confd, string data_to_send){
    if(send(confd, data_to_send.c_str(), data_to_send.length(), 0) == -1)
        err_sys("Error in sending command to server");
}

/*******************************************************************************************************/

string insertActiveClient(string cli_recv){
    size_t d_pos = 0;
    string token;
    string val[4];

    int i = 0;
    /*Format: Alias + IP + CliPort + download_port + client_root*/
    while ((d_pos = cli_recv.find(DELIMETER)) != string::npos) {
        token = cli_recv.substr(0, d_pos);
        val[i++] = token;
        cli_recv.erase(0, d_pos + DELIMETER.length());
    }
    val[i] = cli_recv;
    int null_pos = val[i].find('\0');
    val[i] = val[i].substr(0, null_pos);

    tuple <string, string, string> tup(val[1], val[2], val[3]);
    pair <string, tuple <string, string, string> > temp_pair(val[0], tup);

    string insert_into_file = val[0] + ":" + val[1] + ":" + val[2] + ":" + val[3] + "\n";

    ofstream active_client_file;

    /*Locking the entry to client file and data structure*/
    client_file_lock.lock(); 
    
    string active_file_name = server_root_global + '/' + serv_info.cli_info_file;

    active_client_file.open(active_file_name.c_str(), ofstream::app);
    
    //cout << "Entering into file";
    if(!active_client_file.bad()){
        active_client_file << insert_into_file;
    }
    else{
        string message = "Could not open the active client file";
        cerr << message;
        logServer(message.c_str());
    }
    active_client_file.close();

    active_client.insert(temp_pair);
    
    /* Unlocking the locked entry to client file*/
    client_file_lock.unlock();

    return val[0];
}


/******************************************************************************************************/

vector<string> split(const char *received_string){
    vector<string> list;
    string s = string(received_string);
    size_t pos;
    string token;
    while ((pos = s.find(DELIMETER)) != string::npos) {
        token = s.substr(0, pos);
        list.push_back(token);
        s.erase(0, pos + DELIMETER.length());
    }
    token = s.substr(0, s.find('\0'));
    list.push_back(token);
    return list;
}

/******************************************************************************************************/
string toLower(string s){
    locale l;
    string ret;
    for (int i = 0; i < (int)s.length(); ++i){
        ret += tolower(s[i], l);
    }
    return ret;
}
/******************************************************************************************************/

/* Splitting the filename from path and inserting into the repo file and corresponding DS(set of tuple)*/
void share(string file_to_share, string client_alias){
    repo_file_structure entry;
    size_t file_name_index = file_to_share.find_last_of('/');
    entry.file_name = file_to_share.substr(file_name_index + 1);
    entry.path = file_to_share;
    entry.author_alias = client_alias;
    repo.push_back(entry);

    ofstream repo_file;
 

    /*Locking the repofile access for synchronization*/
    repo_file_lock.lock();

    static string repo_file_name = server_root_global + '/' + serv_info.repo_file;
    repo_file.open(repo_file_name.c_str(), ofstream::app);
    string insert_into_file = entry.file_name + ":" + entry.path + ":" + entry.author_alias + "\n";
    
    // cout << "Entering into file\n";
    if(!repo_file.bad()){
        repo_file << insert_into_file;
    }
    else{
        cerr << "Could not open the active client file";
    }
    repo_file.close();

    /*Unlocking the mutex lock*/
    repo_file_lock.unlock();

    return;
}
/**********************************************************************************/
void populate(){
    ifstream repo_file;
    string repo_file_name = server_root_global + '/' + serv_info.repo_file;
    repo_file.open(repo_file_name.c_str());

    string token;
    int pos;

    if(repo_file.good()){
        for( std::string s; getline( repo_file, s ); ){
            repo_file_structure entry;

            pos = s.find(':');
            token = s.substr(0, pos);
            entry.file_name = token;

            s.erase(0, pos + 1);

            pos = s.find(':');
            token = s.substr(0, pos);
            entry.path = token;

            s.erase(0, pos + 1);
            entry.author_alias = s;
            repo.push_back(entry);
        }
    }
    return;
}


/********************************************************************************************************/
string del(string file_to_delete, string client_alias){
    string message_to_send = "";
    int found = 0;

    /*Locking the repofile access for synchronization*/
    repo_file_lock.lock();

    for (std::vector<struct repo_file_structure>::iterator i = repo.begin(); i != repo.end(); ++i)
    {
        if(i->path == file_to_delete && i->author_alias == client_alias){
            repo.erase(i);
            found = 1;
            break;
        }
    }

    if(found == 1)
        message_to_send = "SUCCESS:FILE_REMOVED\n";
    else
        message_to_send = "FAILURE:FILE_NOT_FOUND\n";


    if(found == 1){
        ofstream repo_file;
        repo_file.open(serv_info.repo_file);
        
        
        if(!repo_file.bad()){
            for (std::vector<struct repo_file_structure>::iterator it = repo.begin(); it != repo.end(); ++it){
                string insert_into_file = it->file_name + ":" + it->path + ":" + it->author_alias + "\n";
                repo_file << insert_into_file;
            }
        }
        else{
            cerr << "Could not open the repo file";
            logServer("Could not open the repo file");
        }
        repo_file.close();
    }

    /*Unlocking the mutex lock*/
    repo_file_lock.unlock();

    return message_to_send;
}

/************************************************************************************************************/
string search(string file_to_search){
    string message_to_send = "";
    int file_hits = 0;
    regex match((file_to_search)+"(.*)", std::regex_constants::icase);

    for (std::vector<struct repo_file_structure>::iterator it = repo.begin(); it != repo.end(); ++it){
        if(regex_match((it->file_name).c_str(), match)){
            for(auto var : active_client){
                if(var.first == it->author_alias){
                    /*[SNO] FILENAME:RELATIVE_PATH_TO_FILE_WRT_CLIENT:CLIENT_ALIAS:CLIENT_IP:CLIENT_PORT:CLIENT_DOWNLOAD_PORT*/
                    file_hits++;
                    message_to_send += "\"" + it->file_name + "\"" + " " + "\"" + it->path + "\"" + " " + "\"" + it->author_alias + "\"" + " " + "\"" + get<0>(var.second) + "\"" + " " + "\"" + get<1>(var.second) + "\"" + " " + "\"" + get<2>(var.second) + "\"" + " ";
                }
            }
        }
    }
    message_to_send = to_string(file_hits) + " " + message_to_send;
    return message_to_send;
}

/*********************************************************************************************************/

string get(string alias_to_search){
    string message_to_send = "";
    
    client_file_lock.lock();

    /* Finding the alias in the active client structure*/
    int found = 0;
    for(auto var : active_client){
        if(var.first == alias_to_search){
            message_to_send = "\"" + get<0>(var.second) + "\"" + " " + "\"" + " " + "\"" + get<2>(var.second) + "\"" + " ";
            // cout << "Port Sent: " << get<2>(var.second) << endl;
            found = 1;
            break;
        }
    }
    client_file_lock.unlock();

    if(found == 0){
        message_to_send = "ERROR";
    }

    return message_to_send;
}

/******************************************************************************************************/
string exec(string alias_to_search){
    string message_to_send = "";
    
    client_file_lock.lock();

    /* Finding the alias in the active client structure*/
    int found = 0;
    for(auto var : active_client){
        if(var.first == alias_to_search){
            message_to_send = "\"" + get<0>(var.second) + "\"" + " " + "\"" + " " + "\"" + get<1>(var.second) + "\"" + " ";
            // cout << "Port Sent: " << get<1>(var.second) << endl;
            found = 1;
            break;
        }
    }
    client_file_lock.unlock();

    if(found == 0){
        message_to_send = "ERROR";
    }
    return message_to_send;
}


/*****************************************************************************************************/

string addDelim(string input){
    
    string parsed_string = "";
    int flag = 0;

    for(int i = 0; i < (int)input.length(); i++){
        char c = input[i];
        if(c == ' ' && flag == 0){
            parsed_string += DELIMETER;
        }
        else if (c == '"'){
            if(flag == 0)
                flag = 1;
            else
                flag = 0;
        }
        if(c != ' ' || flag  == 1)
            parsed_string += c;
    }

    input = parsed_string;
    parsed_string = "";
    flag = 0;

    /* Removing Escape characters here*/
    for(int i = 0; i < (int)input.length(); i++){
        //char c = input[i];
        if(input[i] == '"'){
            continue;
        }
        else if(input[i] == 92){
            parsed_string += input[++i];
        }
        else
            parsed_string += input[i];
    }
    return parsed_string;
}

/********************************************************************************************************/
void* thread_proc(void * connection_details)
{   
    connection_data connection_info;
    connection_info = *((connection_data *)connection_details);

    client_data_struct client_data;

    string message = "Thread ID:" + to_string(pthread_self()) + " with pid " + to_string(getpid()) +" created"; 
    logServer(message.c_str());
    message = "Connection Established with " + connection_info.conn_ip + " from port " + to_string(connection_info.conn_port);
    logServer(message.c_str());

    string cli_recv = receiveData(connection_info.confd);
    //cout << "Client Data: "  << cli_recv << "\n";

    client_data.client_alias = insertActiveClient(cli_recv);


    //int sock;
    char buffer[BUFFSIZE];
    int nread;
 
    while(1){
        string message_to_send = "";

        if((nread = (recv(connection_info.confd, buffer, sizeof(buffer), 0))) < 0)
            err_sys("Error in receiving from the client");
        if(nread == 0){
            // cout << "Client_Disconnected";
            message = "Client with IP " + connection_info.conn_ip + " disconnected";
            logServer(message.c_str());
            break;
        }
        buffer[nread] = '\0';

        std::vector<string> received_tokens;
        std::vector<string>::iterator it;

        /*Break the string received from the client into tokents delimited by @#@ */
        received_tokens = split(buffer);
        it = received_tokens.begin();

        /* Query contains the command sent from the client */
        string query = *it;
        if(toLower(query) == "share"){
            if(*(++it) != ""){
                
                message = "Share request from " + connection_info.conn_ip;
                logServer(message.c_str());

                share(*it, client_data.client_alias);
                repofile_iterator = &(*(repo.end()-1));
                                
                message_to_send = "SUCCESS:FILE_SHARED\n";
            }
            else{
                message_to_send = "Invalid file to share";
            }
        }

        else if(toLower(query) == "del"){

            message = "Delete request from " + connection_info.conn_ip;
            logServer(message.c_str());

            if(*(++it) != ""){
                message_to_send = del(*it, client_data.client_alias);
            }
            else{
                message_to_send += "Invalid file to delete";
            }
        }

        else if(toLower(query) == "search"){

            message = "Search request from " + connection_info.conn_ip;
            logServer(message.c_str());

            if(*(++it) != ""){
                message_to_send = search(*it);
                message_to_send = addDelim(message_to_send);
            }
            else{
                message_to_send += "Invalid file to search";
            }
        }

        else if(toLower(query) == "exec"){
            /*Code for exec*/

            message = "Exec request from " + connection_info.conn_ip;
            logServer(message.c_str());

             if(*(++it) != ""){
                message_to_send = exec(*it);
                message_to_send = addDelim(message_to_send);
            }
            else{
                message_to_send += "Invalid Alias to Execute Shell Command";
            }
        }

        else if(toLower(query) == "get"){
           
            message = "Get request from " + connection_info.conn_ip;
            logServer(message.c_str());
            
            if(*(++it) != ""){
                message_to_send = get(*it);
                message_to_send = addDelim(message_to_send);
            }
            else{
                message_to_send += "Invalid Alias to Search";
            }
        }

        else{
            message_to_send = "Invalid Command: " + query;
        }

        message = "Data received from " + connection_info.conn_ip + " from port " + to_string(connection_info.conn_port);
        logServer(message.c_str());
        
        send(connection_info.confd, message_to_send.c_str(), message_to_send.length(), 0);
        memset(&buffer, 0, sizeof(buffer));
    }
    
    close(connection_info.confd);
    message =  "Thread " + to_string(pthread_self()) + " with pid " + to_string(getpid()) + " closed\n\n";
    logServer(message.c_str());

    pthread_exit(EXIT_SUCCESS);

}



/***************************************************************************************************************/
int main(int argc, char* argv[])
{
    system("clear");
    //system("wmctrl -r :ACTIVE: -N \"Server\"");

    if(argc != 6){
        err_sys("Usage Error: ./executable server_ip server_port main_repofile client_list_file server_root");
    }

    /* Defining the local data to be used */
    vector<char> recv_buffer(BUFFSIZE);
    string send_buffer = "", rec = "";
    int listenfd, confd;
    pthread_t thread_id;


    /* Structure Decleration to receive command line values*/
    serv_info.serv_ip = argv[1];
    serv_info.serv_port = atoi(argv[2]);
    serv_info.repo_file = argv[3];
    serv_info.cli_info_file = argv[4];
    serv_info.rel_path = argv[5];

    server_root_global = serv_info.rel_path;
    // ofstream log_file;
    // log_file.open(repo.log, ios::ate)

    //Clearing the client info file and repo file 
    ofstream active_client_file;
    string active_file_name = server_root_global + '/' + serv_info.cli_info_file;
    active_client_file.open(active_file_name.c_str());
    active_client_file.close();

    populate();
    // TODO: Prepopulate both the data structures


    /* Socket structures*/
    struct sockaddr_in servaddr, cliaddr;
    //socklen_t size;


    /* Creating socket (TCP SOCKET) */
    listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenfd < 0) 
        err_sys("Error in creating the socket");

    /* 
    Setting the socket options so that the socket can be reused
    SOL_SOCKET: For specifying the options at socket level
    SO_REUSEADDR: So that the socket can be reused by multiple threads
    */
    int val = 1, result;
    result = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
    if (result < 0) {
        err_sys("Error in setting the socket options");
    }


    /* Assigning the values to the server structure */
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(serv_info.serv_ip.c_str());
    servaddr.sin_port = htons(serv_info.serv_port);


    /* Binding the socket to the values provided */
    if ((bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr))) < 0) 
        err_sys("Bind Error");

    cout << "-- SERVER STARTED --" << endl;


    /* Listening for incoming connections */
    if( listen(listenfd, 10) < 0)
        err_sys("Listen Error");

    /* The server accepting the connections in infinite loop */
    while(1){

        socklen_t clilen;
        clilen = sizeof(cliaddr);

        confd = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen);
        if(confd < 0)
            err_sys("Accept Error");


        connection_data connection_info;
        connection_info.conn_ip = inet_ntoa(cliaddr.sin_addr);
        connection_info.conn_port = ntohs(cliaddr.sin_port);
        connection_info.confd = confd; 

        int thread_status;
        thread_status = pthread_create(&thread_id, NULL, thread_proc, (void *) &connection_info);
        if (thread_status != 0) {
            cout<<"Could not create new threads";
            logServer("Could not create new threads");
        }

        pthread_detach(thread_id);
        sched_yield();
    }   

    close(listenfd);
    close(confd);
    cout << "-- Server terminated successfully --";
    return 0;
}



