/*
 CPLEX solver with SOCP transformations

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

#include "mp/expr-visitor.h"
#include "mp/nl.h"
#include "mp/problem.h"
#include "asl/aslbuilder.h"
#include "asl/aslproblem.h"
#include "asl.h"

namespace asl = mp::asl;
using asl::internal::ASLBuilder;

using mp::Problem;

namespace {

// Converts a linear expression.
// expr: an expression to convert
// builder: a builder for converted expression
template <typename LinearExprBuilder>
void ConvertLinearExpr(mp::LinearExpr expr, LinearExprBuilder builder) {
  for (mp::LinearExpr::iterator i = expr.begin(), e = expr.end(); i != e; ++i)
    builder.AddTerm(i->var_index(), i->coef());
}

// A nonlinear expression converter.
class ExprConverter :
    public mp::ExprVisitor<ExprConverter, asl::NumericExpr, asl::LogicalExpr> {
 private:
  ASLBuilder &builder_;

 public:
  explicit ExprConverter(ASLBuilder &b) : builder_(b) {}

  asl::NumericExpr VisitNumericConstant(mp::NumericConstant c) {
    return builder_.MakeNumericConstant(c.value());
  }

  asl::NumericExpr VisitVariable(mp::Variable v) {
    return builder_.MakeVariable(v.index());
  }

  asl::NumericExpr VisitUnary(mp::UnaryExpr e) {
    return builder_.MakeUnary(e.kind(), Visit(e.arg()));
  }

  asl::NumericExpr VisitBinary(mp::BinaryExpr e) {
    return builder_.MakeBinary(e.kind(), Visit(e.lhs()), Visit(e.rhs()));
  }

  asl::NumericExpr VisitPow(mp::BinaryExpr e) {
    mp::NumericConstant rhs = mp::Cast<mp::NumericConstant>(e.rhs());
    mp::expr::Kind kind = rhs && rhs.value() == 2 ? mp::expr::POW2 : e.kind();
    return builder_.MakeUnary(kind, Visit(e.lhs()));
  }

  asl::NumericExpr VisitSum(mp::IteratedExpr e) {
    ASLBuilder::NumericExprBuilder sum = builder_.BeginSum(e.num_args());
    for (mp::IteratedExpr::iterator i = e.begin(), end = e.end(); i != end; ++i)
      sum.AddArg(Visit(*i));
    return builder_.EndSum(sum);
  }

  // TODO: convert all expresion kinds
};
}  // namespace

// Adapts Problem interface for use with .nl reader.
class ProblemBuilder : public Problem {
 public:
  typedef mp::Function Function;
  typedef mp::Expr Expr;
  typedef mp::NumericExpr NumericExpr;
  typedef mp::LogicalExpr LogicalExpr;
  typedef mp::CountExpr CountExpr;
  typedef mp::Variable Variable;

  typedef IteratedExprBuilder NumericExprBuilder;
  typedef IteratedExprBuilder VarArgExprBuilder;
  typedef IteratedExprBuilder NumberOfExprBuilder;

  VarArgExprBuilder BeginVarArg(mp::expr::Kind kind, int num_args) {
    return BeginIterated(kind, num_args);
  }
  NumericExpr EndVarArg(VarArgExprBuilder builder) {
    return EndIterated(builder);
  }

  NumericExprBuilder BeginSum(int num_args) {
    return BeginIterated(mp::expr::SUM, num_args);
  }
  NumericExpr EndSum(NumericExprBuilder builder) {
    return EndIterated(builder);
  }

  struct ColumnSizeHandler {
    void Add(int) {
      // Ignore column sizes as the constraints are stored row-wise.
    }
  };

  // Returns a handler that receives column sizes in Jacobian.
  ColumnSizeHandler GetColumnSizeHandler() {
    return ColumnSizeHandler();
  }
};

// Detects if a problem is convertible to an SOCP.
class SOCPDetector {
 // TODO
};

class SOCPConverter {
 private:
  ProblemBuilder problem_;
  ASLBuilder builder_;
  std::vector<int> col_sizes_;

  // Converts a nonlinear expression to ASL format.
  asl::NumericExpr Convert(mp::NumericExpr expr) {
    ExprConverter converter(builder_);
    return expr ? converter.Visit(expr) : asl::NumericExpr();
  }

  typedef Problem::ObjList ObjList;

 public:
  explicit SOCPConverter(ASL *asl) : builder_(asl) {}

  void Run(const char *stub);

  // Converts the problem into ASL format.
  void ConvertToASL();
};

void SOCPConverter::Run(const char *stub) {
  mp::ProblemBuilderToNLAdapter<ProblemBuilder> adapter(problem_);
  ReadNLFile(fmt::format("{}.nl", stub), adapter);
  if (!problem_.HasComplementarity()) {
    // TODO
    // 1. check if the problem can be converted to SOCP
    // 2. convert to SOCP
  }
  mp::ProblemInfo info = mp::ProblemInfo();
  info.num_vars = problem_.num_vars();
  info.num_algebraic_cons = problem_.num_algebraic_cons();
  info.num_objs = problem_.num_objs();

  // Count nonzeros in objectives.
  ObjList objs = problem_.objs();
  for (ObjList::iterator i = objs.begin(), end = objs.end(); i != end; ++i)
    info.num_obj_nonzeros += i->linear_expr().num_terms();

  // Get algebraic constraint information.
  col_sizes_.resize(info.num_vars);
  double infinity = std::numeric_limits<double>::infinity();
  Problem::AlgebraicConList cons = problem_.algebraic_cons();
  for (Problem::AlgebraicConList::iterator
       i = cons.begin(), end = cons.end(); i != end; ++i) {
    if (-infinity < i->lb() && i->ub() < infinity)
      ++info.num_ranges;
    if (i->nonlinear_expr())
      ++info.num_nl_cons;
    mp::LinearExpr expr = i->linear_expr();
    info.num_con_nonzeros += expr.num_terms();
    for (mp::LinearExpr::iterator j = expr.begin(), e = expr.end(); j != e; ++j)
      ++col_sizes_[j->var_index()];
  }

  // TODO: convert all problem info
  builder_.SetInfo(info);
  builder_.set_stub(stub);
}

void SOCPConverter::ConvertToASL() {
  // Convert variables.
  int num_vars = problem_.num_vars();
  for (int i = 0; i < num_vars; ++i) {
    Problem::Variable var = problem_.var(i);
    builder_.AddVar(var.lb(), var.ub(), var.type());
  }

  // Convert objectives.
  ObjList objs = problem_.objs();
  for (ObjList::iterator i = objs.begin(), end = objs.end(); i != end; ++i) {
    mp::LinearExpr expr = i->linear_expr();
    ASLBuilder::LinearObjBuilder obj_builder = builder_.AddObj(
          i->type(), Convert(i->nonlinear_expr()), expr.num_terms());
    ConvertLinearExpr(expr, obj_builder);
  }

  ASLBuilder::ColumnSizeHandler cols = builder_.GetColumnSizeHandler();
  for (int i = 0; i < num_vars - 1; ++i)
    cols.Add(col_sizes_[i]);

  // Convert algebraic constraints.
  Problem::AlgebraicConList algebraic_cons = problem_.algebraic_cons();
  for (Problem::AlgebraicConList::iterator
       i = algebraic_cons.begin(), end = algebraic_cons.end(); i != end; ++i) {
    mp::LinearExpr expr = i->linear_expr();
    ASLBuilder::LinearConBuilder con_builder = builder_.AddCon(
          i->lb(), i->ub(), Convert(i->nonlinear_expr()), expr.num_terms());
    ConvertLinearExpr(expr, con_builder);
  }

  // Convert logical constraints.
  ExprConverter converter(builder_);
  Problem::LogicalConList logical_cons = problem_.logical_cons();
  for (Problem::LogicalConList::iterator
       i = logical_cons.begin(), end = logical_cons.end(); i != end; ++i) {
    builder_.AddCon(converter.Visit(i->expr()));
  }

  // TODO: convert common expressions, complementarity conditions and suffixes.

  builder_.EndBuild();
}

extern "C" void *socp_jac0dim(ASL *asl, const char *stub, ftnlen) {
  SOCPConverter *converter = new SOCPConverter(asl); // TODO: smart pointer
  converter->Run(stub);
  return converter;
}

extern "C" int socp_qp_read(ASL *, void *converter_ptr, int) {
  SOCPConverter *converter = static_cast<SOCPConverter*>(converter_ptr);
  converter->ConvertToASL();
  return 0;
}

extern "C" void socp_write_sol(
    ASL *asl, const char *msg, double *x, double *y, Option_Info *oi) {
  // TODO: convert and write solution
  write_sol_ASL(asl, msg, x, y, oi);
}
