#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <vector>

bool read_exact(int fd, void* buf, size_t len)
{
    auto* p = static_cast<uint8_t*>(buf);
    size_t total = 0;

    while (total < len) {
        ssize_t n = read(fd, p + total, len - total);

        if (n == 0) {
            return false; // EOF
        }

        if (n < 0) {
            perror("read");
            return false;
        }

        total += static_cast<size_t>(n);
    }

    return true;
}

void handle_itch(char msg_type, const uint8_t* buffer, uint16_t len)
{
    std::cout
        << "type=" << msg_type
        << " len=" << len
        << '\n';
}

int main()
{
    const char* filename =
        "/home/wqi/Desktop/01302019.NASDAQ_ITCH50";

    int fd = open(filename, O_RDONLY);

    if (fd < 0) {
        perror("open");
        return 1;
    }

    uint64_t msg_count = 0;

    while (true) {

        uint16_t len_be;

        ssize_t n = read(fd, &len_be, sizeof(len_be));

        if (n == 0) {
            break; // normal EOF
        }

        if (n < 0) {
            perror("read length");
            break;
        }

        if (n != sizeof(len_be)) {
            std::cerr << "partial length field\n";
            break;
        }

        uint16_t len = ntohs(len_be);

        if (len == 0 || len > 4096) {
            std::cerr
                << "invalid ITCH message length: "
                << len << '\n';
            break;
        }

        std::vector<uint8_t> buffer(len);

        if (!read_exact(fd, buffer.data(), len)) {
            std::cerr << "unexpected EOF inside message\n";
            break;
        }

        char msg_type = static_cast<char>(buffer[0]);

        handle_itch(msg_type, buffer.data(), len);

        ++msg_count;
    }

    std::cout
        << "total messages: "
        << msg_count
        << '\n';

    close(fd);

    return 0;
}