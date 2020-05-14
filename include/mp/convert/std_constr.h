#ifndef STD_CONSTR_H
#define STD_CONSTR_H

#include <vector>

#include "mp/convert/basic_constr.h"
#include "mp/convert/affine_expr.h"

namespace mp {

////////////////////////////////////////////////////////////////////////
/// Standard linear constraint
class LinearConstraint : public BasicConstraint {
  const std::vector<double> coefs_;
  const std::vector<int> vars_;
  const double lb_, ub_;
public:
  template <class CV=std::vector<double>, class VV=std::vector<int> >
  LinearConstraint(CV&& c, VV&& v, double l, double u)
    : coefs_(std::forward<CV>(c)), vars_(std::forward<VV>(v)),
      lb_(l), ub_(u) { assert(coefs_.size()==vars_.size()); }
  int nnz() const { return (int)coefs_.size(); }
  const double* coefs() const { return coefs_.data(); }
  const int* vars() const { return vars_.data(); }
  double lb() const { return lb_; }
  double ub() const { return ub_; }
};

////////////////////////////////////////////////////////////////////////
/// Converting linear expr to 2 vectors.
struct LinearExprUnzipper {
  std::vector<double> c_;
  std::vector<int> v_;
  LinearExprUnzipper() { }
  LinearExprUnzipper(const LinearExpr& e) {
    Reserve(e.num_terms());
    for (LinearExpr::const_iterator it=e.begin(); it!=e.end(); ++it) {
      AddTerm(it->var_index(), it->coef());
    }
  }
  int num_terms() const { return c_.size(); }
  void Reserve(size_t s) { c_.reserve(s); v_.reserve(s); }
  void AddTerm(int v, double c) { c_.push_back(c); v_.push_back(v); }
};

////////////////////////////////////////////////////////////////////////
/// Linear Defining Constraint: r = affine_expr
class LinearDefiningConstraint :
    public BasicConstraint, public DefiningConstraint {
  AffineExpr affine_expr_;
public:
  using Arguments = AffineExpr;
  using DefiningConstraint::GetResultVar;
  LinearDefiningConstraint(AffineExpr&& ae, int r) :
    DefiningConstraint(r), affine_expr_(std::move(ae)) {
    /// TODO sort elements
  }
  const AffineExpr& GetAffineExpr() const { return affine_expr_; }
  LinearConstraint to_linear_constraint() const {
    const auto& ae = GetAffineExpr();
    LinearExprUnzipper aeu(ae);
    aeu.AddTerm(DefiningConstraint::GetResultVar(), -1.0);
    return LinearConstraint(std::move(aeu.c_), std::move(aeu.v_),
                            -ae.constant_term(), -ae.constant_term());
  }
};

////////////////////////////////////////////////////////////////////////
struct MaximumConstraintId {
  static constexpr auto description_ = "r = max(v1, v2, ..., vn)";
};
using MaximumConstraint =
   CustomDefiningConstraint<VarArrayArgConstraint<>, MaximumConstraintId>;

////////////////////////////////////////////////////////////////////////
struct MinimumConstraintId {
  static constexpr auto description_ = "r = min(v1, v2, ..., vn)";
};
using MinimumConstraint =
   CustomDefiningConstraint<VarArrayArgConstraint<>, MinimumConstraintId>;

////////////////////////////////////////////////////////////////////////
struct NotEqualId {
  static constexpr auto description_ = "r = (v1 != v2)";
};
using NEConstraint =
   CustomDefiningConstraint<VarArray2ArgConstraint, NotEqualId>;

////////////////////////////////////////////////////////////////////////
struct LessOrEqualId {
  static constexpr auto description_ = "r = (v1 != v2)";
};
using LEConstraint =
   CustomDefiningConstraint<VarArray2ArgConstraint, LessOrEqualId>;

////////////////////////////////////////////////////////////////////////
struct DisjunctionId {
  static constexpr auto description_ = "r = (v1 || v2)";
};
using DisjunctionConstraint =
   CustomDefiningConstraint<VarArray2ArgConstraint, DisjunctionId>;

////////////////////////////////////////////////////////////////////////
/// Indicator: b==bv -> c'x <= rhs
class IndicatorConstraintLinLE: public BasicConstraint {
public:
  const int b_=-1;                            // the indicator variable
  const int bv_=1;                            // the value, 0/1
  const std::vector<double> c_;
  const std::vector<int> v_;
  const double rhs_;
  /// Getters
  int get_binary_var() const { return b_; }
  int get_binary_value() const { return bv_; }
  bool is_binary_value_1() const { return 1==get_binary_value(); }
  const std::vector<double>& get_lin_coefs() const { return c_; }
  const std::vector<int>& get_lin_vars() const { return v_; }
  double get_lin_rhs() const { return rhs_; }
  /// Produces affine expr ae so that the inequality is equivalent to ae<=0.0
  AffineExpr to_lhs_affine_expr() const {
    return {get_lin_coefs(), get_lin_vars(), -get_lin_rhs()};
  }
  /// Constructor
  template <class CV=std::vector<double>, class VV=std::vector<int> >
  IndicatorConstraintLinLE(int b, int bv,
                           CV&& c, VV&& v,
                           double rhs) :
    b_(b), bv_(bv), c_(std::forward<CV>(c)), v_(std::forward<VV>(v)), rhs_(rhs)
  { assert(check()); }
  bool check() const { return (b_>=0) && (bv_==0 || bv_==1); }
};

} // namespace mp

#endif // STD_CONSTR_H
