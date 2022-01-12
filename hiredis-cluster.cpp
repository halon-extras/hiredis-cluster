#include <hircluster.h>
#include <hiredis/alloc.h>
#include <HalonMTA.h>
#include <syslog.h>
#include <queue>
#include <mutex>
#include <condition_variable>

std::queue<redisClusterContext*> pool;
std::condition_variable cv;
std::mutex mutex;
hiredisAllocFuncs hiredisAllocFns;
std::string nodes;
std::string password;
std::string connect_timeout;
std::string timeout;
std::string max_retry;

HALON_EXPORT
int Halon_version()
{
	return HALONMTA_PLUGIN_VERSION;
}

redisClusterContext* redisClusterContextInit2()
{
	redisClusterContext *cc = redisClusterContextInit();

	if (!connect_timeout.empty())
	{
		struct timeval tv = { strtol(connect_timeout.c_str(), nullptr, 10) , 0 };
		redisClusterSetOptionConnectTimeout(cc, tv);
	}
	if (timeout.empty())
	{
		struct timeval tv = { strtol(timeout.c_str(), nullptr, 10) , 0 };
		redisClusterSetOptionTimeout(cc, tv);
	}
	if (!max_retry.empty())
	{
		redisClusterSetOptionMaxRetry(cc, strtol(max_retry.c_str(), nullptr, 10));
	}
	if (!password.empty())
	{
		redisClusterSetOptionPassword(cc, password.c_str());
	}

	int ret = redisClusterSetOptionAddNodes(cc, nodes.c_str());
	if (ret != REDIS_OK)
	{
		throw std::runtime_error("bad nodes");
	}

	return cc;
}

HALON_EXPORT
bool Halon_init(HalonInitContext* hic)
{
	hiredisResetAllocators();

	HalonConfig* cfg = nullptr;
	HalonMTA_init_getinfo(hic, HALONMTA_INIT_CONFIG, nullptr, 0, &cfg, nullptr);
	if (!cfg)
		return false;

	const char* pool_size_ = HalonMTA_config_string_get(HalonMTA_config_object_get(cfg, "pool_size"), nullptr);
	const char* nodes_ = HalonMTA_config_string_get(HalonMTA_config_object_get(cfg, "nodes"), nullptr);
	if (nodes_) nodes = nodes_;
	const char* password_ = HalonMTA_config_string_get(HalonMTA_config_object_get(cfg, "password"), nullptr);
	if (password_) password = password_;
	const char* connect_timeout_ = HalonMTA_config_string_get(HalonMTA_config_object_get(cfg, "connect_timeout"), nullptr);
	if (connect_timeout_) connect_timeout = connect_timeout_;
	const char* timeout_ = HalonMTA_config_string_get(HalonMTA_config_object_get(cfg, "timeout"), nullptr);
	if (timeout_) timeout = timeout_;
	const char* max_retry_ = HalonMTA_config_string_get(HalonMTA_config_object_get(cfg, "max_retry"), nullptr);
	if (max_retry_) max_retry = max_retry_;

	if (nodes.empty())
	{
		syslog(LOG_CRIT, "hiredis-cluster: no nodes");
		return false;
	}

	size_t pool_size = pool_size_ ? strtoul(pool_size_, nullptr, 10) : 1;
	for (size_t i = 0; i < pool_size; ++i)
	{
		redisClusterContext *cc;
		try {
			cc = redisClusterContextInit2();
		} catch (const std::runtime_error& e) {
			syslog(LOG_CRIT, "hiredis-cluster: %s", e.what());
			return false;
		}
		redisClusterConnect2(cc);
		if (cc->err)
		{
			syslog(LOG_WARNING, "hiredis-cluster: %s", cc->errstr);
		}
		pool.push(cc);
	}

	return true;
}

void set_ret_error(HalonHSLValue* ret, char const *value)
{
	HalonHSLValue *array_key, *array_value;
	HalonMTA_hsl_value_array_add(ret, &array_key, &array_value);
	HalonMTA_hsl_value_set(array_key, HALONMTA_HSL_TYPE_STRING, "error", 0);
	HalonMTA_hsl_value_set(array_value, HALONMTA_HSL_TYPE_STRING, value, 0);
}

