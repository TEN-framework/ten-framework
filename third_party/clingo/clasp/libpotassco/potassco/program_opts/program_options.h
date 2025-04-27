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
#ifndef PROGRAM_OPTIONS_PROGRAM_OPTIONS_H_INCLUDED
#define PROGRAM_OPTIONS_PROGRAM_OPTIONS_H_INCLUDED
#include <potassco/program_opts/value.h>
#include <potassco/program_opts/detail/refcountable.h>
#include <iosfwd>
#include <set>
#include <map>
#include <vector>
#include <stdexcept>
#include <cstdio>
namespace Potassco { namespace ProgramOptions {

//! Represents one program option.
/*!
 * An Option consists of a description (long name, short name, description),
 * a (typed) value, and an optional default value.
 *
 * \note
 *   When printing an option, occurrences of %D, %I and %A in its description are replaced
 *   with the option's default value, implicit value and the argument name,
 *   respectively.
 */
class Option : public detail::RefCountable {
public:
	/*!
	* \pre longName != ""
	* \pre vd != 0
	* \param longName    name (and unique key) of the option
	* \param shortName   possible alias name
	* \param description description of the option, used for printing help
	* \param value       value object to be associated with this option
	*/
	Option(const std::string& longName, char shortName,
		const char* description, Value* value);

	~Option();

	const std::string& name()           const { return name_; }
	char               alias()          const { return value_->alias(); }
	Value*             value()          const { return value_; }
	const char*        description()    const { return description_; }
	const char*        argName()        const { return value_->arg(); }
	bool               assignDefault()  const;
	std::size_t        maxColumn()      const;
	DescriptionLevel   descLevel()      const { return value_->level(); }
private:
	std::string name_;        // name (and unique key) of option
	const char* description_; // description of the option (used for --help)
	Value*      value_;       // the option's value manager
};

typedef detail::IntrusiveSharedPtr<Option> SharedOptPtr;

class OptionInitHelper;
class OptionContext;
class OptionParser;
class ParsedValues;
class ParsedOptions;
class OptionOutput;

//! A list of options logically grouped under a caption.
/*!
 * The class provides a logical grouping of options that
 * is mainly useful for printing help.
 */
class OptionGroup {
public:
	typedef std::vector<SharedOptPtr>  OptionList;
	typedef OptionList::const_iterator option_iterator;

	/*!
	 * Creates a new group of options under the given caption.
	 */
	OptionGroup(const std::string& caption = "", DescriptionLevel descLevel = desc_level_default);
	~OptionGroup();

	//! Returns the caption of this group.
	const std::string& caption() const { return caption_; }

	std::size_t      size()     const { return options_.size(); }
	bool             empty()    const { return options_.empty(); }
	option_iterator  begin()    const { return options_.begin(); }
	option_iterator  end()      const { return options_.end(); }
	DescriptionLevel descLevel()const { return level_; }

	//! Returns an object that can be used to add options.
	/*!
	 * \par usage \n
	 * \code
	 * OptionGroup g("Some Options");
	 * ValueMap m;
	 * g.addOptions()
	 *   ("opt1", store<int>(m), "some int value")   // <- no semicolon
	 *   ("opt2", store<double>(m))                  // <- no semicolon
	 *   ("opt3", store<char>(m))                    // <- no semicolon
	 * ;                                            // <- note the semicolon!
	 * \endcode
	 */
	OptionInitHelper addOptions();

	//! Adds option to this group.
	void addOption(const SharedOptPtr& option);

	void setDescriptionLevel(DescriptionLevel level) { level_ = level; }

	//! Creates a formated description of all options with level() <= level in this group.
	void format(OptionOutput& out, size_t maxW, DescriptionLevel level = desc_level_default) const;

	std::size_t maxColumn(DescriptionLevel level) const;
private:
	friend class OptionContext;
	std::string      caption_;
	OptionList       options_;
	DescriptionLevel level_;
};

class OptionInitHelper {
public:
	explicit OptionInitHelper(OptionGroup& owner);

