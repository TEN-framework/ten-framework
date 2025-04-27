//
// Copyright (c) 2006-2017 Benjamin Kaufmann
//
// This file is part of Clasp. See http://www.cs.uni-potsdam.de/clasp/
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
#ifndef CLASP_CLI_CLASP_OPTIONS_H_INCLUDED
#define CLASP_CLI_CLASP_OPTIONS_H_INCLUDED

#ifdef _MSC_VER
#pragma once
#endif

#include <clasp/clasp_facade.h>
#include <string>
#include <iosfwd>
namespace Potassco { namespace ProgramOptions {
class OptionContext;
class OptionGroup;
class ParsedOptions;
}}
/*!
 * \file
 * \brief Types and functions for processing command-line options.
 */
namespace Clasp {
//! Namespace for types and functions used by the command-line interface.
namespace Cli {

/**
 * \defgroup cli Cli
 * \brief Types mainly relevant to the command-line interface.
 * \ingroup facade
 * @{
 */

class ClaspCliConfig;
//! Class for iterating over a set of configurations.
class ConfigIter {
public:
	const char* name() const;
	const char* base() const;
	const char* args() const;
	bool        valid()const;
	bool        next();
private:
	friend class ClaspCliConfig;
	ConfigIter(const char* x);
	const char* base_;
};
//! Valid configuration keys.
/*!
 * \see clasp_cli_configs.inl
 */
enum ConfigKey {
#define CONFIG(id,k,c,s,p) config_##k,
#define CLASP_CLI_DEFAULT_CONFIGS config_default = 0,
#define CLASP_CLI_AUX_CONFIGS     config_default_max_value,
#include <clasp/cli/clasp_cli_configs.inl>
	config_aux_max_value,
	config_many, // default portfolio
	config_max_value,
	config_asp_default   = config_tweety,
	config_sat_default   = config_trendy,
	config_tester_default= config_tester,
};
/*!
 * \brief Class for storing/processing command-line options.
 *
 * Caveats (when using incrementally, e.g. from clingo):
 * - supp-models: State Transition (yes<->no) not supported.
 *     - supp-models=yes is irreversible for a step
 *       because it enables possibly destructive simplifications
 *       and skips SCC-checking (i.e. new SCCs are silently discarded).
 *     - Nogoods learnt during supp-models=no are not tagged and
 *       hence can't simply be removed on transition to yes.
 *     .
 * - stats: Stats level can only be increased.
 *     - A stats level once activated stays activated even if
 *       level is subsequently decreased via option.
 *     .
 * - save-progress, sign-fix, opt-heuristic: No unset of previously set values.
 *     - Once set, signs are only unset if forgetOnStep includes sign.
 *     .
 * - no-lookback: State Transition (yes<->no) not supported.
 *     - noLookback=yes is a destructive meta-option that disables lookback-options by changing their value
 *     - noLookback=no does not re-enable those options.
 *     .
 */
class ClaspCliConfig : public ClaspConfig {
public:
	//! Returns defaults for the given problem type.
	static const char* getDefaults(ProblemType f);
	//! Returns the configuration with the given key.
	static ConfigIter  getConfig(ConfigKey key);
	//! Returns the ConfigKey of k or -1 if k is not a known configuration.
	static int         getConfigKey(const char* k);

	ClaspCliConfig();
	~ClaspCliConfig();
	// Base interface
	virtual void prepare(SharedContext&);
	virtual void reset();
	virtual Configuration* config(const char*);

	/*!
	 * \name Key-based low-level interface
	 *
	 * The functions in this group do not throw exceptions but
	 * signal logic errors via return values < 0.
	 * @{ */

	typedef uint32 KeyType;
	static const KeyType KEY_INVALID; //!< Invalid key used to signal errors.
	static const KeyType KEY_ROOT;    //!< Root key of a configuration, i.e. "."
	static const KeyType KEY_TESTER;  //!< Root key for tester options, i.e. "tester."
	static const KeyType KEY_SOLVER;  //!< Root key for (array of) solver options, i.e. "solver."

