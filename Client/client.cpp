/* Name: Kushagra Gupta 
 * */

#include "client_header.h"

mutex cout_lock;
mutex log_file_lock;

vector<struct search_results> search_list;
string DELIMETER = "@#@";
string client_root_global;

/* The search results have a column size of 6 (column entries) */
int search_results_len = 6;

void logClient(const char * log){
    ofstream client_log_file;

    time_t result = time(NULL);

    /* Local time by default gives newline */
    string time = string(asctime(localtime(&result)));
    size_t index = time.find('\n');
    time = time.substr(0,index);

    string logger = time + " : " + string(log) + "\n";
    
    log_file_lock.lock();

    static string log_file_name = client_root_global + '/' + "client.log";
    client_log_file.open(log_file_name.c_str(), ofstream::app);

    if(client_log_file.good())
        client_log_file << logger;
    else
         cout << "Error opening server log file";

    client_log_file.close();

    log_file_lock.unlock();
}



void err_sys(const char* x) 
{ 
    /* perror() prints discriptive error message to stderr*/
    perror(x); 
    exit(1); 
}  

/***************************************************************************************************/
void sendData(SOCKET confd, string data_to_send){
    if(send(confd, data_to_send.c_str(), data_to_send.length(), 0) == -1)
        err_sys("Error in sending command to server");
}


/***************************************************************************************************/

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

/***************************************************************************************************/

string parseString(string input){
    string parsed_string = "";
    int flag = 0;


     /*Embedding the delimeters*/
    for(int i = 0; i < (int)input.length(); i++){
        char c = input[i];
        if(c == ' ' && flag == 0){
            parsed_string += "@#@";
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

/**********************************************************************************************************/

string toLower(string s){
    locale l;
    string ret;
    for (int i = 0; i < (int)s.length(); ++i){
        ret += tolower(s[i], l);
    }
    return ret;
}

/**********************************************************************************************************/

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

    if(s.find('\0') != string::npos){
        token = s.substr(0, s.find('\0'));
        list.push_back(token);
    }
    else if(s.find('\0') == string::npos){
        list.push_back(s);
    }
    return list;
}

/***************************************************************************************************/
void * uploadFile(void * connection_details){
    string message_to_send;
    connection_data connection_info;
    connection_info = *((connection_data *)connection_details);

    //cout << "----------------------------Client_Server Thread-------------------------------------------";
    logClient(("Thread " + to_string(pthread_self()) + " with pid " + to_string(getpid()) + " created").c_str());
    logClient(("Connection Established with " + connection_info.conn_ip + " from port " + to_string(connection_info.conn_port)).c_str());

    string data_received = receiveData(connection_info.confd);
    
    if(data_received[0] != '/'){
        data_received = '/' + data_received;
    }

    string file_to_download = connection_info.client_root + data_received;

    int readCounter, writeCounter;
    char* bufptr;
    char buf[BUFFSIZE];

    int file_fd;
    
    file_fd = open(file_to_download.c_str(), O_RDONLY);

    if(file_fd == -1){
        message_to_send = "File to download could not be opened";
    }

    /* reset the read counter */
    readCounter = 0;

    /* read the file, and send it to the client in chunks of size MAXBUF */
    while((readCounter = read(file_fd, buf, BUFFSIZE)) > 0){
        writeCounter = 0;
        bufptr = buf;
        while (writeCounter < readCounter){
            readCounter -= writeCounter;
            bufptr += writeCounter;
            writeCounter = write(connection_info.confd, bufptr, readCounter);
    
            if (writeCounter == -1){
                logClient("Could not write file to client!");
                cerr << "Could not write file to client!";
                close(connection_info.confd);
                continue;
            }
        }
    }

    close(file_fd);

    close(connection_info.confd);
    string log_message = "Thread " + to_string(pthread_self()) + " with pid " + to_string(getpid()) + "(download-server) closed";
    logClient(log_message.c_str());
    
    pthread_exit(EXIT_SUCCESS);
}

/***************************************************************************************************/

void * threadServer(void * server_details){

    client_data client_info;
    client_info = *((client_data *) server_details);

    struct sockaddr_in servaddr, cliaddr;
    int listenfd, confd;
    pthread_t thread_id;

    /* Creating a listening socket for the client_server*/
    listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenfd < 0){
        logClient("Error in creating the socket");
        err_sys("Error in creating the socket");
    }

    int val = 1, result;
    result = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
    if (result < 0) {
        logClient("Error in setting the socket options");
        err_sys("Error in setting the socket options");
    }

    logClient("Cli-Server Socket Created");

    /* Assigning the values to the cli-server structure */
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr((client_info.client_ip).c_str());
    servaddr.sin_port = htons(client_info.cli_download_port);

     /* Binding the socket to the values provided */
    if ((bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr))) < 0) 
        err_sys("Bind Error in Client-Server");

    cout << "-- Cli-Server waiting to accept Connections --" << endl;


    /* Listening for incoming connections */
    if( listen(listenfd, 10) < 0)
        err_sys("Listen Error in Server-Client");

    while(1){

        socklen_t clilen;
        clilen = sizeof(cliaddr);

        confd = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen);
        if(confd < 0){
            logClient("Error In Accept");
            err_sys("Accept Error");
        }

        connection_data connection_info;
        connection_info.conn_ip = inet_ntoa(cliaddr.sin_addr);
        connection_info.conn_port = ntohs(cliaddr.sin_port);
        connection_info.confd = confd;
        connection_info.client_root = client_info.client_root;

        int thread_status;
        thread_status = pthread_create(&thread_id, NULL, uploadFile, (void *) &connection_info);
        if (thread_status != 0) {
            logClient("Could not create new threads");
            cerr<<"Could not create new threads";
        }

        pthread_detach(thread_id);
        sched_yield();
    }
    pthread_exit(EXIT_SUCCESS);
}

