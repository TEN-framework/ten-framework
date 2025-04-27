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
#ifndef CLASP_LITERAL_H_INCLUDED
#define CLASP_LITERAL_H_INCLUDED

#ifdef _MSC_VER
#pragma once
#endif
#include <clasp/config.h>
#include <clasp/pod_vector.h>
#include <algorithm>  // std::swap
#include <limits>
/*!
 * \file
 * \brief Types and functions related to literals and variables.
 */
namespace Clasp {
/*!
 * \addtogroup constraint
 */
//@{

//! A variable is an integer in the range [0..varMax).
typedef uint32 Var;

//! varMax is not a valid variale, i.e. currently Clasp supports at most 2^30 variables.
const Var varMax = (Var(1) << 30);

//! The variable 0 has a special meaning in the solver.
const Var sentVar = 0;

//! Literal ids are in the range [0..litIdMax).
const uint32 litIdMax = (Var(1) << 31);

//! Possible types of a variable.
struct Var_t {
	enum Type { Atom = 1, Body = 2, Hybrid = Atom | Body};
	static bool isBody(Type t) {
		return (static_cast<uint32>(t) & static_cast<uint32>(Body)) != 0;
	}
	static bool isAtom(Type t) {
		return (static_cast<uint32>(t) & static_cast<uint32>(Atom)) != 0;
	}
};
typedef Var_t::Type VarType;

//! A literal is a variable or its negation.
/*!
 * \par Implementation:
 * A literal's state is stored in a single 32-bit integer as follows:
 *  - 30-bits   : variable id
 *  - 1-bit     : sign, 1 if negative, 0 if positive
 *  - 1-bit     : general purpose flag for marking a literal instance
 */
class Literal {
public:
	static const uint32 sign_bit = 2u;
	static const uint32 flag_bit = 1u;

	//! The default constructor creates the positive literal of the special sentinel var.
	Literal() : rep_(0) { }

	//! Creates a literal of the variable var with sign s.
	/*!
	 * \param var  The literal's variable.
	 * \param sign true if new literal should be negative.
	 * \pre var < varMax
	 */
	Literal(Var var, bool sign) : rep_( (var<<sign_bit) + (uint32(sign)<<flag_bit) ) {
		assert( var < varMax );
	}

	//! Returns the variable of the literal.
	Var var() const { return rep_ >> sign_bit; }

	//! Returns the sign of the literal.
	/*!
	 * \return true if the literal is negative. Otherwise false.
	 */
	bool sign() const { return (rep_ & sign_bit) != 0; }

	//! Returns var and sign encoded in a unique id.
	/*!
	 * \note The watch-flag is ignored and thus the id of a literal can be stored in 31-bits.
	 */
	uint32 id() const { return rep_ >> flag_bit; }

	//! Returns the stored representation of this literal.
	uint32& rep()       { return rep_; }
	uint32  rep() const { return rep_; }

	//! Creates a literal from an id.
	static Literal fromId(uint32 id) {
		assert(id < litIdMax);
		return Literal(id<<flag_bit);
	}
	//! Creates a literal from an unsigned integer.
	static Literal fromRep(uint32 rep) { return Literal(rep); }

	void swap(Literal& other) { std::swap(rep_, other.rep_); }

	//! Sets the watched-flag of this literal.
	Literal& flag() { rep_ |= flag_bit; return *this; }

	//! Clears the watched-flag of this literal.
	Literal& unflag() { rep_ &= ~flag_bit; return *this; }

	//! Returns true if the watched-flag of this literal is set.
	bool flagged() const { return (rep_ & flag_bit) != 0; }

