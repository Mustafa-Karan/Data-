#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <dirent.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>

#define BUFFER_SIZE 1024
#define PORT 8080
#define MAX_FILES 100
#define TIMEOUT 20
#define MAX_CLIENTS 5

int client_count = 0;
struct Servernfo  {
    int server_socket;
} serv_socket;

typedef struct {
    char name[BUFFER_SIZE];
    int file_index;
} FileInfo;

FileInfo *server_files ,*server_upload_files= NULL;
int server_file_count, server_upload_file_count = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void load_server_files(const char *info);
void send_file_list_with_name(int client_socket);
void send_file_by_name(int client_socket, const char *file_name);
void create_directories();
void *handle_client(void *argint);
void handle_upload(int client_socket, const char *file_name);
void time_func(int server_socket);
void send_upload_file_list_with_name(int client_socket);
void load_upload_server_files();
void send_upload_file_by_name(int client_socket, const char *file_name);
int main() {
    static int server_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;
    pthread_t tid;
    char buffer[BUFFER_SIZE];

    create_directories();
    load_server_files("downloads");

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Socket oluşturulamadı");
        exit(EXIT_FAILURE);
    }
    serv_socket.server_socket=server_socket;

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind başarısız");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 10) < 0) {
        perror("Dinleme başarısız");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Sunucu çalışıyor. İstemci bekleniyor...\n");

    while (1) {
        if (client_count == 0) {
            time_func(server_socket);
        }
        if (client_count == MAX_CLIENTS) {
            printf("Maksimum client sayısına ulaşıldı. Daha fazla istemci kabul edilmeyecek.\n");
            continue;
        }
        
        addr_size = sizeof(client_addr);
        int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_size);

        if (client_socket < 0) {
            perror("Bağlantı kabul edilemedi");
            continue;
        }
        int *socket_copy = malloc(sizeof(int));
        if (!socket_copy) {
            perror("Bellek tahsisi başarısız");
            close(client_socket);
            continue;
        }
        *socket_copy = client_socket;
        if (client_count == MAX_CLIENTS) {
            printf("Maksimum client sayısına ulaşıldı. Daha fazla istemci kabul edilmeyecek.\n");
            continue;
        }
        snprintf(buffer, sizeof(buffer), "Client ID: %d\n", client_count+1);
        send(client_socket, buffer, strlen(buffer), 0);
        pthread_mutex_lock(&mutex);
        client_count++;
        pthread_mutex_unlock(&mutex);
        printf("Bir istemci bağlandı.\n Toplam istemci sayısı: %d\n", client_count);
        if (pthread_create(&tid, NULL, handle_client, socket_copy) != 0) {
            perror("Thread oluşturulamadı");
            free(socket_copy);
            close(client_socket);
        }
    }

    close(server_socket);
    return 0;
}
void load_server_files(const char *info) {
    //clear(BUFFER_SIZE);
    
    if(strcmp(info,"downloads")==0){
        DIR *dir = opendir("files/downloads");
        struct dirent *entry;
    int max_files=MAX_FILES;
    if (dir) {
        pthread_mutex_lock(&mutex);
        server_files = (FileInfo *)malloc(MAX_FILES * sizeof(FileInfo));
        server_file_count = 0;

        while ((entry = readdir(dir)) != NULL) {
    if (entry->d_type == DT_REG) {
        if (server_file_count >= max_files) {
            max_files *= 2; 
            server_files = (FileInfo *)realloc(server_files, max_files * sizeof(FileInfo));
        }
        strncpy(server_files[server_file_count].name, entry->d_name, BUFFER_SIZE);
        server_files[server_file_count].file_index = server_file_count;
        server_file_count++;
    }
}
        closedir(dir);
        pthread_mutex_unlock(&mutex);
    } else {
        perror("Dizin açılamadı");
        exit(EXIT_FAILURE);
    }
    }else{
        DIR *dir = opendir("files/uploads");struct dirent *entry;

    if (dir) {
        pthread_mutex_lock(&mutex);
        server_upload_files = (FileInfo *)malloc(MAX_FILES * sizeof(FileInfo));
        server_upload_file_count = 0;

        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_type == DT_REG) {
                strncpy(server_upload_files[server_upload_file_count].name, entry->d_name, BUFFER_SIZE);
                //server_files[server_file_count].file_pointer = NULL;
                server_upload_files[server_upload_file_count].file_index=server_file_count;
                server_upload_file_count++;
            }
        }

        closedir(dir);
        pthread_mutex_unlock(&mutex);
    } else {
        perror("Dizin açılamadı");
        exit(EXIT_FAILURE);
    }
        
    }   
}

