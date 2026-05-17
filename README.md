# SISOP-4-2026-IT-003
Praktikum Sistem Operasi Modul 4 - File System - FUSE

Nama : Chilmi Muhammad Ulin Nuha

NRP : 5027251003

## Reporting
### Soal 1 - Save Asisten Kenz

Pada soal ini, kita diminta untuk menyelamatkan Asisten Kenz dengan cara menemukan koordinat rahasia dari Mas Amba. Kita harus membuat sebuah program *Filesystem in Userspace* (FUSE) bernama `kenz_rescue.c`. FUSE ini bertindak sebagai *passthrough* (cermin) dari sebuah direktori sumber (`amba_files`), namun dengan modifikasi untuk menyuntikkan sebuah file virtual (`tujuan.txt`) yang isinya dibangkitkan secara dinamis (*on-the-fly*) dari gabungan file-file sumber tanpa mengubah direktori aslinya sama sekali.

#### Persiapan dan Setup - kenz_rescue.c

Langkah pertama pada program FUSE ini adalah mendefinisikan *header* yang dibutuhkan dan sebuah variabel global untuk menyimpan *absolute path* dari direktori sumber (dalam hal ini direktori `amba_files`). 

```c
#define FUSE_USE_VERSION 31
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdlib.h>

char dirpath[1000]; // Menyimpan path asli (source directory)

// Menggabungkan path FUSE dengan path asli
static void get_full_path(char fpath[1000], const char *path) {
    if (strcmp(path, "/") == 0) {
        path = "";
    }
    sprintf(fpath, "%s%s", dirpath, path);
}
```

Sebuah fungsi *helper* `get_full_path` juga dibuat untuk mempermudah translasi direktori (contoh: me-*mapping* akses `/1.txt` di *mount point* menjadi `/path/ke/amba_files/1.txt`).

#### Modifikasi Atribut File (getattr)

Untuk membuat file virtual `tujuan.txt` seolah-olah nyata, kita harus mencegat pemanggilan `getattr`. Jika *user* mencari atribut untuk `/tujuan.txt`, program tidak akan meneruskannya ke direktori asli, melainkan mengembalikan struct stat palsu (*mock stat*) yang mendefinisikan perizinan *read-only* dan ukuran fiktif. 

```c
// === POIN C: Modifikasi Getattr ===
static int xmp_getattr(const char *path, struct stat *stbuf) {
    // Memalsukan keberadaan file virtual tujuan.txt
    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/tujuan.txt") == 0) {
        stbuf->st_mode = S_IFREG | 0444; // Read-Only
        stbuf->st_nlink = 1;
        stbuf->st_size = 1024;
        return 0;
    }

    char fpath[1000];
    get_full_path(fpath, path);
    int res = lstat(fpath, stbuf);

    if (res == -1) return -errno;
    return 0;
}
```

#### Injeksi Direktori (readdir)

Agar file virtual tersebut muncul saat perintah `ls` dijalankan di *root mount point*, fungsi `readdir` dimodifikasi. Program akan membaca direktori asli seperti biasa, namun pada akhir fungsi, program menyuntikkan satu entri tambahan bernama `tujuan.txt` menggunakan fungsi `filler`.

```c
// === POIN C: Modifikasi Readdir ===
static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    char fpath[1000];
    get_full_path(fpath, path);

    DIR *dp = opendir(fpath);
    if (dp == NULL) return -errno;

    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;
        filler(buf, de->d_name, &st, 0);
    }
    closedir(dp);

    // Menambahkan file virtual tujuan.txt saat ls root
    if (strcmp(path, "/") == 0) {
        filler(buf, "tujuan.txt", NULL, 0);
    }
    return 0;
}
```

#### Manipulasi String On-The-Fly (read)

Ini adalah inti dari penemuan koordinat rahasia. Ketika perintah `cat mnt/tujuan.txt` dijalankan, program tidak membaca dari disk fisik. Program akan melakukan iterasi untuk membuka file `1.txt` hingga `7.txt` secara fisik, mencari baris yang mengandung *substring* `"KOORD:"`, lalu merangkai hasilnya (*concatenate*) ke dalam satu *buffer string* besar yang akhirnya dikembalikan kepada *user*. File selain `tujuan.txt` akan dilewatkan menggunakan operasi `pread` standar.

