/*
 ASL expression classes

 Copyright (C) 2012 AMPL Optimization Inc

 Permission to use, copy, modify, and distribute this software and its
 documentation for any purpose and without fee is hereby granted,
 provided that the above copyright notice appear in all copies and that
 both that the copyright notice and this permission notice and warranty
 disclaimer appear in supporting documentation.

 The author and AMPL Optimization Inc disclaim all warranties with
 regard to this software, including all implied warranties of
 merchantability and fitness.  In no event shall the author be liable
 for any special, indirect or consequential damages or any damages
 whatsoever resulting from loss of use, data or profits, whether in an
 action of contract, negligence or other tortious action, arising out
 of or in connection with the use or performance of this software.

 Author: Victor Zverovich
 */

#ifndef MP_ASLEXPR_H_
#define MP_ASLEXPR_H_

#ifdef MP_USE_UNORDERED_MAP
# include <unordered_map>
#endif

#include <cassert>
#include <cstddef>
#include <algorithm>
#include <iterator>
#include <map>
#include <string>
#include <utility>
#include <vector>

extern "C" {
#include "solvers/nlp.h"
}

// Undefine ASL macros because they often clash with names used in solver
// libraries.
#undef solve_code
#undef var_name
#undef con_name
#undef filename
#undef ampl_vbtol
#undef strtod
#ifndef ASL_PRESERVE_DEFINES
# undef Char
# undef A_colstarts
# undef A_rownos
# undef A_vals
# undef Cgrad
# undef Fortran
# undef LUrhs
# undef LUv
# undef Lastx
# undef Ograd
# undef Urhsx
# undef Uvx
# undef X0
# undef adjoints
# undef adjoints_nv1
# undef amax
# undef ampl_options
# undef amplflag
# undef archan
# undef awchan
# undef binary_nl
# undef c_cexp1st
# undef c_vars
# undef co_index
# undef comb
# undef combc
# undef comc
# undef comc1
# undef como
# undef como1
# undef cv_index
# undef cvar
# undef err_jmp
# undef err_jmp1
# undef fhash
# undef funcs
# undef funcsfirst
# undef funcslast
# undef havepi0
# undef havex0
# undef lnc
# undef maxcolnamelen
# undef maxrownamelen
# undef n_cc
# undef n_con
# undef n_conjac
# undef n_obj
# undef n_var
# undef nbv
# undef nclcon
# undef ncom0
# undef ncom1
# undef nderps
# undef need_nl
# undef n_eqn
# undef nfunc
# undef niv
# undef nlc
# undef nlcc
# undef nlnc
# undef nlo
# undef n_lcon
# undef nlogv
# undef nlvb
# undef nlvbi
# undef nlvc
# undef nlvci
# undef nlvo
# undef nlvoi
# undef nranges
# undef nwv
# undef nzc
# undef nzjac
# undef nzo
# undef o_cexp1st
# undef o_vars
# undef obj_no
# undef objtype
# undef pi0
# undef plterms
# undef real
# undef return_nofile
# undef size_expr_n
# undef skip_int_derivs
# undef sputinfo
# undef stub_end
# undef want_deriv
# undef want_xpi0
# undef x0kind
# undef x0len
# undef xscanf
# undef zaC
# undef zac
# undef zao
# undef zerograds
# undef write_sol
#endif  // ASL_PRESERVE_DEFINES

#include "mp/error.h"
#include "mp/problem-base.h"

namespace mp {

class Problem;

namespace asl {
class Expr;
class NumericExpr;
class LogicalExpr;
}
}

#ifdef MP_USE_UNORDERED_MAP
namespace std {
template <>
struct hash<mp::asl::NumericExpr> {
  std::size_t operator()(mp::asl::NumericExpr e) const;
};
template <>
struct hash<mp::asl::LogicalExpr> {
  std::size_t operator()(mp::asl::LogicalExpr e) const;
};
}
#endif