void send_upload_file_list_with_name(int client_socket) {
    load_server_files("uploads");
    char buffer[BUFFER_SIZE] = "Servera yüklenmiş dosyalar:\n";
    char temp[BUFFER_SIZE];
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < server_upload_file_count; i++) {
        snprintf(temp, sizeof(temp), "%s\n", server_upload_files[i].name);
    }
    pthread_mutex_unlock(&mutex);
    send(client_socket, temp, strlen(temp), 0);

    send(client_socket, "END", strlen("END"), 0);
}
void send_file_list_with_name(int client_socket) {
    load_server_files("downloads");

    char buffer[BUFFER_SIZE] = "Mevcut dosyalar:\n";

    pthread_mutex_lock(&mutex);
    for (int i = 0; i < server_file_count; i++) {
        char temp[BUFFER_SIZE];
        snprintf(temp, sizeof(temp), "%s\n", server_files[i].name);
        strcat(buffer, temp);
    }
    pthread_mutex_unlock(&mutex);

    send(client_socket, buffer, strlen(buffer), 0);
    send(client_socket, "END", strlen("END"), 0);
}
void create_directories() {
    if (mkdir("files", 0755) != 0 && errno != EEXIST) {
        perror("files dizini oluşturulamadı");
    }
    if (mkdir("files/downloads", 0755) != 0 && errno != EEXIST) {
        perror("files/downloads dizini oluşturulamadı");
    }
    if (mkdir("files/uploads", 0755) != 0 && errno != EEXIST) {
        perror("files/uploads dizini oluşturulamadı");
    }
}
void send_upload_file_by_name(int client_socket, const char *file_name) {
    load_server_files("downloads");
    char buffer[BUFFER_SIZE];
    strncpy(buffer, file_name, BUFFER_SIZE - 1);
    buffer[BUFFER_SIZE - 1] = '\0'; 
    int i;
    pthread_mutex_lock(&mutex);
    for(i=0;i<server_file_count;i++){
        if(strcmp(server_files[i].name,file_name)==0) break;
        else{
            printf("Sunucuda upload edilmiş bu isimde bir dosya bulunmamaktadır: %s",file_name);
            return;
        }
    }
    pthread_mutex_unlock(&mutex);
    char file_path[BUFFER_SIZE];
    snprintf(file_path, sizeof(file_path), "files/uploads/%s", server_files[i].name);
    FILE *file = fopen(file_path, "rb");
    if (!file) {
        snprintf(buffer, sizeof(buffer), "Dosya açılamadı: %s\n", server_files[i].name);
        send(client_socket, buffer, strlen(buffer), 0); //bunu denicem karşıda çıksın diye
        return;
    }
    printf("Dosya gönderiliyor: %s\n", server_files[i].name);
    while (1) {
        size_t bytes_read = fread(buffer, 1, BUFFER_SIZE, file);
        if (bytes_read > 0) {
            send(client_socket, buffer, bytes_read, 0);
        }
        if (bytes_read < BUFFER_SIZE) {
            break;
        }
    }
    fclose(file);
    send(client_socket, "END", strlen("END"), 0); // İndirme sonlandırma sinyali
}
/*void send_file_by_name(int client_socket, char *file_name) {
    char buffer[BUFFER_SIZE];
    strncpy(buffer, file_name, BUFFER_SIZE );
    //buffer[BUFFER_SIZE - 1] = '\0'; // Null-terminator eklemek için
    int i;
    pthread_mutex_lock(&mutex);
    for(i=0;i<server_file_count;i++){
        if(strcmp(server_files[i].name,file_name)==0) break;
        else{
            printf("Sunucuda bu isimde bir dosya bulunmamaktadır: %s",file_name);
            send(client_socket, "END", strlen("END"), 0);
            return;
        }
    }
    pthread_mutex_unlock(&mutex);
    char file_path[BUFFER_SIZE];
    snprintf(file_path, sizeof(file_path), "files/downloads/%s", server_files[i].name);
    FILE *file = fopen(file_path, "rb");
    size_t bytes_read;
    if (!file) {
        snprintf(buffer, sizeof(buffer), "Dosya açılamadı: %s\n", server_files[i].name);
        send(client_socket, buffer, strlen(buffer), 0); //bunu denicem karşıda çıksın diye
        return;
    }
    printf("Dosya gönderiliyor: %s\n", server_files[i].name);
    for(;;){
        printf("D:");
        fread(buffer, 1, BUFFER_SIZE, file);
        if (buffer> 0) {
            send(client_socket, buffer, bytes_read, 0);
        }
        else{
            send(client_socket, "END", strlen("END"), 0); // İndirme sonlandırma sinyali
            break;
        }
    }
    fclose(file);
}*/
    


