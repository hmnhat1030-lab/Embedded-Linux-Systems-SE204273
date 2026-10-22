#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "asgn1_ioctl.h"

int main(int argc, char *argv[]) {
    if (argc < 2) return 1;
    int fd = open("/dev/asgn1", O_RDWR);
    if (fd < 0) { perror("open"); return 1; }
    
    if (!strcmp(argv[1], "reset")) {
        ioctl(fd, ASGN1_RESET_BUFFER);
        printf("  -> [IOCTL] ASGN1_RESET_BUFFER: OK\n");
    } else if (!strcmp(argv[1], "stats")) {
        struct asgn1_stats st;
        ioctl(fd, ASGN1_GET_STATS, &st);
        printf("  -> [IOCTL] ASGN1_GET_STATS: open=%d, write=%d, read=%d, len=%d, last=%d\n",
               st.open_count, st.write_count, st.read_count, st.buffer_len, st.last_write_size);
    } else if (!strcmp(argv[1], "mode") && argc >= 3) {
        int m = atoi(argv[2]);
        ioctl(fd, ASGN1_SET_MODE, &m);
        printf("  -> [IOCTL] ASGN1_SET_MODE: %d OK\n", m);
    } else if (!strcmp(argv[1], "version")) {
        char ver[64] = {0};
        ioctl(fd, ASGN1_GET_VERSION, ver);
        printf("  -> [IOCTL] ASGN1_GET_VERSION: \"%s\"\n", ver);
    }
    close(fd);
    return 0;
}