	//! Returns true if k is a leaf, i.e. has no subkeys.
	static bool  isLeafKey(KeyType k);

	//! Retrieves a handle to the specified key.
	/*!
	 * \param key A valid handle to a key.
	 * \param name The name of the subkey to retrieve.
	 * \return
	 *   - key, if name is 0 or empty.
	 *   - KEY_INVALID, if name is not a subkey of key.
	 *   - A handle to the subkey.
	 *   .
	 */
	KeyType getKey(KeyType key, const char* name = 0) const;

	//! Retrieves a handle to the specified element of the given array key.
	/*!
	 * \param arr     A valid handle to an array.
	 * \param element The index of the element to retrieve.
	 * \return
	 *   - A handle to the requested element, or
	 *   - KEY_INVALID, if arr does not reference an array or element is out of bounds.
	 *   .
	 */
	KeyType getArrKey(KeyType arr, unsigned element) const;

	//! Retrieves information about the specified key.
	/*!
	 * \param key  A valid handle to a key.
	 * \param[out] nSubkeys The number of subkeys of this key or 0 if key is a leaf.
	 * \param[out] arrLen   If key is an array, the length of the array (can be 0). Otherwise, -1.
	 * \param[out] help     A description of the key.
	 * \param[out] nValues  The number of values the key currently has (0 or 1) or -1 if it can't have values.
	 * \note All out parameters are optional, i.e. can be 0.
	 * \return The number of out values or -1 if key is invalid.
	 */
	int getKeyInfo(KeyType key, int* nSubkeys = 0, int* arrLen = 0, const char** help = 0, int* nValues = 0) const;

	//! Returns the name of the i'th subkey of k or 0 if no such subkey exists.
	const char* getSubkey(KeyType k, uint32 i) const;

	//! Creates and returns a string representation of the value of the given key.
	/*!
	 * \param k  A valid handle to a key.
	 * \param[out] value The current value of the key.
	 * \return The length of value or < 0 if k either has no value (-1) or an error occurred while writing the value (< -1).
	 */
	int getValue(KeyType k, std::string& value) const;

	//! Writes a null-terminated string representation of the value of the given key into the supplied buffer.
	/*!
	 * \param k A valid handle to a key.
	 * \param[out] buffer The current value of the key.
	 * \param bufSize The size of buffer.
	 * \note Although the number returned can be larger than the bufSize, the function
	 *   never writes more than bufSize bytes into the buffer.
	 */
	int getValue(KeyType k, char* buffer, std::size_t bufSize) const;

	//! Sets the option identified by the given key.
	/*!
	 * \param key A valid handle to a key.
	 * \param value The value to set.
	 * \return
	 *   - > 0: if the value was set.
	 *   - = 0: if value is not a valid value for the given key.
	 *   - < 0: f key does not accept a value (-1), or some error occurred (< -1).
	 *   .
	 */
	int setValue(KeyType key, const char* value);

	//@}

	/*!
	 * \name String-based interface
	 *
	 * The functions in this group wrap the key-based functions and
	 * signal logic errors by throwing exceptions.
	 * @{ */
	//! Returns the value of the option identified by the given key.
	std::string getValue(const char* key) const;
	//! Returns true if the given key has an associated value.
	bool        hasValue(const char* key) const;
	//! Sets the option identified by the given key.
	bool        setValue(const char* key, const char* value);
	//@}

	//! Validates this configuration.
	bool        validate();

