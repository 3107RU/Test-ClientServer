#include <iostream>
#include <thread>
#include <list>
#include <mutex>

#include <sys/signal.h>

#include <boost/asio.hpp>
#include <openssl/evp.h>

#include "netlib.h"
#include "netorder.h"

using tcp = boost::asio::ip::tcp;

namespace netlib
{
	static void calcMD5(std::shared_ptr<Msg> msg)
	{
		auto mdctx = EVP_MD_CTX_new();
		EVP_DigestInit_ex(mdctx, EVP_md5(), NULL);
		EVP_DigestUpdate(mdctx, msg->data.data(), msg->data.size() * sizeof(decltype(msg->data)::value_type));
		unsigned int md5_digest_len = sizeof(Msg::Header::md5); // EVP_MD_size(EVP_md5());
		EVP_DigestFinal_ex(mdctx, msg->header.md5, &md5_digest_len);
		EVP_MD_CTX_free(mdctx);
	}

	static void convertHeader(std::shared_ptr<Msg> msg)
	{
		msg->header.idx = netorder::convert(msg->header.idx);
		msg->header.size = netorder::convert(msg->header.size);
		msg->header.time = netorder::convert(msg->header.time);
	}

	class Server::Impl
	{
		OnMsgFn onMsg;

		std::thread worker;

		boost::asio::io_service ios;
		tcp::acceptor acceptor;

		struct Session
		{
			OnMsgFn onMsg;
			std::shared_ptr<tcp::socket> socket;

			void decodeBody(std::shared_ptr<Msg> msg)
			{
				netorder::convert(msg->data.begin(), msg->data.end());
				uint8_t md5[sizeof(Msg::Header::md5)];
				memcpy(md5, msg->header.md5, sizeof(Msg::Header::md5));
				calcMD5(msg);
				msg->valid = memcmp(md5, msg->header.md5, sizeof(Msg::Header::md5)) == 0;
			}

			void readBody(std::shared_ptr<Msg> msg)
			{
				auto cb = [this, msg](boost::system::error_code ec, std::size_t length)
				{
					if (!ec)
					{
						decodeBody(msg);
						onMsg(msg);
						readHeader();
					}
					else
						std::cerr << "Server got error while reading message: " << ec.message() << std::endl;
				};
				msg->data.resize(msg->header.size);
				boost::asio::async_read(
					*socket,
					boost::asio::buffer(msg->data),
					cb);
			}

			void readHeader()
			{
				auto msg = std::make_shared<Msg>();
				auto cb = [this, msg](boost::system::error_code ec, std::size_t length)
				{
					if (!ec)
					{
						convertHeader(msg);
						readBody(msg);
					}
					else
						std::cerr << "Server got error while reading message: " << ec.message() << std::endl;
				};
				boost::asio::async_read(
					*socket,
					boost::asio::buffer((char *)&msg->header, sizeof(Msg::Header)),
					cb);
			}

			Session(std::shared_ptr<tcp::socket> socket, OnMsgFn onMsg) : socket(socket), onMsg(onMsg)
			{
				readHeader();
			}
		};

		std::shared_ptr<Session> session;

		void accept()
		{
			auto socket = std::make_shared<tcp::socket>(ios);
			acceptor.async_accept(
				*socket,
				[this, socket](boost::system::error_code ec)
				{
					if (!ec)
						session = std::make_shared<Session>(socket, onMsg);
					accept();
				});
		}

		void work()
		{
			std::cerr << "Server started." << std::endl;

			try
			{
				accept();
				ios.run();
			}
			catch (std::exception &e)
			{
				std::cerr << "Server exception: " << e.what() << std::endl;
			}

			std::cerr << "Server finished." << std::endl;
		}

	public:
		Impl(OnMsgFn onMsg)
			: onMsg(onMsg),
			  acceptor(ios, tcp::endpoint(tcp::v4(), SERVER_PORT)),
			  worker(&Impl::work, this)
		{
		}

