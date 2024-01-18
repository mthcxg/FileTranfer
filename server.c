#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <malloc.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>
#include <ctype.h>
#include <stdlib.h>
#include "utils.c"

typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr SOCKADDR;

#define MAX_CLIENT 1024
#define MAX_ROOM 100
client *clients[MAX_CLIENT];
int g_clientcount = 0;
char *g_path = "./server_file_storage/";

int compare(const void *a, const void *b)
{
    const char *room1 = (const char *)a;
    const char *room2 = (const char *)b;
    return strcmp(room1, room2);
}
int chuyen (char c){
    return (int) c - 48;
}
void read_clients_file()
{
    FILE *f = fopen("clients.txt", "rt");
    while (!feof(f))
    {
        char s[1024] = {0};
        fgets(s, sizeof(s), f);
        if (strlen(s) < 2)
        {
            break;
        }
        printf("User: %s", s);
        int cfd, room;
        char username[20] = {0};
        char password[20] = {0};
        sscanf(s, "%d %s %s %d", &cfd, username, password, &room);
        client *new_client = (client *)calloc(1, sizeof(client));
        new_client->cfd = cfd;
        new_client->username = strdup(username);
        new_client->password = strdup(password);
        new_client->room = room;
        new_client->online = 0;
        // LUU VAO DANH SACH
        clients[g_clientcount++] = new_client;
    }
    fclose(f);
}

void write_clients_file()
{
    FILE *f = fopen("clients.txt", "wt");
    for (int i = 0; i < g_clientcount; i++)
    {
        fprintf(f, "%d %s %s %d\n", clients[i]->cfd, clients[i]->username,
                clients[i]->password, clients[i]->room);
    }
    fclose(f);
}

void send_server_noti(int current_user_index, char *current_user_msg, char *room_msg)
{
    for (int i = 0; i < g_clientcount; i++)
    {
        if (current_user_index == i)
        {
            sendPacket(clients[i]->cfd, current_user_msg, strlen(current_user_msg));
            continue;
        };
        if (clients[i]->room == clients[current_user_index]->room &&
            clients[i]->online)
        {
            sendPacket(clients[i]->cfd, room_msg, strlen(room_msg));
        }
    }
}
void list_all_user_in_room(int cfd, char *buffer, int current_user_index)
{
    char USER[5] = {0};
    sscanf(buffer, "%s", USER);
    FILE *f = fopen("clients.txt", "rt");
    if (f == NULL)
    {
        char *msg = "Cannot open file.\n";
        sendPacket(cfd, msg, strlen(msg));
        return;
    }
    char line[1024];
    while (fgets(line, sizeof(line), f))
    {
        int cfd_val, room;
        char username[20];
        sscanf(line, "%*d %s %*s %d", username, &room);
        if (room == clients[current_user_index]->room)
        {
            printf("User in room: %s\n", username);
            int username_length = strlen(username);
            username[username_length] = '\n';
            username[username_length + 1] = '\0';
            sendPacket(cfd, username, strlen(username));
        }
    }
    fclose(f);
}
void list_all_file(int cfd, char *buffer, int current_user_index)
{
    FILE *f = fopen("serverlog.txt", "r");
    if (f == NULL)
    {
        char *msg = "Cannot open file.\n";
        sendPacket(cfd, msg, strlen(msg));
        return;
    }

    char line[1024];
    while (fgets(line, sizeof(line), f))
    {
        // Tìm vị trí của dấu '[' và ']'
        char *pos1 = strchr(line, '[');
        char *pos2 = strchr(line, ']');
        if (pos1 == NULL || pos2 == NULL)
        {
            continue;
        }

        char datetime[25] = {0};
        strncpy(datetime, pos1 + 1, pos2 - pos1 - 1);

        // Tìm vị trí của dấu ' và '
        char *pos3 = strchr(line, '\'');
        char *pos4 = strrchr(line, '\'');
        if (pos3 == NULL || pos4 == NULL)
        {
            continue;
        }

        // Trích xuất tên người dùng
        char username[50] = {0};
        strncpy(username, pos2 + 1, pos3 - pos2 - 2);

        // Trích xuất tên file
        char filename[256] = {0};
        strncpy(filename, pos3 + 1, pos4 - pos3 - 1);

        // Trích xuất số phòng
        char room[5] = {0};
        char *pos5 = strrchr(line, '\'');
        strncpy(room, pos5 - 1, 1);

        // Kiểm tra phòng của người dùng
        char userRoom[5];
        sprintf(userRoom, "%d", clients[current_user_index]->room);
        if (strcmp(userRoom, room) == 0)
        {
            // Gửi tên file đến client
            sendPacket(cfd, filename, strlen(filename));
            sendPacket(cfd, "\n", 1);
        }
    }
    fclose(f);
}
int handle_register(int cfd, char *buffer)
{
    char REG[5] = {0};
    char username[20] = {0};
    char password[20] = {0};
    sscanf(buffer, "%s %s %s", REG, username, password);
    if (!checkUsername(username, clients, g_clientcount))
    {
        char *msg = "Username is already used.\n";
        sendPacket(cfd, msg, strlen(msg));
        return -1;
    }
    client *new_client = (client *)calloc(1, sizeof(client));
    new_client->cfd = cfd;
    new_client->username = strdup(username);
    new_client->password = strdup(password);
    new_client->room = 0;
    new_client->online = 1;
    // LUU VAO DANH SACH
    clients[g_clientcount++] = new_client;
    char *msg = "Registered successfully!";
    sendPacket(cfd, msg, strlen(msg));
    write_clients_file();
    // TRA VE VI TRI TRONG DANH SACH
    return g_clientcount - 1;
}

