/*
 A C++ interface to AMPL expressions.

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

#include "solvers/util/expr.h"

#include <cstdio>
#include <cstring>

using std::size_t;

using ampl::Cast;
using ampl::Expr;
using ampl::NumericConstant;
using ampl::NumericExpr;
using ampl::LogicalExpr;
using ampl::internal::HashCombine;

namespace std {
template <>
struct hash<Expr> {
  std::size_t operator()(Expr e) const;
};
}

namespace {
// An operation type.
// Numeric values for the operation types should be in sync with the ones in
// op_type.hd.
enum OpType {
  OPTYPE_UNARY    =  1,  // Unary operation
  OPTYPE_BINARY   =  2,  // Binary operation
  OPTYPE_VARARG   =  3,  // Variable-argument function such as min or max
  OPTYPE_PLTERM   =  4,  // Piecewise-linear term
  OPTYPE_IF       =  5,  // The if-then-else expression
  OPTYPE_SUM      =  6,  // The sum expression
  OPTYPE_FUNCALL  =  7,  // Function call
  OPTYPE_STRING   =  8,  // String
  OPTYPE_NUMBER   =  9,  // Number
  OPTYPE_VARIABLE = 10,  // Variable
  OPTYPE_COUNT    = 11   // The count expression
};

namespace prec {
enum Precedence {
  UNKNOWN,
  CONDITIONAL,       // if-then-else
  IFF,               // <==>
  IMPLICATION,       // ==> else
  LOGICAL_OR,        // or ||
  LOGICAL_AND,       // and &&
  NOT,               // not !
  RELATIONAL,        // < <= = == >= > != <>
  PIECEWISE_LINEAR,  // a piecewise-linear expression
  ADDITIVE,          // + - less
  ITERATIVE,         // sum prod min max
  MULTIPLICATIVE,    // * / div mod
  EXPONENTIATION,    // ^
  UNARY,             // + - (unary)
  CALL,              // a function call including functional forms of
                     // min and max
  PRIMARY            // variable or constant
};
}  // namespace prec

// An expression visitor that writes AMPL expressions in a textual form
// to fmt::Writer. It takes into account precedence and associativity
// of operators avoiding unnecessary parentheses except for potentially
// confusing cases such as "!x = y" which is written as "!(x = y) instead.
class ExprWriter : public ampl::ExprVisitor<ExprWriter, void, void> {
 private:
  fmt::Writer &writer_;
  int precedence_;

  typedef ampl::ExprVisitor<ExprWriter, void, void> ExprVisitor;

  // Writes an argument list surrounded by parentheses.
  template <typename Iter>
  void WriteArgs(Iter begin, Iter end, const char *sep = ", ",
      int precedence = prec::UNKNOWN);

  template <typename Expr>
  void WriteArgs(Expr e, const char *sep = ", ",
      int precedence = prec::UNKNOWN) {
    WriteArgs(e.begin(), e.end(), sep, precedence);
  }

  // Writes a function or an expression that has a function syntax.
  template <typename Expr>
  void WriteFunc(Expr e) {
    writer_ << e.opstr();
    WriteArgs(e);
  }

  template <typename Expr>
  void WriteBinary(Expr e);

  void WriteCallArg(Expr arg);

  class Parenthesizer {
   private:
    ExprWriter &writer_;
    int saved_precedence_;
    bool write_paren_;

   public:
    Parenthesizer(ExprWriter &w, Expr e, int precedence);
    ~Parenthesizer();
  };

 public:
  explicit ExprWriter(fmt::Writer &w)
  : writer_(w), precedence_(prec::UNKNOWN) {}

  void Visit(NumericExpr e, int precedence = -1) {
    Parenthesizer p(*this, e, precedence);
    ExprVisitor::Visit(e);
  }

  void Visit(ampl::LogicalExpr e, int precedence = -1) {
    Parenthesizer p(*this, e, precedence);
    ExprVisitor::Visit(e);
  }

  void VisitUnary(ampl::UnaryExpr e) {
    writer_ << e.opstr() << '(';
    Visit(e.arg(), prec::UNKNOWN);
    writer_ << ')';
  }

  void VisitUnaryMinus(ampl::UnaryExpr e) {
    writer_ << '-';
    Visit(e.arg());
  }

  void VisitPow2(ampl::UnaryExpr e) {
    Visit(e.arg(), prec::EXPONENTIATION + 1);
    writer_ << " ^ 2";
  }

  void VisitBinary(ampl::BinaryExpr e) { WriteBinary(e); }
  void VisitBinaryFunc(ampl::BinaryExpr e);
  void VisitVarArg(ampl::VarArgExpr e) { WriteFunc(e); }
  void VisitIf(ampl::IfExpr e);
  void VisitSum(ampl::SumExpr e);
  void VisitCount(ampl::CountExpr e) { WriteFunc(e); }
  void VisitNumberOf(ampl::NumberOfExpr e);
  void VisitPiecewiseLinear(ampl::PiecewiseLinearExpr e);
  void VisitCall(ampl::CallExpr e);
  void VisitNumericConstant(NumericConstant c) { writer_ << c.value(); }
  void VisitVariable(ampl::Variable v) { writer_ << 'x' << (v.index() + 1); }

  void VisitNot(ampl::NotExpr e) {
     writer_ << '!';
     // Use a precedence higher then relational to print expressions
     // as "!(x = y)" instead of "!x = y".
     ampl::LogicalExpr arg = e.arg();
     Visit(arg,
         arg.precedence() == prec::RELATIONAL ? prec::RELATIONAL + 1 : -1);
  }

  void VisitBinaryLogical(ampl::BinaryLogicalExpr e) { WriteBinary(e); }
  void VisitRelational(ampl::RelationalExpr e) { WriteBinary(e); }
  void VisitLogicalCount(ampl::LogicalCountExpr e);
  void VisitIteratedLogical(ampl::IteratedLogicalExpr e);
  void VisitImplication(ampl::ImplicationExpr e);
  void VisitAllDiff(ampl::AllDiffExpr e) { WriteFunc(e); }
  void VisitLogicalConstant(ampl::LogicalConstant c) { writer_ << c.value(); }
};

ExprWriter::Parenthesizer::Parenthesizer(ExprWriter &w, Expr e, int precedence)
: writer_(w), write_paren_(false) {
  saved_precedence_ = w.precedence_;
  if (precedence == -1)
    precedence = w.precedence_;
  write_paren_ = e.precedence() < precedence;
  if (write_paren_)
    w.writer_ << '(';
  w.precedence_ = e.precedence();
}

ExprWriter::Parenthesizer::~Parenthesizer() {
  writer_.precedence_ = saved_precedence_;
  if (write_paren_)
    writer_.writer_ << ')';
}

template <typename Iter>
void ExprWriter::WriteArgs(
    Iter begin, Iter end, const char *sep, int precedence) {
  writer_ << '(';
  if (begin != end) {
    Visit(*begin, precedence);
    for (++begin; begin != end; ++begin) {
      writer_ << sep;
      Visit(*begin, precedence);
    }
  }
  writer_ << ')';
}

template <typename Expr>
void ExprWriter::WriteBinary(Expr e) {
  int precedence = e.precedence();
  bool right_associative = precedence == prec::EXPONENTIATION;
  Visit(e.lhs(), precedence + (right_associative ? 1 : 0));
  writer_ << ' ' << e.opstr() << ' ';
  Visit(e.rhs(), precedence + (right_associative ? 0 : 1));
}

void ExprWriter::WriteCallArg(Expr arg) {
  if (NumericExpr e = Cast<NumericExpr>(arg)) {
    Visit(e, prec::UNKNOWN);
    return;
  }
  assert(arg.opcode() == OPHOL);
  writer_ << "'";
  const char *s = Cast<ampl::StringLiteral>(arg).value();
  for ( ; *s; ++s) {
    char c = *s;
    switch (c) {
    case '\n':
      writer_ << '\\' << c;
      break;
    case '\'':
      // Escape quote by doubling.
      writer_ << c;
      // Fall through.
    default:
      writer_ << c;
    }
  }
  writer_ << "'";
}

void ExprWriter::VisitBinaryFunc(ampl::BinaryExpr e) {
  writer_ << e.opstr() << '(';
  Visit(e.lhs(), prec::UNKNOWN);
  writer_ << ", ";
  Visit(e.rhs(), prec::UNKNOWN);
  writer_ << ')';
}

void ExprWriter::VisitIf(ampl::IfExpr e) {
  writer_ << "if ";
  Visit(e.condition(), prec::UNKNOWN);
  writer_ << " then ";
  NumericExpr false_expr = e.false_expr();
  bool has_else = !IsZero(false_expr);
  Visit(e.true_expr(), prec::CONDITIONAL + (has_else ? 1 : 0));
  if (has_else) {
    writer_ << " else ";
    Visit(false_expr);
  }
}

void ExprWriter::VisitSum(ampl::SumExpr e) {
  writer_ << "/* sum */ (";
  ampl::SumExpr::iterator i = e.begin(), end = e.end();
  if (i != end) {
    Visit(*i);
    for (++i; i != end; ++i) {
      writer_ << " + ";
      Visit(*i);
    }
  }
  writer_ << ')';
}

