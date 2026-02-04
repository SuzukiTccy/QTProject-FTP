#pragma once
#include <event2/event.h>

#ifndef OPENSSL_NO_SSL_INCLUDES
#include <openssl/ssl.h>
#endif

#define DEBUG
class XTask
{
public:
	// libevent的事件循环基座
	struct event_base *base = 0;

	// socket
	int sock = 0;

	// 线程池中的线程ID
	int thread_id = -1;

	// 强制子类实现初始化逻辑
	virtual bool Init() = 0;

	virtual ~XTask(){};

	#ifndef OPENSSL_NO_SSL_INCLUDES
	SSL *ssl = nullptr;            // SSL 连接对象
	bool use_ssl = false;          // 是否使用 SSL
	#endif
};