int handle_login(int cfd, char *buffer)
{
    char LOGIN[10] = {0};
    char username[20] = {0};
    char password[20] = {0};
    sscanf(buffer, "%s %s %s", LOGIN, username, password);
    // NEU THANH CONG SE LA VI TRI CUA USER TRONG LIST
    int current_user_index = login(username, password, clients, g_clientcount);
    if (current_user_index == -1)
    {
        char *msg = "Wrong username or password!\n";
        sendPacket(cfd, msg, strlen(msg));
    }
    else
    {
        if (!clients[current_user_index]->room)
        {
            char *msg = "Logged in successfully! You haven't joined any room.\n";
            sendPacket(cfd, msg, strlen(msg));
        }
        else
        {
            char msg[1024] = {0};
            sprintf(msg, "Logged in successfully! Your current room: %d.\n", clients[current_user_index]->room);
            sendPacket(cfd, msg, strlen(msg));
            for (int i = 0; i < g_clientcount; i++)
            {
                if (current_user_index == i)
                    continue;
                char msg[1024] = {0};
                sprintf(msg, "%s has reconnected.\n", clients[current_user_index]->username);
                if (clients[i]->room == clients[current_user_index]->room &&
                    clients[i]->online)
                {
                    sendPacket(clients[i]->cfd, msg, strlen(msg));
                }
            }
            char activity[75];
            snprintf(activity, sizeof(activity), "%s has reconnected.\n", clients[current_user_index]->username);
            log_activity(activity);
        }
        // THAY DOI SOCKET 
        clients[current_user_index]->cfd = cfd;
        clients[current_user_index]->online = 1;
        write_clients_file();
    }
    return current_user_index;
}

void handle_logout(int *current_user_index)
{
    int index = *current_user_index;
    clients[index]->online = 0;
    char room_msg[1024] = {0};
    char activity[24];
    sprintf(room_msg, "%s has disconnected.\n", clients[index]->username);
    snprintf(activity, sizeof(activity), "%s has disconnected.\n", clients[index]->username);
    log_activity(activity);
    send_server_noti(index, "Logged out successfully\n", room_msg);
    *current_user_index = -1;
}

