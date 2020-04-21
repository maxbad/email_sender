#pragma once
#include <asio.hpp>
#include <string>
#include <fstream>
struct email_msg {
	std::string host_;
	std::string port_;
	std::string user_name_;
	std::string user_pass_;
	std::string reciper_;
	std::string subject_;
	std::string content_;
	std::string content_type_;
	std::string attachment_;
};
class email_sender {
public:
	email_sender() :socket_(std::make_shared<asio::ip::tcp::socket>(io_context_)) {

	}
public:
	void send(email_msg const& msg) {
		asio::ip::tcp::resolver::query query(msg.host_, msg.port_);
		asio::ip::tcp::resolver resolver(io_context_);
		auto endpointIter = resolver.resolve(query);
		for (; endpointIter != asio::ip::tcp::resolver::iterator(); ++endpointIter) {
			asio::ip::tcp::endpoint endpoint = *endpointIter;
			try {
				std::error_code ec;
				socket_->connect(endpoint, ec);
				if (ec) {
					socket_->close();
					continue;
				}
				msg_ = msg;
				communicate();
			}
			catch (std::runtime_error const& err) {
				std::cout << err.what() << std::endl;
			}
		}
	}
private:
	static size_t base64_encode(char* _dst, const void* _src, size_t len, int url_encoded) {
		char* dst = _dst;
		const uint8_t* src = reinterpret_cast<const uint8_t*>(_src);
		static const char* MAP_URL_ENCODED = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			"abcdefghijklmnopqrstuvwxyz"
			"0123456789-_";
		static const char* MAP = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			"abcdefghijklmnopqrstuvwxyz"
			"0123456789+/";
		const char* map = url_encoded ? MAP_URL_ENCODED : MAP;
		uint32_t quad;

		for (; len >= 3; src += 3, len -= 3) {
			quad = ((uint32_t)src[0] << 16) | ((uint32_t)src[1] << 8) | src[2];
			*dst++ = map[quad >> 18];
			*dst++ = map[(quad >> 12) & 63];
			*dst++ = map[(quad >> 6) & 63];
			*dst++ = map[quad & 63];
		}
		if (len != 0) {
			quad = (uint32_t)src[0] << 16;
			*dst++ = map[quad >> 18];
			if (len == 2) {
				quad |= (uint32_t)src[1] << 8;
				*dst++ = map[(quad >> 12) & 63];
				*dst++ = map[(quad >> 6) & 63];
				if (!url_encoded)
					* dst++ = '=';
			}
			else {
				*dst++ = map[(quad >> 12) & 63];
				if (!url_encoded) {
					*dst++ = '=';
					*dst++ = '=';
				}
			}
		}

		*dst = '\0';
		return dst - _dst;
	}

