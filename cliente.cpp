#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <ifaddrs.h>
#include <map>
#include <vector>
#include "usuario.h"
#include <time.h>
#include <ctime>
#include <iostream>
#include <iomanip>
#include "json.hpp"

using namespace std;
using json = nlohmann::json;

#define CONNECTED 1
#define DISCONNECTED 0
#define BUFFER_SIZE 8192

map<string, vector<string>> inbox_messages;
int connected;
int accept_incoming = 0;
pthread_mutex_t mtx;
json priv_chat_temp;
json gen_chat_temp;

void *listen(void *args){

    while (true)
	{

        json response;
        char buffer[BUFFER_SIZE];
        memset(buffer, 0, BUFFER_SIZE);
        int received_message = recv(*(int *)args, buffer, BUFFER_SIZE, 0);
        response = json::parse(buffer);
        
        string response_option = response["response"];
        if (response_option.compare("NEW_MESSAGE") == 0) {
            string to_who = response["body"][3];
            string message = response["body"][0];
            string who = response["body"][1];
            string delivered_at = response["body"][2];


            cout << "Message from " << who << " to " << to_who << " at " << delivered_at << ": ";
            cout << message;
            printf("%s", "\n\n"); 

            // string new_message_incoming = response["body"][0];
            // cout<<"general chat -- "<<gen_chat_temp["body"].empty()<< endl;            



            // if (to_who.compare("all") == 0) {
            //     if (gen_chat_temp["body"].empty() != 1){
            //         // cout <<"gen chat temp body "<<gen_chat_temp<< endl;
            //         // if (st1.compare(st2) != 0) {
            //         //     cout << "Message from " << who << " to " << to_who << " at " << delivered_at << ": ";
            //         //     cout << message;
            //         //     printf("%s", "\n\n");
            //         // }
            //     } else {
            //         cout << "Message from " << who << " to " << to_who << " at " << delivered_at << ": ";
            //         cout << message;
            //         printf("%s", "\n\n"); 
            //     }
            // } else {
            //     // if (priv_chat_temp["body"].empty() == 0){
            //     //     string st1 = priv_chat_temp["body"].back();
            //     //     if (st1.compare(st2) != 0) {
            //     //         cout << "Message from " << who << " to " << to_who << " at " << delivered_at << ": "<< endl;
            //     //         cout << message<< endl;
            //     //         printf("%s", "\n\n");;   
            //     //     }
            //     // } else {
            //     //     cout << "Message from " << who << " to " << to_who << " at " << delivered_at << ": "<< endl;
            //     //     cout << message<< endl;
            //     //     printf("%s", "\n\n");;
            //     // }
            // }   
        }
		if (connected == 0) pthread_exit(0);
	}
}

void send_message_to_server(string username, string ip, int flag, string message, string extra, char *buffer, int socket)   
{
    time_t server_time;
    time(&server_time);
    string serialized_message;
    memset(buffer, 0, BUFFER_SIZE);

    switch (flag)
    {
    case 1:
        {
        
        json request;

        request["request"] = "POST_CHAT";
        request["body"] = {message, username, ctime(&server_time), "all"};
        
        string parsed_request = request.dump();
        strcpy(buffer, parsed_request.c_str());
        send(socket, buffer, parsed_request.size() + 1, 0);
        }
        break;

    case 2:
        {
        json request;

        request["request"] = "POST_CHAT";
        request["body"] = {message, username, ctime(&server_time), extra};
        
        string parsed_request = request.dump();

        strcpy(buffer, parsed_request.c_str());
        send(socket, buffer, parsed_request.size() + 1, 0);

        }
        break;

    case 3:
        {
        
        json request;

        request["request"] = "PUT_STATUS";
        request["body"] = message;
        
        string parsed_request = request.dump();

        strcpy(buffer, parsed_request.c_str());
        send(socket, buffer, parsed_request.size() + 1, 0);

        }
        break;

    case 4:
        {

        json request;
        request["request"] = "GET_USER";
        request["body"] = "all";
        
        string parsed_request = request.dump();

        strcpy(buffer, parsed_request.c_str());
        send(socket, buffer, parsed_request.size() + 1, 0);
        }
        break;
    case 5:
        {

        json request;

        request["request"] = "GET_USER";
        request["body"] = message;
        
        string parsed_request = request.dump();

        strcpy(buffer, parsed_request.c_str());
        send(socket, buffer, parsed_request.size() + 1, 0);

        }
        break;
    case 7: 
    {
        json request;

        request["request"] = "END_CONEX";
        
        string parsed_request = request.dump();

        strcpy(buffer, parsed_request.c_str());
        send(socket, buffer, parsed_request.size() + 1, 0);

    }
        break;
    case 10:
    {
        json request;
        request["request"] = "GET_CHAT";
        request["body"] = "all";
        
        string parsed_request = request.dump();

        strcpy(buffer, parsed_request.c_str());
        send(socket, buffer, parsed_request.size() + 1, 0);

    }
        break;
    case 11:
    {
        json request;
        request["request"] = "GET_CHAT";
        request["body"] = extra;
        
        string parsed_request = request.dump();

        strcpy(buffer, parsed_request.c_str());
        send(socket, buffer, parsed_request.size() + 1, 0);
    }
        break;
    }
    
}