void ExprWriter::VisitNumberOf(ampl::NumberOfExpr e) {
  writer_ << "numberof ";
  ampl::NumberOfExpr::iterator i = e.begin();
  Visit(*i++, prec::UNKNOWN);
  writer_ << " in ";
  WriteArgs(i, e.end());
}

void ExprWriter::VisitPiecewiseLinear(ampl::PiecewiseLinearExpr e) {
  writer_ << "<<" << e.breakpoint(0);
  for (int i = 1, n = e.num_breakpoints(); i < n; ++i)
    writer_ << ", " << e.breakpoint(i);
  writer_ << "; " << e.slope(0);
  for (int i = 1, n = e.num_slopes(); i < n; ++i)
    writer_ << ", " << e.slope(i);
  writer_ << ">> " << "x" << (e.var_index() + 1);
}

void ExprWriter::VisitCall(ampl::CallExpr e) {
  writer_ << e.function().name() << '(';
  int num_args = e.num_args();
  if (num_args > 0) {
    WriteCallArg(e[0]);
    for (int i = 1; i < num_args; ++i) {
      writer_ << ", ";
      WriteCallArg(e[i]);
    }
  }
  writer_ << ')';
}

void ExprWriter::VisitLogicalCount(ampl::LogicalCountExpr e) {
  writer_ << e.opstr() << ' ';
  Visit(e.lhs());
  writer_ << ' ';
  WriteArgs(e.rhs());
}

