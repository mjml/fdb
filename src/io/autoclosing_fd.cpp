#include <unistd.h>
#include "io/autoclosing_fd.h"

vanilla_fd::~vanilla_fd() {}

autoclosing_fd::~autoclosing_fd() {
    //Logger::debug2("autoclosing_fd destroyed with .fd=%d", fd);
    if (fd) {
        auto flags = fcntl(fd, F_GETFD);
        if (flags != -1 && errno != EBADF) {
            ::close(fd);
        }
    }
}