/****************************************************************************************************/
void * downloadFile(void * cServer_details){
    getResults download_info;
    download_info = *((getResults *) cServer_details);

    SOCKET confd;
    struct sockaddr_in servaddr;

    confd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(confd < 0)
        err_sys("Error in creating socket");

    cout << "\n-- Client-download-socket successfully created --" << endl;

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(atoi(download_info.cServer_downPort.c_str()));
    inet_pton(AF_INET, download_info.cServer_ip.c_str(), &servaddr.sin_addr);

    /* Connecting to the server */
    if (connect(confd, (struct sockaddr *) &servaddr, sizeof(servaddr)) == -1){
        // err_sys("Error in Connect");
        cout << "FAILURE:CLIENT_OFFLINE";
        pthread_exit(EXIT_SUCCESS);
    }
    else
        cout << "\nConnection to the download-server established!\n";

    sendData(confd, download_info.relative_path_client);

    int output_file_fd, counter;
    char buf[BUFFSIZE];

    if(download_info.output_file[0] != '/')
        download_info.output_file = '/' + download_info.output_file;

    string outFile = string(download_info.client_root) + download_info.output_file;

    output_file_fd = open(outFile.c_str(), O_WRONLY | O_CREAT | O_TRUNC , 0777);
    if (output_file_fd == -1){
        logClient("Could not open the destination file");
        cout << "Could not open destination file, using stdout.\n";
        output_file_fd = 1;
    }
    
    while ((counter = read(confd, buf, BUFFSIZE)) > 0){
        //cout << "writing";
        write(output_file_fd, buf, counter);
    memset(&buf, 0, sizeof(buf));
    }
    //cout << "File Received";
    if (counter == -1){
        err_sys("Could not read the download file from server");
    }
    else{
        logClient(("SUCCESS:" + download_info.output_file).c_str());
        cout << "\nSUCCESS:" << download_info.output_file;
    }

    close(confd);
    logClient(("Thread " + to_string(pthread_self()) + " with pid " + to_string(getpid()) + " have downloaded information").c_str());
    pthread_exit(EXIT_SUCCESS);

}
/****************************************************************************************************/

void * receiveRpcResuls(void * rpc_server_details){
    rpcResults rpc_command_info;
    rpc_command_info = *((rpcResults *) rpc_server_details);

    SOCKET confd;
    struct sockaddr_in servaddr;

    confd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(confd < 0)
        err_sys("Error in creating socket");

    cout << "\n-- Client-rpc-socket successfully created --" << endl;

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(atoi(rpc_command_info.rpc_server_port.c_str()));
    inet_pton(AF_INET, rpc_command_info.rpc_server_ip.c_str(), &servaddr.sin_addr);

    /* Connecting to the server */
    if (connect(confd, (struct sockaddr *) &servaddr, sizeof(servaddr)) == -1)
        err_sys("Error in Connect");
    else
        cout << "\nConnection to the rpc_server established!\n";
    
    sendData(confd, rpc_command_info.command);

    string data_received = receiveData(confd);

    cout << "SUCCESS :" << endl;
    cout << data_received << endl;

    close(confd);
    logClient(("Thread " + to_string(pthread_self()) + " with pid " + to_string(getpid()) + " have received RPC results").c_str());
    pthread_exit(EXIT_SUCCESS);

}