void ExprWriter::VisitIteratedLogical(ampl::IteratedLogicalExpr e) {
  // There is no way to produce an AMPL forall/exists expression because
  // its indexing is not available any more. So we write a count expression
  // instead with a comment about the original expression.
  writer_ << "/* " << e.opstr() << " */ ";
  int precedence = prec::LOGICAL_AND + 1;
  const char *op = " && ";
  if (e.opcode() == ORLIST) {
    precedence = prec::LOGICAL_OR + 1;
    op = " || ";
  }
  WriteArgs(e, op, precedence);
}

void ExprWriter::VisitImplication(ampl::ImplicationExpr e) {
  Visit(e.condition());
  writer_ << " ==> ";
  Visit(e.true_expr(), prec::IMPLICATION + 1);
  ampl::LogicalExpr false_expr = e.false_expr();
  ampl::LogicalConstant c = Cast<ampl::LogicalConstant>(false_expr);
  if (!c || c.value() != 0) {
    writer_ << " else ";
    Visit(false_expr);
  }
}
}

#ifdef HAVE_UNORDERED_MAP

// Computes a hash value for an expression.
class ExprHasher : public ampl::ExprVisitor<ExprHasher, size_t> {
 private:
  static size_t Hash(Expr e) {
    return HashCombine(0, e.opcode());
  }

  template <typename T>
  static size_t Hash(Expr e, const T &value) {
    return HashCombine(Hash(e), value);
  }