void handle_register_room(int cfd, char *buffer, int current_user_index)
{
    char ROOM[10] = {0};
    int room = 0;
    sscanf(buffer, "%s %d", ROOM, &room);
    FILE *f = fopen("clients.txt", "rt");
    if (f == NULL) {
        char *msg = "Cannot open file.\n";
        sendPacket(cfd, msg, strlen(msg));
        return;
    }
  
    int isRoomFound = 0; // Biến cờ đánh dấu
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        int cfd_val, user_room;
        sscanf(line, "%d %*s %*s %d", &cfd_val, &user_room);
        
        if (user_room == room) {
            isRoomFound = 1;
            break;
        }
    }
  
    fclose(f);
  
    if (!isRoomFound) {
        char msg[1024] = {0};
        sprintf(msg, "Room %d does not exist.\n", room);
        sendPacket(cfd, msg, strlen(msg));
        return;
    }
    // XU LY CHON TRUNG ROOM
    if (clients[current_user_index]->room == room)
    {
        char msg[1024] = {0};
        sprintf(msg, "You are already in room %d\n", clients[current_user_index]->room);
        sendPacket(cfd, msg, strlen(msg));
        return;
    }
    // THONG BAO CHO CAC THANH VIEN CON LAI TRONG NHOM CHAT
    if (clients[current_user_index]->room)
    {
        char msg[1024] = {0};
        char activity[100];
        sprintf(msg, "%s has left the chat.\n", clients[current_user_index]->username);
        snprintf(activity, sizeof(activity), "%s has left the chat.\n", clients[current_user_index]->username);
        log_activity(activity);
        send_server_noti(current_user_index, "", msg);
    }
    // THONG BAO CHO CAC THANH VIEN TRONG NHOM CHAT MOI
    clients[current_user_index]->room = room;
    char current_user_msg[1024] = {0};
    sprintf(current_user_msg, "You have joined room %d.\n", room);
    char room_msg[1024] = {0};
    char activity[100];
    sprintf(room_msg, "%s has joined the chat.\n", clients[current_user_index]->username);
    snprintf(activity, sizeof(activity), "%s has joined the chat.\n", clients[current_user_index]->username);
    log_activity(activity);
    send_server_noti(current_user_index, current_user_msg, room_msg);
    write_clients_file();
    return;
}
void list_all_room(int cfd, char*buffer){
    FILE *f = fopen("clients.txt", "rt");
    if (f == NULL)
    {
        char *msg = "Cannot open file.\n";
        sendPacket(cfd, msg, strlen(msg));
        return;
    }

    char line[1024];
    char temp_room_list[MAX_ROOM][100];
    int temp_room_count = 0;

    while (fgets(line, sizeof(line), f))
    {
        char room_str[100];
        sscanf(line, "%*d %*s %*s %s", room_str);
        strcpy(temp_room_list[temp_room_count], room_str);
        temp_room_count++;
    }

    fclose(f);

    int room_list_count = 0;
    char room_list[MAX_ROOM][100];

    for (int i = 0; i < temp_room_count; i++)
    {
        int is_duplicate = 0;
        for (int j = 0; j < room_list_count; j++)
        {
            if (strcmp(room_list[j], temp_room_list[i]) == 0)
            {
                is_duplicate = 1;
                break;
            }
        }
        if (!is_duplicate)
        {
            strcpy(room_list[room_list_count], temp_room_list[i]);
            room_list_count++;
        }
    }
    qsort(room_list, room_list_count, sizeof(room_list[0]), compare);
    char ALL[10] = {0};
    sscanf(buffer, "%s", ALL);

    char msg1[1024] = {0};
    sprintf(msg1, "List of rooms: \n");
    sendPacket(cfd, msg1, strlen(msg1));

    for (int i = 0; i < room_list_count; i++)
    {
        int room_length = strlen(room_list[i]);
        room_list[i][room_length] = '\n';
        room_list[i][room_length + 1] = '\0';
        sendPacket(cfd, room_list[i], strlen(room_list[i]));
    }
    char msg2[1024] = {0};
    sprintf(msg2, "Use ROOM <number> to join a room, or CREATE <number> to create a new room\n");
    sendPacket(cfd, msg2, strlen(msg2));
}
void handle_create_room(int cfd, char *buffer, int current_user_index)
{

    FILE *f = fopen("clients.txt", "rt");
    if (f == NULL)
    {
        char *msg = "Cannot open file.\n";
        sendPacket(cfd, msg, strlen(msg));
        return;
    }

    char line[1024];
    char temp_room_list[MAX_ROOM][100];
    int temp_room_count = 0;

    while (fgets(line, sizeof(line), f))
    {
        char room_str[100];
        sscanf(line, "%*d %*s %*s %s", room_str);
        strcpy(temp_room_list[temp_room_count], room_str);
        temp_room_count++;
    }

    fclose(f);

    int room_list_count = 0;
    char room_list[MAX_ROOM][100];

    for (int i = 0; i < temp_room_count; i++)
    {
        int is_duplicate = 0;
        for (int j = 0; j < room_list_count; j++)
        {
            if (strcmp(room_list[j], temp_room_list[i]) == 0)
            {
                is_duplicate = 1;
                break;
            }
        }
        if (!is_duplicate)
        {
            strcpy(room_list[room_list_count], temp_room_list[i]);
            room_list_count++;
        }
    }
    qsort(room_list, room_list_count, sizeof(room_list[0]), compare);
    char CREATE[10] = {0};
    char room;
    sscanf(buffer, "%s %s", CREATE, &room);

    int is_duplicate = 0;
    for (int i = 0; i < room_list_count; i++)
    {
        if (strcmp(room_list[i], &room) == 0)
        {
            is_duplicate = 1;
            break;
        }
    }

    if (is_duplicate)
    {
        char *msg = "Room already exists.\n";
        sendPacket(cfd, msg, strlen(msg));
    }
    else
    {
        // Thêm phòng vào room_list
        if (room_list_count < MAX_ROOM)
        {
            strcpy(room_list[room_list_count], &room);
            room_list_count++;
            int room_int;
            char msg[1024] = {0};
            sprintf(msg, "Create room %s success\n", &room);
            sendPacket(cfd, msg, strlen(msg));
            room_int = chuyen(room);
            clients[current_user_index]->room = room_int;
            write_clients_file();
        }
        else
        {
            char *msg = "Room limit exceeded.\n";
            sendPacket(cfd, msg, strlen(msg));
        }
    }

    // In danh sách các phòng
    char msg1[1024] = {0};
    sprintf(msg1, "List of rooms: \n");
    sendPacket(cfd, msg1, strlen(msg1));

    for (int i = 0; i < room_list_count; i++)
    {
        int room_length = strlen(room_list[i]);
        room_list[i][room_length] = '\n';
        room_list[i][room_length + 1] = '\0';
        sendPacket(cfd, room_list[i], strlen(room_list[i]));
    }
}