```c
// === POIN D: Modifikasi Read ===
static int xmp_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    // Merakit isi tujuan.txt dari file 1.txt - 7.txt
    if (strcmp(path, "/tujuan.txt") == 0) {
        char hasil_akhir[2048] = "Tujuan Mas Amba: ";

        for (int i = 1; i <= 7; i++) {
            char file_target[1050];
            sprintf(file_target, "%s/%d.txt", dirpath, i);
            FILE *fp = fopen(file_target, "r");

            if (fp != NULL) {
                char line[512];
                while (fgets(line, sizeof(line), fp)) {
                    char *koord_ptr = strstr(line, "KOORD:");
                    if (koord_ptr != NULL) {
                        koord_ptr += 6;
                        while (*koord_ptr == ' ') koord_ptr++;
                        koord_ptr[strcspn(koord_ptr, "\r\n")] = 0;
                        strcat(hasil_akhir, koord_ptr);
                        break;
                    }
                }
                fclose(fp);
            }
        }
        strcat(hasil_akhir, "\n");
        size_t len = strlen(hasil_akhir);

        if (offset >= len) return 0;
        if (offset + size > len) size = len - offset;
        
        memcpy(buf, hasil_akhir + offset, size);
        return size;
    }

    // Passthrough read untuk file biasa (1.txt - 7.txt)
    char fpath[1000];
    get_full_path(fpath, path);
    int fd = open(fpath, O_RDONLY);
    if (fd == -1) return -errno;

    int res = pread(fd, buf, size, offset);
    if (res == -1) res = -errno;
    close(fd);
    return res;
}
```

#### Eksekusi Main dan Registrasi Callback

Fungsi `main` digunakan untuk memvalidasi argumen yang di-*passing* melalui terminal. Sesuai soal yang meminta parameter `./kenz_rescue <source_directory> <mount_directory>`, kita menggunakan fungsi `realpath` untuk menyimpan *source* dan menggeser pointer array `argv` agar `fuse_main` dapat mengenali lokasi *mount point* yang diinginkan.

```c
static struct fuse_operations xmp_oper = {
    .getattr = xmp_getattr,
    .readdir = xmp_readdir,
    .read    = xmp_read,
};

int main(int argc, char *argv[]) {
    // Validasi argumen input
    if (argc < 3) {
        printf("Usage: %s <source_dir> <mount_dir>\n", argv[0]);
        return 1;
    }

    // Mengambil absolute path source directory
    realpath(argv[1], dirpath);

    // Menggeser argumen agar FUSE membaca mount_dir
    argv[1] = argv[2];
    argc--;

    // Menjalankan filesystem FUSE
    return fuse_main(argc, argv, &xmp_oper, NULL);
}
```

#### Output

##### Ekstraksi File Zip Amba
![s1_unzip](assets/s1_unzip.png)

##### FUSE Terpasang & File Virtual Muncul
![s1_fuse](assets/s1_fuse.png)

##### Hasil Operasi `cat tujuan.txt`
![s1_cat](assets/s1_cat.png)

### Soal 2 - Poke MOO

Pada soal ini, kita diminta untuk mengimplementasikan sistem manajemen basis data (`DBMS`) relasional sederhana yang berjalan di dalam container Docker (`server`), berinteraksi menggunakan aplikasi client (`client.c`), dan datanya diamankan menggunakan sistem berkas virtual FUSE (`fuse.c`). FUSE bertindak sebagai lapisan keamanan yang secara otomatis mengenkripsi isi file database menggunakan algoritma XOR dengan key 0x76 sebelum disimpan ke direktori fisik (`encrypted_storage`), serta mendekripsinya kembali ketika diakses melalui direktori mount (`fuse_mount`).

#### FUSE Enkripsi & Dekripsi - fuse.c

Pada berkas `fuse.c`, kita mendefinisikan struktur operasi FUSE yang lengkap untuk mendukung manipulasi file database oleh server, seperti pembuatan, penulisan, pembaruan, hingga penghapusan berkas/direktori. Algoritma enkripsi utama menggunakan fungsi XOR sederhana.

```c
#define FUSE_USE_VERSION 31
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>

char *dirpath = "/home/chil/SISOP-4-2026-IT-003/soal_2/encrypted_storage";

// Fungsi XOR 0x76 untuk enkripsi/dekripsi isi file
void xor_cipher(char *data, size_t size) {
    for (size_t i = 0; i < size; i++) {
        data[i] ^= 0x76;
    }
}

// Fungsi pembantu untuk menambah .enc pada path berkas
void encode_path(char *fpath, const char *path) {
    char temp_path[1000];
    sprintf(temp_path, "%s%s", dirpath, path);

    struct stat st;
    if (stat(temp_path, &st) == 0 && S_ISDIR(st.st_mode)) {
        strcpy(fpath, temp_path);
    } else {
        sprintf(fpath, "%s%s.enc", dirpath, path);
    }
}
```

