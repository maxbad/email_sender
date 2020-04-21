#include <iostream>
#include "email_sender.hpp"
int main() {
	email_msg msg = { "smtp.qq.com","25","970252187@qq.com","password","xmh970252187@163.com","fjfjfj","hello,world","text/plain; charset=\"utf - 8\"","./test.txt" };
	email_sender sender;
	sender.send(msg);
	std::getchar();
}