void send_file_by_name(int client_socket, const char *file_name) {
    char buffer[BUFFER_SIZE];
    int i;

    pthread_mutex_lock(&mutex); 
    for (i = 0; i <= server_file_count; i++) {
         if(i == server_file_count){
            snprintf(buffer, sizeof(buffer), "Dosya bulunamadı: %s\n", file_name);
            send(client_socket, buffer, strlen(buffer), 0);
            send(client_socket, "END", strlen("END"), 0); // İndirme sonlandırma sinyali bunu başaka bi isimle cliente bildirebilirim
        return;
        }
        else if (strcmp(server_files[i].name, file_name) == 0) {
            printf("Dosya bulundu !!!");
            break; 
        }
        
    }
    pthread_mutex_unlock(&mutex);

  
    char file_path[BUFFER_SIZE];
    snprintf(file_path, sizeof(file_path), "files/downloads/%s", server_files[i].name);

    FILE *file = fopen(file_path, "rb");
    if (!file) {
        snprintf(buffer, sizeof(buffer), "Dosya açılamadı: %s\n", file_name);
        send(client_socket, buffer, strlen(buffer), 0);
        send(client_socket, "END", strlen("END"), 0); // İndirme sonlandırma sinyali bunu cliente başka şekilde bildirebiliriim
        return;
    }

    printf("Dosya gönderiliyor: %s\n", file_name);
    size_t bytes_read;
    
    while (1) {
        //size_t bytes_read;
        //size_t bytes_read = fread(buffer, 1, BUFFER_SIZE, file);
        //if (bytes_read > 0) {
            
             if (feof(file)) {
                printf("Dosya gönderimi tamamlandı: %s\n", file_name);
                break; 
            }
            bytes_read = fread(buffer, 1, BUFFER_SIZE, file);//()>0
            if (send(client_socket, buffer, bytes_read, 0) == -1) {
                printf("Veri gönderim hatası!\n");
                break;
            }
        //}
        if (bytes_read < BUFFER_SIZE) {
            if (feof(file)) {
                printf("Dosya gönderimi tamamlandı: %s\n", file_name);
                break; 
            }
            if (ferror(file)) {
                printf("Dosya okuma hatası: %s\n", file_name);
                break;
            }
        }
        //memset(bytes_read,'\0',sizeof(bytes_read));
    }

    fclose(file);
    snprintf(buffer, sizeof(buffer), "Dosya başarıyla alındı: %s\n", file_name);
    send(client_socket, buffer, strlen(buffer), 0);
    send(client_socket, "END", strlen("END"), 0);
}
void handle_upload(int client_socket, const char *file_name) {
    char buffer[BUFFER_SIZE];
    char file_path[BUFFER_SIZE];

    snprintf(file_path, sizeof(file_path), "files/uploads/%s", file_name);
  
    FILE *file = fopen(file_path, "wb");
    if (!file) {
        snprintf(buffer, sizeof(buffer), "Dosya açılamadı: %s\n", file_name);
        send(client_socket, buffer, strlen(buffer), 0);
        send(client_socket, "END", strlen("END"), 0);
        return;
    }
    printf("Dosya yükleniyor: %s\n", file_name);
    ssize_t bytes_received;
    while (1) {//(bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);) > 0
        bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        
        if (strncmp(buffer, "END", 3) == 0) {
            break;
        }
        fwrite(buffer, 1, bytes_received, file);
    }
    fclose(file);
    printf("Dosya yüklendi: %s\n", file_name);
    // Başarılı yükleme mesajı gönder
    snprintf(buffer, sizeof(buffer), "Dosya başarıyla yüklendi: %s\n", file_name);
    send(client_socket, buffer, strlen(buffer), 0);
}
void time_func(int server_socket) {
    if (client_count > 0) {
        return; 
    }
    time_t start_time = time(NULL);
    printf("Bağlı istemci bekleniyor...\n");
    fd_set read_fds;
    struct timeval timeout;
    int select_result;
    while (1) {
        if (client_count > 0) {
       
            printf("Bir istemci bağlandı, geri sayım durduruldu.\n");
            return;
        }
        
        if (time(NULL) - start_time >= TIMEOUT) {
            printf("Hiçbir istemci bağlanmadı. Sunucu kapatılıyor...\n");
            close(server_socket);
            exit(EXIT_SUCCESS);
        }
        
        FD_ZERO(&read_fds);
        FD_SET(server_socket, &read_fds);

        timeout.tv_sec = 1; 
        timeout.tv_usec = 0;

        select_result = select(server_socket + 1, &read_fds, NULL, NULL, &timeout);
        if (select_result > 0 && FD_ISSET(server_socket, &read_fds)) {
            
            printf("Bir bağlantı algılandı. Geri sayım durduruldu.\n");
            return;
        }
        
        //if ((TIMEOUT - (time(NULL) - start_time)) == 0) {
            printf("GERI SAYIM: %ld saniye kaldı...\n", (long)(TIMEOUT - (time(NULL) - start_time)));
       // }
    }
}