/***************************************************************************************************/

void * threadClient(void * client_details){

    client_data client_info;
    client_info = *((client_data *) client_details);

    SOCKET confd;
    string user_command = "";
    
    vector<char> recv_buffer(BUFFSIZE);
    string rec;

    struct sockaddr_in servaddr;

    confd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(confd < 0)
        err_sys("Error in creating socket");

    cout << "\n-- Client socket successfully created --" << endl;
    
    /* Providing client with the specific ip and port information as there can be multiple network interfaces*/
    /*cliaddr.sin_family = AF_INET;
    cliaddr.sin_port = htons(client_info.client_port);
    cliaddr.sin_addr.s_addr = inet_addr(client_info.client_ip.c_str());
    bind(confd, (struct sockaddr *)&cliaddr, sizeof(cliaddr));
    */


    /* Providing the server information so the client can connect to it*/
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(client_info.server_port);
    inet_pton(AF_INET, client_info.server_ip.c_str(), &servaddr.sin_addr);

    /* Connecting to the server */
    if (connect(confd, (struct sockaddr *) &servaddr, sizeof(servaddr)) == -1)
        err_sys("Error in Connect");
    else
        cout << "\nConnection to the server established!\n";


    string info_to_send;
    info_to_send = client_info.client_alias + "@#@" + client_info.client_ip + "@#@" + to_string(client_info.client_port) + "@#@" + to_string(client_info.cli_download_port);

    /*Sending the client constant data to server*/
    sendData(confd, info_to_send);

    while(1){

        cout << endl <<client_info.client_alias << ": ";

        /* getting the user command from STDIN */
        getline(cin, user_command);
          
        if(user_command.length() == 0){
            user_command = "";
            continue;
        }

        if(user_command.c_str() == NULL){
            break;
        }

        user_command = parseString(user_command);
        string option;
        size_t option_pos;
        if((option_pos = user_command.find(DELIMETER)) != string::npos){
            option = user_command.substr(0, option_pos);    
        }
        
        if(toLower(option) == "share"){
            std::vector<string> v;
            
            v = split(user_command.c_str());
            if(v.size() != 2){
                cout << "FAILURE:INVALID_ARGUMENTS";
                continue;
            }

            string share_file_name = v[1];
            if(share_file_name[0] != '/')
                share_file_name = '/' + share_file_name; 
            share_file_name = string(client_info.client_root) + share_file_name;
            
            ifstream check_file;
            check_file.open(share_file_name.c_str());

            if(!check_file.good()){
                cout << "FAILURE:FILE_NOT_FOUND";
                continue;
            }

            logClient(("Share request sent to " + client_info.server_ip).c_str());
            sendData(confd, user_command);
            string rec = receiveData(confd);
            cout << rec << "\n";
            rec = "";
        }

        if(toLower(option) == "del"){
            logClient(("DELETE request sent to " + client_info.server_ip).c_str());
            sendData(confd, user_command);
            string rec = receiveData(confd);
            cout << rec << "\n";
            rec = "";
        }

        if(toLower(option) == "get"){
           
            int down_thread_status;
            pthread_t down_thread_id;
            getResults cServer_details;
            std::vector<string> v;
            
            v = split(user_command.c_str());

            if(search_list.size() <= 0){
                cout << "FAILURE:SEARCH_FOR_FILES_FIRST";
                continue;
            }
            else{
                if(v[1][0] == '['){
                    if((int)v.size() != 3){
                        cout << "FAILURE:INALID_ARGUMENTS";
                        continue;
                    }

                    /* Value after the [ (converting that char value to int) */
                    int index = (int)v[1][1]-48;

                    if(index > (int)search_list.size()){
                        cout << "FAILURE:INVALID_ARGUMENTS:SEARCH ENTRY NOT PRESENT";
                        continue;
                    }
                    else{
                        /* Taking the data values from the search results */
                        cServer_details.filename = search_list[index-1].filename;
                        cServer_details.relative_path_client = search_list[index-1].relative_path;
                        cServer_details.cServer_ip = search_list[index-1].client_ip;
                        cServer_details.cServer_downPort = search_list[index-1].client_download_port;
                        cServer_details.output_file = v[2];
                        cServer_details.client_root = client_info.client_root;

                    }
                }
                else{
                    if((int)v.size() != 4){
                        cout << "FAILURE:INALID_ARGUMENTS";
                        continue;
                    }
                    logClient(("GET Alias request sent to " + client_info.server_ip).c_str());
                    sendData(confd, user_command);
                    logClient(("GET Results received from " + client_info.server_ip).c_str());
                    string details_received = receiveData(confd);

                    if(details_received.find("ERROR") != string::npos){
                        cout << "FAILURE:CLIENT_OFFLINE";
                    }
                    else{
                            /*client_ip: client_download_port*/
                        std::vector<string> alias_details;
                        alias_details = split(details_received.c_str());

                        size_t file_name_index = v[2].find_last_of('/');
                        cServer_details.filename = v[2].substr(file_name_index + 1);
                        cServer_details.relative_path_client = v[2];
                        cServer_details.cServer_ip = alias_details[0];
                        cServer_details.cServer_downPort = alias_details[1];
                        cServer_details.output_file = v[3];
                        cServer_details.client_root = client_info.client_root;

                    }
                }

                string get_file_name;
                get_file_name = string(client_info.client_root) + '/' + cServer_details.filename;

                ifstream check_file;
                check_file.open(get_file_name.c_str());

                if(check_file.good()){
                    cout << "FAILURE:ALREADY_EXISTS";
                    continue;
                }

                down_thread_status = pthread_create(&down_thread_id, NULL, downloadFile, (void *) &cServer_details);
                if(down_thread_status != 0){
                    cout << "Could not create downloading thread";
                }

                pthread_detach(down_thread_id);
                sched_yield();
            }
           
        }

        if(toLower(option) == "exec"){
            int rpc_thread_status;
            pthread_t rpc_thread_id;
            rpcResults rpc_server_details;

            // vector to receive the user command in tokens format
            std::vector<string> v;
            
            v = split(user_command.c_str());

            
            if((int)v.size() != 3){
                cout << "FAILURE:INALID_ARGUMENTS";
                continue;
            }

            sendData(confd, user_command);

            string details_received = receiveData(confd);
            if(details_received.find("ERROR") != string::npos){
                cout << "FAILURE:CLIENT_OFFLINE";
                continue;
            }
            else{
                    /*client_ip: client_port*/
                std::vector<string> alias_details;
                alias_details = split(details_received.c_str());

                rpc_server_details.rpc_server_ip = alias_details[0];
                rpc_server_details.rpc_server_port = alias_details[1];
                rpc_server_details.command = v[2];

                }

            rpc_thread_status = pthread_create(&rpc_thread_id, NULL, receiveRpcResuls, (void *) &rpc_server_details);
            if(rpc_thread_status != 0){
                cout << "Could not create downloading thread";
            }

            pthread_detach(rpc_thread_id);
            sched_yield();

        }
        /*Recive if the file is present on the server. If yes: receive size else receive Error message regarding the same*/
        
        if(toLower(option) == "search"){
            sendData(confd, user_command);
            string rec = receiveData(confd);

            std::vector<string> search_items;
            search_items = split(rec.c_str());
            string search_hits = search_items[0];
            cout << "\nFOUND:" << search_hits << "\n";

            for(int i = 0; i < (int)((search_items.size()-1)/search_results_len); i++){
                search_results search_item;
                search_item.filename             = search_items[ (search_results_len * i) + 1];
                search_item.relative_path        = search_items[ (search_results_len * i) + 2];
                search_item.client_alias         = search_items[ (search_results_len * i) + 3];
                search_item.client_ip            = search_items[ (search_results_len * i) + 4];
                search_item.client_rpc_port      = search_items[ (search_results_len * i) + 5];
                search_item.client_download_port = search_items[ (search_results_len * i) + 6];

                string output = "[" + to_string(i+1) + "] " + search_item.filename + ":" + search_item.relative_path + ":" + search_item.client_alias + ":" + search_item.client_ip + ":" + search_item.client_rpc_port + ":" + search_item.client_download_port + "\n";  
                cout << output; 
                search_list.push_back(search_item);
            }
        }
        
        fill(recv_buffer.begin(), recv_buffer.end(), 0);
    }
    close(confd);

    pthread_exit(EXIT_SUCCESS);
}
/*****************************************************************************************************/

