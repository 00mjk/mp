/*
 AMPL solver interface to LocalSolver.

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

#include "localsolver-direct/localsolver-direct.h"

#include <cmath>
#include "mp/clock.h"
#include "asl/problem.h"

namespace {
// Returns the value of an expression.
inline double GetValue(localsolver::LSExpression e) {
  return e.isDouble() ? e.getDoubleValue() : e.getValue();
}
}

namespace mp {

LSProblemBuilder::HyperbolicTerms
    LSProblemBuilder::MakeHyperbolicTerms(ls::LSExpression arg) {
  HyperbolicTerms terms;
  terms.exp_x = model_.createExpression(ls::O_Exp, arg);
  terms.exp_minus_x = model_.createExpression(ls::O_Exp, Negate(arg));
  return terms;
}

LSProblemBuilder::LSProblemBuilder(ls::LSModel model)
  : model_(model), num_continuous_vars_(0) {
}

void LSProblemBuilder::SetInfo(const NLHeader &header) {
  vars_.resize(header.num_vars);
  objs_.resize(header.num_objs);
  cons_.resize(header.num_algebraic_cons);
  num_continuous_vars_ = header.num_continuous_vars();
  for (int i = 0; i < num_continuous_vars_; ++i)
    vars_[i] = model_.createExpression(ls::O_Float);
  for (int i = num_continuous_vars_; i < header.num_vars; ++i)
    vars_[i] = model_.createExpression(ls::O_Int);
}

void LSProblemBuilder::EndBuild() {
  // Add objectives.
  for (std::size_t i = 0, n = objs_.size(); i < n; ++i) {
    const ObjInfo &obj = objs_[i];
    model_.addObjective(obj.expr, obj.direction);
  }
  // LocalSolver requires at least one objective - create a dummy one.
  if (objs_.empty())
    model_.addObjective(model_.createConstant(MakeInt(0)), ls::OD_Minimize);
  // Add constraints.
  double inf = std::numeric_limits<double>::infinity();
  for (std::size_t i = 0, n = cons_.size(); i < n; ++i) {
    const ConInfo &con = cons_[i];
    ls::LSExpression expr;
    if (con.lb <= -inf) {
      expr = model_.createExpression(ls::O_Leq, con.expr, con.ub);
    } else if (con.ub >= inf) {
      expr = model_.createExpression(ls::O_Geq, con.expr, con.lb);
    } else if (con.lb == con.ub) {
      expr = model_.createExpression(ls::O_Eq, con.expr, con.lb);
    } else {
      expr = model_.createExpression(ls::O_Geq, con.expr, con.lb);
      expr = model_.createExpression(ls::O_Leq, con.expr, con.ub);
    }
    model_.addConstraint(expr);
  }
  model_.close();
}

ls::LSExpression LSProblemBuilder::MakeUnary(
    expr::Kind kind, ls::LSExpression arg) {
  ls::LSOperator op = ls::O_Bool;
  switch (kind) {
  case expr::FLOOR:
    op = ls::O_Floor;
    break;
  case expr::CEIL:
    op = ls::O_Ceil;
    break;
  case expr::ABS:
    op = ls::O_Abs;
    break;
  case expr::MINUS:
    return Negate(arg);
  case expr::TANH: {
    HyperbolicTerms terms = MakeHyperbolicTerms(arg);
    return MakeBinary(ls::O_Div,
                      MakeBinary(ls::O_Sub, terms.exp_x, terms.exp_minus_x),
                      MakeBinary(ls::O_Sum, terms.exp_x, terms.exp_minus_x));
  }
  case expr::TAN:
    op = ls::O_Tan;
    break;
  case expr::SQRT:
    op = ls::O_Sqrt;
    break;
  case expr::SINH: {
    HyperbolicTerms terms = MakeHyperbolicTerms(arg);
    return Half(MakeBinary(ls::O_Sub, terms.exp_x, terms.exp_minus_x));
  }
  case expr::SIN:
    op = ls::O_Sin;
    break;
  case expr::LOG10:
    return MakeBinary(ls::O_Div,
                      model_.createExpression(ls::O_Log, arg), std::log(10.0));
  case expr::LOG:
    op = ls::O_Log;
    break;
  case expr::EXP:
    op = ls::O_Exp;
    break;
  case expr::COSH:{
    HyperbolicTerms terms = MakeHyperbolicTerms(arg);
    return Half(MakeBinary(ls::O_Sum, terms.exp_x, terms.exp_minus_x));
  }
  case expr::COS:
    return model_.createExpression(ls::O_Cos, arg);
  case expr::ATANH:
    arg = MakeBinary(ls::O_Div, Plus1(arg),                    // (1 + x) /
                     MakeBinary(ls::O_Sub, MakeInt(1), arg));  // (1 - x)
    return Half(model_.createExpression(ls::O_Log, arg));
  case expr::ASINH: {
    ls::LSExpression arg2 = model_.createExpression(
          ls::O_Sqrt, Plus1(MakeBinary(ls::O_Pow, arg, MakeInt(2))));
    return model_.createExpression(ls::O_Log, MakeBinary(ls::O_Sum, arg, arg2));
  }
  case expr::ACOSH: {
    ls::LSExpression x_minus_1 = MakeBinary(ls::O_Sub, arg, MakeInt(1));
    ls::LSExpression arg2 = MakeBinary(
          ls::O_Prod, model_.createExpression(ls::O_Sqrt, Plus1(arg)),
          model_.createExpression(ls::O_Sqrt, x_minus_1));
    return model_.createExpression(ls::O_Log, MakeBinary(ls::O_Sum, arg, arg2));
  }
  case expr::POW2:
    return MakeBinary(ls::O_Pow, arg, MakeInt(2));
  case expr::ATAN: case expr::ASIN: case expr::ACOS:
    // LocalSolver doesn't support these expressions.
    // Fall through.
  default:
    // TODO: report type of expression
    return Base::MakeUnary(kind, arg);
  }
  return model_.createExpression(op, arg);
}

ls::LSExpression LSProblemBuilder::MakeBinary(
    expr::Kind kind, ls::LSExpression lhs, ls::LSExpression rhs) {
  ls::LSOperator op = ls::O_Bool;
  switch (kind) {
  case expr::ADD:
    op = ls::O_Sum;
    break;
  case expr::SUB:
    op = ls::O_Sub;
    break;
  case expr::MUL:
    op = ls::O_Prod;
    break;
  case expr::DIV:
    op = ls::O_Div;
    break;
  case expr::INT_DIV:
    return IntDiv(lhs, rhs);
  case expr::MOD:
    op = ls::O_Mod;
    break;
  case expr::POW:
  case expr::POW_CONST_BASE:
  case expr::POW_CONST_EXP:
    op = ls::O_Pow;
    break;
  case expr::LESS:
    return MakeBinary(ls::O_Max, MakeBinary(ls::O_Sub, lhs, rhs), MakeInt(0));
  case expr::ROUND:
    RequireZero(rhs, "round");
    return model_.createExpression(ls::O_Round, lhs);
  case expr::TRUNC:
    RequireZero(rhs, "trunc");
    return IntDiv(lhs, MakeInt(1));
  case expr::PRECISION:
  case expr::ATAN2:
    // LocalSolver doesn't support these functions.
    // Fall through.
  default:
    // TODO: report type of expression
    return Base::MakeBinary(kind, lhs, rhs);
  }
  return MakeBinary(op, lhs, rhs);
}

ls::LSExpression LSProblemBuilder::MakeBinaryLogical(
    expr::Kind kind, ls::LSExpression lhs, ls::LSExpression rhs) {
  ls::LSOperator op = ls::O_Bool;
  switch (kind) {
  case expr::OR:
    op = ls::O_Or;
    break;
  case expr::AND:
    op = ls::O_And;
    break;
  case expr::IFF:
    op = ls::O_Eq;
    break;
  default:
    return Base::MakeBinaryLogical(kind, lhs, rhs);
  }
  return MakeBinary(op, lhs, rhs);
}

ls::LSExpression LSProblemBuilder::MakeRelational(
    expr::Kind kind, ls::LSExpression lhs, ls::LSExpression rhs) {
  ls::LSOperator op = ls::O_Bool;
  switch (kind) {
  case expr::LT:
    op = ls::O_Lt;
    break;
  case expr::LE:
    op = ls::O_Leq;
    break;
  case expr::EQ:
    op = ls::O_Eq;
    break;
  case expr::GE:
    op = ls::O_Geq;
    break;
  case expr::GT:
    op = ls::O_Gt;
    break;
  case expr::NE:
    op = ls::O_Neq;
    break;
  default:
    return Base::MakeRelational(kind, lhs, rhs);
  }
  return MakeBinary(op, lhs, rhs);
}

ls::LSExpression LSProblemBuilder::MakeLogicalCount(
    expr::Kind kind, ls::LSExpression lhs, ls::LSExpression rhs) {
  ls::LSOperator op = ls::O_Bool;
  switch (kind) {
  case expr::ATLEAST:
    op = ls::O_Leq;
    break;
  case expr::ATMOST:
    op = ls::O_Geq;
    break;
  case expr::EXACTLY:
    op = ls::O_Eq;
    break;
  case expr::NOT_ATLEAST:
    op = ls::O_Gt;
    break;
  case expr::NOT_ATMOST:
    op = ls::O_Lt;
    break;
  case expr::NOT_EXACTLY:
    op = ls::O_Neq;
    break;
  default:
    return Base::MakeLogicalCount(kind, lhs, rhs);
  }
  return MakeBinary(op, lhs, rhs);
}

ls::LSExpression LSProblemBuilder::EndAllDiff(AllDiffArgHandler handler) {
  // TODO
  return ls::LSExpression();
}

/*
void NLToLocalSolverConverter::Convert(const Problem &p) {
  // Convert logical constraints.
  int num_logical_cons = p.num_logical_cons();
  for (int i = 0; i < num_logical_cons; ++i) {
    LogicalExpr e = p.logical_con_expr(i);
    AllDiffExpr alldiff = Cast<AllDiffExpr>(e);
    ICLSetter icl_setter(icl_, GetICL(p.num_cons() + i));
    if (!alldiff) {
      rel(problem_, Visit(e), icl_);
      continue;
    }
    int num_args = alldiff.num_args();
    IntVarArgs args(num_args);
    for (int i = 0; i < num_args; ++i) {
      NumericExpr arg(alldiff[i]);
      if (Variable var = ampl::Cast<Variable>(arg))
        args[i] = vars[var.index()];
      else
        args[i] = Gecode::expr(problem_, Visit(arg), icl_);
    }
    distinct(problem_, args, icl_);
  }
}*/