void handle_leave_room(int current_user_index)
{
    // GUI THONG BAO TOI CAC THANH VIEN TRONG DOAN CHAT
    char msg[1024] = {0};
    char activity[100];
    sprintf(msg, "%s has left the chat.\n", clients[current_user_index]->username);
    snprintf(activity, sizeof(activity), "%s has left the chat.\n", clients[current_user_index]->username);
    log_activity(activity);
    send_server_noti(current_user_index, "Leave room successfully\n", msg);
    // DAT LAI SO PHONG CUA USER
    clients[current_user_index]->room = 0;
    write_clients_file();
}

void handle_recv_file(int cfd, char *buffer, int current_user_index)
{
    char POST[10] = {0};
    char filename[100] = {0};
    int content_length = 0;
    sscanf(buffer, "%s %s %d", POST, filename, &content_length);
    char *data = (char *)calloc(content_length, 1);
    recvPacket(cfd, data, content_length);
    char *saved_filename = create_save_path(g_path, filename);
    FILE *f = fopen(saved_filename, "wb");
    fwrite(data, 1, content_length, f);
    fclose(f);
    char room_msg[1024] = {0};
    char activity[100];
    sprintf(room_msg, "%s has uploaded a file. Use 'GET %s' to download.\n", clients[current_user_index]->username, saved_filename + strlen(g_path));
    snprintf(activity, sizeof(activity), "%s has uploaded a file name '%s' in room '%d'.\n", clients[current_user_index]->username, saved_filename + strlen(g_path), clients[current_user_index]->room);
    log_activity(activity);
    send_server_noti(current_user_index, "File uploaded successfully\n", room_msg);
}

