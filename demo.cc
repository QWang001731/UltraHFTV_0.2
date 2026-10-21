#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>

#include <cstdint>
#include <iostream>

bool read_exact(int fd, void* buf, size_t len)
{
    auto* p = static_cast<uint8_t*>(buf);
    size_t done = 0;

    while (done < len) {
        ssize_t n = read(fd, p + done, len - done);

        if (n <= 0) {
            return false;
        }

        done += static_cast<size_t>(n);
    }

    return true;
}

int main()
{
    const char* path =
        "/home/wqi/Desktop/01302019.NASDAQ_ITCH50";

    int fd = open(path, O_RDONLY);

    if (fd < 0) {
        perror("open");
        return 1;
    }

    uint8_t buffer[4096];

    for (int i = 0; i < 50; ++i) {

        uint16_t len_be;

        if (!read_exact(fd, &len_be, 2)) {
            break;
        }

        uint16_t len = ntohs(len_be);

        if (len == 0 || len > sizeof(buffer)) {
            std::cerr << "bad len: " << len << '\n';
            break;
        }

        if (!read_exact(fd, buffer, len)) {
            break;
        }

        char type = static_cast<char>(buffer[0]);

        std::cout
            << "msg=" << i
            << " type=" << type
            << " len=" << len
            << '\n';
    }

    close(fd);
}