LocalSolver::LocalSolver()
  : SolverImpl<LSProblemBuilder>("localsolver", 0, 20140710), timelimit_(0) {
  std::string version = fmt::format("{}.{}",
      localsolver::LSVersion::getMajorVersionNumber(),
      localsolver::LSVersion::getMinorVersionNumber());
  set_long_name("localsolver " + version);
  set_version("LocalSolver " + version);

  set_option_header(
      "LocalSolver Options for AMPL\n"
      "----------------------------\n"
      "\n"
      "To set these options, assign a string specifying their values to "
      "the AMPL option ``localsolver_options``. For example::\n"
      "\n"
      "  ampl: option localsolver_options 'version timelimit=30;\n");

  AddIntOption("timelimit",
      "Time limit in seconds (positive integer). Default = no limit.",
      &LocalSolver::GetTimeLimit, &LocalSolver::SetTimeLimit);
}

/*ls::LSExpression NLToLocalSolverConverter::VisitAllDiff(AllDiffExpr e) {
  ls::LSExpression result = model_.createExpression(ls::O_And);
  int num_args = e.num_args();
  std::vector<ls::LSExpression > args(num_args);
  for (int i = 0; i < num_args; ++i)
    args[i] = Visit(e[i]);
  for (int i = 0; i < num_args; ++i) {
    for (int j = i + 1; j < num_args; ++j)
      result.addOperand(model_.createExpression(ls::O_Neq, args[i], args[j]));
  }
  return result;
}*/