void handle_send_file(int cfd, char *buffer, int current_user_index)
{
    char GET[10] = {0};
    char filename[100] = {0};
    sscanf(buffer, "%s %s\n", GET, filename);
    char *path = NULL;
    append(&path, g_path);
    append(&path, filename);
    printf("%s\n", path);
    FILE *f = fopen(path, "rb");
    if (f != NULL)
    {
        fseek(f, 0, SEEK_END);
        int fsize = ftell(f);
        fseek(f, 0, SEEK_SET);
        char *data = (char *)calloc(fsize, 1);
        fread(data, 1, fsize, f);
        char msg[1024] = {0};
        sprintf(msg, "GET %s %d", filename, fsize);

        int sent = 0;
        while (sent < strlen(msg))
        {
            int tmp = send(cfd, msg + sent, strlen(msg) - sent, 0);
            sent += tmp;
        }
        sleep(0.1);

        // printf("%s", mes);

        sent = 0;
        do
        {
            sent += send(cfd, data + sent, fsize - sent, 0);
        } while (sent >= 0 && sent < fsize);
    }
    else
    {
        char *msg = "Cannot find file with that name.";
        sendPacket(cfd, msg, strlen(msg));
    }
}

void send_msg_to_room(int current_user_index, char *buffer)
{
    for (int i = 0; i < g_clientcount; i++)
    {
        if (current_user_index == i)
            continue;
        char *msg = NULL;
        append(&msg, clients[current_user_index]->username);
        append(&msg, ": ");
        append(&msg, buffer);
        if (clients[i]->room == clients[current_user_index]->room &&
            clients[i]->online)
        {
            sendPacket(clients[i]->cfd, msg, strlen(msg));
        }
    }
    char activity[100];
    snprintf(activity, sizeof(activity), "%s: %s\n", clients[current_user_index]->username, buffer);
    log_activity(activity);
}

