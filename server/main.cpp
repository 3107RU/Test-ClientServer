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

std::mutex mtx;
using Lock = std::lock_guard<std::mutex>;
int received = 0, processed = 0;

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
    received++;
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
    processed++;  
    auto msg = buf.front();
    buf.pop_front();
    return msg;
  }
};

Buf buf;

//#define PRINT_BODY

void printStatus(const std::string &name, std::shared_ptr<netlib::Msg> msg)
{
  Lock l(mtx);
  std::cout << name << ": " << msg->header.idx << " ";
  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
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

  int noop = 0;
  int receivedReported = 0;
  netlib::Server server(onMsg);
  while (true)
  {
    auto msg = buf.pop();
    if (msg)
    {
      noop = 0;
      std::this_thread::sleep_for(std::chrono::milliseconds(PROCESS_TIMEOUT));
      printStatus("Processed", msg);
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      if (++noop > 1000) {
        noop = 0;
        Lock l(mtx);
        if (received > 0 && received != receivedReported) {
          receivedReported = received;
          std::cout << "Stat: total=" << received << ", lost=" << (received - processed) << " (" << ((received - processed) * 100 / received) << "%)" << std::endl;
        }
      }
    }
  };
  return 0;
}
