//
// Copyright (c) 2004-2017 Benjamin Kaufmann
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//
//
// NOTE: ProgramOptions is inspired by Boost.Program_options
//       see: www.boost.org/libs/program_options
//
#ifdef _MSC_VER
#pragma warning (disable : 4786)
#pragma warning (disable : 4503)
#pragma warning (disable : 4996)
#endif
#include <potassco/program_opts/program_options.h>
#include <potassco/program_opts/errors.h>
#include <cassert>
#include <cstring>
#include <climits>
#include <ostream>  // for op<<
#include <istream>  // for CfgFileParser
#include <algorithm>// std::sort
#include <stdlib.h>
#include <stdio.h>
#include <cctype>
using namespace std;

namespace Potassco { namespace ProgramOptions {
///////////////////////////////////////////////////////////////////////////////
// DefaultFormat
///////////////////////////////////////////////////////////////////////////////
std::size_t DefaultFormat::format(std::vector<char>& buf, const Option& o, std::size_t maxW) {
	buf.clear();
	size_t bufSize = std::max(maxW, o.maxColumn()) + 3;
	const char* arg = o.argName();
	const char* np = "";
	const char* ap = "";
	if (o.value()->isNegatable()) {
		if (!*arg) { np = "[no-]"; }
		else { ap = "|no"; bufSize += strlen(ap); }
	}
	buf.resize(bufSize);
	char*  buffer = &buf[0];
	size_t n = sprintf(buffer, "  --%s%s", np, o.name().c_str());
	if (o.value()->isImplicit() && *arg) {
		n += sprintf(buffer+n, "[=%s%s]", arg, ap);
	}
	if (o.alias()) {
		n += sprintf(buffer+n, ",-%c", o.alias());
	}
	if (!o.value()->isImplicit()) {
		n += sprintf(buffer+n, "%c%s%s", (!o.alias()?'=':' '), arg, ap);
	}
	if (n < maxW) n += sprintf(buffer+n, "%-*.*s", int(maxW-n), int(maxW-n), " ");
	assert(n <= bufSize);
	return n;
}
std::size_t DefaultFormat::format(std::vector<char>& buf, const char* desc, const Value& val, std::size_t) {
	std::size_t minS = strlen(desc);
	const char* temp = 0;
	if (!desc)  desc = "";
	buf.clear();
	buf.reserve(minS+2);
	buf.push_back(':');
	buf.push_back(' ');
	for (const char* look;; ++desc) {
		look = desc;
		while (*look && *look != '%') {
			++look;
		}
		if (look != desc) { buf.insert(buf.end(), desc, look); }
		if (!*look++ || !*look) break;
		else if (*look == 'D') { temp = val.defaultsTo(); }
		else if (*look == 'A') { temp = val.arg(); }
		else if (*look == 'I') { temp = val.implicit(); }
		else { buf.push_back(*look); }
		if (temp) { buf.insert(buf.end(), temp, temp + strlen(temp)); }
		desc = look;
		temp = 0;
	}
	buf.push_back('\n');
	return buf.size();
}
std::size_t DefaultFormat::format(std::vector<char>& buffer, const OptionGroup& grp) {
	buffer.clear();
	if (grp.caption().length()) {
		buffer.reserve(grp.caption().length() + 4);
		buffer.push_back('\n');
		buffer.insert(buffer.end(), grp.caption().begin(), grp.caption().end());
		buffer.push_back(':');
		buffer.push_back('\n');
		buffer.push_back('\n');
	}
	return buffer.size();
}
void OstreamWriter::write(const std::vector<char>& buf, std::size_t n) {
	if (n) { out.write(&buf[0], n); }
}
void StringWriter::write(const std::vector<char>& buf, std::size_t n) {
	if (n) { out.append(&buf[0], n); }
}
void FileWriter::write(const std::vector<char>& buf, std::size_t n) {
	if (n) { fwrite(&buf[0], 1, n, out); }
}
///////////////////////////////////////////////////////////////////////////////
// class Value
///////////////////////////////////////////////////////////////////////////////
Value::Value(byte_t flagSet, State initial)
	: state_(static_cast<byte_t>(initial))
	, flags_(flagSet)
	, descFlag_(0)
	, optAlias_(0) {
	desc_.value = 0;
}

Value::~Value() {
	if (descFlag_ == desc_pack) {
		::operator delete(desc_.pack);
	}
}

const char* Value::arg() const {
	const char* x = desc(desc_name);
	if (x) return x;
	return isFlag() ? "" : "<arg>";
}

Value* Value::desc(DescType t, const char* n) {
	if (n == 0) return this;
	if (t == desc_implicit) {
		setProperty(property_implicit);
		if (!*n) return this;
	}
	if (descFlag_ == 0 || descFlag_ == t) {
		desc_.value = n;
		descFlag_ = static_cast<byte_t>(t);
		return this;
	}
	if (descFlag_ != desc_pack) {
		const char* oldVal = desc_.value;
		unsigned    oldKey = descFlag_ >> 1u;
		desc_.pack = (const char**)::operator new(sizeof(const char*[3]));
		desc_.pack[0] = desc_.pack[1] = desc_.pack[2] = 0;
		descFlag_ = desc_pack;
		desc_.pack[oldKey] = oldVal;
	}
	desc_.pack[t>>1u] = n;
	return this;
}
const char* Value::desc(DescType t) const {
	if (descFlag_ == t || descFlag_ == desc_pack) {
		return descFlag_ == t
			? desc_.value
			: desc_.pack[t >> 1u];
	}
	return 0;
}

const char* Value::implicit() const {
	if (!hasProperty(property_implicit)) return 0;
	const char* x = desc(desc_implicit);
	return x ? x : "1";
}

bool Value::parse(const std::string& name, const std::string& value, State st) {
	if (!value.empty() || !isImplicit()) return state(doParse(name, value), st);
	const char* x = implicit();
	assert(x);
	return state(doParse(name, x), st);
}
///////////////////////////////////////////////////////////////////////////////
// class Option
///////////////////////////////////////////////////////////////////////////////
Option::Option(const string& longName, char alias, const char* desc, Value* v)
	: name_(longName)
	, description_(desc ? desc : "")
	, value_(v) {
	assert(v);
	assert(!longName.empty());
	value_->alias(alias);
}

Option::~Option() {
	delete value_;
}
std::size_t Option::maxColumn() const {
	std::size_t col = 4 + name_.size(); //  --name
	if (alias()) {
		col += 3; // ,-o
	}
	std::size_t argN = strlen(argName());
	if (argN) {
		col += (argN + 1); // =arg
		if (value()->isImplicit()) {
			col += 2; // []
		}
		if (value()->isNegatable()) {
			col += 3; // |no
		}
	}
	else if (value()->isNegatable()) {
		col += 5; // [no-]
	}
	return col;
}

bool Option::assignDefault() const {
	if (value()->defaultsTo() != 0 && value()->state() != Value::value_defaulted) {
		return value()->parse(name(), value()->defaultsTo(), Value::value_defaulted);
	}
	return true;
}
///////////////////////////////////////////////////////////////////////////////
// class OptionGroup
///////////////////////////////////////////////////////////////////////////////
OptionGroup::OptionGroup(const std::string& caption, DescriptionLevel hl) : caption_(caption), level_(hl) {}
OptionGroup::~OptionGroup() {}

OptionInitHelper OptionGroup::addOptions() {
	return OptionInitHelper(*this);
}

void OptionGroup::addOption(const SharedOptPtr& option) {
	options_.push_back(option);
}

std::size_t OptionGroup::maxColumn(DescriptionLevel level) const {
	std::size_t maxW = 0;
	for (option_iterator it = options_.begin(), end = options_.end(); it != end; ++it) {
		if ((*it)->descLevel() <= level) {
			maxW = std::max(maxW, (*it)->maxColumn());
		}
	}
	return maxW;
}

void OptionGroup::format(OptionOutput& out, size_t maxW, DescriptionLevel dl) const {
	for (option_iterator it = options_.begin(), end = options_.end(); it != end; ++it) {
		if ((*it)->descLevel() <= dl) {
			out.printOption(**it, maxW);
		}
	}
}
///////////////////////////////////////////////////////////////////////////////
// class OptionInitHelper
///////////////////////////////////////////////////////////////////////////////
OptionInitHelper::OptionInitHelper(OptionGroup& owner)
	: owner_(&owner) {
}

OptionInitHelper& OptionInitHelper::operator()(const char* name, Value* val, const char* desc) {
	detail::Owned<Value> exit = {val};
	if (!name || !*name || *name == ',' || *name == '!') {
		throw Error("Invalid empty option name");
	}
	const char* n = strchr(name, ',');
	string longName; char shortName = 0;
	if (!n) {
		longName = name;
	}
	else {
		longName.assign(name, n);
		unsigned level = owner_->descLevel();
		const char* x = ++n;
		if (*x && (!x[1] || x[1] == ',')) {
			shortName = *x++;
			x += *x == ',';
		}
		if (*x == '@') {
			++x; level = 0;
			while (*x >= '0' && *x <= '9') {
				level *= 10;
				level += *x - '0';
				++x;
			}
		}
		if (!*n || *x || level > desc_level_hidden) {
			throw Error(std::string("Invalid Key '").append(name).append("'"));
		}
		val->level(DescriptionLevel(level));
	}
	if (*(longName.end()-1) == '!') {
		bool neg = *(longName.end()-2) != '\\';
		longName.erase(longName.end()- (1+!neg), longName.end());
		if (neg) val->negatable();
		else     longName += '!';
	}
	owner_->addOption(SharedOptPtr(new Option(longName, shortName, desc, val)));
	exit.obj = 0;
	return *this;
}
///////////////////////////////////////////////////////////////////////////////
// class OptionContext
///////////////////////////////////////////////////////////////////////////////
OptionContext::OptionContext(const std::string& cap, DescriptionLevel def)
	: caption_(cap)
	, descLevel_(def) {
}
OptionContext::~OptionContext() {
}
const std::string& OptionContext::caption() const {
	return caption_;
}
void OptionContext::setActiveDescLevel(DescriptionLevel x) {
	descLevel_ = std::min(x, desc_level_all);
}
size_t OptionContext::findGroupKey(const std::string& name) const {
	for (size_t i = 0; i != groups_.size(); ++i) {
		if (groups_[i].caption() == name) { return i; }
	}
	return size_t(-1);
}

OptionContext& OptionContext::add(const OptionGroup& options) {
	size_t k = findGroupKey(options.caption());
	if (k >= groups_.size()) {
		// add as new group
		k = groups_.size();
		groups_.push_back(OptionGroup(options.caption(), options.descLevel()));
	}
	for (option_iterator it = options.begin(), end = options.end(); it != end; ++it) {
		insertOption(k, *it);
	}
	groups_[k].setDescriptionLevel(std::min(options.descLevel(), groups_[k].descLevel()));
	return *this;
}

OptionContext& OptionContext::addAlias(const std::string& aliasName, option_iterator option) {
	if (option != end() && !aliasName.empty()) {
		key_type k(option - begin());
		if (!index_.insert(Name2Key::value_type(aliasName, k)).second) { throw DuplicateOption(caption(), aliasName); }
	}
	return *this;
}

const OptionGroup& OptionContext::findGroup(const std::string& name) const {
	std::size_t x = findGroupKey(name);
	if (x < groups_.size()) { return groups_[x]; }
	throw ContextError(caption(), ContextError::unknown_group, name);
}
const OptionGroup* OptionContext::tryFindGroup(const std::string& name) const {
	std::size_t x = findGroupKey(name);
	return x < groups_.size() ? &groups_[x] : 0;
}

OptionContext& OptionContext::add(const OptionContext& other) {
	if (this == &other) return *this;
	for (size_t g = 0; g != other.groups_.size(); ++g) {
		add(other.groups_[g]);
	}
	return *this;
}

void OptionContext::insertOption(size_t groupId, const SharedOptPtr& opt) {
	const string& l = opt->name();
	key_type k(options_.size());
	if (opt->alias()) {
		char sName[2] = {'-', opt->alias()};
		std::string shortName(sName, 2);
		if (!index_.insert(Name2Key::value_type(shortName, k)).second) {
			throw DuplicateOption(caption(), l);
		}
	}
	if (!l.empty()) {
		if (!index_.insert(Name2Key::value_type(l, k)).second) {
			throw DuplicateOption(caption(), l);
		}
	}
	options_.push_back(opt);
	groups_[groupId].options_.push_back(opt);
}

OptionContext::option_iterator OptionContext::find(const char* key, FindType t) const {
	return options_.begin() + findImpl(key, t, unsigned(-1)).first->second;
}

OptionContext::option_iterator OptionContext::tryFind(const char* key, FindType t) const {
	PrefixRange r = findImpl(key, t, 0u);
	return std::distance(r.first, r.second) == 1 ? options_.begin() + r.first->second : end();
}

OptionContext::PrefixRange OptionContext::findImpl(const char* key, FindType t, unsigned eMask, const std::string& eCtx) const {
	std::string k(key ? key : "");
	if (t == find_alias && !k.empty() && k[0] != '-') {
		k += k[0];
		k[0] = '-';
	}
	index_iterator it = index_.lower_bound(k);
	index_iterator up = it;
	if (it != index_.end()) {
		if ((it->first == k) && ((t & (find_alias|find_name)) != 0)) {
			++up;
		}
		else if ((t & find_prefix) != 0) {
			k += char(CHAR_MAX);
			up = index_.upper_bound(k);
			k.erase(k.end()-1);
		}
	}
	if (std::distance(it, up) != 1 && eMask) {
		if ((eMask & 1u) && it == up) { throw UnknownOption(eCtx, k); }
		if ((eMask & 2u) && it != up) {
			std::string str;
			for (; it != up; ++it) {
				str += "  ";
				str += it->first;
				str += "\n";
			}
			throw AmbiguousOption(eCtx, k, str);
		}
	}
	return PrefixRange(it, up);
}

OptionOutput& OptionContext::description(OptionOutput& out) const {
	DescriptionLevel dl = descLevel_;
	if (out.printContext(*this)) {
		size_t maxW = 23;
		for (size_t i = 0; i != groups(); ++i) {
			maxW = std::max(maxW, groups_[i].maxColumn(dl));
		}
		// print all visible groups
		for (std::size_t i = 1; i < groups_.size(); ++i) {
			if (groups_[i].descLevel() <= dl && out.printGroup(groups_[i])) {
				groups_[i].format(out, maxW, dl);
			}
		}
		if (!groups_.empty() && groups_[0].descLevel() <= dl && out.printGroup(groups_[0])) {
			groups_[0].format(out, maxW, dl);
		}
	}
	return out;
}

std::string OptionContext::defaults(std::size_t n) const {
	DescriptionLevel dl = descLevel_;
	std::size_t line = n;
	std::string defs;
	defs.reserve(options_.size());
	std::string opt; opt.reserve(80);
	for (int g = 0; g < 2; ++g) {
		// print all sub-groups followed by main group
		for (std::size_t i = (g == 0), end = (g == 0) ? groups_.size() : 1; i < end; ++i) {
			if (groups_[i].descLevel() <= dl) {
				for (option_iterator it = groups_[i].begin(), oEnd = groups_[i].end(); it != oEnd; ++it) {
					const Option& o = **it;
					if (o.value()->defaultsTo() && o.descLevel() <= dl) {
						((((opt += "--") += o.name()) += "=") += o.value()->defaultsTo());
						if (line + opt.size() > 78) {
							defs += '\n';
							defs.append(n, ' ');
							line = n;
						}
						defs += opt;
						defs += ' ';
						line += opt.size() + 1;
						opt.clear();
					}
				}
			}
		}
	}
	return defs;
}
std::ostream& operator<<(std::ostream& os, const OptionContext& grp) {
	StreamOut out(os);
	grp.description(out);
	return os;
}

bool OptionContext::assignDefaults(const ParsedOptions& opts) const {
	for (option_iterator it = begin(), end = this->end(); it != end; ++it) {
		const Option& o = **it;
		if (opts.count(o.name()) == 0 && !o.assignDefault()) {
			throw ValueError(caption(), ValueError::invalid_default, o.name(), o.value()->defaultsTo());
		}
	}
	return true;
}
///////////////////////////////////////////////////////////////////////////////
// class ParsedOptions
///////////////////////////////////////////////////////////////////////////////
ParsedOptions::ParsedOptions() {}
ParsedOptions::~ParsedOptions() { parsed_.clear(); }
bool ParsedOptions::assign(const ParsedValues& p, const ParsedOptions* exclude) {
	if (!p.ctx) return false;
	struct Assign {
		Assign(ParsedOptions* x, const ParsedOptions* exclude) : self(x), ignore(exclude) {}
		void assign(const ParsedValues& p) {
			begin = it = p.begin();
			// assign parsed values
			for (ParsedValues::iterator end = p.end(); it != end; ++it) {
				const Option& o = *it->first;
				if (ignore && ignore->count(o.name()) != 0 && !o.value()->isComposing()) {
					continue;
				}
				if (int ret = self->assign(o, it->second)) {
					throw ValueError(p.ctx ? p.ctx->caption() : "", static_cast<ValueError::Type>(ret-1), o.name(), it->second);
				}
			}
		}
		~Assign() {
			for (ParsedValues::iterator x = begin, end = this->it; x != end; ++x) {
				const Option& o = *x->first;
				assert(o.value()->state() == Value::value_fixed || self->parsed_.count(o.name()) != 0 || ignore->count(o.name()) != 0);
				if (o.value()->state() == Value::value_fixed) {
					self->parsed_.insert(x->first->name());
					o.value()->state(Value::value_unassigned);
				}
			}
		}
		ParsedOptions* self;
		const ParsedOptions*   ignore;
		ParsedValues::iterator begin;
		ParsedValues::iterator it;
	} scoped(this, exclude);
	scoped.assign(p);
	return true;
}
int ParsedOptions::assign(const Option& o, const std::string& value) {
	unsigned badState = 0;
	if (!o.value()->isComposing()) {
		if (parsed_.count(o.name())) { return 0; }
		badState = (Value::value_fixed & o.value()->state());
	}
	if (badState || !o.value()->parse(o.name(), value, Value::value_fixed)) {
		return badState
			? 1 + ValueError::multiple_occurrences
			: 1 + ValueError::invalid_value;
	}
	return 0;
}
///////////////////////////////////////////////////////////////////////////////
// class ParsedValues
///////////////////////////////////////////////////////////////////////////////
namespace {
template <class P>
struct LessFirst {
	bool operator()(const P& lhs, const P& rhs) const {
		return lhs.first.get() < rhs.first.get();
	}
};
} // namespace
void ParsedValues::add(const std::string& name, const std::string& value) {
	OptionContext::option_iterator it = ctx->tryFind(name.c_str());
	if (it != ctx->end()) {
		add(*it, value);
	}
}
///////////////////////////////////////////////////////////////////////////////
// class OptionParser
///////////////////////////////////////////////////////////////////////////////
OptionParser::OptionParser(ParseContext& o)
	: ctx_(&o) {
}

OptionParser::~OptionParser() {
}

ParseContext& OptionParser::parse() {
	doParse();
	return *ctx_;
}
ParseContext::~ParseContext() {}
namespace {
///////////////////////////////////////////////////////////////////////////////
// class CommandLineParser
///////////////////////////////////////////////////////////////////////////////
class CommandLineParser : public OptionParser {
public:
	enum OptionType { short_opt, long_opt, end_opt, no_opt };
	CommandLineParser(ParseContext& ctx, unsigned f)
		: OptionParser(ctx)
		, flags(f) {
	}
	std::vector<const char*> remaining;
	unsigned flags;
private:
	virtual const char* next() = 0;
	void doParse() {
		bool breakEarly = false;
		int  posKey = 0;
		const char* curr;
		while ((curr = next()) != 0 && !breakEarly) {
			switch (getOptionType(curr)) {
				case short_opt: if (handleShortOpt(curr + 1)) curr = 0; break;
				case long_opt:  if (handleLongOpt(curr + 2))  curr = 0; break;
				case end_opt:   curr = 0; breakEarly = true; break;
				case no_opt: {
					SharedOptPtr opt = getOption(posKey++, curr);
					if (opt.get()) {
						addOptionValue(opt, curr);
						curr = 0;
					}
					break; }
				default:
					assert(0);
			}
			if (curr) {
				remaining.push_back(curr);
			}
		}
		while (curr) {
			remaining.push_back(curr);
			curr = next();
		}
	}
	OptionType getOptionType(const char* o) const {
		if (strncmp(o, "--", 2) == 0) {
			return o[2] ? long_opt : end_opt;
		}
		return *o == '-' && *(o + 1) != '\0' ? short_opt : no_opt;
	}
	bool handleShortOpt(const char* optName) {
		// either -o value or -o[value|opts]
		char optn[2];
		optn[1] = '\0';
		SharedOptPtr o;
		while (*optName) {
			optn[0] = *optName;
			const char* val = optName + 1;
			if ((o = getOption(optn, OptionContext::find_alias)).get()) {
				if (o->value()->isImplicit()) {
					// -ovalue or -oopts
					if (!o->value()->isFlag()) {
						// consume (possibly empty) value
						addOptionValue(o, val);
						return true;
					}
					else {
						// -o + more options
						addOptionValue(o, "");
						++optName;
					}
				}
				else if (*val != 0 || (val = next()) != 0) {
					// -ovalue or -o value
					addOptionValue(o, val);
					return true;
				}
				else {
					throw SyntaxError(SyntaxError::missing_value, optn);
				}
			}
			else {
				return false;
			}
		}
		return true;
	}
	bool handleLongOpt(const char* optName) {
		string name(optName);
		string value;
		string::size_type p = name.find('=');
		if (p != string::npos) {
			value.assign(name, p + 1, string::npos);
			name.erase(p, string::npos);
		}
		SharedOptPtr o, on;
		bool neg = false;
		if (value.empty() && std::strncmp(optName, "no-", 3) == 0) {
			try { on = getOption(optName+3, OptionContext::find_name_or_prefix); }
			catch (...) {}
			if (on.get() && !on->value()->isNegatable()) { on.reset(); }
		}
		try { o = getOption(name.c_str(), OptionContext::find_name_or_prefix); }
		catch (const UnknownOption&) {
			if (!on.get()) { throw; }
		}
		if (!o.get() && on.get()) {
			o.swap(on);
			value = "no";
			neg = true;
		}
		if (o.get()) {
			if (!o->value()->isImplicit() && value.empty()) {
				if (const char* v = next()) { value = v; }
				else { throw SyntaxError(SyntaxError::missing_value, name); }
			}
			else if (o->value()->isFlag() && !value.empty() && !neg && (flags & unsigned(command_line_allow_flag_value)) == 0u) {
				// flags don't have values
				throw SyntaxError(SyntaxError::extra_value, name);
			}
			addOptionValue(o, value);
			return true;
		}
		return false;
	}
};

class ArgvParser : public CommandLineParser {
public:
	ArgvParser(ParseContext& ctx, int startPos, int endPos, const char*const* argv, unsigned flags)
		: CommandLineParser(ctx, flags)
		, currentArg_(0)
		, argPos_(startPos)
		, endPos_(endPos)
		, argv_(argv) {
	}
private:
	const char* next() {
		currentArg_ = argPos_ != endPos_ ? argv_[argPos_++] : 0;
		return currentArg_;
	}
	const char*       currentArg_;
	int               argPos_;
	int               endPos_;
	const char*const* argv_;
};

class CommandStringParser : public CommandLineParser {
public:
	CommandStringParser(const char* cmd, ParseContext& ctx, unsigned flags)
		: CommandLineParser(ctx, flags)
		, cmd_(cmd ? cmd : "") {
		tok_.reserve(80);
	}
private:
	const char* next() {
		// skip leading white
		while (std::isspace(static_cast<unsigned char>(*cmd_))) { ++cmd_; }
		if (!*cmd_) return 0;
		tok_.clear();
		// find end of current arg
		for (char c, t = ' ', n; (c = *cmd_) != 0; ++cmd_) {
			if (c == t) { if (t == ' ') break; t = ' '; }
			else if ((c == '\'' || c == '"') && t == ' ') { t = c; }
			else if (c != '\\') { tok_ += c; }
			else if ((n = cmd_[1]) == '"' || n == '\'' || n == '\\') { tok_ += n; ++cmd_; }
			else { tok_ += c; }
		}
		return tok_.c_str();
	}
	CommandStringParser& operator=(const CommandStringParser&);
	const char* cmd_;
	std::string tok_;
};
///////////////////////////////////////////////////////////////////////////////
// class CfgFileParser
///////////////////////////////////////////////////////////////////////////////
class CfgFileParser : public OptionParser {
public:
	CfgFileParser(ParseContext& ctx, std::istream& in)
		: OptionParser(ctx)
		, in_(in) {
	}
private: void operator=(const CfgFileParser&);
				 inline void trimLeft(std::string& str, const std::string& charList = " \t") {
					 std::string::size_type pos = str.find_first_not_of(charList);
					 if (pos != 0)
						 str.erase(0, pos);
				 }
				 inline void trimRight(std::string& str, const std::string& charList = " \t") {
					 std::string::size_type pos = str.find_last_not_of(charList);
					 if (pos != std::string::npos)
						 str.erase(pos + 1, std::string::npos);
				 }
				 bool splitHalf(const std::string& str, const std::string& seperator,
					 std::string& leftSide,
					 std::string& rightSide) {
					 std::string::size_type sepPos = str.find(seperator);
					 leftSide.assign(str, 0, sepPos);
					 if (sepPos != std::string::npos) {
						 rightSide.assign(str, sepPos + seperator.length(), std::string::npos);
						 return true;
					 }
					 return false;
				 }
				 void doParse() {
					 int lineNr = 0;
					 std::string sectionName;      // current section name
					 std::string sectionValue;     // current section value
					 bool inSection = false;       // true if multi line section value
					 FindType ft = OptionContext::find_name_or_prefix;
					 SharedOptPtr opt;
					 // reads the config file.
					 // A config file may only contain empty lines, single line comments or
					 // sections structured in a name = value fashion.
					 // value can span multiple lines, but parts in different lines than name
					 // must not contain a '='-Character.
					 for (std::string line; std::getline(in_, line);) {
						 ++lineNr;
						 trimLeft(line);
						 trimRight(line);

						 if (line.empty() || line.find("#") == 0) {
							 // An empty line or single line comment stops a multi line section value.
							 if (inSection) {
								 if ((opt = getOption(sectionName.c_str(), ft)).get())
									 addOptionValue(opt, sectionValue);
								 inSection = false;
							 }
							 continue;
						 }
						 std::string::size_type pos;
						 if ((pos = line.find("=")) != std::string::npos) {
							 // A new section terminates a multi line section value.
							 // First process the current section value...
							 if (inSection && (opt = getOption(sectionName.c_str(), ft)).get()) {
								 addOptionValue(opt, sectionValue);
							 }
							 // ...then save the new section's value.
							 splitHalf(line, "=", sectionName, sectionValue);
							 trimRight(sectionName);
							 trimLeft(sectionValue, " \t\n");
							 inSection = true;
						 }
						 else if (inSection) {
							 sectionValue += " ";
							 sectionValue += line;
						 }
						 else {
							 throw SyntaxError(SyntaxError::invalid_format, line);
						 }
					 }
					 if (inSection) { // file does not end with an empty line
						 if ((opt = getOption(sectionName.c_str(), ft)).get())
							 addOptionValue(opt, sectionValue);
					 }
				 }
				 std::istream& in_;
};
class DefaultContext : public ParseContext {
public:
	DefaultContext(const OptionContext& o, bool allowUnreg, PosOption po)
		: posOpt(po)
		, parsed(o)
		, eMask(2u + unsigned(!allowUnreg)) {
	}
	SharedOptPtr  getOption(const char* name, FindType ft) {
		OptionContext::OptionRange r = parsed.ctx->findImpl(name, ft, eMask);
		if (r.first != r.second) { return *(parsed.ctx->begin() + r.first->second); }
		return SharedOptPtr(0);
	}
	SharedOptPtr  getOption(int, const char* tok) {
		std::string optName;
		if (!posOpt || !posOpt(tok, optName)) { return getOption("Positional Option", OptionContext::find_name_or_prefix); }
		return getOption(optName.c_str(), OptionContext::find_name_or_prefix);
	}
	void          addValue(const SharedOptPtr& key, const std::string& value) { parsed.add(key, value); }
	PosOption    posOpt;
	ParsedValues parsed;
	unsigned     eMask;
};

} // end unnamed namespace

ParsedValues parseCommandLine(int& argc, char** argv, const OptionContext& o, bool allowUnreg, PosOption po, unsigned flags) {
	DefaultContext ctx(o, allowUnreg, po);
	return static_cast<DefaultContext&>(parseCommandLine(argc, argv, ctx, flags)).parsed;
}
ParseContext& parseCommandLine(int& argc, char** argv, ParseContext& ctx, unsigned flags) {
	while (argv[argc]) ++argc;
	ArgvParser parser(ctx, 1, argc, argv, flags);
	parser.parse();
	argc = 1 + (int)parser.remaining.size();
	for (int i = 1; i != argc; ++i) {
		argv[i] = const_cast<char*>(parser.remaining[i-1]);
	}
	argv[argc] = 0;
	return ctx;
}
ParsedValues parseCommandArray(const char* const* argv, unsigned nArgs, const OptionContext& o, bool allowUnreg, PosOption po, unsigned flags) {
	DefaultContext ctx(o, allowUnreg, po);
	ArgvParser parser(ctx, 0, nArgs, argv, flags);
	parser.parse();
	return static_cast<DefaultContext&>(ctx).parsed;
}
ParseContext& parseCommandString(const char* cmd, ParseContext& ctx, unsigned flags) {
	return CommandStringParser(cmd, ctx, flags).parse();
}
ParsedValues parseCommandString(const std::string& cmd, const OptionContext& o, bool allowUnreg, PosOption po, unsigned flags) {
	DefaultContext ctx(o, allowUnreg, po);
	return static_cast<DefaultContext&>(CommandStringParser(cmd.c_str(), ctx, flags).parse()).parsed;
}

ParsedValues parseCfgFile(std::istream& in, const OptionContext& o, bool allowUnreg) {
	DefaultContext ctx(o, allowUnreg, 0);
	return static_cast<DefaultContext&>(CfgFileParser(ctx, in).parse()).parsed;
}

///////////////////////////////////////////////////////////////////////////////
// Errors
///////////////////////////////////////////////////////////////////////////////
static std::string quote(const std::string& x) {
	return std::string("'").append(x).append("'");
}
static std::string format(SyntaxError::Type t, const std::string& key) {
	std::string ret("SyntaxError: ");
	ret += quote(key);
	switch (t) {
		case SyntaxError::missing_value: ret += " requires a value!";       break;
		case SyntaxError::extra_value:   ret += " does not take a value!";  break;
		case SyntaxError::invalid_format:ret += " unrecognized line!";      break;
		default:                         ret += " unknown syntax!";
	};
	return ret;
}
static std::string format(ContextError::Type t, const std::string& ctx, const std::string& key, const std::string& alt) {
	std::string ret;
	if (!ctx.empty()) { ret += "In context "; ret += quote(ctx); ret += ": "; }
	switch (t) {
		case ContextError::duplicate_option: ret += "duplicate option: "; break;
		case ContextError::unknown_option:   ret += "unknown option: ";   break;
		case ContextError::ambiguous_option: ret += "ambiguous option: "; break;
		case ContextError::unknown_group:    ret += "unknown group: ";    break;
		default:                             ret += "unknown error in: ";
	};
	ret += quote(key);
	if (t == ContextError::ambiguous_option && !alt.empty()) {
		ret += " could be:\n";
		ret += alt;
	}
	return ret;
}
static std::string format(ValueError::Type t, const std::string& ctx, const std::string& key, const std::string& value) {
	std::string ret; const char* x = "";
	if (!ctx.empty()) { ret += "In context "; ret += quote(ctx); ret += ": "; }
	switch (t) {
		case ValueError::multiple_occurrences: ret += "multiple occurrences: "; break;
		case ValueError::invalid_default: x = "default ";  // FALLTHRU
		case ValueError::invalid_value:
			ret += quote(value);
			ret += " invalid ";
			ret += x;
			ret += "value for: ";
			break;
		default: ret += "unknown error in: ";
	};
	ret += quote(key);
	return ret;
}
SyntaxError::SyntaxError(Type t, const std::string& key)
	: Error(format(t, key))
	, key_(key)
	, type_(t) {
}
ContextError::ContextError(const std::string& ctx, Type t, const std::string& key, const std::string& alt)
	: Error(format(t, ctx, key, alt))
	, ctx_(ctx)
	, key_(key)
	, type_(t) {
}
ValueError::ValueError(const std::string& ctx, Type t, const std::string& opt, const std::string& value)
	: Error(format(t, ctx, opt, value))
	, ctx_(ctx)
	, key_(opt)
	, value_(value)
	, type_(t) {
}
} // namespace ProgramOptions
} // namespace Potassco