		~Impl()
		{
			acceptor.close();
			session.reset();
			ios.stop();
			worker.join();
		}
	};

	Server::Server(OnMsgFn onMsg)
	{
		impl = std::make_shared<Impl>(onMsg);
	}

	class Client::Impl
	{
		std::string address;
		boost::asio::io_service ios;
		std::shared_ptr<boost::asio::io_service::work> wg;
		tcp::resolver resolver;
		tcp::socket socket;
		volatile bool connected = false;
		volatile bool finished = false;
		bool writing = false;

		std::list<std::shared_ptr<Msg>> list;

		std::thread worker;

		void convertMsg(std::shared_ptr<Msg> msg)
		{
			convertHeader(msg);
			netorder::convert(msg->data.begin(), msg->data.end());
		}

		void connect()
		{
			auto cb = [this](const boost::system::error_code &ec, tcp::resolver::results_type results)
			{
				if (!ec)
				{
					auto cb = [this](boost::system::error_code ec, const tcp::endpoint &endpoint)
					{
						if (!ec)
						{
							std::cout << "Connected to: " << endpoint.address().to_string() << std::endl;
							connected = true;
						}
						else
						{
							std::cerr << "Error while connecting: " << ec.message() << std::endl;
							ios.stop();
						}
					};
					boost::asio::async_connect(socket, results, cb);
				}
				else
				{
					std::cerr << "Error while resolving network address: " << ec.message() << std::endl;
					ios.stop();
				}
			};
			std::cout << "Connecting to: " << address << std::endl;
			resolver.async_resolve(address, std::to_string(SERVER_PORT), cb);
		}

		void writeBody(std::shared_ptr<Msg> msg)
		{
			auto cb = [this](boost::system::error_code ec, std::size_t length)
			{
				writing = false;
				if (!ec)
				{
					write();
				}
				else
				{
					std::cerr << "Error sending message: " << ec.message() << std::endl;
					ios.stop();
				}
			};
			boost::asio::async_write(socket,
									 boost::asio::buffer(msg->data),
									 cb);
		}

		void write()
		{
			if (writing || list.empty())
				return;
			
			writing = true;

			auto msg = list.front();
			list.pop_front();

			auto cb = [this, msg](boost::system::error_code ec, std::size_t length)
			{
				if (!ec)
				{
					writeBody(msg);
				}
				else
				{
					std::cerr << "Error sending message: " << ec.message() << std::endl;
					writing = false;
					ios.stop();
				}
			};
			boost::asio::async_write(socket,
									 boost::asio::buffer(&msg->header, sizeof(Msg::Header)),
									 cb);
		}

		void work()
		{
			try
			{
				connect();
				ios.run();
			}
			catch (std::exception &e)
			{
				std::cerr << "Client exception: " << e.what() << std::endl;
			}
			finished = true;
		}

	public:
		Impl(const std::string &address)
			: address(address),
			  resolver(ios),
			  socket(ios)
		{
			wg = std::make_shared<boost::asio::io_service::work>(ios);
			worker = std::thread(&Impl::work, this);
		}
		~Impl()
		{
			resolver.cancel();
			socket.close();
			ios.stop();
			worker.join();
		};

		bool isConnected()
		{
			return connected;
		}

		bool send(std::shared_ptr<Msg> msg)
		{
			if (!connected)
				return false;
			calcMD5(msg);
			ios.post(
				[this, msg]()
				{
					convertMsg(msg);
					list.push_back(msg);
					write();
				});
			return true;
		}

		void stop()
		{
			wg.reset();
		}

		bool isFinished()
		{
			return finished;
		}
	};

	Client::Client(const std::string &address)
	{
		impl = std::make_shared<Impl>(address);
	}

	bool Client::isConnected()
	{
		return impl->isConnected();
	}

	bool Client::send(std::shared_ptr<Msg> msg)
	{
		return impl->send(msg);
	}

	void Client::stop()
	{
		impl->stop();
	}

	bool Client::isFinished()
	{
		return impl->isFinished();
	}
}