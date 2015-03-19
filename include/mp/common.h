/*
 Common declarations

 Copyright (C) 2014 AMPL Optimization Inc

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

#ifndef MP_COMMON_H_
#define MP_COMMON_H_

#include <cassert>
#include <cstddef>  // for std::size_t

#define MP_DISPATCH(call) static_cast<Impl*>(this)->call

// Suppresses warnings about unused variables.
#define MP_UNUSED(x) (void)(x)
#define MP_UNUSED2(x, y) MP_UNUSED(x); MP_UNUSED(y)
#define MP_UNUSED3(x, y, z) MP_UNUSED2(x, y); MP_UNUSED(z)

/** The mp namespace. */
namespace mp {

/** Expression information. */
namespace expr {

/** Expression kind. */
enum Kind {
  // An unknown expression.
  UNKNOWN = 0,

  FIRST_EXPR,

  // To simplify checks, numeric expression kinds are in the range
  // [FIRST_NUMERIC, LAST_NUMERIC].
  /**
    \rst
    .. _numeric-expr:

    A numeric expression.
    \endrst
  */
  FIRST_NUMERIC = FIRST_EXPR,

  /**
    \rst
    .. _numeric-constant:

    A number such as ``42`` or ``-1.23e-4``.
    \endrst
   */
  NUMBER = FIRST_NUMERIC,

  // Reference expressions.
  FIRST_REFERENCE,

  /**
    \rst
    .. _variable:

    A reference to a variable.
    \endrst
  */
  VARIABLE = FIRST_REFERENCE,

  /**
    \rst
    .. _common-expr:

    A reference to a common expression.
    \endrst
  */
  COMMON_EXPR,

  LAST_REFERENCE = COMMON_EXPR,

  // Unary expressions.
  /**
    \rst
    .. _unary-expr:

    A unary numeric expression.
    Examples: ``-x``, ``sin(x)``, where ``x`` is a variable.
    \endrst
  */
  FIRST_UNARY,
  MINUS = FIRST_UNARY,
  ABS,
  FLOOR,
  CEIL,
  SQRT,
  POW2,
  EXP,
  LOG,
  LOG10,
  SIN,
  SINH,
  COS,
  COSH,
  TAN,
  TANH,
  ASIN,
  ASINH,
  ACOS,
  ACOSH,
  ATAN,
  ATANH,
  LAST_UNARY = ATANH,

  // Binary expressions.
  /**
    \rst
    .. _binary-expr:

    A binary numeric expression.
    Examples: ``x / y``, ``atan2(x, y)``, where ``x`` and ``y`` are variables.
    \endrst
  */
  FIRST_BINARY,
  ADD = FIRST_BINARY,
  SUB,
  LESS,
  MUL,
  DIV,
  INT_DIV,
  MOD,
  POW,
  POW_CONST_BASE,
  POW_CONST_EXP,
  ATAN2,
  PRECISION,
  ROUND,
  TRUNC,
  LAST_BINARY = TRUNC,

  /**
    \rst
    .. _if-expr:

    An if-then-else expression.
    Example: ``if x != 0 then y else z``, where ``x``, ``y`` and ``z`` are
    variables.
    \endrst
  */
  IF,

  /**
    \rst
    .. _plterm:

    A piecewise-linear term.
    Example: ``<<0; -1, 1>> x``, where ``x`` is a variable.
    \endrst
  */
  PLTERM,

  /**
    \rst
    .. _call-expr:

    A function call expression.
    Example: ``f(x)``, where ``f`` is a function and ``x`` is a variable.
    \endrst
  */
  CALL,

  // Iterated expressions.
  // The term "iterated" in the context of operators and expressions comes
  // from the article "AMPL: A Mathematical Programming Language" and is
  // used to denote operators indexed over sets.
  FIRST_ITERATED,

  // TODO: document expressions
  /**
    \rst
    .. _vararg-expr:
    .. _sum-expr:
    .. _numberof-expr:
    .. _numberof-sym-expr:
    .. _count-expr:
    .. _logical-expr:
    .. _logical-constant:
    .. _not-expr:
    .. _binary-logical-expr:
    .. _relational-expr:
    .. _logical-count-expr:
    .. _implication-expr:
    .. _iterated-logical-expr:
    .. _pairwise-expr:
    .. _string-expr:
    .. _symbolic-if-expr:

    Expression kind.
    \endrst
   */
  FIRST_VARARG = FIRST_ITERATED,
  MIN = FIRST_VARARG,
  MAX,
  LAST_VARARG = MAX,
  SUM,
  NUMBEROF,
  LAST_ITERATED = NUMBEROF,

  NUMBEROF_SYM,
  COUNT,
  LAST_NUMERIC = COUNT,

  // To simplify checks, logical expression kinds are in the range
  // [FIRST_LOGICAL, LAST_LOGICAL].
  FIRST_LOGICAL,
  BOOL = FIRST_LOGICAL,
  NOT,