	//! Factory function for creating an option.
	/*!
	 * \param key <name>[!][,<alias>][,@<level>]
	 * \param val  Value of the option
	 * \param desc Description of the option
	 *
	 * \note If <name> is followed by an exclamation mark ('!')
	 *       the option is marked as negatable.
	 */
	OptionInitHelper& operator()(const char* key,
		Value* val, const char* desc);
private:
	OptionGroup* owner_;
};

//! A (logically grouped) list of unique options.
/*!
 * An option context stores a list of option groups.
 * Options in a context have to be unique (w.r.t name and alias)
 * within that context.
 *
 * An OptionContext defines the granularity of option parsing
 * and option lookup.
 */
class OptionContext {
private:
	typedef std::size_t                               key_type;
	typedef std::map<std::string, key_type>           Name2Key;
	typedef std::vector<OptionGroup>                  GroupList;
	typedef Name2Key::const_iterator                  index_iterator;
	typedef std::pair<index_iterator, index_iterator> PrefixRange;
	typedef OptionGroup::OptionList                   OptionList;
public:
	//! Type for identifying an option within a context
	typedef OptionList::const_iterator option_iterator;
	typedef PrefixRange                OptionRange;

	OptionContext(const std::string& caption = "", DescriptionLevel desc_default = desc_level_default);
	~OptionContext();

	const std::string& caption() const;

	//! Adds the given group of options to this context.
	/*!
	 * \note  If this object already contains a group with
	 *        the same caption as group, the groups are merged.
	 *
	 * \throw DuplicateOption if an option in group
	 *        has the same short or long name as one of the
	 *        options in this context.
	 */
	OptionContext& add(const OptionGroup& group);

	//! Adds an alias name for the given option.
	/*!
	 * \throw DuplicateOption if an option with the name aliasName already exists.
	 */
	OptionContext& addAlias(const std::string& aliasName, option_iterator option);

	//! Adds all groups (and their options) from other to this context.
	/*!
	 * \throw DuplicateOption if an option in other
	 *        has the same short or long name as one of the
	 *        options in this context.
	 *
	 * \see OptionContext& add(const OptionGroup&);
	 */
	OptionContext& add(const OptionContext& other);

	option_iterator begin() const { return options_.begin(); }
	option_iterator end()   const { return options_.end(); }

	//! Returns the number of options in this context.
	std::size_t    size()   const { return options_.size(); }
	//! Returns the number of groups in this context
	std::size_t    groups() const { return groups_.size(); }

	enum FindType { find_name = 1, find_prefix = 2, find_name_or_prefix = find_name|find_prefix, find_alias = 4 };

	//! Returns the option with the given key.
	/*!
	 * \note The second parameter defines how key is interpreted:
	 *        - find_name:   search for an option whose name equals key.
	 *        - find_prefix: search for an option whose name starts with the given key.
	 *        - find_alias:  search for an option whose alias equals key.
	 *       .
	 *
	 * \note If second parameter is find_alias, a starting '-'
	 *       in key is valid but not required.
	 *
	 * \throw UnknownOption if no option matches key.
	 * \throw AmbiguousOption if more than one option matches key.
	 */
	option_iterator find(const char* key, FindType t = find_name) const;
	/*!
	 * Behaves like find but returns end() instead of throwing
	 * UnknownOption or AmbiguousOption.
	 */
	option_iterator tryFind(const char* key, FindType t = find_name) const;

	OptionRange findImpl(const char* key, FindType t, unsigned eMask = unsigned(-1)) const { return findImpl(key, t, eMask, caption()); }
	OptionRange findImpl(const char* key, FindType t, unsigned eMask, const std::string& eCtx) const;

	const OptionGroup& findGroup(const std::string& caption) const;
	const OptionGroup* tryFindGroup(const std::string& caption) const;

	//! Sets the description level to be used when generating description.
	/*!
	 * Once set, functions generating descriptions will only consider groups
	 * and options with description level <= std::min(level, desc_level_all).
	 */
	void             setActiveDescLevel(DescriptionLevel level);
	DescriptionLevel getActiveDescLevel() const { return descLevel_; }

	//! Writes a formatted description of options in this context.
	OptionOutput& description(OptionOutput& out) const;

	//! Returns the default command-line of this context.
	std::string defaults(std::size_t prefixSize = 0) const;

	//! Writes a formatted description of options in this context to os.
	friend std::ostream& operator<<(std::ostream& os, const OptionContext& ctx);

	//! Assigns any default values to all options not in exclude.
	/*!
	 * \throw ValueError if some default value is actually invalid for its option.
	 */
	bool assignDefaults(const ParsedOptions& exclude) const;
private:
	void        insertOption(size_t groupId, const SharedOptPtr& o);
	size_t      findGroupKey(const std::string& name) const;