void *handle_client(void *arg)
{
    int cfd = *(int *)arg;
    free(arg);
    // BIEN DUNG DE KIEM TRA TRANG THAI DANG NHAP, = -1 NEU CHUA DANG NHAP DUOC
    int current_user_index = -1;
    char *msg = "============================\nSIMPLE SEND FILE SYSTEM\n============================\nManual\n Step 1: \nUsing the keyword LOGIN to log in to system (Ex: LOGIN username password)\nOr you can use keyword REG to register (Ex: REG username password)\nWarning: You must log in to use the system\n Step 2: Using keyword: \n'ROOM' to join a room for file transfer\n'LEAVE' to leave a room\n'LOGOUT' to log out system\n'POST' to post a file (Ex: POST filename)\n'GET' to download a file (Ex: GET filename)\n'USER' to list all user in a room\n'ALL' to list all room\n'FILES' to list all file in a room\n'CREATE' to create new room (Ex: CREATE roomnumber)\n============================\n";
    sendPacket(cfd, msg, strlen(msg));
    while (1)
    {
        char buffer[1024] = {0};
        int r = recv(cfd, buffer, sizeof(buffer), 0);
        if (r > 0)
        {
            printf("Received: %s\n", buffer);
            // CHI CO REG VA LOGIN THUC HIEN DUOC KHI CHUA DANG NHAP
            if (strncmp(buffer, "REG", 3) == 0)
            {
                current_user_index = handle_register(cfd, buffer);
            }
            else if (strncmp(buffer, "LOGIN", 5) == 0)
            {
                current_user_index = handle_login(cfd, buffer);
            }
            else
            {
                if (current_user_index == -1)
                {
                    char *msg = "You have not logged in\n";
                    sendPacket(cfd, msg, strlen(msg));
                    // PHAN XU LY KHI DA LOGIN THANH CONG
                }
                else
                {
                    if (strncmp(buffer, "ROOM", 4) == 0)
                    {
                        handle_register_room(cfd, buffer, current_user_index);
                    }
                    else if (strncmp(buffer, "LEAVE", 5) == 0)
                    {
                        handle_leave_room(current_user_index);
                    }
                    else if (strncmp(buffer, "LOGOUT", 6) == 0)
                    {
                        handle_logout(&current_user_index);
                    }
                    else if (strncmp(buffer, "POST", 4) == 0)
                    {
                        handle_recv_file(cfd, buffer, current_user_index);
                    }
                    else if (strncmp(buffer, "GET", 3) == 0)
                    {
                        handle_send_file(cfd, buffer, current_user_index);
                    }
                    else if (strncmp(buffer, "USER", 4) == 0)
                    {
                        list_all_user_in_room(cfd, buffer, current_user_index);
                    }
                    else if (strncmp(buffer, "FILES", 5) == 0)
                    {
                        list_all_file(cfd, buffer, current_user_index);
                    }
                    else if (strncmp(buffer, "CREATE", 6) == 0)
                    {
                        handle_create_room(cfd, buffer, current_user_index);
                    }else if (strncmp(buffer, "ALL", 3) == 0)
                    {
                        list_all_room(cfd,buffer);
                    }
                    else
                    {
                        // KIEM TRA XEM CO PHONG CHUA
                        if (!clients[current_user_index]->room)
                        {
                            char *msg = "CANNOT chat! You have not registered room, use \"ROOM <number>\" to register.\n";
                            sendPacket(cfd, msg, strlen(msg));
                        }
                        else
                        {
                            send_msg_to_room(current_user_index, buffer);
                        }
                    }
                }
            }
        }
    }
}

void free_clients()
{
    for (int i = 0; i < g_clientcount; i++)
    {
        free(clients[i]->username);
        free(clients[i]->password);
        free(clients[i]);
    }
}
void log_activity(char *msg)
{
    FILE *log_file = fopen("serverlog.txt", "a");
    time_t raw_time;
    struct tm *time_info;

    time(&raw_time);
    time_info = localtime(&raw_time);
    char *time_str = asctime(time_info);
    time_str[strlen(time_str) - 1] = '\0';

    printf("[%s]%s", time_str, msg);
    fprintf(log_file, "[%s]%s", time_str, msg);

    fclose(log_file);
}

int main()
{
    read_clients_file();
    int sfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    SOCKADDR_IN saddr, caddr;
    int clen = sizeof(caddr);
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(5000);
    saddr.sin_addr.s_addr = 0;
    bind(sfd, (SOCKADDR *)&saddr, sizeof(saddr));
    listen(sfd, 20);
    while (1)
    {
        int cfd = accept(sfd, (SOCKADDR *)&caddr, &clen);
        pthread_t pid;
        int *arg = (int *)calloc(1, sizeof(int));
        *arg = cfd;
        pthread_create(&pid, NULL, handle_client, arg);
    }
    write_clients_file();
    free_clients();
    close(sfd);
}