namespace mp {

namespace internal {
// Casts expression to type ExprType.
// If assertions are enabled, it generates an assertion failure when
// e is not of runtime type ExprType. Otherwise no runtime check is
// performed.
template <typename ExprType>
ExprType Cast(asl::Expr e);
}

namespace asl {

namespace internal {

class ASLBuilder;

// Returns true if the non-null expression e is of type ExprT.
template <typename ExprT>
bool Is(expr::Kind k);
}

// Specialize Is<ExprT> for the class ExprClass corresponding to a single
// expression kind.
#define MP_SPECIALIZE_IS(ExprT, expr_kind) \
namespace internal { \
template <> \
inline bool Is<ExprT>(expr::Kind k) { return k == expr::expr_kind; } \
}

// Specialize Is<ExprT> for the class ExprClass corresponding to a range
// of expression kinds [start, end].
#define MP_SPECIALIZE_IS_RANGE(ExprT, expr_kind) \
namespace internal { \
template <> \
inline bool Is<ExprT>(expr::Kind k) { \
  return k >= expr::FIRST_##expr_kind && k <= expr::LAST_##expr_kind; \
} \
}

// An expression.
// An Expr object represents a reference to an expression so
// it is cheap to construct and pass by value. A type safe way to
// process expressions of different types is by using ExprVisitor.
class Expr {
 private:
  void True() const {}
  typedef void (Expr::*SafeBool)() const;

  template <typename Impl, typename Result, typename LResult>
  friend class ExprConverter;

  friend class internal::ASLBuilder;
  friend class mp::Problem;

  template <typename ExprType>
  friend ExprType mp::internal::Cast(asl::Expr e);

  // Constructs an Expr object representing a reference to an AMPL
  // expression e. Only a minimal check is performed when assertions are
  // enabled to make sure that the opcode is within the valid range.
  explicit Expr(::expr *e) : expr_(e) {
    assert(!expr_ || (kind() >= expr::FIRST_EXPR && kind() <= expr::LAST_EXPR));
  }

 protected:
  ::expr *expr_;

  // Creates an expression object from a raw expr pointer.
  // For safety reason expression classes don't provide constructors
  // taking raw pointers and this method should be used instead.
  template <typename ExprType>
  static ExprType Create(::expr *e) {
    assert(!e || asl::internal::Is<ExprType>(Expr(e).kind()));
    ExprType expr;
    expr.expr_ = e;
    return expr;
  }

  // An expression proxy used for implementing operator-> in iterators.
  template <typename ExprT>
  class Proxy {
   private:
    ExprT expr_;

   public:
    explicit Proxy(::expr *e) : expr_(Create<ExprT>(e)) {}

    const ExprT *operator->() const { return &expr_; }
  };

  // An expression array iterator.
  template <typename ExprT>
  class ArrayIterator :
    public std::iterator<std::forward_iterator_tag, ExprT> {
   private:
    ::expr *const *ptr_;

   public:
    explicit ArrayIterator(::expr *const *p = 0) : ptr_(p) {}

    ExprT operator*() const { return Create<ExprT>(*ptr_); }

    Proxy<ExprT> operator->() const {
      return Proxy<ExprT>(*ptr_);
    }

    ArrayIterator &operator++() {
      ++ptr_;
      return *this;
    }

    ArrayIterator operator++(int ) {
      ArrayIterator it(*this);
      ++ptr_;
      return it;
    }

    bool operator==(ArrayIterator other) const { return ptr_ == other.ptr_; }
    bool operator!=(ArrayIterator other) const { return ptr_ != other.ptr_; }
  };

 public:
  // Constructs an Expr object representing a null reference to an AMPL
  // expression. The only operation permitted for such expression is
  // copying, assignment and check whether it is null using operator SafeBool.
  Expr() : expr_() {}

  // Returns a value convertible to bool that can be used in conditions but not
  // in comparisons and evaluates to "true" if this expression is not null
  // and "false" otherwise.
  // Example:
  //   void foo(Expr e) {
  //     if (e) {
  //       // Do something if e is not null.
  //     }
  //   }
  operator SafeBool() const { return expr_ ? &Expr::True : 0; }

  // Returns the expression kind.
  expr::Kind kind() const {
    return expr::GetOpCodeInfo(reinterpret_cast<std::size_t>(expr_->op)).kind;
  }

  // Returns the function name or operator for this expression as a
  // string. Expressions with different opcodes can have identical
  // strings. For example, OPPOW, OP1POW and OPCPOW all use the
  // same operator "^".
  const char *opstr() const { return str(kind()); }

  int precedence() const { return mp::internal::precedence(kind()); }

  bool operator==(Expr other) const { return expr_ == other.expr_; }
  bool operator!=(Expr other) const { return expr_ != other.expr_; }

