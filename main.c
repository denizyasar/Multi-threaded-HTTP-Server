#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <limits.h>

#define PORT 6789
#define MAX_CONNECTION 10

const int READ_BUFFER_SIZE = 256;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int thread_count = 1;
const char *GET = "GET";
const char *HEAD = "HEAD";
const char *ERROR_400 = "HTTP/1.1 400 Bad Request";
const char *ERROR_404 = "HTTP/1.1 404 Not Found";
const char *ERROR_301 = "HTTP/1.1 301 Moved Permanently";
const char *SUCCESS = "HTTP/1.1 200 Ok";
const char *HTML = "html";
const char *JPEG = "jpeg";
char *content_type;
size_t file_size;

char *read_file (char *full_path)
{
    //open file
    FILE *file = fopen (full_path, "r");

    //find file size
    fseek (file, 0, SEEK_END);
    long fileLength = ftell(file);
    //buffer for file reading
    char *text = malloc (fileLength);
    rewind(file);
    //read file
    file_size=fread(text,1, fileLength,file);

    fclose (file);
    return text;
}

char *get_full_path (char *file_path)
{
    //add current directory to path
    char current_directory[PATH_MAX];
    getcwd (current_directory, PATH_MAX);

    char *full_path=malloc(strlen (file_path) + strlen (current_directory));
    strcpy (full_path, current_directory);
    strcat (full_path, file_path);

    return full_path;
}

char *get_file_path (char *request)
{
    //get path from request
    int path_begin = strlen (GET) + 1;

    int c = 0;
    char *path = malloc (256);
    while (request[path_begin] != ' ')
    {
        path[c] = request[path_begin];
        path_begin++;
        c++;
    }
    path[c]='\0';
    return path;
}

void write_socket (int socketid, char *status, char *message)
{
    //prepare date time
    char time_buffer[256];
    time_t now = time(0);
    struct tm tm = *gmtime(&now);
    strftime(time_buffer, sizeof time_buffer, "%a, %d %b %Y %H:%M:%S %Z", &tm);
    char *date=malloc(strlen("Date :") + strlen(time_buffer)+1);
    strcpy(date,"Date :");
    strcat(date,time_buffer);

    //prepare content length
    //find message length integer value length
    char m_con_length[100];
    if(strcmp(content_type, JPEG) == 0 && status==SUCCESS)
        sprintf(m_con_length,"%d", file_size);
    else
        sprintf(m_con_length,"%d", strlen(message));
    char *contenth_length=malloc(strlen("Content-Length: ") + strlen(m_con_length) +1);
    strcpy(contenth_length,"Content-Length: ");
    strcat(contenth_length,m_con_length);


    write (socketid, status, strlen (status));
    puts(status);
    write (socketid, "\r\n", 2);
    write (socketid, date, strlen(date));
    puts(date);
    write (socketid, "\r\n", 2);
    write (socketid, "Server: My Multi-Threaded Server/1.0", strlen ("Server: My Multi-Threaded Server/1.0"));
    puts("Server: My Multi-Threaded Server/1.0");
    write (socketid, "\r\n", 2);
    //check content for image or text
    if(strcmp(content_type, JPEG) == 0 && status==SUCCESS)
    {
        write (socketid, "Content-Type: image/jpeg", strlen ("Content-Type: image/jpeg"));
        puts("Content-Type: image/jpeg");
    }
    else
    {
        write (socketid, "Content-Type: text/html; charset=UTF-8", strlen ("Content-Type: text/html; charset=UTF-8"));
        puts("Content-Type: text/html; charset=UTF-8");
    }
    write (socketid, "\r\n", 2);
    write (socketid, contenth_length, strlen (contenth_length));
    puts(contenth_length);
    write (socketid, "\r\n", 2);
    //put space before data
    write (socketid, "", strlen (""));
    write (socketid, "\r\n", 2);
    if(strcmp(content_type, JPEG) == 0 && status==SUCCESS)
        //not show image data on console
        write(socketid,message,file_size);
    else
    {
        write (socketid, message, strlen (message));
        puts(message);
    }
    write (socketid, "\r\n", 2);
    puts("******************************************************");
}

