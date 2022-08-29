#include <iostream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <thread>
#include <mutex>

#include <boost/circular_buffer.hpp>

#include <netlib.h>

const int BUFFER_LENGTH = 16;
const int PROCESS_TIMEOUT = 15;

class Buf
{
  std::mutex mtx;
  using Lock = std::lock_guard<std::mutex>;
  boost::circular_buffer<std::shared_ptr<netlib::Msg>> buf;

public:
  Buf() : buf(BUFFER_LENGTH) {}
  void push(std::shared_ptr<netlib::Msg> msg)
  {
    Lock l(mtx);
    if (buf.size() < BUFFER_LENGTH) {
      buf.push_back(msg);
      return;
    }
    std::cout << "Server buffer overflow, skip msg: " << msg->header.idx << std::endl;
  }
  std::shared_ptr<netlib::Msg> pop()
  {
    Lock l(mtx);
    if (buf.empty())
      return nullptr;
    auto msg = buf.front();
    buf.pop_front();
    return msg;
  }
};

Buf buf;

//#define PRINT_BODY

void printStatus(std::string_view name, std::shared_ptr<netlib::Msg> msg)
{
  static std::mutex mtx;
  using Lock = std::lock_guard<std::mutex>;

  Lock l(mtx);

  std::cout << name << ": " << msg->header.idx << " ";
  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);
  auto ms = duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
  std::cout << std::put_time(std::localtime(&time), "%Y-%m-%d %X");
  std::cout << '.' << std::setfill('0') << std::setw(3) << ms.count();
  std::cout << " " << (msg->valid ? "PASS" : "FAIL") << std::endl;
#ifdef PRINT_BODY
  std::cout << "Length=" << msg->data.size() << ": ";
  std::for_each(msg->data.begin(), msg->data.end(), [](auto value) {
      std::cout << value << ' ';
  });
  std::cout << std::endl;
#endif  
}

int main(int argc, char **argv)
{
  auto onMsg = [](std::shared_ptr<netlib::Msg> msg)
  {
    printStatus("Received", msg);
    buf.push(msg);
  };

  netlib::Server server(onMsg);
  while (true)
  {
    auto msg = buf.pop();
    if (msg)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(PROCESS_TIMEOUT));
      printStatus("Processed", msg);
    } else
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
  };
  return 0;
}
