#ifndef MK_EXCEPTION
#define MK_EXCEPTION(name,base)\
	struct name : virtual base{\
		typedef base base_t;\
		name(const std::string &s):base_t(s){}\
		name(const char *s):base_t(s){}\
		name():base(""){}\
	};
#endif
