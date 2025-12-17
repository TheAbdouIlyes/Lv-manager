#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>

#define LOGFILE "/var/log/lvm_monitor.log"
#define BUF_SIZE 512
#define THRESHOLD_DEFAULT 80

void log_msg(const char *msg) {
    FILE *f = fopen(LOGFILE, "a");
    if (!f) return;

    time_t t = time(NULL);
    fprintf(f, "%s - %s\n", ctime(&t), msg);
    fclose(f);
}

int get_usage(const char *mount_point) {
    char cmd[BUF_SIZE], line[BUF_SIZE];
    snprintf(cmd, sizeof(cmd), "df %s | awk 'NR==2 {print $5}'", mount_point);

    FILE *fp = popen(cmd, "r");
    if (!fp) return -1;

    fgets(line, sizeof(line), fp);
    pclose(fp);
    return atoi(line);
}
/* STEP 1: Cleanup */
void cleanup_old_files(const char *mount_point) {
    char tmp_path[BUF_SIZE];
    snprintf(tmp_path, sizeof(tmp_path), "%s/tmp", mount_point);

    DIR *dir = opendir(tmp_path);
    if (!dir) {
        log_msg("No tmp directory, skipping cleanup");
        return;
    }

    struct dirent *entry;
    char filepath[BUF_SIZE];
    time_t now = time(NULL);

    while ((entry = readdir(dir))) {
        if (entry->d_type != DT_REG) continue;

        if (snprintf(filepath, sizeof(filepath), "%s/%s", tmp_path, entry->d_name) >= (int)sizeof(filepath))
            continue;

        struct stat st;
        if (stat(filepath, &st) == 0) {
            if (difftime(now, st.st_mtime) > 7 * 24 * 3600) {
                remove(filepath);
            }
        }
    }
    closedir(dir);
    log_msg("Cleanup completed");
}
/* STEP 3: Extend LV from VG */
int extend_lv_from_vg(const char *lv_path) {
    char cmd[BUF_SIZE];
    snprintf(cmd, sizeof(cmd),
             "lvextend -r -L +2G %s >> %s 2>&1",
             lv_path, LOGFILE);

    return system(cmd);
}
/* STEP 4: Move files */
int move_files(const char *src_mount, const char *dst_mount) {
    char src_dir[BUF_SIZE];
    snprintf(src_dir, sizeof(src_dir), "%s/moveable", src_mount);

    DIR *dir = opendir(src_dir);
    if (!dir) return 0;

    struct dirent *entry;
    char src[BUF_SIZE], dst[BUF_SIZE];
    int moved = 0;

    while ((entry = readdir(dir)) && moved < 5) {
        if (entry->d_type != DT_REG) continue;

        if (snprintf(src, sizeof(src), "%s/%s", src_dir, entry->d_name) >= (int)sizeof(src)) continue;
        if (snprintf(dst, sizeof(dst), "%s/%s", dst_mount, entry->d_name) >= (int)sizeof(dst)) continue;

        if (rename(src, dst) == 0) moved++;
    }
    closedir(dir);
    return moved;
}
int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Usage: %s <lv_path> <mount_point> <threshold>\n", argv[0]);
        return 1;
    }

    const char *lv_path = argv[1];
    const char *mount_point = argv[2];
    int threshold = atoi(argv[3]);

    char msg[BUF_SIZE];

    cleanup_old_files(mount_point);
    int used = get_usage(mount_point);

    if (used < threshold) {
        snprintf(msg, sizeof(msg), "SUCCESS: %s at %d%% after cleanup", mount_point, used);
        log_msg(msg);
        return 0;
    }

    if (extend_lv_from_vg(lv_path) == 0) {
        log_msg("SUCCESS: LV extended from VG");
        return 0;
    }

    const char *others[] = {"/mnt/data1", "/mnt/data2", "/mnt/data3"};
    for (int i = 0; i < 3; i++) {
        if (strcmp(others[i], mount_point) != 0) {
            if (move_files(mount_point, others[i]) > 0) {
                log_msg("SUCCESS: Files moved to another LV");
                return 0;
            }
        }
    }

    log_msg("ALERT: No solution found, admin intervention required");
    return 1;
}
