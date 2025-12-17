#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/statvfs.h>
#include <errno.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <linux/falloc.h>

#define MAX_PATH 512

// Parse size string (e.g., "10G", "500M", "1024K")
long long parse_size(const char *size_str) {
    char *endptr;
    long long value = strtoll(size_str, &endptr, 10);
    if (value <= 0) {
        fprintf(stderr, "Invalid size value\n");
        return -1;
    }

    switch (*endptr) {
        case 'K': case 'k': return value * 1024LL;
        case 'M': case 'm': return value * 1024LL * 1024LL;
        case 'G': case 'g': return value * 1024LL * 1024LL * 1024LL;
        case 'T': case 't': return value * 1024LL * 1024LL * 1024LL * 1024LL;
        case '\0': return value; // bytes
        default:
            fprintf(stderr, "Invalid size suffix: %c (use K, M, G, or T)\n", *endptr);
            return -1;
    }
}

// Display filesystem usage
void display_usage(const char *mount_point) {
    struct statvfs stat;
    if (statvfs(mount_point, &stat) != 0) {
        fprintf(stderr, "Failed to get filesystem info: %s\n", strerror(errno));
        return;
    }

    unsigned long long total_bytes = stat.f_blocks * stat.f_frsize;
    unsigned long long avail_bytes = stat.f_bavail * stat.f_frsize;
    unsigned long long used_bytes = total_bytes - avail_bytes;
    int use_percent = (int)((used_bytes * 100) / total_bytes);

    printf("\nFilesystem usage for %s:\n", mount_point);
    printf("Size: %.2f GB\n", total_bytes / (1024.0*1024.0*1024.0));
    printf("Used: %.2f GB (%d%%)\n", used_bytes / (1024.0*1024.0*1024.0), use_percent);
    printf("Available: %.2f GB\n", avail_bytes / (1024.0*1024.0*1024.0));
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <mount_point> <size>\n", argv[0]);
        return 1;
    }

    const char *mount_point = argv[1];
    const char *size_str = argv[2];
    long long size_bytes = parse_size(size_str);
    if (size_bytes < 0) return 1;

    char filename[MAX_PATH];
    time_t now = time(NULL);
    snprintf(filename, sizeof(filename), "%s/fillfile_%ld", mount_point, now);

    int fd = open(filename, O_CREAT | O_WRONLY, 0644);
    if (fd < 0) {
        fprintf(stderr, "Failed to create file: %s\n", strerror(errno));
        return 1;
    }
    // Try fallocate
    if (fallocate(fd, 0, 0, size_bytes) != 0) {
        perror("fallocate failed, using ftruncate");
        if (ftruncate(fd, size_bytes) != 0) {
            fprintf(stderr, "Failed to allocate file with ftruncate: %s\n", strerror(errno));
            close(fd);
            unlink(filename);
            return 1;
        }
    }

    close(fd);

    double size_gb = size_bytes / (1024.0*1024.0*1024.0);
    printf("Filled %s with %.2f GB\n", filename, size_gb);

    display_usage(mount_point);
    return 0;
}
