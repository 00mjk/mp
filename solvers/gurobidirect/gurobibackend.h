#ifndef MP_GUROBI_BACKEND_H_
#define MP_GUROBI_BACKEND_H_

#ifdef __APPLE__
#include <limits.h>
#include <string.h>
#endif

#if __clang__
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wunused-parameter"
# pragma clang diagnostic ignored "-Wunused-private-field"
#elif _MSC_VER
# pragma warning(push)
# pragma warning(disable: 4244)
#endif

extern "C" {
  #include "gurobi_c.h"
}

#if __clang__
# pragma clang diagnostic pop
#elif _MSC_VER
# pragma warning(pop)
#endif

#include <string>

#include "mp/clock.h"
#include "mp/convert/model.h"
#include "mp/solver.h"

#include "mp/backend.h"
#include "mp/convert/std_constr.h"

namespace mp {

class GurobiBackend : public SolverImpl<BasicModel<std::allocator<char>>>,  // TODO no SolverImpl
    public BasicBackend<GurobiBackend>
{
  using BaseSolverImpl = SolverImpl<BasicModel<std::allocator<char>>>;
  using BaseBackend = BasicBackend<GurobiBackend>;
 private:
  GRBenv   *env   = NULL;
  GRBmodel *model = NULL;

  FMT_DISALLOW_COPY_AND_ASSIGN(GurobiBackend);


  struct Stats {
    steady_clock::time_point time;
    double setup_time;
    double solution_time;
  };
  Stats stats;

  void SolveWithGurobi(Problem &p,
                      Stats &stats, SolutionHandler &sh);

 public:
  GurobiBackend();
  ~GurobiBackend();

  void InitBackend();
  void CloseBackend();

  /// Model attributes
  bool IsMIP() const;
  bool IsQCP() const;

  int NumberOfConstraints() const;
  int NumberOfVariables() const;
  int NumberOfObjectives() const;

  /// Solution values
  void PrimalSolution(std::vector<double>& x);
  void DualSolution(std::vector<double>& pi);
  double ObjectiveValue() const;

  /// Solution attributes
  double NodeCount() const;
  double Niterations() const;

  static bool IsPlusMinusInf(double n) { return n<=MinusInfinity() || n>=Infinity(); }
  static double Infinity() { return GRB_INFINITY; }
  static double MinusInfinity() { return -GRB_INFINITY; }

  int GetGrbIntAttribute(const char* attr_id) const;
  double GetGrbDblAttribute(const char* attr_id) const;


  void Solve(Problem &p, SolutionHandler &sh);

 public:                    // [[ The interface ]]
  void Resolve(Problem& p, SolutionHandler &sh);

  /// [[ Surface the incremental interface ]]
  void InitProblemModificationPhase(const Problem& p);
  void AddVariables(int n, double* lbs, double* ubs, var::Type* types);
  /// Supporting linear stuff for now
  void AddLinearObjective( obj::Type sense, int nnz,
                           const double* c, const int* v);
  void AddLinearConstraint(int nnz, const double* c, const int* v,
                           double lb, double ub);

  //////////////////////////// GENERAL CONSTRAINTS ////////////////////////////
  USE_BASE_CONSTRAINT_HANDLERS(BaseBackend)

  ACCEPT_CONSTRAINT(MaximumConstraint, AcceptedButNotRecommended)
  void AddConstraint(const MaximumConstraint& mc);
  ACCEPT_CONSTRAINT(MinimumConstraint, AcceptedButNotRecommended)
  void AddConstraint(const MinimumConstraint& mc);
  ACCEPT_CONSTRAINT(DisjunctionConstraint, Recommended)
  void AddConstraint(const DisjunctionConstraint& mc);
  ACCEPT_CONSTRAINT(IndicatorConstraintLinLE, AcceptedButNotRecommended)
  void AddConstraint(const IndicatorConstraintLinLE& mc);

  void FinishProblemModificationPhase();

public:
 // Integer options.
 enum Option {
   DEBUGEXPR,
   USENUMBEROF,
   SOLUTION_LIMIT,
   NUM_OPTIONS
 };

private:
 int options_[NUM_OPTIONS];

 void ExportModel(const std::string& file);

 int GetOption(Option id) const {
   assert(id >= 0 && id < NUM_OPTIONS);
   return options_[id];
 }

 enum FileKind {
   DUMP_FILE,
   EXPORT_FILE,
   NUM_FILES
 };

 std::string filenames_[NUM_FILES];

 std::string GetOptimizer(const SolverOption &) const;
 void SetOptimizer(const SolverOption &opt, fmt::StringRef value);

 int DoGetIntOption(const SolverOption &, Option id) const {
   return options_[id];
 }
 void SetBoolOption(const SolverOption &opt, int value, Option id);
 void DoSetIntOption(const SolverOption &opt, int value, Option id);

 std::string GetFile(const SolverOption &, FileKind kind) const {
   assert(kind < NUM_FILES);
   return filenames_[kind];
 }
 void SetFile(const SolverOption &, fmt::StringRef filename, FileKind kind) {
   assert(kind < NUM_FILES);
   filenames_[kind] = filename.to_string();
 }

 // Returns an integer option of the CPLEX optimizer.
 int GetCPLEXIntOption(const SolverOption &opt, int param) const;

 // Sets an integer option of the CPLEX optimizer.
 void SetCPLEXIntOption(const SolverOption &opt, int value, int param);

};

}

#endif  // MP_GUROBI_BACKEND_H_
