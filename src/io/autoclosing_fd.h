#ifndef AUTOCLOSING_FD_H
#define AUTOCLOSING_FD_H

#include <fcntl.h>
#include <memory>

struct vanilla_fd {
    int fd;
    vanilla_fd (int fdesc) : fd(fdesc) { }
    virtual ~vanilla_fd ();
    const vanilla_fd& operator= (int fdesc) {
        fd = fdesc;
        return *this;
    }
    const vanilla_fd& operator= (vanilla_fd&& moved) {
        fd = moved.fd;
        moved.fd = 0;
        return *this;
    }
    const vanilla_fd& operator= (const vanilla_fd& illegal) = delete;
    const vanilla_fd& operator= (vanilla_fd& illegal) = delete;
    operator int() { return fd; }
};

struct autoclosing_fd : public vanilla_fd
{
    autoclosing_fd (int fdesc) : vanilla_fd(fdesc) { }
    virtual ~autoclosing_fd () override;
    const autoclosing_fd& operator= (int fdesc) {
        fd = fdesc;
        return *this;
    }
    const autoclosing_fd& operator= (autoclosing_fd&& moved) {
        fd = moved.fd;
        moved.fd = 0;
        return *this;
    }
};

typedef std::shared_ptr<vanilla_fd>    autoclosing_pfd;



#endif // AUTOCLOSING_FD_H