  template <typename ExprT>
  friend ExprT Cast(Expr e);
};

namespace internal {
template <typename ExprT>
inline bool Is(Expr e) { return Is<ExprT>(e.kind()); }
}

// Casts an expression to type ExprT. Returns a null expression if the cast
// is not possible.
template <typename ExprT>
ExprT Cast(Expr e) {
  return internal::Is<ExprT>(e) ? mp::internal::Cast<ExprT>(e) : ExprT();
}

MP_SPECIALIZE_IS_RANGE(Expr, EXPR)

// A numeric expression.
class NumericExpr : public Expr {
 public:
  NumericExpr() {}
};

MP_SPECIALIZE_IS_RANGE(NumericExpr, NUMERIC)

// A logical or constraint expression.
class LogicalExpr : public Expr {
 public:
  LogicalExpr() {}
};

MP_SPECIALIZE_IS_RANGE(LogicalExpr, LOGICAL)

// A numeric constant.
// Examples: 42, -1.23e-4
class NumericConstant : public NumericExpr {
 public:
  NumericConstant() {}

  // Returns the value of this number.
  double value() const { return reinterpret_cast<expr_n*>(expr_)->v; }
};

MP_SPECIALIZE_IS(NumericConstant, CONSTANT)

// A reference to a variable.
// Example: x
class Variable : public NumericExpr {
 public:
  Variable() {}

  // Returns the index of the referenced variable.
  int index() const { return expr_->a; }
};

MP_SPECIALIZE_IS(Variable, VARIABLE)

template <typename Base>
class BasicUnaryExpr : public Base {
 public:
  BasicUnaryExpr() {}

  // Returns the argument of this expression.
  Base arg() const { return Expr::Create<Base>(this->expr_->L.e); }
};

// A unary numeric expression.
// Examples: -x, sin(x), where x is a variable.
typedef BasicUnaryExpr<NumericExpr> UnaryExpr;
MP_SPECIALIZE_IS_RANGE(UnaryExpr, UNARY)

// A binary expression.
// Base: base expression class.
// Arg: argument expression class.
template <typename Base, typename Arg = Base>
class BasicBinaryExpr : public Base {
 public:
  BasicBinaryExpr() {}

  // Returns the left-hand side (the first argument) of this expression.
  Arg lhs() const { return Expr::Create<Arg>(this->expr_->L.e); }

  // Returns the right-hand side (the second argument) of this expression.
  Arg rhs() const { return Expr::Create<Arg>(this->expr_->R.e); }
};

// A binary numeric expression.
// Examples: x / y, atan2(x, y), where x and y are variables.
typedef BasicBinaryExpr<NumericExpr> BinaryExpr;
MP_SPECIALIZE_IS_RANGE(BinaryExpr, BINARY)

template <typename Base>
class BasicIfExpr : public Base {
 public:
  BasicIfExpr() {}

  LogicalExpr condition() const {
    return Expr::Create<LogicalExpr>(
        reinterpret_cast<expr_if*>(this->expr_)->e);
  }

  Base true_expr() const {
    return Expr::Create<Base>(reinterpret_cast<expr_if*>(this->expr_)->T);
  }

  Base false_expr() const {
    return Expr::Create<Base>(reinterpret_cast<expr_if*>(this->expr_)->F);
  }
};

// An if-then-else expression.
// Example: if x != 0 then y else z, where x, y and z are variables.
typedef BasicIfExpr<NumericExpr> IfExpr;
MP_SPECIALIZE_IS(IfExpr, IF)

// A piecewise-linear expression.
// Example: <<0; -1, 1>> x, where x is a variable.
class PiecewiseLinearExpr : public NumericExpr {
 public:
  PiecewiseLinearExpr() {}

  // Returns the number of breakpoints in this term.
  int num_breakpoints() const {
    return num_slopes() - 1;
  }

  // Returns the number of slopes in this term.
  int num_slopes() const {
    assert(expr_->L.p->n >= 1);
    return expr_->L.p->n;
  }

  // Returns a breakpoint with the specified index.
  double breakpoint(int index) const {
    assert(index >= 0 && index < num_breakpoints());
    return expr_->L.p->bs[2 * index + 1];
  }

  // Returns a slope with the specified index.
  double slope(int index) const {
    assert(index >= 0 && index < num_slopes());
    return expr_->L.p->bs[2 * index];
  }

  int var_index() const {
    return reinterpret_cast<expr_v*>(expr_->R.e)->a;
  }
};

MP_SPECIALIZE_IS(PiecewiseLinearExpr, PLTERM)

class Function {
 private:
  func_info *fi_;