void LocalSolver::Solve(ProblemBuilder &builder, SolutionHandler &sh) {
  steady_clock::time_point time = steady_clock::now();

  // Set options. LS requires this to be done after the model is closed.
  ls::LSPhase phase = solver_.createPhase();
  if (timelimit_ != 0)
    phase.setTimeLimit(timelimit_);

  double setup_time = GetTimeAndReset(time);

  // Solve the problem.
  solver_.solve();

  // Convert solution status.
  int solve_code = 0;
  ls::LSSolution sol = solver_.getSolution();
  const char *status = "unknown";
  switch (sol.getStatus()) {
  case ls::SS_Inconsistent:
    solve_code = INFEASIBLE;
    status = "infeasible problem";
    break;
  case ls::SS_Infeasible:
    // Solution is infeasible, but problem may be feasible.
    // This can only happen if stopped by a limit.
    solve_code = LIMIT;
    status = "infeasible solution";
    break;
  case ls::SS_Feasible:
    solve_code = SOLVED_MAYBE;
    status = "feasible solution";
    break;
  case ls::SS_Optimal:
    solve_code = SOLVED;
    status = "optimal solution";
    break;
  default:
    solve_code = FAILURE;
    status = "unknown solution status";
    break;
  }
  // TODO
  //p.set_solve_code(solve_code);

  int num_vars = builder.num_vars();
  const ls::LSExpression *vars = builder.vars();
  std::vector<double> solution(num_vars);
  int num_continuous_vars = builder.num_continuous_vars();
  for (int i = 0; i < num_continuous_vars; ++i)
    solution[i] = vars[i].getDoubleValue();
  for (int i = num_continuous_vars; i < num_vars; ++i)
    solution[i] = vars[i].getValue();
  double solution_time = GetTimeAndReset(time);

  fmt::MemoryWriter w;
  w.write("{}: {}\n", long_name(), status);
  w.write("{}", solver_.getStatistics().toString());
  double obj_val = std::numeric_limits<double>::quiet_NaN();
  if (builder.num_objs() != 0) {
    obj_val = GetValue(solver_.getModel().getObjective(0));
    w.write("objective {}", FormatObjValue(obj_val));
  }
  sh.HandleSolution(w.c_str(),
                    solution.empty() ? 0 : solution.data(), 0, obj_val);
  double output_time = GetTimeAndReset(time);

  if (timing()) {
    Print("Setup time = {:.6f}s\n"
          "Solution time = {:.6f}s\n"
          "Output time = {:.6f}s\n",
          setup_time, solution_time, output_time);
  }
}

// TODO
SolverPtr CreateSolver(const char *) { return SolverPtr(new LocalSolver()); }
}  // namespace mp