void *response_job (void *socket_arg)
{
    //prevent data inconsistency-race condition check
    //increase thread count by 1
    pthread_mutex_lock (&mutex);
    thread_count++;
    pthread_mutex_unlock (&mutex);

    //read from socket
    int sockid = *((int *) socket_arg);
    char *read_buffer = malloc (READ_BUFFER_SIZE);
    if ((read (sockid, read_buffer, READ_BUFFER_SIZE)) < 0)
        perror ("Reading from socket failed !");

    puts(read_buffer);
    puts("******************************************************");

    //only GET and HEAD requests implemented, otherwise Error 400
    if (strncmp (read_buffer, GET, strlen (GET)) == 0 || strncmp (read_buffer, HEAD, strlen (HEAD)) == 0)
    {
        char* file_path=get_file_path (read_buffer);
        //find file extension
        char* file_extension = strrchr(file_path, '.');
        //check if file is in url
        if (file_extension)
        {
            file_extension++;
            content_type = file_extension;
            //check if html or jpg file
            if(strcmp(file_extension, HTML) == 0 || strcmp(file_extension, JPEG) == 0 )
                //check if file exists
                if (access (get_full_path (file_path), F_OK) != -1)
                    write_socket (sockid, SUCCESS, read_file (get_full_path(file_path)));
                else			//file not found
                    write_socket (sockid, ERROR_404, "<html><body><h1>404 Not Found</h1></body></html>\r\n");
            else			//not html or JPEG
                write_socket (sockid, ERROR_301, "<html><body><h1>301 Moved Permanently</h1></body></html>\r\n");
        }
    }
    else				//not GET or HEAD
        write_socket (sockid, ERROR_400, "<html><body><h1>400 Bad Request</h1></body></html>\r\n");

    //thread finished-decrease thread count by 1
    pthread_mutex_lock (&mutex);
    thread_count--;
    pthread_mutex_unlock (&mutex);

    //free resources
    free (read_buffer);

    shutdown(socket_arg, SHUT_WR);
    close(socket_arg);
}

int create_listen_socket ()
{
    //IPv4 Protocol, TCP, IP (0)
    int serverfd = socket (AF_INET, SOCK_STREAM, 0);
    //check if the socket created
    if (serverfd == -1)
    {
        perror ("Error on creating socket");
        exit (EXIT_FAILURE);
    }
    printf ("Socket created successfully...\n");

    //check address and port in use, reuse them
    const int opt = 1;
    if (setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof (opt)))
    {
        perror ("Assign address and port");
        exit (EXIT_FAILURE);
    }
    printf ("Address and port usage checked successfully...\n");

    //bind socket to server address and port
    struct sockaddr_in srv_address;
    srv_address.sin_family = AF_INET;
    srv_address.sin_addr.s_addr = INADDR_ANY;	//localhost
    srv_address.sin_port = htons (PORT);	//port=6789

    if (bind (serverfd, (struct sockaddr *) &srv_address, sizeof (srv_address)) < 0)
    {
        perror ("Binding error");
        exit (EXIT_FAILURE);
    }
    printf ("Socket binding done successfully...\n");
    //listen to socket connection requests
    if (listen (serverfd, 5) < 0)
    {
        perror ("Listening of socket failed");
        exit (EXIT_FAILURE);
    }
    printf ("Listening of socket started...\n");
    printf ("Waiting for incoming connections...\n");

    return serverfd;
}

int main ()
{
    int serverfd = create_listen_socket ();

    //client connection and thread variables
    struct sockaddr_in client_address;
    int add_len = sizeof (client_address);
    pthread_t conn_thread;

    while (1)
    {
        int newsockfd = accept (serverfd, (struct sockaddr *) &client_address, (socklen_t *) & add_len);
        if (newsockfd < 0)
        {
            perror ("Error on accepting connection");
        }
        else
        {
            if (thread_count < MAX_CONNECTION)
            {
                int *arg = malloc (sizeof (int));
                *arg = newsockfd;

                //create new thread with socket as arg
                if (pthread_create (&conn_thread, NULL, response_job, arg) != 0)
                {
                    perror ("Error on creating thread");
                    exit (EXIT_FAILURE);
                }
                printf("New thread created for connection. Thread count :%d/%d \n", thread_count, MAX_CONNECTION);
                puts("---------------------------------------------------------------");
            }
            else
            {
                puts ("server is busy\n");
                write_socket (newsockfd, SUCCESS, "<html><body><h1>Server is busy !</h1></body></html>\r\n");
            }
            // sleep (1);
        }
    }
    close (serverfd);		//Stop both reception and transmission.

    exit (EXIT_SUCCESS);
}