	/*!
	 * \name App interface
	 *
	 * Functions for connecting a configuration with the ProgramOptions library.
	 * @{ */
	//! Adds all available options to root.
	/*!
	 * Once options are added, root can be used with an option source (e.g. the command-line)
	 * to populate this object.
	 */
	void addOptions(Potassco::ProgramOptions::OptionContext& root);
	//! Adds options that are disabled by the options contained in parsed to parsed.
	void addDisabled(Potassco::ProgramOptions::ParsedOptions& parsed);
	//! Applies the options in parsed and finalizes and validates this configuration.
	bool finalize(const Potassco::ProgramOptions::ParsedOptions& parsed, ProblemType type, bool applyDefaults);
	//! Populates this configuration with the options given in [first, last) and finalizes it.
	/*!
	 * \param [first, last) a range of options in argv format.
	 * \param t Problem type for which this configuration is created. Used to set defaults.
	 */
	template <class IT>
	bool setConfig(IT first, IT last, ProblemType t) {
		RawConfig config("setConfig");
		while (first != last) { config.addArg(*first++); }
		return setAppConfig(config, t);
	}
	//! Releases internal option objects needed for command-line style option processing.
	/*!
	 * \note Subsequent calls to certain functions of this object (e.g. addOptions(), setConfig())
	 *       recreate the option objects if necessary.
	 */
	void releaseOptions();
	//@}
private:
	static const uint8 mode_solver = 1u;
	static const uint8 mode_tester = 2u;
	static const uint8 mode_relaxed= 4u;
	struct ParseContext;
	struct OptIndex;
	class  ProgOption;
	typedef Potassco::ProgramOptions::OptionContext OptionContext;
	typedef Potassco::ProgramOptions::OptionGroup   Options;
	typedef SingleOwnerPtr<Options>                 OptionsPtr;
	typedef PodVector<std::string>::type            ConfigVec;
	typedef Potassco::ProgramOptions::ParsedOptions ParsedOpts;
	struct ScopedSet {
		ScopedSet(ClaspCliConfig& s, uint8 mode, uint32 sId = 0);
		~ScopedSet();
		ClaspCliConfig* operator->()const { return self; }
		ClaspCliConfig* self;
	};
	struct RawConfig {
		std::string raw;
		explicit RawConfig(const char* name);
		void addArg(const char* arg);
		void addArg(const std::string& arg);
		ConfigIter iterator() const { return ConfigIter(raw.data()); }
	};
	// Operations on active config and solver
	int  setActive(int o, const char* value);
	int  getActive(int o, std::string* value, const char** desc, const char** opt) const;
	int  applyActive(int o, const char* setValue, std::string* getValue, const char** getDesc, const char** name);
	// App interface impl
	bool setAppConfig(const RawConfig& c, ProblemType t);
	int  setAppOpt(int o, const char* value);
	bool setAppDefaults(UserConfig* active, uint32 sId, const ParsedOpts& exclude, ProblemType t);
	bool finalizeAppConfig(UserConfig* active, const ParsedOpts& exclude, ProblemType t, bool defs);
	const ParsedOpts& finalizeParsed(UserConfig* active, const ParsedOpts& parsed, ParsedOpts& exclude) const;
	void              createOptions();
	ProgOption*       createOption(int o);
	bool assignDefaults(const ParsedOpts&);
	// Configurations
	static bool       appendConfig(std::string& to, const std::string& line);
	static bool       loadConfig(std::string& to, const char* fileName);
	ConfigIter        getConfig(uint8 key, std::string& tempMem);
	bool              setConfig(const ConfigIter& it, bool allowAppOpt, const ParsedOpts& exclude, ParsedOpts* out);
	// helpers
	bool             isGenerator() const { return (cliMode & mode_tester) == 0; }
	const UserConfig*active()const       { return isGenerator() ? this : testerConfig(); }
	UserConfig*      active()            { return isGenerator() ? this : testerConfig(); }
	bool             match(const char*& path, const char* what) const;
	static OptIndex  index_g;
	OptionsPtr       opts_;
	std::string      config_[2];
	bool             initTester_;
};
//! Validates the given solver configuration and returns an error string if invalid.
const char* validate(const SolverParams& solver, const SolveParams& search);
//@}

}}
#endif