  // Binary logical expressions.
  FIRST_BINARY_LOGICAL,
  OR = FIRST_BINARY_LOGICAL,
  AND,
  IFF,
  LAST_BINARY_LOGICAL = IFF,

  // Relational expressions.
  FIRST_RELATIONAL,
  LT = FIRST_RELATIONAL,  // <
  LE,                     // <=
  EQ,                     // =
  GE,                     // >=
  GT,                     // >
  NE,                     // !=
  LAST_RELATIONAL = NE,

  FIRST_LOGICAL_COUNT,
  ATLEAST = FIRST_LOGICAL_COUNT,
  ATMOST,
  EXACTLY,
  NOT_ATLEAST,
  NOT_ATMOST,
  NOT_EXACTLY,
  LAST_LOGICAL_COUNT = NOT_EXACTLY,

  IMPLICATION,

  // Iterated logical expressions.
  FIRST_ITERATED_LOGICAL,
  EXISTS = FIRST_ITERATED_LOGICAL,
  FORALL,
  LAST_ITERATED_LOGICAL = FORALL,

  // Pairwise expressions.
  FIRST_PAIRWISE,
  ALLDIFF = FIRST_PAIRWISE,
  NOT_ALLDIFF,
  LAST_PAIRWISE = NOT_ALLDIFF,
  LAST_LOGICAL = LAST_PAIRWISE,

  // String expressions.
  STRING,
  IFSYM,
  LAST_EXPR = IFSYM
};

// Maximum opcode.
enum { MAX_OPCODE = 82 };

class OpCodeInfo {
 private:
  static const OpCodeInfo INFO[MAX_OPCODE + 1];

 public:
  expr::Kind kind;
  expr::Kind first_kind;  // First member of a kind.

  friend const OpCodeInfo &GetOpCodeInfo(int opcode);
};

inline const OpCodeInfo &GetOpCodeInfo(int opcode) {
  assert(opcode >= 0 && opcode <= MAX_OPCODE);
  return OpCodeInfo::INFO[opcode];
}

int opcode(expr::Kind kind);

// Returns the string representation of this expression kind.
// Expressions of different kinds can have identical strings.
// For example, POW, POW_CONST_BASE and POW_CONST_EXP all have
// the same representation "^".
const char *str(expr::Kind kind);
}  // namespace expr

namespace internal {

template<bool B, typename T = void>
struct enable_if {};

template<class T>
struct enable_if<true, T> { typedef T type; };

// Returns true if ExprType is of kind k.
template <typename ExprType>
inline bool Is(expr::Kind k) {
  int kind = k;
  // If FIRST_KIND == LAST_KIND, then a decent optimizing compiler simplifies
  // this to kind == ExprType::FIRST_KIND (checked with GCC 4.8.2).
  // No need to do it ourselves.
  return ExprType::FIRST_KIND <= kind && kind <= ExprType::LAST_KIND;
}

int precedence(expr::Kind kind);

// Expression information.
class ExprInfo {
 private:
  static const ExprInfo INFO[];

  friend int expr::opcode(expr::Kind kind);
  friend const char *expr::str(expr::Kind kind);

 public:
  int opcode;
  int precedence;
  const char *str;

  friend int precedence(expr::Kind kind) {
    assert(kind >= expr::UNKNOWN && kind <= expr::LAST_EXPR);
    return internal::ExprInfo::INFO[kind].precedence;
  }
};
}

inline int expr::opcode(expr::Kind kind) {
  assert(kind >= expr::UNKNOWN && kind <= expr::LAST_EXPR);
  return internal::ExprInfo::INFO[kind].opcode;
}

inline const char *expr::str(expr::Kind kind) {
  assert(kind >= expr::UNKNOWN && kind <= expr::LAST_EXPR);
  return internal::ExprInfo::INFO[kind].str;
}

/** Function information. */
namespace func {
/** Function type. */
enum Type {
  /** A numeric function. */
  NUMERIC  = 0,
  /** A symbolic function - accepts numeric and string arguments. */
  SYMBOLIC = 1
};
}

namespace var {
// Variable type.
enum Type { CONTINUOUS, INTEGER };
}

/** Objective information. */
namespace obj {
/** Objective type. */
enum Type {
  MIN = 0, /**< A minimization objective. */
  MAX = 1  /**< A maximization objective. */
};
}

// Complementarity namespace. It would make more sense to call it compl,
// but the latter is a reserved word in C++.
namespace comp {
// Flags for complementarity constraints.
enum { INF_LB = 1, INF_UB = 2 };
}

namespace suf {
// Suffix kinds.
enum {
  VAR     =  0,  // Applies to variables.
  CON     =  1,  // Applies to constraints.
  OBJ     =  2,  // Applies to objectives.
  PROBLEM =  3,  // Applies to problems.
  NUM_KINDS,     // The number of suffix kinds.
  MASK    =  3,  // Mask for suffix kind.
  FLOAT   =  4,  // Suffix values are floating-point numbers.
  IODECL  =  8,  // Tell AMPL to make this an INOUT suffix.
  OUTPUT  = 16,  // Output suffix: return values to AMPL.
  INPUT   = 32,  // Input suffix: values were received from AMPL.
  OUTONLY = 64   // Output only: reject as an input value.
};
}

namespace sol {
// Solution status.
enum Status {
  UNKNOWN     =  -1,

