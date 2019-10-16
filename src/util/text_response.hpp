#ifndef TEXT_RESPONSE_H
#define TEXT_RESPONSE_H

#include <list>
#include <string>
#include <regex>

struct string_equality_matcher
{
	string_equality_matcher (const std::string& str) : pattern(str) {}
	virtual ~string_equality_matcher () = default;

	bool operator()(const std::string& match);

	std::string pattern;
};

struct regex_matcher
{
	regex_matcher (const std::string& pat) : pattern(pat) {}
	regex_matcher (const std::regex& regex) : pattern(regex) {}
	virtual ~regex_matcher () = default;

	std::regex pattern;
};

struct base_text_response
{
	virtual bool match (const std::string& text) = 0;
	virtual void operator()() = 0;
};

template<typename Functor, typename Matcher = string_equality_matcher>
struct text_response : public base_text_response
{
	text_response(int tries, std::string& s, Functor f) : _tries(tries), _m(s), _f(f) {}
	~text_response() = default;

	bool match(const std::string& text) override {
		bool r = _m(text);
		if(r) { _tries--; }
		return r;
	}

	void operator()() override { _f(); }

	int _tries;
	Matcher _m;
	Functor _f;
};

template<typename Functor>
explicit text_response(int tries, std::string& s, Functor F) -> text_response<Functor, string_equality_matcher>;

struct text_responder
{
	typedef std::list<base_text_response*> response_list;

	response_list lst;

	text_responder();
	~text_responder() = default;

	void reply_to (std::string& s);


};

#endif // TEXT_RESPONSE_H