void *handle_client(void *arg) {
    int client_socket = *(int *)arg;
    free(arg);
    char buffer[BUFFER_SIZE], command[BUFFER_SIZE], file_name[BUFFER_SIZE];
    ssize_t bytes_read;

    while (1) {//() > 0
        bytes_read = recv(client_socket, buffer, BUFFER_SIZE, 0);
        buffer[bytes_read] = '\0';
        sscanf(buffer, "%s %s", command, file_name);

        if (strcmp(command, "list") == 0) {
            send_file_list_with_name(client_socket);
        } else if (strcmp(command, "download") == 0) {
            send_file_by_name(client_socket, file_name);
        } else if (strcmp(command, "upload") == 0) {
            handle_upload(client_socket, file_name);
        }    
          else if (strcmp(command, "uploadshow") == 0) {
            load_server_files("uploads");
            send_upload_file_list_with_name(client_socket);
        } else if (strcmp(command, "uploaddownload") == 0) {
            send_upload_file_by_name(client_socket,file_name);
         
        } else if (strcmp(command, "exit") == 0) {
            printf("Bir istemci bağlantıyı sonlandırdı.\n");
            pthread_mutex_lock(&mutex);
            client_count--;
            pthread_mutex_unlock(&mutex);
            close(client_socket);
            if (client_count == 0) {
                time_func(serv_socket.server_socket); 
            }
            return NULL;
        } else {
            snprintf(buffer, sizeof(buffer), "Bilinmeyen komut: %s\n", command);
            send(client_socket, buffer, strlen(buffer), 0);
        }
    }
 
    if (bytes_read == 0) {
        printf("Bir istemci bağlantısı aniden kesildi.\n");
        pthread_mutex_lock(&mutex);
        client_count--;
        pthread_mutex_unlock(&mutex);
        close(client_socket);
        if (client_count == 0) {
            time_func(serv_socket.server_socket); // Geri sayımı başlat
        }
    } else if (bytes_read < 0) {
        perror("recv başarısız");
    }
    return NULL;
}