 public:
  size_t VisitNumericConstant(NumericConstant c) { return Hash(c, c.value()); }
  size_t VisitVariable(ampl::Variable v) { return Hash(v, v.index()); }

  template <typename E>
  size_t VisitUnary(E e) { return Hash(e, e.arg()); }

  template <typename E>
  size_t VisitBinary(E e) { return HashCombine(Hash(e, e.lhs()), e.rhs()); }

  template <typename E>
  size_t VisitIf(E e) {
    size_t hash = HashCombine(Hash(e), e.condition());
    return HashCombine(HashCombine(hash, e.true_expr()), e.false_expr());
  }

  size_t VisitPiecewiseLinear(ampl::PiecewiseLinearExpr e) {
    size_t hash = Hash(e);
    int num_breakpoints = e.num_breakpoints();
    for (int i = 0; i < num_breakpoints; ++i) {
      hash = HashCombine(hash, e.slope(i));
      hash = HashCombine(hash, e.breakpoint(i));
    }
    hash = HashCombine(hash, e.slope(num_breakpoints));
    return HashCombine(hash, e.var_index());
  }

  size_t VisitCall(ampl::CallExpr e) {
    // Function name is hashed as a pointer. This works because the function
    // object is the same for all calls to the same function.
    size_t hash = Hash(e, e.function().name());
    for (int i = 0, n = e.num_args(); i < n; ++i)
      hash = HashCombine(hash, e[i]);
    return hash;
  }

  template <typename E>
  size_t VisitVarArg(E e) {
    size_t hash = Hash(e);
    for (typename E::iterator i = e.begin(), end = e.end(); i != end; ++i)
      hash = HashCombine(hash, *i);
    return hash;
  }

  size_t VisitSum(ampl::SumExpr e) { return VisitVarArg(e); }
  size_t VisitCount(ampl::CountExpr e) { return VisitVarArg(e); }
  size_t VisitNumberOf(ampl::NumberOfExpr e) { return VisitVarArg(e); }

  size_t VisitLogicalConstant(ampl::LogicalConstant c) {
    return Hash(c, c.value());
  }

  size_t VisitNot(ampl::NotExpr e) { return Hash(e, e.arg()); }

  size_t VisitBinaryLogical(ampl::BinaryLogicalExpr e) {
    return VisitBinary(e);
  }

  size_t VisitRelational(ampl::RelationalExpr e) { return VisitBinary(e); }

  size_t VisitLogicalCount(ampl::LogicalCountExpr e) {
    NumericExpr rhs = e.rhs();
    return HashCombine(Hash(e, e.lhs()), rhs);
  }

  size_t VisitImplication(ampl::ImplicationExpr e) { return VisitIf(e); }

  size_t VisitIteratedLogical(ampl::IteratedLogicalExpr e) {
    return VisitVarArg(e);
  }

  size_t VisitAllDiff(ampl::AllDiffExpr e) { return VisitVarArg(e); }

  size_t VisitStringLiteral(ampl::StringLiteral s) {
    size_t hash = Hash(s);
    for (const char *value = s.value(); *value; ++value)
      hash = HashCombine(hash, *value);
    return hash;
  }
};

size_t std::hash<Expr>::operator()(Expr expr) const {
  ExprHasher hasher;
  NumericExpr n = Cast<NumericExpr>(expr);
  return n ? hasher.Visit(n) :
             hasher.VisitStringLiteral(Cast<ampl::StringLiteral>(expr));
}

size_t std::hash<NumericExpr>::operator()(NumericExpr expr) const {
  return ExprHasher().Visit(expr);
}

size_t std::hash<LogicalExpr>::operator()(LogicalExpr expr) const {
  return ExprHasher().Visit(expr);
}
#else
# if defined(AMPL_NO_UNORDERED_MAP_WARNING)
  // Do nothing.
# elif defined(_MSC_VER)
#  pragma message("warning: unordered_map not available, numberof may be slow")
# else
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wpedantic"
#  warning "unordered_map not available, numberof may be slow"
#  pragma clang diagnostic pop
# endif
#endif

