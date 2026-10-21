#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <unistd.h>
#include "/home/wqi/ultraHFT/common_tools/big_to_small_endian.h"
#include <optional>

constexpr const char* TARGET_SYMBOL = "AAPL";

static bool read_exact(int fd, void* dst, size_t len)
{
    auto* p = static_cast<uint8_t*>(dst);

    size_t done = 0;

    while (done < len) {
        size_t n = ::read(fd, p + done, len - done);

        if (n == 0)
            return false;

        if (n < 0) {
            perror("read");
            return false;
        }

        done += static_cast<size_t>(n);
    }

    return true;
}

static std::string read_stock(const uint8_t* p)
{
    std::string s(reinterpret_cast<const char*>(p), 8);

    while (!s.empty() && s.back() == ' ')
        s.pop_back();

    return s;
}

struct ItchHeader
{
    char type;
    uint16_t stock_locate;
    uint16_t tracking_number;
    uint64_t timestamp;
};

static ItchHeader parse_header(const uint8_t* p)
{
    ItchHeader h{};

    h.type            = static_cast<char>(p[0]);
    h.stock_locate    = read_be::read_be16(p + 1);
    h.tracking_number = read_be::read_be16(p + 3);
    h.timestamp       = read_be::read_be48(p + 5);

    return h;
}

static void parse_system_event(const uint8_t* p, uint16_t len)
{
    if (len != 12) {
        std::cerr << "Bad S length: " << len << '\n';
        return;
    }

    auto h = parse_header(p);

    char event_code = static_cast<char>(p[11]);

    std::cout
        << "S"
        << " locate=" << h.stock_locate
        << " tracking=" << h.tracking_number
        << " ts=" << h.timestamp
        << " event=" << event_code
        << '\n';
}

static void parse_stock_directory(
    const uint8_t* p,
    uint16_t len,
    std::optional<uint16_t>& target_locate)
{
    if (len != 39) {
        std::cerr << "Bad R length: " << len << '\n';
        return;
    }

    auto h = parse_header(p);

    std::string stock = read_stock(p + 11);

    if (stock == TARGET_SYMBOL) {
        target_locate = h.stock_locate;
    }

    char market_category = static_cast<char>(p[19]);
    char financial_status = static_cast<char>(p[20]);
    uint32_t round_lot_size = read_be::read_be32(p + 21);

    std::cout
        << "R"
        << " locate=" << h.stock_locate
        << " stock=" << stock
        << " market=" << market_category
        << " financial_status=" << financial_status
        << " round_lot=" << round_lot_size
        << '\n';
}

static void parse_add_order(const uint8_t* p, uint16_t len)
{
    if (len != 36) {
        std::cerr << "Bad A length: " << len << '\n';
        return;
    }

    auto h = parse_header(p);

    uint64_t order_ref = read_be::read_be64(p + 11);
    char side          = static_cast<char>(p[19]);
    uint32_t shares    = read_be::read_be32(p + 20);
    std::string stock  = read_stock(p + 24);
    uint32_t price     = read_be::read_be32(p + 32);

    std::cout
        << "A"
        << " locate=" << h.stock_locate
        << " order=" << order_ref
        << " side=" << side
        << " shares=" << shares
        << " stock=" << stock
        << " price=" << price / 10000.0
        << '\n';
}

static void parse_add_order_mpid(const uint8_t* p, uint16_t len)
{
    if (len != 40) {
        std::cerr << "Bad F length: " << len << '\n';
        return;
    }

    auto h = parse_header(p);

    uint64_t order_ref = read_be::read_be64(p + 11);
    char side          = static_cast<char>(p[19]);
    uint32_t shares    = read_be::read_be32(p + 20);
    std::string stock  = read_stock(p + 24);
    uint32_t price     = read_be::read_be32(p + 32);

    std::string mpid(
        reinterpret_cast<const char*>(p + 36),
        4
    );

    std::cout
        << "F"
        << " locate=" << h.stock_locate
        << " order=" << order_ref
        << " side=" << side
        << " shares=" << shares
        << " stock=" << stock
        << " price=" << price / 10000.0
        << " mpid=" << mpid
        << '\n';
}

static void parse_order_executed(const uint8_t* p, uint16_t len)
{
    if (len != 31) {
        std::cerr << "Bad E length: " << len << '\n';
        return;
    }

    auto h = parse_header(p);

    uint64_t order_ref = read_be::read_be64(p + 11);
    uint32_t executed  = read_be::read_be32(p + 19);
    uint64_t match_no  = read_be::read_be64(p + 23);

    std::cout
        << "E"
        << " locate=" << h.stock_locate
        << " order=" << order_ref
        << " executed=" << executed
        << " match=" << match_no
        << '\n';
}

