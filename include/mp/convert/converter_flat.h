#ifndef CONVERTER_FLAT_H
#define CONVERTER_FLAT_H

#include <unordered_map>
#include <cmath>

#include "mp/convert/basic_converters.h"
#include "mp/expr-visitor.h"
#include "mp/convert/expr2constraint.h"
#include "mp/convert/model.h"
#include "mp/convert/std_constr.h"

namespace mp {

/// Result expression type for expression conversions
class EExpr : public AffineExpr {
public:
  EExpr() = default;
  EExpr(Constant c) : AffineExpr(c) {}
  EExpr(Variable v) : AffineExpr(v) {}
  EExpr(int i, double c) { AddTerm(i, c); }
};

/// BasicMPFlatConverter: it "flattens" most expressions by replacing them by a result variable and constraints
/// Such constraints might need to be decomposed, which is handled by overloaded methods in derived classes
template <class Impl, class Backend,
          class Model = BasicModel<std::allocator<char> > >
class BasicMPFlatConverter
    : public BasicMPConverter<Impl, Backend, Model>,
      public ExprVisitor<Impl, EExpr>,
      public BasicConstraintConverter
{
public:
  using VarArray = std::vector<int>;

protected:
  using ClassName = BasicMPFlatConverter<Impl, Backend, Model>;
  using BaseExprVisitor = ExprVisitor<Impl, EExpr>;

  using EExprArray = std::vector<EExpr>;

private:
  std::unordered_map<double, int> map_fixed_vars_;

protected:
  //////////////////////////// CREATE OR FIND A FIXED VARIABLE //////////////////////////////
  int MakeFixedVar(double value) {
    auto it = map_fixed_vars_.find(value);
    if (map_fixed_vars_.end()!=it)
      return it->second;
    auto v = this->AddVar(value, value);
    map_fixed_vars_[value] = v;
    return v;
  }

  //////////////////////////// UTILITIES /////////////////////////////////
  struct BoundsAndType {
    double lb_=Impl::MinusInfinity(), ub_=Impl::PlusInfinity();
    var::Type type_=var::CONTINUOUS;
    BoundsAndType(double l, double u, var::Type t) : lb_(l), ub_(u), type_(t) { }
  };

  BoundsAndType ComputeBoundsAndType(const AffineExpr& ae) {
    BoundsAndType result(ae.constant_term(), ae.constant_term(), var::INTEGER);
    for (const auto& term: ae) {
      auto v = this->GetModel().var(term.var_index());
      if (term.coef() >= 0.0) {
        result.lb_ += term.coef() * v.lb();
        result.ub_ += term.coef() * v.ub();
      } else {
        result.lb_ += term.coef() * v.ub();
        result.ub_ += term.coef() * v.lb();
      }
      if (var::INTEGER!=v.type() || std::floor(term.coef())!=std::ceil(term.coef()))
        result.type_=var::CONTINUOUS;
    }
    return result;
  }

public:

  //////////////////////////// CONVERTERS OF STANDRAD MP ITEMS //////////////////////////////
  ///
  ///////////////////////////////////////////////////////////////////////////////////////////
  void Convert(typename Model::MutCommonExpr e) {
    throw std::runtime_error("MPToMIPConverter: No common exprs convertible yet TODO");
  }

  void Convert(typename Model::MutObjective obj) {
    if (obj.nonlinear_expr())
      throw std::runtime_error("MPToMIPConverter: Only linear objectives allowed TODO");
  }

  void Convert(typename Model::MutAlgebraicCon con) {
    LinearExpr &linear = con.linear_expr();
    if (NumericExpr e = con.nonlinear_expr()) {
      linear.AddTerms(this->Visit(e));
      con.unset_nonlinear_expr();                  // delete the non-linear expr
    } // Modifying the original constraint by replacing the expr
  }

  void Convert(typename Model::MutLogicalCon e) {
    throw std::runtime_error("MPToMIPConverter: Only algebraic constraints implemented TODO");
  }


  //////////////////////////// CUSTOM CONSTRAINTS CONVERSION ////////////////////////////
  ///
  //////////////////////////// THE CONVERSION LOOP: BREADTH-FIRST ///////////////////////
  void ConvertExtraItems() {
    for (int endConstraintsThisLoop = 0, endPrevious = 0;
         (endConstraintsThisLoop = this->GetModel().num_custom_cons()) > endPrevious;
         endPrevious = endConstraintsThisLoop
         ) {
      PreprocessIntermediate();                        // preprocess before each level
      ConvertExtraItemsInRange(endPrevious, endConstraintsThisLoop);
    }
    PreprocessFinal();                                 // final prepro
  }

  void ConvertExtraItemsInRange(int first, int after_last) {
    for (; first<after_last; ++first) {
      auto* pConstraint = this->GetModel().custom_con(first);
      if (!pConstraint->IsRemoved()) {
        if (BasicConstraintAdder::Recommended !=
            pConstraint->BackendAcceptance(this->GetBackend())) {
          pConstraint->ConvertWith(*this);
          pConstraint->Remove();
        }
      }
    }
  }

  //////////////////////////// CUSTOM CONSTRAINTS CONVERSION ////////////////////////////
  ///
  //////////////////////////// SPECIFIC CONSTRAINT CONVERTERS ///////////////////////////

  USE_BASE_CONSTRAINT_CONVERTERS(BasicConstraintConverter)      // reuse default converters

  /// If backend does not like LDC, we can redefine it
  void Convert(const LinearDefiningConstraint& ldc) {
    this->AddConstraint(ldc.to_linear_constraint());
  }

  //////////////////////// PREPROCESSING /////////////////////////
  void PreprocessIntermediate() { }
  void PreprocessFinal() { }

public:
  //////////////////////// Add custom constraint ///////////////////////
  //////////////////////// Takes ownership /////////////////////////////
  void AddConstraint(BasicConstraintKeeper* pbc) {
    MP_DISPATCH( GetModel() ).AddConstraint(pbc);
  }
  template <class Constraint>
  void AddConstraint(Constraint&& con) {
    AddConstraint(makeConstraint<Impl, Constraint>(std::move(con)));
  }

public:
  //////////////////////////////////// Visitor Adapters /////////////////////////////////////////

  /// Convert an expression to an EExpr
  EExpr Convert2EExpr(Expr e) {
    return this->Visit(e);
  }

  /// From an expression:
  /// Adds a result variable r and constraint r == expr
  int Convert2Var(Expr e) {
    return Convert2Var( Convert2EExpr(e) );
  }
  int Convert2Var(EExpr ee) {
    if (ee.is_variable())
      return ee.get_representing_variable();
    if (ee.is_constant())
      return MakeFixedVar(ee.constant_term());
    auto bnt = ComputeBoundsAndType(ee);
    auto r = this->AddVar(bnt.lb_, bnt.ub_, bnt.type_);
    auto lck = makeConstraint<Impl, LinearDefiningConstraint>(std::move(ee), r);
    AddConstraint(lck);
    return r;
  }

  /// Generic functional expression array visitor
  /// Can produce a new variable/expression and specified constraints on it
  template <class FuncConstraint>
  EExpr VisitFunctional(typename BaseExprVisitor::VarArgExpr ea) {
    auto args = Exprs2Vars(ea);
    return VisitFunctional<FuncConstraint>(std::move(args));
  }

  template <class ExprArray>
  VarArray Exprs2Vars(const ExprArray& ea) {
    VarArray result;
    result.reserve(ea.num_args());
    for (const auto& e: ea)
      result.push_back( MP_DISPATCH( Convert2Var(e) ) );
    return result;
  }

  template <class FuncConstraint>
  EExpr VisitFunctional(VarArray&& va) {
    auto e2c = makeFuncConstrConverter<Impl, FuncConstraint>(*this, std::move(va));
    return EExpr::Variable{ e2c.Convert() };
  }


  ///////////////////////////////// EXPRESSION VISITORS ////////////////////////////////////
  ///
  //////////////////////////////////////////////////////////////////////////////////////////

  EExpr VisitNumericConstant(NumericConstant n) {
    return EExpr::Constant{ n.value() };
  }

  EExpr VisitVariable(Reference r) {
    return EExpr::Variable{ r.index() };
  }

  EExpr VisitMinus(UnaryExpr e) {
    auto ee = Convert2EExpr(e.arg());
    ee.Negate();
    return ee;
  }

  EExpr VisitAdd(BinaryExpr e) {
    auto ee = Convert2EExpr(e.lhs());
    ee.Add( Convert2EExpr(e.rhs()) );
    return ee;
  }

  EExpr VisitSub(BinaryExpr e) {
    auto el = Convert2EExpr(e.lhs());
    auto er = Convert2EExpr(e.rhs());
    er.Negate();
    el.Add(er);
    return el;
  }

  EExpr VisitMax(typename BaseExprVisitor::VarArgExpr e) {       // TODO why need Base:: here in g++ 9.2.1?
    return VisitFunctional<MaximumConstraint>(e);
  }

  EExpr VisitMin(typename BaseExprVisitor::VarArgExpr e) {
    return VisitFunctional<MinimumConstraint>(e);
  }

};


} // namespace mp

#endif // CONVERTER_FLAT_H
