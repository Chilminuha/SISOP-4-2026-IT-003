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

char dirpath[1000];

// Menggabungkan path FUSE dengan path asli
static void get_full_path(char fpath[1000], const char *path) {
    if (strcmp(path, "/") == 0) {
        path = "";
    }

    sprintf(fpath, "%s%s", dirpath, path);
}

// Modifikasi Getattr
static int xmp_getattr(const char *path, struct stat *stbuf) {

    // Memalsukan keberadaan file virtual tujuan.txt
    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/tujuan.txt") == 0) {
        stbuf->st_mode = S_IFREG | 0444;
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

// Modifikasi Readdir
static int xmp_readdir(
    const char *path,
    void *buf,
    fuse_fill_dir_t filler,
    off_t offset,
    struct fuse_file_info *fi
) {

    // Membaca isi direktori asli
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

// Modifikasi Read
static int xmp_read(
    const char *path,
    char *buf,
    size_t size,
    off_t offset,
    struct fuse_file_info *fi
) {

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
                        while (*koord_ptr == ' ') {
                            koord_ptr++;
                        }
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
        if (offset + size > len) {
            size = len - offset;
        }
        memcpy(buf, hasil_akhir + offset, size);

        return size;
    }

    // Passthrough read untuk file biasa
    char fpath[1000];
    get_full_path(fpath, path);

    int fd = open(fpath, O_RDONLY);

    if (fd == -1) return -errno;

    int res = pread(fd, buf, size, offset);

    if (res == -1) {
        res = -errno;
    }

    close(fd);

    return res;
}

// Struct operasi FUSE
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

    return fuse_main(argc, argv, &xmp_oper, NULL);
}