void HSLRedisClusterCommand(HalonHSLContext* hhc, HalonHSLArguments* args, HalonHSLValue* ret)
{
	std::vector<const char*> argv;
	std::vector<size_t> lens;
	size_t a = 0;
	while (HalonHSLValue* a_ = HalonMTA_hsl_argument_get(args, a++))
	{
		if (HalonMTA_hsl_value_type(a_) != HALONMTA_HSL_TYPE_STRING)
		{
			set_ret_error(ret, "argument is not a string");
			return;
		}
		char* a = nullptr;
		size_t al;
		HalonMTA_hsl_value_get(a_, HALONMTA_HSL_TYPE_STRING, &a, &al);
		argv.push_back(a);
		lens.push_back(al);
	}

	redisClusterContext* cc = nullptr;

	std::unique_lock<std::mutex> ul(mutex);
	cv.wait(ul, [&]() { return !pool.empty(); });
	cc = pool.front();
	pool.pop();
	ul.unlock();

	struct type
	{
		HalonHSLValue* v;
		enum {
			PLAIN,
			KEY,
			VALUE
		} type;
		size_t idx;
	};

	std::queue<std::pair<redisReply*, type>> stack;

	redisReply* reply = (redisReply*)redisClusterCommandArgv(cc, argv.size(), &argv[0], &lens[0]);
	if (!reply)
	{
		redisClusterFree(cc);
		try {
			cc = redisClusterContextInit2();
		} catch (const std::runtime_error& e) {
			set_ret_error(ret, e.what());
			goto add_back;
		}
		redisClusterConnect2(cc);
		if (cc->err)
		{
			set_ret_error(ret, cc->errstr);
			goto add_back;
		}
		reply = (redisReply*)redisClusterCommandArgv(cc, argv.size(), &argv[0], &lens[0]);
		if (!reply)
		{
			set_ret_error(ret, cc->errstr);
			goto add_back;
		}
	}

	HalonHSLValue *array_key, *array_value;
	HalonMTA_hsl_value_array_add(ret, &array_key, &array_value);
	HalonMTA_hsl_value_set(array_key, HALONMTA_HSL_TYPE_STRING, "reply", 0);
	stack.push({ reply, { array_value, type::PLAIN, 0 } });
	while (!stack.empty())
	{
		redisReply* r = stack.front().first;
		HalonHSLValue* v;
		switch (stack.front().second.type)
		{
			case type::PLAIN:
				v = stack.front().second.v;
			break;
			case type::VALUE:
				v = HalonMTA_hsl_value_array_get(stack.front().second.v, stack.front().second.idx, nullptr);
			break;
			case type::KEY:
				HalonMTA_hsl_value_array_get(stack.front().second.v, stack.front().second.idx, &v);
			break;
		}
		stack.pop();

		/* handle types of RESP2 and RESP3 */
		switch (r->type)
		{
			case REDIS_REPLY_STRING:
			case REDIS_REPLY_STATUS:
			case REDIS_REPLY_ERROR:
			case REDIS_REPLY_BIGNUM:
			{
				HalonMTA_hsl_value_set(v, HALONMTA_HSL_TYPE_STRING, r->str, r->len);
			}
			break;
			case REDIS_REPLY_ARRAY:
			case REDIS_REPLY_SET:
			{
				HalonMTA_hsl_value_set(v, HALONMTA_HSL_TYPE_ARRAY, nullptr, 0);
				for (size_t i = 0; i < r->elements; ++i)
				{
					HalonHSLValue *k, *v2;
					HalonMTA_hsl_value_array_add(v, &k, &v2);
					double d = i;
					HalonMTA_hsl_value_set(k, HALONMTA_HSL_TYPE_NUMBER, &d, 0);
					stack.push({ r->element[i], { v, type::VALUE, i }});
				}
			}
			break;
			case REDIS_REPLY_INTEGER:
			{
				double x = r->integer; /* truncated to double */
				HalonMTA_hsl_value_set(v, HALONMTA_HSL_TYPE_NUMBER, (const void*)&x, 0);
			}
			break;
			case REDIS_REPLY_NIL:
				/* null */
			break;
			case REDIS_REPLY_DOUBLE:
				HalonMTA_hsl_value_set(v, HALONMTA_HSL_TYPE_NUMBER, (const void*)&r->dval, 0);
			break;
			case REDIS_REPLY_BOOL:
			{
				bool x = r->integer ? true : false;
				HalonMTA_hsl_value_set(v, HALONMTA_HSL_TYPE_BOOLEAN, (const void*)&x, 0);
			}
			break;
			case REDIS_REPLY_MAP:
			{
				HalonMTA_hsl_value_set(v, HALONMTA_HSL_TYPE_ARRAY, nullptr, 0);
				for (size_t i = 0, x = 0; i < r->elements; i += 2, ++x)
				{
					HalonHSLValue *k, *v2;
					HalonMTA_hsl_value_array_add(v, &k, &v2);
					stack.push({ r->element[i], { v, type::KEY, x }});
					stack.push({ r->element[i + 1], { v, type::VALUE, x }});
				}
			}
			break;
			case REDIS_REPLY_ATTR:
				/* unsupported */
			break;
			case REDIS_REPLY_PUSH:
				/* unsupported */
			break;
			case REDIS_REPLY_VERB:
				/* unsupported: string */
			break;
		}
	}

	freeReplyObject(reply);

add_back:
	ul.lock();
	pool.push(cc);
	ul.unlock();
	cv.notify_one();
}

HALON_EXPORT
bool Halon_hsl_register(HalonHSLRegisterContext* hhrc)
{
	HalonMTA_hsl_register_function(hhrc, "redisClusterCommand", HSLRedisClusterCommand);
	return true;
}