  friend class CallExpr;
  friend class internal::ASLBuilder;

  explicit Function(func_info *fi) : fi_(fi) {}

  void True() const {}
  typedef void (Function::*SafeBool)() const;

 public:
  Function() : fi_(0) {}

  // Returns the function name.
  const char *name() const { return fi_->name; }

  // Returns the number of arguments.
  int num_args() const { return fi_->nargs; }

  // Returns a value convertible to bool that can be used in conditions but not
  // in comparisons and evaluates to "true" if this function is not null
  // and "false" otherwise.
  // Example:
  //   void foo(Function f) {
  //     if (f) {
  //       // Do something if e is not null.
  //     }
  //   }
  operator SafeBool() const { return fi_ ? &Function::True : 0; }

  bool operator==(const Function &other) const { return fi_ == other.fi_; }
  bool operator!=(const Function &other) const { return fi_ != other.fi_; }
};

// A function call expression.
// Example: f(x), where f is a function and x is a variable.
class CallExpr : public NumericExpr {
 public:
  CallExpr() {}

  Function function() const {
    return Function(reinterpret_cast<expr_f*>(expr_)->fi);
  }

  int num_args() const { return reinterpret_cast<expr_f*>(expr_)->al->n; }

  Expr operator[](int index) {
    assert(index >= 0 && index < num_args());
    return Create<Expr>(reinterpret_cast<expr_f*>(expr_)->args[index]);
  }

  // An argument iterator.
  typedef ArrayIterator<Expr> iterator;

  iterator begin() const {
    return iterator(reinterpret_cast<expr_f*>(expr_)->args);
  }

  iterator end() const {
    return iterator(reinterpret_cast<expr_f*>(expr_)->args + num_args());
  }
};

MP_SPECIALIZE_IS(CallExpr, CALL)

// A numeric expression with a variable number of arguments.
// The min and max functions always have at least one argument.
// Example: min{i in I} x[i], where I is a set and x is a variable.
class VarArgExpr : public NumericExpr {
 private:
  static const de END;

 public:
  VarArgExpr() {}

  // An argument iterator.
  class iterator :
    public std::iterator<std::forward_iterator_tag, NumericExpr> {
   private:
    const de *de_;

    friend class VarArgExpr;

    explicit iterator(const de *d) : de_(d) {}

   public:
    iterator() : de_(&END) {}

    NumericExpr operator*() const { return Create<NumericExpr>(de_->e); }

    Proxy<NumericExpr> operator->() const {
      return Proxy<NumericExpr>(de_->e);
    }

    iterator &operator++() {
      ++de_;
      return *this;
    }

    iterator operator++(int ) {
      iterator it(*this);
      ++de_;
      return it;
    }

    bool operator==(iterator other) const { return de_->e == other.de_->e; }
    bool operator!=(iterator other) const { return de_->e != other.de_->e; }
  };

  iterator begin() const {
    return iterator(reinterpret_cast<expr_va*>(expr_)->L.d);
  }

  iterator end() const {
    return iterator();
  }
};

MP_SPECIALIZE_IS_RANGE(VarArgExpr, VARARG)

// An iterated expression.
// ID is used to distinguish between expression types that have the same
// base and argument type, namely SumExpr and NumberOfExpr.
template <typename BaseT, typename ArgT = BaseT, int ID = 0>
class BasicIteratedExpr : public BaseT {
 public:
  typedef ArgT Arg;

  BasicIteratedExpr() {}

  // Returns the number of arguments.
  int num_args() const {
    return static_cast<int>(this->expr_->R.ep - this->expr_->L.ep);
  }

  Arg operator[](int index) const {
    assert(index >= 0 && index < num_args());
    return Expr::Create<Arg>(this->expr_->L.ep[index]);
  }

  typedef Expr::ArrayIterator<Arg> iterator;

