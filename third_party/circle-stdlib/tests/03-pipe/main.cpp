
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <poll.h>
#include <sys/time.h>
#include <sys/select.h>

#include "testkernel.h"

int main(void)
{
    return CTestKernel::RunTests("03-pipe");
}

TEST_CASE("Basic pipe() read/write")
{
    int fd[2];
    REQUIRE(pipe(fd) == 0);

    const char *msg = "Hello Pipe!\n";
    ssize_t written = write(fd[1], msg, strlen(msg));
    REQUIRE(written == (ssize_t)strlen(msg));

    char buf[64];
    memset(buf, 0, sizeof(buf));
    ssize_t bytes_read = read(fd[0], buf, sizeof(buf) - 1);
    REQUIRE(bytes_read == (ssize_t)strlen(msg));
    REQUIRE(strcmp(buf, msg) == 0);

    REQUIRE(close(fd[0]) == 0);
    REQUIRE(close(fd[1]) == 0);
}

TEST_CASE("pipe2() with O_NONBLOCK")
{
    int fd[2];
    REQUIRE(pipe2(fd, O_NONBLOCK) == 0);

    char buf[64];
    
    // Read from empty non-blocking pipe should return -1 with EAGAIN / EWOULDBLOCK
    ssize_t bytes_read = read(fd[0], buf, sizeof(buf));
    REQUIRE(bytes_read == -1);
    REQUIRE((errno == EWOULDBLOCK || errno == EAGAIN));

    const char *msg = "Non-blocking message";
    ssize_t written = write(fd[1], msg, strlen(msg));
    REQUIRE(written == (ssize_t)strlen(msg));

    memset(buf, 0, sizeof(buf));
    bytes_read = read(fd[0], buf, sizeof(buf) - 1);
    REQUIRE(bytes_read == (ssize_t)strlen(msg));
    REQUIRE(strcmp(buf, msg) == 0);

    REQUIRE(close(fd[0]) == 0);
    REQUIRE(close(fd[1]) == 0);
}

TEST_CASE("poll() with pipe")
{
    int fd[2];
    REQUIRE(pipe(fd) == 0);

    struct pollfd pfd;
    pfd.fd = fd[1];
    pfd.events = POLLOUT;
    pfd.revents = 0;

    int ready = poll(&pfd, 1, 0); // Immediate poll
    REQUIRE(ready == 1);
    REQUIRE((pfd.revents & POLLOUT) != 0);

    const char *msg = "Poll test message";
    ssize_t written = write(fd[1], msg, strlen(msg));
    REQUIRE(written == (ssize_t)strlen(msg));

    pfd.fd = fd[0];
    pfd.events = POLLIN;
    pfd.revents = 0;

    ready = poll(&pfd, 1, 0);
    REQUIRE(ready == 1);
    REQUIRE((pfd.revents & POLLIN) != 0);

    char buf[64];
    memset(buf, 0, sizeof(buf));
    ssize_t bytes_read = read(fd[0], buf, sizeof(buf) - 1);
    REQUIRE(bytes_read == (ssize_t)strlen(msg));
    REQUIRE(strcmp(buf, msg) == 0);

    // Test EOF when writer closes
    REQUIRE(close(fd[1]) == 0);

    pfd.fd = fd[0];
    pfd.events = POLLIN;
    pfd.revents = 0;

    // POLLIN should be set because EOF is flagged as readable
    // or POLLHUP might be set
    ready = poll(&pfd, 1, 0);
    REQUIRE(ready == 1);
    
    bytes_read = read(fd[0], buf, sizeof(buf));
    REQUIRE(bytes_read == 0); // EOF

    REQUIRE(close(fd[0]) == 0);
}

TEST_CASE("select() with pipe")
{
    int fd[2];
    REQUIRE(pipe(fd) == 0);

    fd_set readfds, writefds;
    struct timeval tv;

    // Test writability
    FD_ZERO(&writefds);
    FD_SET(fd[1], &writefds);
    tv.tv_sec = 0;
    tv.tv_usec = 0;

    int ready = select(fd[1] + 1, NULL, &writefds, NULL, &tv);
    REQUIRE(ready == 1);
    REQUIRE(FD_ISSET(fd[1], &writefds));

    // Write some data
    const char *msg = "Select test message";
    ssize_t written = write(fd[1], msg, strlen(msg));
    REQUIRE(written == (ssize_t)strlen(msg));

    // Test readability
    FD_ZERO(&readfds);
    FD_SET(fd[0], &readfds);
    tv.tv_sec = 0;
    tv.tv_usec = 0;

    ready = select(fd[0] + 1, &readfds, NULL, NULL, &tv);
    REQUIRE(ready == 1);
    REQUIRE(FD_ISSET(fd[0], &readfds));

    char buf[64];
    memset(buf, 0, sizeof(buf));
    ssize_t bytes_read = read(fd[0], buf, sizeof(buf) - 1);
    REQUIRE(bytes_read == (ssize_t)strlen(msg));
    REQUIRE(strcmp(buf, msg) == 0);

    // Test readability after writer closes (EOF)
    REQUIRE(close(fd[1]) == 0);

    FD_ZERO(&readfds);
    FD_SET(fd[0], &readfds);
    tv.tv_sec = 0;
    tv.tv_usec = 0;

    ready = select(fd[0] + 1, &readfds, NULL, NULL, &tv);
    REQUIRE(ready == 1);
    REQUIRE(FD_ISSET(fd[0], &readfds));

    bytes_read = read(fd[0], buf, sizeof(buf));
    REQUIRE(bytes_read == 0); // EOF

    REQUIRE(close(fd[0]) == 0);
}