static void parse_order_executed_price(
    const uint8_t* p,
    uint16_t len)
{
    if (len != 36) {
        std::cerr << "Bad C length: " << len << '\n';
        return;
    }

    auto h = parse_header(p);

    uint64_t order_ref = read_be::read_be64(p + 11);
    uint32_t executed  = read_be::read_be32(p + 19);
    uint64_t match_no  = read_be::read_be64(p + 23);
    char printable     = static_cast<char>(p[31]);
    uint32_t price     = read_be::read_be32(p + 32);

    std::cout
        << "C"
        << " locate=" << h.stock_locate
        << " order=" << order_ref
        << " executed=" << executed
        << " match=" << match_no
        << " printable=" << printable
        << " price=" << price / 10000.0
        << '\n';
}

static void parse_order_cancel(const uint8_t* p, uint16_t len)
{
    if (len != 23) {
        std::cerr << "Bad X length: " << len << '\n';
        return;
    }

    auto h = parse_header(p);

    uint64_t order_ref = read_be::read_be64(p + 11);
    uint32_t canceled  = read_be::read_be32(p + 19);

    std::cout
        << "X"
        << " locate=" << h.stock_locate
        << " order=" << order_ref
        << " canceled=" << canceled
        << '\n';
}

static void parse_order_delete(const uint8_t* p, uint16_t len)
{
    if (len != 19) {
        std::cerr << "Bad D length: " << len << '\n';
        return;
    }

    auto h = parse_header(p);

    uint64_t order_ref = read_be::read_be64(p + 11);

    std::cout
        << "D"
        << " locate=" << h.stock_locate
        << " order=" << order_ref
        << '\n';
}

static void parse_order_replace(const uint8_t* p, uint16_t len)
{
    if (len != 35) {
        std::cerr << "Bad U length: " << len << '\n';
        return;
    }

    auto h = parse_header(p);

    uint64_t old_order = read_be::read_be64(p + 11);
    uint64_t new_order = read_be::read_be64(p + 19);
    uint32_t shares    = read_be::read_be32(p + 27);
    uint32_t price     = read_be::read_be32(p + 31);

    std::cout
        << "U"
        << " locate=" << h.stock_locate
        << " old=" << old_order
        << " new=" << new_order
        << " shares=" << shares
        << " price=" << price / 10000.0
        << '\n';
}

static void parse_message(
    const uint8_t* p,
    uint16_t len,
    std::optional<uint16_t>& target_locate)
{
    if (len == 0)
        return;

    char type = static_cast<char>(p[0]);

    // R 消息必须先看，因为我们靠它找 AAPL locate
    if (type == 'R') {
        parse_stock_directory(
            p,
            len,
            target_locate
        );

        return;
    }

    // AAPL locate 还没找到
    if (!target_locate.has_value()) {
        return;
    }

    if (len < 11) {
        std::cerr << "ITCH message shorter than common header: " << len << '\n';
        return;
    }

    // ITCH common header 中：
    // p[1], p[2] = stock_locate
    uint16_t locate = read_be::read_be16(p + 1);

    // 不是 AAPL，直接丢掉
    if (locate != *target_locate) {
        return;
    }

    switch (type) {
        case 'A':
            parse_add_order(p, len);
            break;

        case 'F':
            parse_add_order_mpid(p, len);
            break;

        case 'E':
            parse_order_executed(p, len);
            break;

        case 'C':
            parse_order_executed_price(p, len);
            break;

        case 'X':
            parse_order_cancel(p, len);
            break;

        case 'D':
            parse_order_delete(p, len);
            break;

        case 'U':
            parse_order_replace(p, len);
            break;

        case 'H':
            // 后面可以处理 AAPL halt
            break;

        case 'Y':
            // 后面可以处理 AAPL Reg SHO
            break;

        default:
            break;
    }
}

int main()
{
    std::optional<uint16_t> target_locate;

    const char* filename = "/home/wqi/Desktop/01302019.NASDAQ_ITCH50";

    int fd = ::open(filename, O_RDONLY);

    if (fd < 0) {
        perror("open");
        return 1;
    }

    uint8_t buffer[4096];

    uint64_t total = 0;

    while (true) {

        uint8_t len_bytes[2];

        if (!read_exact(fd, len_bytes, 2)) {
            break;
        }

        uint16_t len = read_be::read_be16(len_bytes);

        if (len == 0 || len > sizeof(buffer)) {
            std::cerr
                << "Invalid record length: "
                << len
                << '\n';

            break;
        }

        if (!read_exact(fd, buffer, len)) {
            std::cerr
                << "Unexpected EOF inside ITCH message\n";
            break;
        }

        parse_message(buffer, len, target_locate);

        ++total;

        // 测试阶段先限制一下
        if (total >= 100000) {
            break;
        }
    }

    std::cout
        << "Parsed messages: "
        << total
        << '\n';

    ::close(fd);

    return 0;
}