  iterator begin() const { return iterator(this->expr_->L.ep); }
  iterator end() const { return iterator(this->expr_->R.ep); }
};

// A sum expression.
// Example: sum{i in I} x[i], where I is a set and x is a variable.
typedef BasicIteratedExpr<NumericExpr> SumExpr;
MP_SPECIALIZE_IS(SumExpr, SUM)

// A count expression.
// Example: count{i in I} (x[i] >= 0), where I is a set and x is a variable.
typedef BasicIteratedExpr<NumericExpr, LogicalExpr> CountExpr;
MP_SPECIALIZE_IS(CountExpr, COUNT)

// A numberof expression.
// Example: numberof 42 in ({i in I} x[i]),
// where I is a set and x is a variable.
typedef BasicIteratedExpr<NumericExpr, NumericExpr, 1> NumberOfExpr;
MP_SPECIALIZE_IS(NumberOfExpr, NUMBEROF)

// A logical constant.
// Examples: 0, 1
class LogicalConstant : public LogicalExpr {
 public:
  LogicalConstant() {}

  // Returns the value of this constant.
  bool value() const { return reinterpret_cast<expr_n*>(expr_)->v != 0; }
};

MP_SPECIALIZE_IS(LogicalConstant, CONSTANT)

// A logical NOT expression.
// Example: not a, where a is a logical expression.
typedef BasicUnaryExpr<LogicalExpr> NotExpr;
MP_SPECIALIZE_IS(NotExpr, NOT)

// A binary logical expression.
// Examples: a || b, a && b, where a and b are logical expressions.
typedef BasicBinaryExpr<LogicalExpr> BinaryLogicalExpr;
MP_SPECIALIZE_IS_RANGE(BinaryLogicalExpr, BINARY_LOGICAL)

// A relational expression.
// Examples: x < y, x != y, where x and y are variables.
typedef BasicBinaryExpr<LogicalExpr, NumericExpr> RelationalExpr;
MP_SPECIALIZE_IS_RANGE(RelationalExpr, RELATIONAL)

// A logical count expression.
// Examples: atleast 1 (x < y, x != y), where x and y are variables.
class LogicalCountExpr : public LogicalExpr {
 public:
  LogicalCountExpr() {}

  // Returns the left-hand side (the first argument) of this expression.
  NumericExpr lhs() const { return Create<NumericExpr>(expr_->L.e); }

  // Returns the right-hand side (the second argument) of this expression.
  CountExpr rhs() const { return Create<CountExpr>(expr_->R.e); }
};

MP_SPECIALIZE_IS_RANGE(LogicalCountExpr, LOGICAL_COUNT)

// An implication expression.
// Example: a ==> b else c, where a, b and c are logical expressions.
typedef BasicIfExpr<LogicalExpr> ImplicationExpr;
MP_SPECIALIZE_IS(ImplicationExpr, IMPLICATION)

// An iterated logical expression.
// Example: exists{i in I} x[i] >= 0, where I is a set and x is a variable.
typedef BasicIteratedExpr<LogicalExpr> IteratedLogicalExpr;
MP_SPECIALIZE_IS_RANGE(IteratedLogicalExpr, ITERATED_LOGICAL)

// A pairwise expression.
// Example: alldiff{i in I} x[i], where I is a set and x is a variable.
typedef BasicIteratedExpr<LogicalExpr, NumericExpr> PairwiseExpr;
MP_SPECIALIZE_IS_RANGE(PairwiseExpr, PAIRWISE)

class StringLiteral : public Expr {
 public:
  StringLiteral() {}

  const char *value() const { return reinterpret_cast<expr_h*>(expr_)->sym; }
};

MP_SPECIALIZE_IS(StringLiteral, STRING)

// Recursively compares two expressions and returns true if they are equal.
bool Equal(NumericExpr e1, NumericExpr e2);
bool Equal(LogicalExpr e1, LogicalExpr e2);

template <typename Grad>
class LinearExpr;

// A linear term.
template <typename GradT>
class LinearTerm {
 private:
  typedef GradT Grad;
  Grad *grad_;

  friend class LinearExpr< LinearTerm<Grad> >;

  explicit LinearTerm(Grad *g = 0) : grad_(g) {}

 public:
  // Returns the coefficient.
  double coef() const { return grad_->coef; }

  // Returns the variable index.
  int var_index() const { return grad_->varno; }
};

typedef LinearTerm<ograd> LinearObjTerm;
typedef LinearTerm<cgrad> LinearConTerm;

// A linear expression.
template <typename Term>
class LinearExpr {
 private:
  Term first_term_;

  friend class mp::Problem;

  explicit LinearExpr(typename Term::Grad *first_term)
  : first_term_(Term(first_term)) {}

 public:
  LinearExpr() {}

  class iterator : public std::iterator<std::forward_iterator_tag, Term> {
   private:
    Term term_;

   public:
    explicit iterator(Term t) : term_(t) {}