int main(int argc, char *argv[]) 
{
    int socket_fd, err;
	struct addrinfo hints = {}, *addrs, *addr;
	char server_ip[INET_ADDRSTRLEN];
	char buffer[BUFFER_SIZE];
	char *username, *server, *port;

	if (argc != 4)
	{
		cout <<"Please include the following data when running the chat: <username> <server ip> <port>"<<endl;
		return EXIT_FAILURE;
	}

    username = argv[1];
    server = argv[2];
    port = argv[3];

    memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    err = getaddrinfo(server, port, &hints, &addrs);
	if (err != 0)
	{
		fprintf(stderr, "%s: %s\n", server, gai_strerror(err));
		return EXIT_FAILURE;
	}

	for (addr = addrs; addr != NULL; addr = addr->ai_next)
	{
		if ((socket_fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol)) == -1)
		{
			continue;
		}

		if (connect(socket_fd, addr->ai_addr, addr->ai_addrlen) == -1)
		{
			close(socket_fd);
			continue;
		}
		break;
	}

	if (addr == NULL)
	{
		cout <<"Connection couldn't be established."<<endl;
		return EXIT_FAILURE;
	}

	inet_ntop(
        addr->ai_family, 
        &((struct sockaddr_in *) addr->ai_addr)->sin_addr, 
        server_ip, 
        sizeof server_ip
    );
	freeaddrinfo(addrs);

	cout << "Connecting to " << server_ip << endl;

    string message_serialized;
    time_t server_time;
    time(&server_time);

    json request;
    json response;

    request["request"] = "INIT_CONEX";
    request["body"] = {ctime(&server_time), username};

    string parsed_request = request.dump();
    cout<<parsed_request<<endl;

    strcpy(buffer, parsed_request.c_str());
    send(socket_fd, buffer, parsed_request.size() + 1, 0);
    memset(buffer, 0, BUFFER_SIZE);
	recv(socket_fd, buffer, BUFFER_SIZE, 0);
    cout<<"cara nalga: "<<buffer<<endl;
    
    response = json::parse(buffer);
    

    int code = response["code"];
    if (code != 200) {
        cout <<"Connection couldn't be established."<<endl;
        return EXIT_FAILURE;
    }

    connected = CONNECTED;

	
	pthread_attr_t attrs;
    //int ini = pthread_mutex_init(&mtx, NULL);
	pthread_attr_init(&attrs);

    printf("Finally connected...\n");

    char input_buffer[100] = {0}, *s = input_buffer;
    string temporal_private_recipient, temporal_private_message;
    int input;
    while (connected != DISCONNECTED) {
        //pthread_mutex_lock(&mtx);
        printf("1. Broadcast Message: \n");
        printf("2. Private Message: \n");
        printf("3. Change Status: \n");
        printf("4. User List: \n");
        printf("5. User Info: \n");
        printf("6. Help: \n");
        printf("7. Exit: \n");

        cin>>input;

        switch (input)
        {
        case 1:
            {
                while (true) {
                    string message;
                    cout<<"1. See messages\n2. Send Message\n3. Back"<<endl;

                    string option2;
                    cin>>option2;
                    if (option2.compare("1") == 0) {
                        sleep(1);
                        send_message_to_server(username, server_ip, 10, "", "", buffer, socket_fd);
                        memset(buffer, 0, BUFFER_SIZE);
                        recv(socket_fd, buffer, BUFFER_SIZE, 0);
                        cout<<buffer<<endl;
                        response = json::parse(buffer);

                        string responseType = response["response"];

                        if (responseType.compare("NEW_MESSAGE") == 0) {
                            memset(buffer, 0, BUFFER_SIZE);
                            recv(socket_fd, buffer, BUFFER_SIZE, 0);
                            response = json::parse(buffer);
                        }
                        if (response["body"].empty() == 0) {
                            for (int i = 0; i<response["body"].size(); i++)
                            {
                                cout << response["body"][i] << endl;
                                cout << "Message from " << response["body"][i][1] << " to all at " << response["body"][i][2] << ": "<< endl;
                                cout << response["body"][i][0]<< endl;
                                printf("%s", "\n\n");
                            }
                        } else {
                            // cout << "gen chat chat is empty" << endl;
                        }

                        gen_chat_temp = response;

                        accept_incoming = 1;

                        pthread_t client_thread_id;
                        pthread_create(&client_thread_id, &attrs, listen, (void *)&socket_fd);

                        while (true) {
                            
                            string chat_option;
                            cout<<"1. Back"<<endl;
                            cin>>chat_option;
                            if (chat_option.compare("1") == 0) {
                                pthread_cancel(client_thread_id);
                                accept_incoming = 0;
                                break;
                            }
                        }
                    } 
                    if (option2.compare("3") == 0) {
                        sleep(1);
                        accept_incoming = 0;
                        break;
                    } 
                    if (option2.compare("2") == 0) {
                        while (true) {
                            accept_incoming = 0;
                            sleep(1);

                            string str1 = "Message to all (type 'cancel' to close dialog): ";
                            cout<<str1<<endl;
                            getline(cin, message);
                            if (message.compare("") != 0) {
                                if (message.compare("cancel") == 0) {
                                    break;
                                }
                                else {
                                    send_message_to_server(username, server_ip, 1,
                                    message, "", buffer, socket_fd);
                                    memset(buffer, 0, BUFFER_SIZE);
                                    recv(socket_fd, buffer, BUFFER_SIZE, 0);

                                    response = json::parse(buffer);
                                    // cout<<"RESPONSE: "<<response<<endl;
                                    
                                }
                            }
                        }
                    }
                }
            }
            break;
        case 2:
            {
                while (true) {
                    string user_receiver;
                    string message;
                    cout<<"Whose chat would you like to open? (Type 'cancel' to close menu): "<<endl;
                    cin>>user_receiver;

                    if (user_receiver.compare("cancel") == 0) {
                        break;
                    }

                    while (true)
                    {
                        cout<<"1. See messages\n2. Send Message\n3. Back"<<endl;
                        string option2;
                        cin>>option2;
                        if (option2.compare("1") == 0) {
                            sleep(1);
                            send_message_to_server(username, server_ip, 11, "", user_receiver, buffer, socket_fd);
                        memset(buffer, 0, BUFFER_SIZE);
                        recv(socket_fd, buffer, BUFFER_SIZE, 0);
                        response = json::parse(buffer);

                        string responseType = response["response"];

                        if (responseType.compare("NEW_MESSAGE") == 0) {
                            memset(buffer, 0, BUFFER_SIZE);
                            recv(socket_fd, buffer, BUFFER_SIZE, 0);
                            response = json::parse(buffer);
                        }
                        if (response["body"].empty() == 0) {
                            for (int i = 0; i<response["body"].size(); i++)
                            {
                                cout << response["body"][i] << endl;
                                cout << "Message from " << response["body"][i][1] << " to me" << response["body"][i][2] << ": "<< endl;
                                cout << response["body"][i][0]<< endl;
                                printf("%s", "\n\n");
                            }
                        } else {
                            // cout << "gen chat chat is empty" << endl;
                        }

                            priv_chat_temp = response;

                            accept_incoming = 1;
                            pthread_t client_thread_id;
                            pthread_create(&client_thread_id, &attrs, listen, (void *)&socket_fd);
                            while (true) {
                                string chat_option;
                                cout<<"1. Back"<<endl;
                                cin>>chat_option;
                                if (chat_option.compare("1") == 0) {
                                    accept_incoming = 0;
                                    pthread_cancel(client_thread_id);
                                    break;
                                }
                            }
                        } 
                        if (option2.compare("3") == 0) {
                            sleep(1);
                            accept_incoming = 0;
                            break;
                        } 
                        if (option2.compare("2") == 0) {
                            while (true) {
                                accept_incoming = 0;
                                sleep(1);
                                string str1 = "Message to --";
                                str1.append(user_receiver);
                                str1.append("-- (type 'cancel' to close dialog): ");
                                cout<<str1<<endl;
                                getline(cin, message);
                                if (message.compare("") != 0) {
                                    if (message.compare("cancel") == 0) {
                                        break;
                                    }
                                    else {
                                        send_message_to_server(username, server_ip, 2,
                                        message, user_receiver, buffer, socket_fd);
                                        memset(buffer, 0, BUFFER_SIZE);
                                        recv(socket_fd, buffer, BUFFER_SIZE, 0);

                                        response = json::parse(buffer);
                                    }
                                }
                            }
                        }
                    }
                }
            }
            break;

        case 3:
            {
            printf("0.  Activo: \n");
            printf("1. Inactivo: \n");
            printf("2. Ocupado: \n");
        
            string status_to_change;
            cin>>status_to_change;

            send_message_to_server(username, server_ip, 3, status_to_change, "", buffer, socket_fd);
            memset(buffer, 0, BUFFER_SIZE);
            recv(socket_fd, buffer, BUFFER_SIZE, 0);

            response = json::parse(buffer);
            
            if (response["code"] == 200) {
                if (status_to_change.compare("0") == 0) {
                    cout << username << "'s status has been changed to Activo!"<<endl;
                }
                if (status_to_change.compare("1") == 0) {
                    cout << username << "'s status has been changed to Inactivo!"<<endl;
                }
                if (status_to_change.compare("2") == 0) {
                    cout << username << "'s status has been changed to Ocupado!"<<endl;
                }
            } else {
                cout << "There was an error changing " << username << "'s status!"<<endl;
                break;
            }

            }
            break;
        
        case 4:
            {
                send_message_to_server(username, server_ip, 4, "", "", buffer, socket_fd);
                memset(buffer, 0, BUFFER_SIZE);
                recv(socket_fd, buffer, BUFFER_SIZE, 0);
                
                response = json::parse(buffer);

                if (response["code"] == 200) {
                    for (int i = 0; i<response["body"].size(); i++)
                    {
                        cout << "User: " << response["body"][i][0]<<" - "<< response["body"][i][1] << endl;;
                        printf("%s", "\n");
                    }
                } else {
                    cout << "Unable to retrieve user list!" << endl;
                }
            }
            break;
        case 5:
            {
                while (true) {
                    string user;
                    cout<<"Which do you want to get their information from ? (Type 'cancel' to close menu): ";
                    cin>>user;
                    if (user.compare("cancel") == 0) {
                        break;
                    } else {
                        send_message_to_server(username, server_ip, 5,user, "", buffer, socket_fd);
                        memset(buffer, 0, BUFFER_SIZE);
                        recv(socket_fd, buffer, BUFFER_SIZE, 0);

                        if (response["code"] == 200) {
                            response = json::parse(buffer);
                            cout << "User: " << user <<" - "<< "IP: " << response["body"][0] << " - " << response["body"][1] << endl;;
                            printf("%s", "\n");
                        } else {
                            cout << "Unable to retrieve information from " << user << "!"<< endl;
                        }
                    }
                }
            }
            break; 
        case 6:
        {
            printf("ALL HELP WILL BE DISPLAYED HERE!\n");
        }
            break;
        case 7:
            send_message_to_server(username, server_ip, 7, "", "", buffer, socket_fd);
            memset(buffer, 0, BUFFER_SIZE);
            recv(socket_fd, buffer, BUFFER_SIZE, 0);

            response = json::parse(buffer);
            printf("Exiting...\n");
            connected = DISCONNECTED;
            close(socket_fd);
            return EXIT_SUCCESS;
        }
    }
}