	//! Returns the complimentary literal of this literal.
	/*!
	 *  The complementary Literal of a Literal is a Literal referring to the
	 *  same variable but with inverted sign.
	 */
	inline Literal operator~() const {
		return Literal( (rep_ ^ sign_bit) & ~flag_bit );
	}
private:
	Literal(uint32 rep) : rep_(rep) {}
	uint32 rep_;
};
//! Equality-Comparison for literals.
inline bool operator==(const Literal& lhs, const Literal& rhs) {
	return lhs.id() == rhs.id();
}
//! Defines a strict-weak-ordering for Literals.
inline bool operator<(const Literal& lhs, const Literal& rhs) {
	return lhs.id() < rhs.id();
}
//! Returns !(lhs == rhs).
inline bool operator!=(const Literal& lhs, const Literal& rhs) {
	return !(lhs == rhs);
}
inline Literal operator^(Literal lhs, bool sign) { return Literal::fromId( lhs.id() ^ uint32(sign) ); }
inline Literal operator^(bool sign, Literal rhs) { return rhs ^ sign; }
inline void swap(Literal& l, Literal& r) { l.swap(r); }
//! Creates the negative literal of variable v.
inline Literal negLit(Var v) { return Literal(v, true); }
//! Creates the positive literal of variable v.
inline Literal posLit(Var v) { return Literal(v, false); }
//! Returns negLit(abs(p)) if p < 0 and posLit(p) otherwise.
inline Literal toLit(int32 p) { return p < 0 ? negLit(static_cast<Var>(-p)) : posLit(static_cast<Var>(p)); }
//! Converts the given (non-sentinel) literal to a signed integer s.th. p == toLit(toInt(p)).
inline int32   toInt(Literal p) { return p.sign() ? -static_cast<int32>(p.var()) : static_cast<int32>(p.var()); }
//! Always-true literal.
// TODO: replace with constant using constexpr ctor once we switch to C++11
inline Literal lit_true() { return Literal(sentVar, false); }
//! Always-false literal.
// TODO: replace with constant using constexpr ctor once we switch to C++11
inline Literal lit_false() { return Literal(sentVar, true); }
//! Returns true if p represents the special variable 0
inline bool    isSentinel(Literal p) { return p.var() == sentVar; }

// Low-level conversion between Literals and int literals.
// We cannot use toInt() here because it is not defined for the
// sentinel literals lit_true() and lit_false().
inline int32   encodeLit(Literal x) { return !x.sign() ? static_cast<int32>(x.var()+1) : -static_cast<int32>(x.var()+1); }
inline Var     decodeVar(int32 x)   { return static_cast<Var>(x >= 0 ? x : -x) - 1; }
inline Literal decodeLit(int32 x)   { return Literal(decodeVar(x), x < 0); }

inline unsigned hashId(unsigned key) {
	key = ~key + (key << 15);
	key ^= (key >> 11);
	key += (key << 3);
	key ^= (key >> 5);
	key += (key << 10);
	key ^= (key >> 16);
	return key;
}
inline uint32 hashLit(Literal p) { return hashId(p.id()); }

//! A signed integer type used to represent weights.
typedef int32 weight_t;
//! A signed integer type used to represent sums of weights.
typedef int64 wsum_t;
#define CLASP_WEIGHT_T_MAX ( 2147483647)
#define CLASP_WEIGHT_T_MIN (-2147483647 - 1)
#define CLASP_WEIGHT_SUM_MAX INT64_MAX
#define CLASP_WEIGHT_SUM_MIN INT64_MIN

typedef PodVector<Var>::type      VarVec;    //!< A vector of variables.
typedef PodVector<Literal>::type  LitVec;    //!< A vector of literals.
typedef PodVector<weight_t>::type WeightVec; //!< A vector of weights.
typedef PodVector<wsum_t>::type   SumVec;    //!< A vector of sums of weights.
typedef std::pair<Literal, weight_t>   WeightLiteral; //!< A literal associated with a weight.
typedef PodVector<WeightLiteral>::type WeightLitVec;  //!< A vector of weight-literals.
///////////////////////////////////////////////////////////////////////////////
// Truth values
///////////////////////////////////////////////////////////////////////////////
typedef uint8 ValueRep;           //!< Type of the three value-literals.
const ValueRep value_true   = 1;  //!< Value used for variables that are true.
const ValueRep value_false  = 2;  //!< Value used for variables that are false.
const ValueRep value_free   = 0;  //!< Value used for variables that are unassigned.
typedef PodVector<ValueRep>::type ValueVec;

//! Returns the value that makes the literal lit true.
/*!
 * \param lit The literal for which the true-value should be determined.
 * \return
 *   - value_true  iff lit is a positive literal
 *   - value_false iff lit is a negative literal.
 *   .
 */
inline ValueRep trueValue(const Literal& lit) { return 1 + lit.sign(); }

//! Returns the value that makes the literal lit false.
/*!
 * \param lit The literal for which the false-value should be determined.
 * \return
 *   - value_false iff lit is a positive literal
 *   - value_true  iff lit is a negative literal.
 *   .
 */
inline ValueRep falseValue(const Literal& lit) { return 1 + !lit.sign(); }

//! Returns the sign that matches the value.
/*!
 * \return
 *   - false iff v == value_true
 *   - true  otherwise
 */
inline bool valSign(ValueRep v) { return v != value_true; }
//@}
}
#endif