    Term operator*() const { return term_; }
    const Term *operator->() const { return &term_; }

    iterator &operator++() {
      term_ = Term(term_.grad_->next);
      return *this;
    }

    iterator operator++(int ) {
      iterator it(*this);
      term_ = Term(term_.grad_->next);
      return it;
    }

    bool operator==(iterator other) const {
      return term_.grad_ == other.term_.grad_;
    }
    bool operator!=(iterator other) const {
      return term_.grad_ != other.term_.grad_;
    }
  };

  iterator begin() { return iterator(first_term_); }
  iterator end() { return iterator(Term()); }
};

typedef LinearExpr<LinearObjTerm> LinearObjExpr;
typedef LinearExpr<LinearConTerm> LinearConExpr;

template <typename LinearExpr>
void WriteExpr(fmt::Writer &w, LinearExpr linear, NumericExpr nonlinear);

namespace internal {

#ifdef MP_USE_UNORDERED_MAP
template <class T>
inline std::size_t HashCombine(std::size_t seed, const T &v) {
  return seed ^ (std::hash<T>()(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2));
}

class HashNumberOfArgs {
 public:
  std::size_t operator()(NumberOfExpr e) const;
};
#endif

class EqualNumberOfArgs {
 public:
  bool operator()(NumberOfExpr lhs, NumberOfExpr rhs) const;
};

template <typename NumberOf>
class MatchNumberOfArgs {
 private:
  NumberOfExpr expr_;

 public:
  explicit MatchNumberOfArgs(NumberOfExpr e) : expr_(e) {}

  // Returns true if the stored expression has the same arguments as the nof's
  // expression.
  bool operator()(const NumberOf &nof) const {
    return EqualNumberOfArgs()(expr_, nof.expr);
  }
};
}  // namespace internal

// A map from numberof expressions with the same argument lists to
// values and corresponding variables.
template <typename Var, typename CreateVar>
class NumberOfMap {
 public:
  typedef std::map<double, Var> ValueMap;

  struct NumberOf {
    NumberOfExpr expr;
    ValueMap values;

    explicit NumberOf(NumberOfExpr e) : expr(e) {}
  };

 private:
  CreateVar create_var_;

#ifdef MP_USE_UNORDERED_MAP
  // Map from a numberof expression to an index in numberofs_.
  typedef std::unordered_map<NumberOfExpr, std::size_t,
    internal::HashNumberOfArgs, internal::EqualNumberOfArgs> Map;
  Map map_;
#endif

  std::vector<NumberOf> numberofs_;

 public:
  explicit NumberOfMap(CreateVar cv) : create_var_(cv) {}

  typedef typename std::vector<NumberOf>::const_iterator iterator;

  iterator begin() const {
    return numberofs_.begin();
  }

  iterator end() const {
    return numberofs_.end();
  }

  // Adds a numberof expression with a constant value.
  Var Add(double value, NumberOfExpr e);
};

template <typename Var, typename CreateVar>
Var NumberOfMap<Var, CreateVar>::Add(double value, NumberOfExpr e) {
  assert(Cast<NumericConstant>(e[0]).value() == value);
#ifdef MP_USE_UNORDERED_MAP
  std::pair<typename Map::iterator, bool> result =
      map_.insert(typename Map::value_type(e, numberofs_.size()));
  if (result.second)
    numberofs_.push_back(NumberOf(e));
  ValueMap &values = numberofs_[result.first->second].values;
#else
  typename std::vector<NumberOf>::reverse_iterator np =
      std::find_if(numberofs_.rbegin(), numberofs_.rend(),
                   internal::MatchNumberOfArgs<NumberOf>(e));
  if (np == numberofs_.rend()) {
    numberofs_.push_back(NumberOf(e));
    np = numberofs_.rbegin();
  }
  ValueMap &values = np->values;
#endif
  typename ValueMap::iterator i = values.lower_bound(value);
  if (i != values.end() && !values.key_comp()(value, i->first))
    return i->second;
  Var var(create_var_());
  values.insert(i, typename ValueMap::value_type(value, var));
  return var;
}
}  // namespace asl
}  // namespace mp

template <typename ExprType>
ExprType mp::internal::Cast(asl::Expr e) {
  assert(asl::internal::Is<ExprType>(e.kind()));
  ExprType expr;
  expr.expr_ = e.expr_;
  return expr;
}

#endif  // MP_ASLEXPR_H_
