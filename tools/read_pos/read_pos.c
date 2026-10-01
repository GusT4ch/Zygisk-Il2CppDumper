// read_pos.c — Phantom Position validator
// Reads 12 bytes at Player+0x320 and Player+0x7E8 every 500ms for 30s.
// Logs to /data/local/tmp/position_log.txt
//
// Usage: ./read_pos <pid> <player_hex_addr>
// Example: ./read_pos 5557 0x7637c7bfc000

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <errno.h>
#include <inttypes.h>

#define OFF_320 0x320
#define OFF_7E8 0x7E8
#define SAMPLES 30
#define INTERVAL_US 500000  // 500 ms

static int read_at(int fd, uint64_t addr, void *buf, size_t len) {
    off_t r = lseek(fd, (off_t)addr, SEEK_SET);
    if (r == (off_t)-1) return -1;
    ssize_t n = read(fd, buf, len);
    return (n == (ssize_t)len) ? 0 : -1;
}

static uint64_t now_ms(uint64_t start_ms) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t ms = (uint64_t)tv.tv_sec * 1000ULL + (uint64_t)tv.tv_usec / 1000ULL;
    return ms - start_ms;
}

static uint64_t now_abs_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000ULL + (uint64_t)tv.tv_usec / 1000ULL;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <pid> <player_hex_addr>\n", argv[0]);
        return 1;
    }

    pid_t pid = (pid_t)atoi(argv[1]);
    uint64_t playerAddr = strtoull(argv[2], NULL, 0);

    if (!pid || !playerAddr) {
        fprintf(stderr, "Invalid pid or addr\n");
        return 1;
    }

    char memPath[64];
    snprintf(memPath, sizeof(memPath), "/proc/%d/mem", pid);
    int fd = open(memPath, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "Cannot open %s: %s\n", memPath, strerror(errno));
        return 2;
    }

    FILE *log = fopen("/data/local/tmp/position_log.txt", "w");
    if (!log) {
        fprintf(stderr, "Cannot open log file: %s\n", strerror(errno));
        close(fd);
        return 3;
    }

    fprintf(log, "# Phantom position validation log\n");
    fprintf(log, "# PID=%d  Player*=0x%" PRIx64 "\n", pid, playerAddr);
    fprintf(log, "# Reading Player+0x320 and Player+0x7E8 as Vector3 (3 floats)\n");
    fprintf(log, "# Protocol: 0-5s parado | 5-10s andar | 10-15s pular\n\n");
    fflush(log);

    float v320[3], v7e8[3];
    uint64_t start = now_abs_ms();
    int ok320 = 0, ok7e8 = 0;

    for (int i = 0; i < SAMPLES; ++i) {
        uint64_t t = now_ms(start);

        int r320 = read_at(fd, playerAddr + OFF_320, v320, sizeof(v320));
        int r7e8 = read_at(fd, playerAddr + OFF_7E8, v7e8, sizeof(v7e8));

        if (r320 == 0) ok320++;
        if (r7e8 == 0) ok7e8++;

        fprintf(log, "[%2d] %6" PRIu64 "ms | 0x320=", i, t);
        if (r320 == 0)
            fprintf(log, "(%10.3f,%10.3f,%10.3f)", v320[0], v320[1], v320[2]);
        else
            fprintf(log, "(READ_ERR errno=%d)", errno);
        fprintf(log, " | 0x7E8=");
        if (r7e8 == 0)
            fprintf(log, "(%10.3f,%10.3f,%10.3f)", v7e8[0], v7e8[1], v7e8[2]);
        else
            fprintf(log, "(READ_ERR errno=%d)", errno);
        fprintf(log, "\n");
        fflush(log);

        usleep(INTERVAL_US);
    }

    fprintf(log, "\n# Done. Successful reads: 0x320=%d/30  0x7E8=%d/30\n",
            ok320, ok7e8);
    fclose(log);
    close(fd);

    printf("Done. Log: /data/local/tmp/position_log.txt\n");
    printf("Successful: 0x320=%d/30  0x7E8=%d/30\n", ok320, ok7e8);
    return 0;
}