	static std::string to_base64(std::string const& src) {
		char buff[1024];
		auto size = base64_encode(buff, src.data(), src.size(), 0);
		return std::string(buff, size);
	}
private:
	std::string reader() {
		asio::streambuf buff;
		std::error_code ec;
		asio::read_until(*socket_, buff, "\r\n", ec);
		if (ec) {
			return "";
		}
		std::istream in(&buff);
		std::string msg;
		std::getline(in, msg);
		return msg;
	}
	void write(std::string const& str) {
		asio::write(*socket_, asio::buffer(str.c_str(), str.size()));
	}
	void communicate() {
		std::cout << reader() << std::endl;
		std::stringstream ss;
		ss << "HELO " << msg_.user_name_ << CRLF;
		write(ss.str());
		std::cout << reader() << std::endl;
		login();
	}
	void login() {
		std::stringstream ss;
		ss << "AUTH LOGIN" << CRLF;
		write(ss.str());
		std::cout << reader() << std::endl;
		std::stringstream userName;
		userName << to_base64(msg_.user_name_) << CRLF;
		write(userName.str());
		std::cout << reader() << std::endl;
		std::stringstream passWord;
		passWord << to_base64(msg_.user_pass_) << CRLF;
		write(passWord.str());
		std::cout << reader() << std::endl;
		constructor_mail();
	}
	void constructor_mail() {
		std::stringstream Mailfrom;
		Mailfrom << "MAIL FROM: " << "<" << msg_.user_name_ << ">" << CRLF;
		write(Mailfrom.str());
		std::cout << reader() << std::endl;
		std::stringstream rcptTo;
		rcptTo << "RCPT TO: " << "<" << msg_.reciper_ << ">" << CRLF;
		write(rcptTo.str());
		std::cout << reader() << std::endl;
		//开始交换数据
		std::stringstream DATA;
		DATA << "DATA" << CRLF;
		write(DATA.str());
		std::cout << reader() << std::endl;
		std::stringstream Header;
		Header << "Content-Type:multipart/mixed;\n boundary=\"" << boundary_ << "\"" << CRLF;
		write(Header.str());
		std::stringstream Version;
		Version << "MIME-Version: " << mimeVersion << CRLF;
		write(Version.str());
		// email 正文
		std::stringstream from;
		from << "From:" << msg_.user_name_ << CRLF;
		write(from.str());
		std::stringstream to;
		to << "To:" << msg_.reciper_ << CRLF;
		write(to.str());
		std::stringstream subject;  //标题
		subject << "Subject:" << msg_.subject_ << CRLF;
		write(subject.str());
		process_content();
	}
	void  process_content() {
		std::stringstream boundary;
		boundary << "\n--" << boundary_ << "\n";
		write(boundary.str());
		std::stringstream content_type;
		content_type << "Content-Type:" << msg_.content_type_ << "\n";
		write(content_type.str());
		std::stringstream Version;
		Version << "MIME-Version:" << mimeVersion << "\n";
		write(Version.str());
		std::stringstream content;
		content << "\n" << msg_.content_ << CRLF;
		write(content.str());
		attachment();
	}
	void attachment() {
		if (!msg_.attachment_.empty()) {
			auto filepath = msg_.attachment_;
			auto path_pos = filepath.rfind("/") + 1;
			auto filename = filepath.substr(path_pos, filepath.size() - path_pos);
			std::ifstream fin(filepath.c_str(), std::ios::binary);
			if (fin.is_open()) {
				std::stringstream boundary;
				boundary << "\n--" << boundary_ << "\n";
				write(boundary.str());
				std::stringstream content_type;
				content_type << "Content-Type: "<<"text/plain" << "\n";
				write(content_type.str());
				std::stringstream Version;
				Version << "MIME-Version:" << mimeVersion << "\n";
				write(Version.str());
				write("Content-Transfer-Encoding: base64\n");
				std::stringstream Disposition;
				Disposition << "Content-Disposition: attachment; filename=\"" << filename << "\"\n\n";
				write(Disposition.str());
				char buff[128] = { 0 };
				for (;;) {
					fin.read(buff, sizeof(buff));
					auto size = fin.gcount();
					if (size != 0) {
						std::string encode_str = to_base64(std::string(buff, size));
						write(encode_str);
					}
					else {
						break;
					}
				}
				write(CRLF);
			}
		}
		end();
	}
	void end() {
		std::stringstream boundary_end;
		boundary_end << "\n--" << boundary_ << "--\n" << CRLF;
		write(boundary_end.str());
		std::stringstream rowEnd;
		rowEnd << CRLF << "." << CRLF;
		write(rowEnd.str());
		std::cout << reader() << std::endl;
		std::stringstream quite;
		quite << "QUIT" << CRLF;
		write(quite.str());
		std::cout << reader() << std::endl;
	}
private:
	asio::io_service io_context_;
	std::shared_ptr<asio::ip::tcp::socket> socket_;
	email_msg msg_;
	const std::string CRLF = "\r\n";
	const std::string mimeVersion = "1.0";
	std::string const boundary_ = "75a1bc25812d4de38333a71adff90faf";
};
