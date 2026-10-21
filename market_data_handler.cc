#include "Level2_order_book.h"
#include "read_big_endian_number.h"
#include <unistd.h>
#include <optional>
#include <fcntl.h>

static Level2_Order_Book L2_BOOK_AAPL("AAPL",1000);
constexpr const char* TARGET_SYMBOL = "AAPL";


uint64_t count_A = 0;
uint64_t count_F = 0;
uint64_t count_E = 0;
uint64_t count_C = 0;
uint64_t count_X = 0;
uint64_t count_D = 0;
uint64_t count_U = 0;


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


static void parse_stock_directory(
    const uint8_t* p,
    uint16_t len,
    std::optional<uint16_t>& target_locate)
{
    if (len != 39) {
        std::cerr << "Bad R length: " << len << '\n';
        return;
    }

    std::string stock = read_stock(p + 11);

    if (stock == TARGET_SYMBOL) 
    {
        std::cout << "AAPL R message:\n";

        for (int i = 0; i < 20; ++i) {
            printf("%02X ", p[i]);
        }

        printf("\n");

        target_locate = read_be::read_be16(p + 1);

        std::cout
            << "Found AAPL, stock locate = "
            << *target_locate
            << '\n';
    }

    return ;
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
    if (type == 'R' && !target_locate.has_value()) {

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
    count_A++;
    L2_BOOK_AAPL.Add_order(p);
    break;

case 'F':
    count_F++;
    L2_BOOK_AAPL.Add_order(p);
    break;

case 'E':
    count_E++;
    L2_BOOK_AAPL.Execute_order(p);
    break;

case 'C':
    count_C++;
    L2_BOOK_AAPL.Execute_order(p);
    break;

case 'X':
    count_X++;
    L2_BOOK_AAPL.Cancel_order(p);
    break;

case 'D':
    count_D++;
    L2_BOOK_AAPL.Delete_order(p);
    break;

case 'U':
    count_U++;
    L2_BOOK_AAPL.Replace_order(p);
    break;
}
}

int main()
{

    std::optional<uint16_t> target_locate;

    constexpr size_t MAX_ITCH_MESSAGE_SIZE = 128;

    const char * file = "/home/wqi/Desktop/01302019.NASDAQ_ITCH50";
    int fd = ::open(file,O_RDONLY); 

    if(fd < 0)
    {
        perror("failed open file");
        return 1;
    }

    alignas(64) u_int8_t buffer[MAX_ITCH_MESSAGE_SIZE];
    uint64_t total = 0;

    while (true)
    {
        uint8_t len_bytes[2];
        if(!read_exact(fd,len_bytes,2))
        {
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

        if(!read_exact(fd,buffer,len))
        {
            std::cerr<<"unexpected EOF\n";
            break;

        }
        total++;
        parse_message(buffer,len,target_locate);

        if(total == 100000000)
            break;
    }

    std::string validate_result = L2_BOOK_AAPL.Validate()? "Successful" : "Fail" ;

    std::cout<<"parsed "<<total <<" messgaes .\n Validate result:" << validate_result;

    std::cout
    << "A: " << count_A << '\n'
    << "F: " << count_F << '\n'
    << "E: " << count_E << '\n'
    << "C: " << count_C << '\n'
    << "X: " << count_X << '\n'
    << "D: " << count_D << '\n'
    << "U: " << count_U << '\n';

}



