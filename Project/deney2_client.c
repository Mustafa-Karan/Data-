#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <strings.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <ctype.h>
#include <dirent.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_FILES 100

void create_directories();
void print_menu();
void load_client_files();
void show_file_list_with_name();
void send_file_by_name(int client_socket, const char *file_name);
typedef struct {
    char name[BUFFER_SIZE];
    int file_index;
} FileInfo;
FileInfo *client_files = NULL;
int client_file_count = 0;


int main() {
    int id; // Sunucudan alınacak client ID
    int client_socket;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE],file_name[BUFFER_SIZE];
    ssize_t bytes_read;
    int choice,file_index;

  
    create_directories();
    

    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        perror("Socket oluşturulamadı\n");
        exit(EXIT_FAILURE);
    }

    
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    //server_addr.sin_addr.s_addr = INADDR_ANY;//inet_addr("127.0.0.1"); // localhost
    
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Sunucuya bağlanılamadı\n");
        close(client_socket);
        exit(EXIT_FAILURE);
    }
    printf("Sunucuya başarıyla bağlanıldı\n");

    
        // Client ID alımı
    bytes_read = recv(client_socket, buffer, BUFFER_SIZE, 0);
    if (bytes_read <= 0) {
        perror("Sunucudan Client ID alınamadı\n");
        close(client_socket);
        exit(EXIT_FAILURE);
    }
    buffer[bytes_read] = '\0';
    sscanf(buffer, "Client ID: %d", &id);
    printf("Sunucudan alınan Client ID: %d\n", id);
    printf("Server'a başarıyla bağlanıldı.\n");

    // Menü işlemleri
    while (1) {
        print_menu();
        scanf("%d", &choice);
        getchar(); // Enter tuşunu temizlemek için
        switch (choice) {
            case 3: // Dosya Yükleme
    load_client_files();
    show_file_list_with_name();
    printf("Yüklemek istediğiniz dosya ismini girin: ");
    fgets(file_name, sizeof(file_name), stdin);
    file_name[strcspn(file_name, "\n")] = '\0'; // '\n' karakterini kaldır
    if (strlen(file_name) == 0) {  // Dosya ismi boş mu kontrol ediyorum
        printf("Geçersiz dosya ismi!\n");
        break;
    }
    if (strlen(file_name) >= BUFFER_SIZE - 1) {
    printf("Hata: Dosya ismi çok uzun.\n");
    break;
    }
    snprintf(buffer, sizeof(buffer), "upload %s",file_name);
    send(client_socket, buffer, strlen(buffer), 0);
    send_file_by_name(client_socket, file_name); // Dosyayı gönder
    break;
          case 1: // Dosya Listeleme
                memset(buffer,'\0',BUFFER_SIZE);
    strcpy(buffer, "list");
    send(client_socket, buffer, strlen(buffer), 0);
    printf("Sunucudaki yüklü dosyalar:\n");

    while (1) {
        bytes_read = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);

        if (bytes_read <= 0) { 
            perror("Sunucu bağlantısı kesildi.");
            break;
        }
        buffer[bytes_read] = '\0'; // Alınan veriyi sonlandır
        if (strcmp(buffer, "END") == 0) { // Bitirme mesajı kontrolü
            buffer[bytes_read] = '\0'; // Alınan veriyi sonlandır

            break;
        }
        printf("%s", buffer);
    }
    break;


case 2: // Dosya İndirme
    printf("İndirmek istediğiniz dosya ismini girin: ");
    fgets(file_name, sizeof(file_name), stdin);
    if ( file_name!= NULL) {
        // fgets satır sonu karakterini ('\n') kaldırıyorum
        size_t len = strlen(file_name);
        if (len > 0 && file_name[len - 1] == '\n') {
            file_name[len - 1] = '\0';
        }
    } else {
        printf("Dosya ismi alınamadı.\n");
        break;
    }

    memset(buffer, '\0', BUFFER_SIZE);
    snprintf(buffer, sizeof(buffer), "download %s", file_name);
    send(client_socket, buffer, strlen(buffer), 0);

    char download_path[BUFFER_SIZE];
    snprintf(download_path, sizeof(download_path), "filesclient/clientdownload/%s", file_name);

    FILE *file = fopen(download_path, "wb");
    if (!file) {
        printf("Dosya açılamadı: %s\n", download_path);
        break;
    }

    while (1) {
        printf("Dosya indiriliyor: %s\n", file_name);
        bytes_read = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_read < BUFFER_SIZE && strcmp(buffer, "END") == 0) {
            printf("Dosya indirme tamamlandı: %s\n", file_name);
            break;
        }
        
        if (bytes_read <= 0) {
            printf("Sunucu bağlantısı kesildi veya veri alınamadı.\n");
            break;
        }
        // Gelen veriyi dosyaya yaz
        fwrite(buffer, 1, bytes_read, file);
    }

    fclose(file);
    break;
    case 5: // Yüklenen dosya İndirme
    printf("İndirmek istediğiniz dosya ismini girin: ");
    fgets(file_name, sizeof(file_name), stdin);
    file_name[strcspn(file_name, "\n")] = '\0';
    snprintf(buffer, sizeof(buffer), "uploaddownload %s",file_name);
    send(client_socket, buffer, strlen(buffer), 0);
    char updownload_path[BUFFER_SIZE];
    snprintf(updownload_path, sizeof(updownload_path), "filesclient/clientdownload/%s", file_name);
    FILE *upfile = fopen(updownload_path, "wb");
    if (upfile) {
        printf("Dosya indiriliyor: %s\n", file_name);
        while ((bytes_read = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) {
            if (strcmp(buffer, "END") == 0) break;
            fwrite(buffer, 1, bytes_read, file);
        }
        fclose(file);
        printf("Dosya başarıyla indirildi: %s\n", updownload_path);
    } else {
        printf("Dosya açılamadı: %s\n", updownload_path);
    }
    break;
                   case 4: // upload dosya Listeleme
    snprintf(buffer, sizeof(buffer), "uploadshow");
    send(client_socket, buffer, strlen(buffer), 0);
    printf("Sunucudaki yüklü dosyalar:\n");

    while ((bytes_read = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) {
            if (strcmp(buffer, "END") == 0) break;
            if (bytes_read <= 0) { 
            perror("Sunucu bağlantısı kesildi.");
            break;
        }
        buffer[bytes_read] = '\0'; // Alınan veriyi sonlandır
        printf("%s", buffer);
    }
    break;
            case 6: // Çıkış
                snprintf(buffer, sizeof(buffer), "exit %d", id);
                send(client_socket, buffer, strlen(buffer), 0);
                printf("Client ID gönderildi: %d\n", id);
                close(client_socket);
                return 0;

            default:
                printf("Geçersiz seçim. Tekrar deneyin.\n");
                print_menu();
                break;
        }
    }

    return 0;
}