  // An optimal solution found for an optimization problem or a feasible
  // solution found for a satisfaction problem.
  SOLVED      =   0,

  // Solution returned but it can be non-optimal or even infeasible.
  UNSOLVED    = 100,

  // Problem is infeasible.
  INFEASIBLE  = 200,

  // Problem is unbounded.
  UNBOUNDED   = 300,

  // Stopped by a limit, e.g. on iterations or time.
  LIMIT       = 400,

  FAILURE     = 500,

  // Interrupted by the user.
  INTERRUPTED = 600
};
}

/** Information about an optimization problem. */
struct ProblemInfo {
  // Total number of variables.
  int num_vars;

  // Number of algebraic constraints including ranges and equality constraints.
  // It doesn't include logical constraints.
  int num_algebraic_cons;

  // Total number of objectives.
  int num_objs;

  // Number of ranges (constraints with -Infinity < LHS < RHS < Infinity).
  int num_ranges;

  // Number of equality constraints or -1 if unknown (AMPL prior to 19970627).
  int num_eqns;

  // Number of logical constraints.
  int num_logical_cons;

  int num_integer_vars() const {
    return num_linear_binary_vars + num_linear_integer_vars +
        num_nl_integer_vars_in_both + num_nl_integer_vars_in_cons +
        num_nl_integer_vars_in_objs;
  }

  int num_continuous_vars() const { return num_vars - num_integer_vars(); }

  // Nonlinear and complementarity information
  // -----------------------------------------

  // Total number of nonlinear constraints.
  int num_nl_cons;

  // Total number of nonlinear objectives.
  int num_nl_objs;

  // Total number of complementarity conditions.
  int num_compl_conds;

  // Number of nonlinear complementarity conditions.
  int num_nl_compl_conds;

  // Number of complementarities involving double inequalities
  // (for ASL_cc_simplify).
  int num_compl_dbl_ineqs;

  // Number of complemented variables with a nonzero lower bound
  // (for ASL_cc_simplify).
  int num_compl_vars_with_nz_lb;

  // Information about network constraints
  // -------------------------------------

  // Number of nonlinear network constraints.
  int num_nl_net_cons;

  // Number of linear network constraints.
  int num_linear_net_cons;

  // Information about nonlinear variables
  // -------------------------------------

  // Number of nonlinear variables in constraints including nonlinear
  // variables in both constraints and objectives.
  int num_nl_vars_in_cons;

  // Number of nonlinear variables in objectives including nonlinear
  // variables in both constraints and objectives.
  int num_nl_vars_in_objs;

  // Number of nonlinear variables in both constraints and objectives.
  int num_nl_vars_in_both;

  // Miscellaneous
  // -------------

  // Number of linear network variables (arcs).
  int num_linear_net_vars;

  // Number of functions.
  int num_funcs;

  // Information about discrete variables
  // ------------------------------------

  // Number of linear binary variables.
  int num_linear_binary_vars;

  // Number of linear non-binary integer variables.
  int num_linear_integer_vars;

  // Number of integer nonlinear variables in both constraints and objectives.
  int num_nl_integer_vars_in_both;

  // Number of integer nonlinear variables just in constraints.
  int num_nl_integer_vars_in_cons;

  // Number of integer nonlinear variables just in objectives.
  int num_nl_integer_vars_in_objs;

  // Information about nonzeros
  // --------------------------

  // Number of nonzeros in constraints' Jacobian.
  std::size_t num_con_nonzeros;

  // Number of nonzeros in all objective gradients.
  std::size_t num_obj_nonzeros;

  // Information about names
  // -----------------------

  // Length of longest constraint name (if stub.row exists).
  int max_con_name_len;

  // Length of longest variable name (if stub.col exists).
  int max_var_name_len;

  // Information about common expressions
  // ------------------------------------

  int num_common_exprs_in_both;
  int num_common_exprs_in_cons;
  int num_common_exprs_in_objs;

  // Number of common expressions that only appear in a single constraint
  // and don't appear in objectives.
  int num_common_exprs_in_single_cons;

  // Number of common expressions that only appear in a single objective
  // and don't appear in constraints.
  int num_common_exprs_in_single_objs;

  int num_common_exprs() const {
    return num_common_exprs_in_both + num_common_exprs_in_cons +
        num_common_exprs_in_objs + num_common_exprs_in_single_cons +
        num_common_exprs_in_single_objs;
  }
};
}  // namespace mp

#endif  // MP_COMMON_H_