	Name2Key         index_;
	OptionList       options_;
	GroupList        groups_;
	std::string      caption_;
	DescriptionLevel descLevel_;
};

class OptionParser;
class ParsedValues;

//! Set of options holding a parsed value.
class ParsedOptions {
public:
	ParsedOptions();
	~ParsedOptions();
	bool        empty()                        const { return parsed_.empty(); }
	std::size_t size()                         const { return parsed_.size(); }
	std::size_t count(const std::string& name) const { return parsed_.count(name); }
	void        add(const std::string& name) { parsed_.insert(name); }

	//! Assigns the parsed values in p to their options.
	/*!
	 * Parsed values for options that already have a value (and are
	 * not composing) are ignored. On the other hand, parsed values
	 * overwrite any existing default values.
	 *
	 * \param p parsed values to assign
	 *
	 * \throw ValueError if p contains more than one value
	 *        for a non-composing option or if p contains a value that is
	 *        invalid for its option.
	 */
	bool        assign(const ParsedValues& p, const ParsedOptions* exclude = 0);
private:
	std::set<std::string> parsed_;
	int assign(const Option& o, const std::string& value);
};

/*!
* Container of option-value-pairs representing values found by a parser.
*/
class ParsedValues {
public:
	typedef std::pair<SharedOptPtr, std::string> OptionAndValue;
	typedef std::vector<OptionAndValue> Values;
	typedef Values::const_iterator iterator;

	/*!
	* \param a_ctx The OptionContext for which this object stores raw-values.
	*/
	explicit ParsedValues(const OptionContext& a_ctx)
		: ctx(&a_ctx) {}
	const OptionContext* ctx;

	//! Adds a value for option opt.
	void add(const std::string& opt, const std::string& value);
	void add(const SharedOptPtr& opt, const std::string& value) {
		parsed_.push_back(OptionAndValue(opt, value));
	}

	iterator begin() const { return parsed_.begin(); }
	iterator end()   const { return parsed_.end(); }