void create_directories() {
    mkdir("filesclient", 0777);
    mkdir("filesclient/clientupload", 0777);
    mkdir("filesclient/clientdownload", 0777);
}

void print_menu() {
    printf("\n--- Menü ---\n");
    printf("1. Dosyaları Listele\n");
    printf("2. Dosya İndir\n");
    printf("3. Dosya Yükle\n");
    printf("4. Yüklenen dosyaları listele\n");
    printf("5. Yüklenen dosyalardan indir\n");
    printf("6. Çıkış\n");
    printf("Seçiminizi girin: ");
}
void load_client_files() {
    //clear(BUFFER_SIZE);
    DIR *dir = opendir("filesclient/clientupload");
    struct dirent *entry;

    if (dir) {
        client_files = (FileInfo *)malloc(MAX_FILES * sizeof(FileInfo));
        client_file_count = 0;

        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_type == DT_REG) {
                strncpy(client_files[client_file_count].name, entry->d_name, BUFFER_SIZE);
                //server_files[server_file_count].file_pointer = NULL;
                client_files[client_file_count].file_index=client_file_count;
                client_file_count++;
            }
        }
          closedir(dir);
    } else {
        perror("Dizin açılamadı");
        exit(EXIT_FAILURE);
    }
}
void send_file_by_name(int server_socket, const char *file_name) {
    char buffer[BUFFER_SIZE];
    //memset(buffer,'\0',BUFFER_SIZE);
    char file_path[BUFFER_SIZE];
    snprintf(file_path, sizeof(file_path), "filesclient/clientupload/%s", file_name); // Yerel dosya konumu

    FILE *file = fopen(file_path, "rb");
    if (!file) {
        printf("Dosya açılamadı: %s\n", file_name);
        snprintf(buffer, sizeof(buffer), "Dosya bulunamadı: %s\n", file_name);
        send(server_socket, buffer, strlen(buffer), 0);
        send(server_socket, "END", strlen("END"), 0);
        return;
    }
    size_t bytes_read;
    printf("Dosya gönderiliyor: %s\n", file_name);
    while (1) {
         /*bytes_read = fread(buffer, 1, BUFFER_SIZE, file);
        if (bytes_read > 0) {
            send(server_socket, buffer, bytes_read, 0);
        }
        if (bytes_read < BUFFER_SIZE) {
            break;
        }*/
       if (feof(file)) {
                printf("Dosya gönderimi tamamlandı: %s\n", file_name);
                break; // Dosyanın sonuna ulaşıldı
            }
            bytes_read = fread(buffer, 1, BUFFER_SIZE, file);//()>0
            if (send(server_socket, buffer, bytes_read, 0) == -1) {
                printf("Veri gönderim hatası!\n");
                break;
            }
        //}
        if (bytes_read < BUFFER_SIZE) {
            if (feof(file)) {
                printf("Dosya gönderimi tamamlandı: %s\n", file_name);
                break; // Dosyanın sonuna ulaşıldı
            }
            if (ferror(file)) {
                printf("Dosya okuma hatası: %s\n", file_name);
                break; // Hata oluştu
            }
        }
    }
    fclose(file);
    send(server_socket, "END", strlen("END"), 0); // İndirme sonlandırma sinyali
}
void show_file_list_with_name() {
    char buffer[BUFFER_SIZE] = "Mevcut dosyalar:\n";
    for (int i = 0; i < client_file_count; i++) {
        char temp[BUFFER_SIZE];
        snprintf(temp, sizeof(temp), "%s\n",client_files[i].name);
        strcat(buffer, temp);
    }
    printf("%s",buffer);
}