void * rpcRequest(void * connection_info){
    connection_data rpc_client_details;
    rpc_client_details = *((connection_data *)connection_info);


    logClient(("Thread " + to_string(pthread_self()) + " with pid " + to_string(getpid()) + " created").c_str());
    logClient(("Connection Established with " + rpc_client_details.conn_ip + " from port " + to_string(rpc_client_details.conn_port)).c_str());

    /* Receive RPC REQUEST*/
    string rpc_request = receiveData(rpc_client_details.confd);
   
    string command_request;
    string filename = ".rpc_temp";
    command_request = rpc_request + " | cat > " + filename;
    system(command_request.c_str());

    ifstream ifs(filename);
    string rpc_content((std::istreambuf_iterator<char>(ifs)),(std::istreambuf_iterator<char>()));

    /* Send RPC Results*/
    sendData(rpc_client_details.confd, rpc_content);

    command_request = "rm " + filename;
    system(command_request.c_str());

    logClient(("Thread " + to_string(pthread_self()) + " with pid " + to_string(getpid()) + " have sent rpc-results").c_str());
    pthread_exit(EXIT_SUCCESS);
}

/*****************************************************************************************************/

void * threadRpcServer(void * server_details){

    client_data rpc_server_info;
    rpc_server_info = *((client_data *) server_details);

    struct sockaddr_in servaddr, cliaddr;
    int listenfd, confd;
    pthread_t thread_id;

    /* Creating a listening socket for the client_server*/
    listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenfd < 0) 
        err_sys("Error in creating the socket");

    int val = 1, result;
    result = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
    if (result < 0) {
        err_sys("Error in setting the socket options");
    }

    cout << "\n--RPC Server Socket Created--\n" << endl;

    /* Assigning the values to the cli-server structure */
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr((rpc_server_info.client_ip).c_str());
    servaddr.sin_port = htons(rpc_server_info.client_port);

     /* Binding the socket to the values provided */
    if ((bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr))) < 0) 
        err_sys("Bind Error in RPC-Server");

    cout << "-- RPC Server waiting to accept Connections --" << endl;


    /* Listening for incoming connections */
    if( listen(listenfd, 10) < 0)
        err_sys("Listen Error in Server-Client");

    while(1){

        socklen_t clilen;
        clilen = sizeof(cliaddr);

        confd = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen);
        if(confd < 0)
            err_sys("Accept Error");

        // cout << "\n I am RPC SERVER " << confd;

        connection_data connection_info;
        connection_info.conn_ip = inet_ntoa(cliaddr.sin_addr);
        connection_info.conn_port = ntohs(cliaddr.sin_port);
        connection_info.confd = confd;

        int thread_status;
        thread_status = pthread_create(&thread_id, NULL, rpcRequest, (void *) &connection_info);
        if (thread_status != 0) {
            cout<<"Could not create new threads for rpc request";
        }

        pthread_detach(thread_id);
        sched_yield();
    }
    pthread_exit(EXIT_SUCCESS);
}