	void clear() { parsed_.clear(); }
private:
	Values parsed_;
};

class ParseContext {
public:
	typedef OptionContext::FindType FindType;
	virtual ~ParseContext();
	virtual SharedOptPtr getOption(const char* name, FindType ft) = 0;
	virtual SharedOptPtr getOption(int posKey, const char* tok) = 0;
	virtual void         addValue(const SharedOptPtr& key, const std::string& value) = 0;
};

//! Base class for options parsers.
class OptionParser {
public:
	typedef OptionContext::FindType FindType;
	explicit OptionParser(ParseContext& ctx);
	virtual ~OptionParser();
	ParseContext& parse();
protected:
	ParseContext& ctx() const { return *ctx_; }
	SharedOptPtr  getOption(const char* name, FindType ft) const { return ctx_->getOption(name, ft); }
	SharedOptPtr  getOption(int posKey, const char* tok)   const { return ctx_->getOption(posKey, tok); }
	void          addOptionValue(const SharedOptPtr& key, const std::string& value) { ctx_->addValue(key, value); }
private:
	virtual void doParse() = 0;
	ParseContext* ctx_;
};

//! Default formatting for options.
struct DefaultFormat {
	std::size_t format(std::vector<char>&, const OptionContext&) { return 0; }
	//! Writes g.caption() to buffer.
	std::size_t format(std::vector<char>& buffer, const OptionGroup& g);
	//! Writes long name, short name, and argument name to buffer.
	std::size_t format(std::vector<char>& buffer, const Option& o, std::size_t maxW);
	//! Writes description to buffer.
	/*!
	 * Occurrences of %D, %I and %A in desc are replaced with
	 * the value's default value, implicit value, and name, respectively.
	 */
	std::size_t format(std::vector<char>& buffer, const char* desc, const Value&, std::size_t maxW);
};

//! Base class for printing options.
class OptionOutput {
public:
	OptionOutput() {}
	virtual ~OptionOutput() {}
	virtual bool printContext(const OptionContext& ctx) = 0;
	virtual bool printGroup(const OptionGroup& group) = 0;
	virtual bool printOption(const Option& opt, std::size_t maxW) = 0;
};

//! Implementation class for printing options.
template <class Writer, class Formatter = DefaultFormat>
class OptionOutputImpl : public OptionOutput {
public:
	OptionOutputImpl(const Writer& w = Writer(), const Formatter& form = Formatter())
		: writer_(w)
		, formatter_(form) {}
	bool printContext(const OptionContext& ctx) {
		writer_.write(buffer_, formatter_.format(buffer_, ctx));
		return true;
	}
	bool printGroup(const OptionGroup& group) {
		writer_.write(buffer_, formatter_.format(buffer_, group));
		return true;
	}
	bool printOption(const Option& opt, std::size_t maxW) {
		writer_.write(buffer_, formatter_.format(buffer_, opt, maxW));
		writer_.write(buffer_, formatter_.format(buffer_, opt.description(), *opt.value(), maxW));
		return true;
	}
private:
	std::vector<char> buffer_;
	Writer            writer_;
	Formatter         formatter_;
};
//! Writes formatted option descriptions to an std::ostream.
struct OstreamWriter {
	OstreamWriter(std::ostream& os) : out(os) {}
	void write(const std::vector<char>& buf, std::size_t num);
	std::ostream& out;
private: void operator=(const OstreamWriter&);
};
//! Writes formatted option descriptions to an std::string.
struct StringWriter {
	StringWriter(std::string& str) : out(str) {}
	void write(const std::vector<char>& buf, std::size_t num);
	std::string& out;
private: void operator=(const StringWriter&);
};
//! Writes formatted option descriptions to a FILE.
struct FileWriter {
	FileWriter(FILE* f) : out(f) {}
	void write(const std::vector<char>& buf, std::size_t num);
	FILE* out;
};
typedef OptionOutputImpl<OstreamWriter> StreamOut;
typedef OptionOutputImpl<StringWriter>  StringOut;
typedef OptionOutputImpl<FileWriter>    FileOut;
///////////////////////////////////////////////////////////////////////////////
// parse functions
///////////////////////////////////////////////////////////////////////////////
/*!
 * A function type that is used by parsers for processing tokens that
 * have no option name. Concrete functions shall either return true
 * and store the name of the option that should receive the token as value
 * in its second argument or return false to signal an error.
 */
typedef bool(*PosOption)(const std::string&, std::string&);

enum CommandLineFlags {
	command_line_allow_flag_value = 1u
};

/*!
* Parses the command line starting at index 1 and removes
* all found options from argv.
* \param argc nr of arguments in argv
* \param argv the command line arguments
* \param ctx options to search in the command line.
* \param allowUnregistered Allow arguments that match no option in ctx
* \param posParser parse function for positional options
*
* \return A ParsedOptions-Object containing names and values for all options found.
*
* \throw SyntaxError if command line syntax is incorrect.
* \throw UnknownOption if allowUnregistered is false and an argument is found
* that does not match any option.
*/
ParsedValues parseCommandLine(int& argc, char** argv, const OptionContext& ctx,
	bool allowUnregistered = true,
	PosOption posParser = 0, unsigned flags = 0);

ParseContext& parseCommandLine(int& argc, char** argv, ParseContext& ctx, unsigned flags = 0);

/*!
* Parses the command arguments given in the array args.
* \param args  the arguments to parse
* \param nArgs number of arguments in args.
* \param ctx options to search in the arguments
* \param allowUnregistered Allow arguments that match no option in ctx
* \param posParser parse function for positional options
*
* \return A ParsedOptions-Object containing names and values for all options found.
*
* \throw SyntaxError if argument syntax is incorrect.
* \throw UnknownOption if allowUnregistered is false and an argument is found
* that does not match any option.
*/
ParsedValues parseCommandArray(const char* const args[], unsigned nArgs, const OptionContext& ctx,
	bool allowUnregistered = true,
	PosOption posParser = 0, unsigned flags = 0);

/*!
* Parses the command line given in the first parameter.
* \param cmd command line to parse
* \param ctx options to search in the command string.
* \param allowUnregistered Allow arguments that match no option in ctx
* \param posParser parse function for positional options
*
* \return A ParsedOptions-Object containing names and values for all options found.
*
* \throw SyntaxError if command line syntax is incorrect.
* \throw UnknownOption if an argument is found that does not match any option.
*/
ParsedValues parseCommandString(const std::string& cmd, const OptionContext& ctx, bool allowUnreg = false, PosOption posParser = 0, unsigned flags = command_line_allow_flag_value);
ParseContext& parseCommandString(const char* cmd, ParseContext& ctx, unsigned flags = command_line_allow_flag_value);

/*!
* Parses a config file having the format key = value.
* \param is the stream representing the config file
* \param o options to search in the config file
* \param allowUnregistered Allow arguments that match no option in ctx
*
* \return A ParsedOptions-Object containing names and values for all options found.
*
* \throw SyntaxError if command line syntax is incorrect.
* \throw UnknownOption if an argument is found that does not match any option.
* \note Keys are option's long names.
* \note Lines starting with # are treated as comments and are ignored.
*/
ParsedValues parseCfgFile(std::istream& is, const OptionContext& o, bool allowUnregistered);

} // namespace ProgramOptions
} // namespace Potassco

#endif
