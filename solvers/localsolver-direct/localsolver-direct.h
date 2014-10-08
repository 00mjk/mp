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

#ifndef MP_SOLVERS_LOCALSOLVER_H_
#define MP_SOLVERS_LOCALSOLVER_H_

#include <localsolver.h>
#include "mp/problem-builder.h"
#include "mp/solver.h"

namespace mp {

namespace ls = localsolver;

class LocalSolver;

// This class provides methods for building a problem in LocalSolver format.
class LSProblemBuilder :
    public ProblemBuilder<LSProblemBuilder, ls::LSExpression> {
 private:
  ls::LSModel model_;
  int num_continuous_vars_;

  std::vector<ls::LSExpression> vars_;

  struct ObjInfo {
    ls::LSObjectiveDirection direction;
    ls::LSExpression expr;
    ObjInfo() : direction(ls::OD_Minimize) {}
  };
  std::vector<ObjInfo> objs_;

  struct ConInfo {
    ls::LSExpression expr;
    double lb, ub;
    ConInfo() : lb(0), ub(0) {}
  };
  std::vector<ConInfo> cons_;

  typedef ProblemBuilder<LSProblemBuilder, ls::LSExpression> Base;

  static ls::lsint MakeInt(int value) { return value; }

  ls::lsint ConvertToInt(double value) {
    // TODO
    return static_cast<ls::lsint>(value);
  }

  ls::LSExpression Negate(ls::LSExpression arg) {
    return model_.createExpression(ls::O_Sub, MakeInt(0), arg);
  }

  // Returns true if e is a zero constant.
  static bool IsZero(ls::LSExpression e) {
    return e.getOperator() == ls::O_Float && e.getDoubleValue() == 0;
  }

  void RequireZero(ls::LSExpression e, const char *where) {
    if (!IsZero(e)) {
      // TODO: throw exception
    }
  }

  struct HyperbolicTerms {
    ls::LSExpression exp_x;
    ls::LSExpression exp_minus_x;
  };

  HyperbolicTerms MakeHyperbolicTerms(ls::LSExpression arg);

  // Makes a binary expression.
  template <typename LHS, typename RHS>
  ls::LSExpression MakeBinary(ls::LSOperator op, LHS lhs, RHS rhs) {
    return model_.createExpression(op, lhs, rhs);
  }

  // Makes an expression representing arg / 2.
  ls::LSExpression Half(ls::LSExpression arg) {
    return MakeBinary(ls::O_Div, arg, MakeInt(2));
  }

  // Makes an expression representing arg + 1.
  ls::LSExpression Plus1(ls::LSExpression arg) {
    return MakeBinary(ls::O_Sum, arg, MakeInt(1));
  }

  // Makes an expression representing lhs div rhs.
  template <typename RHS>
  ls::LSExpression IntDiv(ls::LSExpression lhs, RHS rhs) {
    return MakeBinary(ls::O_Div, MakeBinary(
                        ls::O_Sub, lhs, MakeBinary(ls::O_Mod, lhs, rhs)), rhs);
  }

 public:
  explicit LSProblemBuilder(ls::LSModel model);

  int num_vars() const { return vars_.size(); }
  int num_continuous_vars() const { return num_continuous_vars_; }
  int num_objs() const { return model_.getNbObjectives(); }
  int num_cons() const { return cons_.size(); }

  const ls::LSExpression *vars() const { return &vars_[0]; }
  // TODO

  void SetInfo(const NLHeader &header);
  void EndBuild();

  void SetObj(int index, obj::Type type, ls::LSExpression expr) {
    ObjInfo &obj_info = objs_[index];
    if (type == obj::MAX)
      obj_info.direction = ls::OD_Maximize;
    if (!IsZero(expr))
      obj_info.expr = expr;
  }

  void SetCon(int index, ls::LSExpression expr) {
    if (!IsZero(expr))
      cons_[index].expr = expr;
  }

  class LinearExprBuilder {
   private:
    LSProblemBuilder &builder_;
    ls::LSExpression expr_;

   public:
    LinearExprBuilder(LSProblemBuilder &builder,
                      ls::LSExpression &expr, ls::LSExpression sum)
      : builder_(builder), expr_(sum) {
      if (expr != ls::LSExpression()) {
        // Add nonlinear expression.
        sum.addOperand(expr);
        expr = sum;
      } else {
        expr = sum;
      }
    }

    void AddTerm(int var_index, double coef) {
      expr_.addOperand(builder_.model_.createExpression(
                         ls::O_Prod, coef, builder_.vars_[var_index]));
    }
  };

  typedef LinearExprBuilder LinearObjBuilder;

  LinearObjBuilder GetLinearObjBuilder(int obj_index, int) {
    return LinearObjBuilder(
          *this, objs_[obj_index].expr, model_.createExpression(ls::O_Sum));
  }

  typedef LinearExprBuilder LinearConBuilder;

  LinearConBuilder GetLinearConBuilder(int con_index, int) {
    return LinearConBuilder(
          *this, cons_[con_index].expr, model_.createExpression(ls::O_Sum));
  }

  void SetVarBounds(int index, double lb, double ub) {
    ls::LSExpression var = vars_[index];
    if (index < num_continuous_vars_) {
      var.addOperand(lb);
      var.addOperand(ub);
    } else {
      var.addOperand(ConvertToInt(lb));
      var.addOperand(ConvertToInt(ub));
    }
  }

  void SetConBounds(int index, double lb, double ub) {
    ConInfo &con = cons_[index];
    con.lb = lb;
    con.ub = ub;
  }

  // Ignore Jacobian column sizes.
  ColumnSizeHandler GetColumnSizeHandler() { return ColumnSizeHandler(); }

  ls::LSExpression MakeNumericConstant(double value) {
    return model_.createConstant(value);
  }

  ls::LSExpression MakeVariable(int var_index) { return vars_[var_index]; }

  ls::LSExpression MakeUnary(expr::Kind kind, ls::LSExpression arg);
  ls::LSExpression MakeBinary(
      expr::Kind kind, ls::LSExpression lhs, ls::LSExpression rhs);

  ls::LSExpression MakeIf(ls::LSExpression condition,
      ls::LSExpression true_expr, ls::LSExpression false_expr) {
    return model_.createExpression(ls::O_If, condition, true_expr, false_expr);
  }

  // LocalSolver doesn't support piecewise-liner terms and functions.

  class ArgHandler {
   private:
    ls::LSExpression expr_;

   public:
    explicit ArgHandler(ls::LSExpression expr) : expr_(expr) {}

    ls::LSExpression expr() const { return expr_; }

    void AddArg(ls::LSExpression arg) { expr_.addOperand(arg); }
  };

  typedef ArgHandler NumericArgHandler;
  typedef ArgHandler LogicalArgHandler;

  ArgHandler BeginVarArg(expr::Kind kind, int num_args) {
    ls::LSOperator op = ls::O_Min;
    if (kind == expr::MAX)
      op = ls::O_Max;
    else if (kind != expr::MIN)
      Base::BeginVarArg(kind, num_args);
    return ArgHandler(model_.createExpression(op));
  }
  ls::LSExpression EndVarArg(ArgHandler handler) { return handler.expr(); }

  ArgHandler BeginSum(int) {
    return ArgHandler(model_.createExpression(ls::O_Sum));
  }
  ls::LSExpression EndSum(ArgHandler handler) { return handler.expr(); }

  ArgHandler BeginCount(int num_args) { return BeginSum(num_args); }
  NumericExpr EndCount(ArgHandler handler) { return EndSum(handler); }

  ArgHandler BeginNumberOf(int num_args) {
    // TODO
    Base::BeginNumberOf(num_args);
    return ArgHandler(ls::LSExpression());
  }
  ls::LSExpression EndNumberOf(ArgHandler) {
    // TODO
    return ls::LSExpression();
  }

  ls::LSExpression MakeLogicalConstant(bool value) {
    return model_.createConstant(MakeInt(value));
  }

  ls::LSExpression MakeNot(ls::LSExpression arg) {
    return model_.createExpression(ls::O_Not, arg);
  }

  ls::LSExpression MakeBinaryLogical(
      expr::Kind kind, ls::LSExpression lhs, ls::LSExpression rhs);

  ls::LSExpression MakeRelational(
      expr::Kind kind, ls::LSExpression lhs, ls::LSExpression rhs);

  ls::LSExpression MakeLogicalCount(
      expr::Kind kind, ls::LSExpression lhs, ls::LSExpression rhs);

  ls::LSExpression MakeImplication(
      ls::LSExpression condition, ls::LSExpression true_expr,
      ls::LSExpression false_expr) {
    return MakeIf(condition, true_expr, false_expr);
  }

  ArgHandler BeginIteratedLogical(expr::Kind kind, int num_args) {
    ls::LSOperator op = ls::O_Or;
    if (kind == expr::FORALL)
      op = ls::O_And;
    else if (kind != expr::EXISTS)
      Base::BeginIteratedLogical(kind, num_args);
    return ArgHandler(model_.createExpression(op));
  }
  LogicalExpr EndIteratedLogical(ArgHandler handler) { return handler.expr(); }

  struct AllDiffArgHandler {
    std::vector<ls::LSExpression> args;

    explicit AllDiffArgHandler(int num_args) { args.reserve(num_args); }
    void AddArg(ls::LSExpression arg) { args.push_back(arg); }
  };

  AllDiffArgHandler BeginAllDiff(int num_args) {
    return AllDiffArgHandler(num_args);
  }

  ls::LSExpression EndAllDiff(AllDiffArgHandler handler);
};

class LocalSolver : public SolverImpl<LSProblemBuilder> {
 private:
  ls::LocalSolver solver_;
  int timelimit_;

  int GetTimeLimit(const SolverOption &) const { return timelimit_; }

  void SetTimeLimit(const SolverOption &opt, int value) {
    if (value <= 0)
      throw InvalidOptionValue(opt, value);
    timelimit_ = value;
  }

 public:
  LocalSolver();

  ls::LSModel model() { return solver_.getModel(); }

  void Solve(ProblemBuilder &builder, SolutionHandler &sh);

  ls::LSModel GetProblemBuilder(fmt::StringRef) { return solver_.getModel(); }
};
}  // namespace mp

#endif  // MP_SOLVERS_LOCALSOLVER_H_