Setiap file database yang dibuat akan otomatis dialihkan ke format `.enc` di dalam folder asli melalui fungsi helper encode_path. Operasi standar Linux seperti `read`, `write`, `truncate`, `unlink`, dan `rmdir` diimplementasikan secara transparan agar engine database di dalam Docker mengira mereka berinteraksi dengan file system normal.

```c
static int xmp_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    (void) fi;
    char fpath[1000];
    encode_path(fpath, path);
    int fd = open(fpath, O_RDONLY);
    if (fd == -1) return -errno;

    int res = pread(fd, buf, size, offset);
    if (res == -1) res = -errno;
    else xor_cipher(buf, res); // Dekripsi saat dibaca oleh server

    close(fd);
    return res;
}

static int xmp_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    (void) fi;
    char fpath[1000];
    encode_path(fpath, path);
    int fd = open(fpath, O_WRONLY);
    if (fd == -1) return -errno;

    char *temp = malloc(size);
    if (temp == NULL) return -ENOMEM;
    memcpy(temp, buf, size);
    xor_cipher(temp, size); // Enkripsi sebelum ditulis ke encrypted_storage

    int res = pwrite(fd, temp, size, offset);
    free(temp);
    if (res == -1) res = -errno;

    close(fd);
    return res;
}
```

Struktur pendaftaran operasi pada FUSE menggunakan versi lengkap untuk menjamin kestabilan dan fungsionalitas penuh terhadap manajemen database:
```c
static struct fuse_operations xmp_oper = {
    .getattr  = xmp_getattr,
    .readdir  = xmp_readdir,
    .mkdir    = xmp_mkdir,
    .rmdir    = xmp_rmdir,   
    .create   = xmp_create,
    .unlink   = xmp_unlink,   
    .truncate = xmp_truncate, 
    .read     = xmp_read,
    .write    = xmp_write,
    .utimens  = xmp_utimens,
};

int main(int argc, char *argv[]) {
    return fuse_main(argc, argv, &xmp_oper, NULL);
}
```

#### Docker Deployment - Dockerfile
Database Server disebarkan di dalam lingkungan terisolasi menggunakan `Docker`. Berkas biner server dimasukkan ke dalam basis image Ubuntu, membuka port 9000, dan direktori `/app/db` di dalam kontainer di-bind mount secara langsung ke direktori `fuse_mount` milik host.
```docker
FROM ubuntu:latest

WORKDIR /app

COPY server /app/

RUN chmod +x /app/server

EXPOSE 9000

CMD ["./server"]
```

#### Database Client - client.c
Program `client.c` adalah antarmuka berbasis CLI yang menghubungkan pengguna ke Server Database di dalam kontainer menggunakan TCP Socket via port 9000. Pengguna dapat mengirimkan perintah DBMS seperti `CREATE DATABASE`, `CREATE TABLE`, `INSERT`, dan `LIST`.

```c
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 9000

int main(int argc, char const *argv[]) {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[1024] = {0};
    char input[1024];

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        return -1;
    }

    printf("Connected to DB Server on port %d\n", PORT);
    printf("Type HELP for available commands\n");
    printf("Type EXIT to quit\n\n");

    while(1) {
        printf("db > ");
        fgets(input, 1024, stdin);
        input[strcspn(input, "\n")] = 0;

        if(strcmp(input, "EXIT") == 0) {
            break;
        }

        if(strlen(input) == 0) continue;

        send(sock, input, strlen(input), 0);

        memset(buffer, 0, 1024);
        int valread = read(sock, buffer, 1024);

        if (valread > 0) {
            printf("%s\n\n", buffer);
        }
    }

    close(sock);
    return 0;
}
```

#### Output

##### FUSE Terpasang
![s2_fuse](assets/s2_fuse.png)

##### Sinkronisasi Data Transparan / Tes Dekripsi
![s2_decrypt](assets/s2_decrypt.png)

##### Pembuatan & Pembaruan File via Client/FUSE
![s2_client](assets/s2_client.png)

##### Struktur File System Hasil Enkripsi
![s2_encrypt](assets/s2_encrypt.png)
