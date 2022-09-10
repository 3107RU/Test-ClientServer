#include <iostream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <thread>
#include <random>

#include <netlib.h>

// Timeout between messages
const int PACKET_TIMEOUT = 10;
// Timeout between blocks
const int BLOCK_TIMEOUT_SEC = 10;
// Messages in block
const int BLOCK_LENGTH = 1000;
// Blocks to send
const int BLOCK_COUNT = 2;
// Message min, max length
static const int MSG_MIN_LENGTH = 600;
static const int MSG_MAX_LENGTH = 1600;

int idx = 0;

std::random_device rd;
std::mt19937 gen(rd());
std::uniform_int_distribution<> distr(MSG_MIN_LENGTH, MSG_MAX_LENGTH);

void send(netlib::Client &client)
{
  time_t last = 0;
  for (int i = 0; i < BLOCK_LENGTH; i++)
  {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::cout << "Sent: " << ++idx << " ";
    std::cout << std::put_time(std::localtime(&time), "%Y-%m-%d %X");
    std::cout << '.' << std::setfill('0') << std::setw(3) << ms.count() << std::endl;
    auto msg = std::make_shared<netlib::Msg>();
    msg->header.idx = idx;
    msg->header.time = time;
    msg->header.size = distr(gen);
    msg->data.resize(msg->header.size);
    std::iota(msg->data.begin(), msg->data.end(), 0);
    client.send(msg);
    auto delta = last ? last + PACKET_TIMEOUT - time : PACKET_TIMEOUT;
    if (delta > 0)
      std::this_thread::sleep_for(std::chrono::milliseconds(delta));
    last = time;
  };
}

int main(int argc, char **argv)
{
  if (argc < 2)
  {
    std::cerr << "Need server address in command line!" << std::endl;
    return 1;
  }

  netlib::Client client(argv[1]);

  while (!client.isConnected() && !client.isFinished())
    std::this_thread::sleep_for(std::chrono::seconds(1));

  if (client.isConnected())
  {
    for (int i = 0; i < BLOCK_COUNT; i++)
    {
      send(client);
      std::this_thread::sleep_for(std::chrono::seconds(BLOCK_TIMEOUT_SEC));
    }
    client.stop();
  }

  while (!client.isFinished())
    std::this_thread::sleep_for(std::chrono::seconds(1));

  return 0;
}
