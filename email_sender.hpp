#pragma once
#include <asio.hpp>
#include <string>
#include <fstream>
#include <ctime>
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
		char buff[1024] = {0};
		auto size = base64_encode(buff, src.data(), src.size(), 0);
		return std::string(buff, size);
	}
public:
	static std::string get_gmt_time_str(std::time_t t)
	{
		struct tm* GMTime = gmtime(&t);
		char buff[512] = { 0 };
		strftime(buff, sizeof(buff), "%a, %d %b %Y %H:%M:%S +0000", GMTime);
		return buff;
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
		ss << "EHLO " << msg_.user_name_ << CRLF;
		write(ss.str());
		std::cout << reader() << std::endl;
		login();
	}
	void login() {
		std::stringstream ss;
		std::string token;
		token.push_back('\0');
		token.append(msg_.user_name_);
		token.push_back('\0');
		token.append(msg_.user_pass_);
		ss << "AUTH PLAIN "<<to_base64(token) << CRLF;
		write(ss.str());
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
		Header << "Content-Type: multipart/mixed; boundary=\"" << boundary_ << "\"" << CRLF;
		write(Header.str());

		std::stringstream mainContent;
		// email 正文
		//std::stringstream from;
		mainContent << "From: " << msg_.user_name_ << CRLF;
		//write(from.str()); 
		//std::stringstream to;
		mainContent << "To: " << msg_.reciper_ << CRLF;
		//write(to.str());
		//std::stringstream subject;  //标题
		mainContent << "Subject: " << msg_.subject_ << CRLF;
		//write(subject.str());
		//std::stringstream messageId;
		auto pos = msg_.user_name_.find("@");
		auto postFix = msg_.user_name_.substr(pos, msg_.user_name_.size() - pos);
		mainContent << "Message-ID: <" << to_base64(std::to_string(std::time(nullptr))) << "-" << "abcde" << postFix << ">" << CRLF;
		//write(messageId.str());
		mainContent << "Date: " << get_gmt_time_str(std::time(nullptr)) << CRLF;
		//write("Date: " + get_gmt_time_str(std::time(nullptr)) + CRLF);
		//std::stringstream Version;
		mainContent << "MIME-Version: " << mimeVersion << CRLF;
		//write(Version.str());
		mainContent << CRLF;
		write(mainContent.str());
		process_content();
	}
	void  process_content() {
		if (!msg_.content_.empty()) {
			std::stringstream boundary;
			boundary << "--" << boundary_ << CRLF;
			write(boundary.str());
			std::stringstream content_property;
			//std::stringstream content_type;
			content_property << "Content-Type:" << msg_.content_type_ << CRLF;
			//write(content_type.str());
			//std::stringstream Encoding;
			content_property << "Content-Transfer-Encoding: 7bit"<< CRLF<< CRLF;
			//write(Encoding.str());
			write(content_property.str());
			std::stringstream content;
			content << msg_.content_;
			write(content.str());
		}
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
				boundary<< CRLF << "--" << boundary_ << CRLF;
				write(boundary.str());
				std::stringstream attachment_property;
				//std::stringstream content_type;
				attachment_property << "Content-Type: " << "text/plain"<<"; name="<< filename << CRLF;
				//write(content_type.str());
				//write("Content-Transfer-Encoding: base64\r\n");
				attachment_property << "Content-Transfer-Encoding: base64" << CRLF;
				//std::stringstream Disposition;
				attachment_property << "Content-Disposition: attachment; filename=\"" << filename<<"\"" << CRLF<< CRLF;
				//write(Disposition.str());
				write(attachment_property.str());
				char buff[64] = { 0 };
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
				//write(CRLF);
			}
		}
		end();
	}
	void end() {
		std::stringstream boundary_end;
		boundary_end << CRLF << "--" << boundary_ << "--" << CRLF;
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