namespace ampl {

const Expr::Info Expr::INFO[N_OPS] = {
    {Expr::BINARY,           prec::ADDITIVE,       "+"},  // OPPLUS
    {Expr::BINARY,           prec::ADDITIVE,       "-"},  // OPMINUS
    {Expr::BINARY,           prec::MULTIPLICATIVE, "*"},  // OPMULT
    {Expr::BINARY,           prec::MULTIPLICATIVE, "/"},  // OPDIV
    {Expr::BINARY,           prec::MULTIPLICATIVE, "mod"},  // OPREM
    {Expr::BINARY,           prec::EXPONENTIATION, "^"},  // OPPOW
    {Expr::BINARY,           prec::ADDITIVE,       "less"},  // OPLESS
    {Expr::UNKNOWN,          prec::UNKNOWN,        "unknown"},
    {Expr::UNKNOWN,          prec::UNKNOWN,        "unknown"},
    {Expr::UNKNOWN,          prec::UNKNOWN,        "unknown"},
    {Expr::UNKNOWN,          prec::UNKNOWN,        "unknown"},
    {Expr::VARARG,           prec::CALL,           "min"},  // MINLIST
    {Expr::VARARG,           prec::CALL,           "max"},  // MAXLIST
    {Expr::UNARY,            prec::CALL,           "floor"},  // FLOOR
    {Expr::UNARY,            prec::CALL,           "ceil"},  // CEIL
    {Expr::UNARY,            prec::CALL,           "abs"},  // ABS
    {Expr::UNARY,            prec::UNARY,          "unary -"},  // OPUMINUS
    {Expr::UNKNOWN,          prec::UNKNOWN,        "unknown"},
    {Expr::UNKNOWN,          prec::UNKNOWN,        "unknown"},
    {Expr::UNKNOWN,          prec::UNKNOWN,        "unknown"},
    {Expr::BINARY_LOGICAL,   prec::LOGICAL_OR,     "||"},  // OPOR
    {Expr::BINARY_LOGICAL,   prec::LOGICAL_AND,    "&&"},  // OPAND
    {Expr::RELATIONAL,       prec::RELATIONAL,     "<"},  // LT
    {Expr::RELATIONAL,       prec::RELATIONAL,     "<="},  // LE
    {Expr::RELATIONAL,       prec::RELATIONAL,     "="},  // EQ
    {Expr::UNKNOWN,          prec::UNKNOWN,        "unknown"},
    {Expr::UNKNOWN,          prec::UNKNOWN,        "unknown"},
    {Expr::UNKNOWN,          prec::UNKNOWN,        "unknown"},
    {Expr::RELATIONAL,       prec::RELATIONAL,     ">="},  // GE
    {Expr::RELATIONAL,       prec::RELATIONAL,     ">"},  // GT
    {Expr::RELATIONAL,       prec::RELATIONAL,     "!="},  // NE
    {Expr::UNKNOWN,          prec::UNKNOWN,        "unknown"},
    {Expr::UNKNOWN,          prec::UNKNOWN,        "unknown"},
    {Expr::UNKNOWN,          prec::UNKNOWN,        "unknown"},
    {Expr::NOT,              prec::NOT,            "!"},  // OPNOT
    {Expr::IF,               prec::CONDITIONAL,    "if"},  // OPIFnl
    {Expr::UNKNOWN,          prec::UNKNOWN,        "unknown"},
    {Expr::UNARY,            prec::CALL,           "tanh"},  // OP_tanh
    {Expr::UNARY,            prec::CALL,           "tan"},  // OP_tan
    {Expr::UNARY,            prec::CALL,           "sqrt"},  // OP_sqrt
    {Expr::UNARY,            prec::CALL,           "sinh"},  // OP_sinh
    {Expr::UNARY,            prec::CALL,           "sin"},  // OP_sin
    {Expr::UNARY,            prec::CALL,           "log10"},  // OP_log10
    {Expr::UNARY,            prec::CALL,           "log"},  // OP_log
    {Expr::UNARY,            prec::CALL,           "exp"},  // OP_exp
    {Expr::UNARY,            prec::CALL,           "cosh"},  // OP_cosh
    {Expr::UNARY,            prec::CALL,           "cos"},  // OP_cos
    {Expr::UNARY,            prec::CALL,           "atanh"},  // OP_atanh
    {Expr::BINARY,           prec::CALL,           "atan2"},  // OP_atan2
    {Expr::UNARY,            prec::CALL,           "atan"},  // OP_atan
    {Expr::UNARY,            prec::CALL,           "asinh"},  // OP_asinh
    {Expr::UNARY,            prec::CALL,           "asin"},  // OP_asin
    {Expr::UNARY,            prec::CALL,           "acosh"},  // OP_acosh
    {Expr::UNARY,            prec::CALL,           "acos"},  // OP_acos
    {Expr::SUM,              prec::ITERATIVE,      "sum"},  // OPSUMLIST
    {Expr::BINARY,           prec::MULTIPLICATIVE, "div"},  // OPintDIV
    {Expr::BINARY,           prec::CALL,           "precision"},  // OPprecision
    {Expr::BINARY,           prec::CALL,           "round"},  // OPround
    {Expr::BINARY,           prec::CALL,           "trunc"},  // OPtrunc
    {Expr::COUNT,            prec::CALL,           "count"},  // OPCOUNT
    {Expr::NUMBEROF,         prec::CALL,           "numberof"},  // OPNUMBEROF
    // OPNUMBEROFs - not supported yet
    {Expr::UNKNOWN,          prec::UNKNOWN,        "string numberof"},
    {Expr::LOGICAL_COUNT,    prec::CALL,           "atleast"},  // OPATLEAST
    {Expr::LOGICAL_COUNT,    prec::CALL,           "atmost"},  // OPATMOST
    {Expr::PLTERM,           prec::CALL,           "pl term"},  // OPPLTERM
    // OPIFSYM - not supported yet
    {Expr::UNKNOWN,          prec::UNKNOWN,        "string if-then-else"},
    {Expr::LOGICAL_COUNT,    prec::CALL,           "exactly"},  // OPEXACTLY
    {Expr::LOGICAL_COUNT,    prec::CALL,           "!atleast"},  // OPNOTATLEAST
    {Expr::LOGICAL_COUNT,    prec::CALL,           "!atmost"},  // OPNOTATMOST
    {Expr::LOGICAL_COUNT,    prec::CALL,           "!exactly"},  // OPNOTEXACTLY
    {Expr::ITERATED_LOGICAL, prec::CALL,           "forall"},  // ANDLIST
    {Expr::ITERATED_LOGICAL, prec::CALL,           "exists"},  // ORLIST
    {Expr::IMPLICATION,      prec::IMPLICATION,    "==>"},  // OPIMPELSE
    {Expr::BINARY_LOGICAL,   prec::IFF,            "<==>"},  // OP_IFF
    {Expr::ALLDIFF,          prec::CALL,           "alldiff"},  // OPALLDIFF
    {Expr::BINARY,           prec::EXPONENTIATION, "^"},  // OP1POW
    {Expr::UNARY,            prec::EXPONENTIATION, "^2"},  // OP2POW
    {Expr::BINARY,           prec::EXPONENTIATION, "^"},  // OPCPOW
    // OPFUNCALL
    {Expr::CALL,             prec::CALL,           "function call"},
    {Expr::CONSTANT,         prec::PRIMARY,        "number"},  // OPNUM
    {Expr::STRING,           prec::PRIMARY,        "string"},  // OPHOL
    {Expr::VARIABLE,         prec::PRIMARY,        "variable"}  // OPVARVAL
};

const de VarArgExpr::END = {0};

bool Equal(Expr expr1, Expr expr2) {
  if (expr1.opcode() != expr2.opcode())
    return false;

  expr *e1 = expr1.expr_;
  expr *e2 = expr2.expr_;
  switch (optype[expr1.opcode()]) {
  case OPTYPE_UNARY:
    return Equal(Expr(e1->L.e), Expr(e2->L.e));

  case OPTYPE_BINARY:
    return Equal(Expr(e1->L.e), Expr(e2->L.e)) &&
           Equal(Expr(e1->R.e), Expr(e2->R.e));

  case OPTYPE_VARARG: {
    de *d1 = reinterpret_cast<const expr_va*>(e1)->L.d;
    de *d2 = reinterpret_cast<const expr_va*>(e2)->L.d;
    for (; d1->e && d2->e; d1++, d2++)
      if (!Equal(Expr(d1->e), Expr(d2->e)))
        return false;
    return !d1->e && !d2->e;
  }

  case OPTYPE_PLTERM: {
    plterm *p1 = e1->L.p, *p2 = e2->L.p;
    if (p1->n != p2->n)
      return false;
    double *pce1 = p1->bs, *pce2 = p2->bs;
    for (int i = 0, n = p1->n * 2 - 1; i < n; i++) {
      if (pce1[i] != pce2[i])
        return false;
    }
    return Equal(Expr(e1->R.e), Expr(e2->R.e));
  }

  case OPTYPE_IF: {
    const expr_if *eif1 = reinterpret_cast<const expr_if*>(e1);
    const expr_if *eif2 = reinterpret_cast<const expr_if*>(e2);
    return Equal(Expr(eif1->e), Expr(eif2->e)) &&
           Equal(Expr(eif1->T), Expr(eif2->T)) &&
           Equal(Expr(eif1->F), Expr(eif2->F));
  }

  case OPTYPE_SUM:
  case OPTYPE_COUNT: {
    expr **ep1 = e1->L.ep;
    expr **ep2 = e2->L.ep;
    for (; ep1 < e1->R.ep && ep2 < e2->R.ep; ep1++, ep2++)
      if (!Equal(Expr(*ep1), Expr(*ep2)))
        return false;
    return ep1 == e1->R.ep && ep2 == e2->R.ep;
  }

  case OPTYPE_STRING:
    return std::strcmp(
        reinterpret_cast<const expr_h*>(e1)->sym,
        reinterpret_cast<const expr_h*>(e2)->sym) == 0;

  case OPTYPE_NUMBER:
    return reinterpret_cast<const expr_n*>(e1)->v ==
           reinterpret_cast<const expr_n*>(e2)->v;

  case OPTYPE_VARIABLE:
    return e1->a == e2->a;

  default:
    throw UnsupportedExprError::CreateFromExprString(expr1.opstr());
  }
}

std::string internal::FormatOpCode(Expr e) {
  char buffer[64];
  snprintf(buffer, sizeof(buffer), "%d", e.opcode());
  return buffer;
}

#ifdef HAVE_UNORDERED_MAP
size_t internal::HashNumberOfArgs::operator()(NumberOfExpr e) const {
  size_t hash = 0;
  for (int i = 1, n = e.num_args(); i < n; ++i)
    hash = HashCombine(hash, e[i]);
  return hash;
}
#endif

bool internal::EqualNumberOfArgs::operator()(
    NumberOfExpr lhs, NumberOfExpr rhs) const {
  int num_args = lhs.num_args();
  if (num_args != rhs.num_args())
    return false;
  for (int i = 1; i < num_args; ++i) {
    if (!Equal(lhs[i], rhs[i]))
      return false;
  }
  return true;
}

template <typename LinearExpr>
void WriteExpr(fmt::Writer &w, LinearExpr linear, NumericExpr nonlinear) {
  bool have_terms = false;
  typedef typename LinearExpr::iterator Iterator;
  for (Iterator i = linear.begin(), e = linear.end(); i != e; ++i) {
    double coef = i->coef();
    if (coef != 0) {
      if (have_terms)
        w << " + ";
      else
        have_terms = true;
      if (coef != 1)
        w << coef << " * ";
      w << "x" << (i->var_index() + 1);
    }
  }
  if (!nonlinear || IsZero(nonlinear)) {
    if (!have_terms)
      w << "0";
    return;
  }
  if (have_terms)
    w << " + ";
  ExprWriter(w).Visit(nonlinear);
}

template
void WriteExpr<LinearObjExpr>(
    fmt::Writer &w, LinearObjExpr linear, NumericExpr nonlinear);

template
void WriteExpr<LinearConExpr>(
    fmt::Writer &w, LinearConExpr linear, NumericExpr nonlinear);
}