/***************************************************************************************************/
int main(int argc, char* argv[])
{
    system("clear");
    //system("wmctrl -r :ACTIVE: -N \"Server\"");

    if(argc != 8){
        err_sys("Usage Error: ./executable client_alias client_ip client_port server_ip server_port downloading_port client_root");
    }
    
    client_data client_info;
    client_info.client_alias = argv[1];
    client_info.client_ip = argv[2];
    client_info.client_port = atoi(argv[3]);
    client_info.server_ip = argv[4];
    client_info.server_port = atoi(argv[5]);
    client_info.cli_download_port = atoi(argv[6]);
    client_info.client_root = argv[7];

    client_root_global = client_info.client_root;

    // cout << client_info.client_root << " Len :" << string(client_info.client_root).length() << "\n";
    // cout.flush();

    if(client_info.client_root[string(client_info.client_root).length() - 1] == '/'){
        client_info.client_root[string(client_info.client_root).length() - 1] = '\0';
    }

    pthread_t thread_cServer_id;
    pthread_t thread_cClient_id;
    pthread_t thread_rpcServer_id;

    int thread_cClient_status, thread_cServer_status, thread_rpcServer_status;


    thread_cServer_status = pthread_create(&thread_cServer_id, 0, threadServer, (void *) &client_info);
    if(thread_cServer_status != 0){
        cout << "Could not create the server thread";
    }

    thread_cClient_status = pthread_create(&thread_cClient_id, NULL, threadClient, (void *) &client_info);    
    if(thread_cClient_status != 0){
        cout << "Could not create the client thread";
    }

    thread_rpcServer_status = pthread_create(&thread_rpcServer_id, NULL, threadRpcServer, (void *) &client_info);    
    if(thread_rpcServer_status != 0){
        cout << "Could not create the client thread";
    }

    pthread_exit(NULL);
    return 0;
}
