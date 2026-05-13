#define FUSE_USE_VERSION 31
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>

// Path folder asli sesuai soal
char *dirpath = "/home/chil/SISOP-4-2026-IT-003/soal_2/encrypted_storage";

// Fungsi XOR 0x76 untuk enkripsi/dekripsi isi file
void xor_cipher(char *data, size_t size) {
    for (size_t i = 0; i < size; i++) {
        data[i] ^= 0x76;
    }
}

// Fungsi pembantu untuk menambah .enc pada path
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

static int xmp_getattr(const char *path, struct stat *stbuf) {
    char fpath[1000];
    encode_path(fpath, path);
    int res = lstat(fpath, stbuf);
    if (res == -1) return -errno;
    return 0;
}

static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    (void) offset; (void) fi;
    char fpath[1000];
    encode_path(fpath, path);
    DIR *dp = opendir(fpath);
    if (dp == NULL) return -errno;

    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;

        char name[256];
        strcpy(name, de->d_name);
        char *ext = strstr(name, ".enc");
        if (ext) *ext = '\0';

        if (filler(buf, name, &st, 0)) break;
    }
    closedir(dp);
    return 0;
}

static int xmp_mkdir(const char *path, mode_t mode) {
    char fpath[1000];
    encode_path(fpath, path);
    int res = mkdir(fpath, mode);
    if (res == -1) return -errno;
    return 0;
}

static int xmp_rmdir(const char *path) {
    char fpath[1000];
    encode_path(fpath, path);
    int res = rmdir(fpath);
    if (res == -1) return -errno;
    return 0;
}

static int xmp_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    char fpath[1000];
    encode_path(fpath, path);
    int res = open(fpath, fi->flags, mode);
    if (res == -1) return -errno;
    fi->fh = res;
    return 0;
}

static int xmp_unlink(const char *path) {
    char fpath[1000];
    encode_path(fpath, path);
    int res = unlink(fpath);
    if (res == -1) return -errno;
    return 0;
}

static int xmp_truncate(const char *path, off_t size) {
    char fpath[1000];
    encode_path(fpath, path);
    int res = truncate(fpath, size);
    if (res == -1) return -errno;
    return 0;
}

static int xmp_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    (void) fi;
    char fpath[1000];
    encode_path(fpath, path);
    int fd = open(fpath, O_RDONLY);
    if (fd == -1) return -errno;

    int res = pread(fd, buf, size, offset);
    if (res == -1) res = -errno;
    else xor_cipher(buf, res);

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
    xor_cipher(temp, size);

    int res = pwrite(fd, temp, size, offset);
    free(temp);
    if (res == -1) res = -errno;

    close(fd);
    return res;
}

static int xmp_utimens(const char *path, const struct timespec tv[2]) {
    char fpath[1000];
    encode_path(fpath, path);
    int res = utimensat(0, fpath, tv, AT_SYMLINK_NOFOLLOW);
    if (res == -1) return -errno;
    return 0;
}

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
