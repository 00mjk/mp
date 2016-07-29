/*
 SMPS writer implemented as an AMPL solver.

 Copyright (C) 2013 - 2016 AMPL Optimization Inc

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

#include "smpswriter/smpswriter.h"
#include "sp.h"
#include "mp/expr-visitor.h"

#include <cstdio>  // std::fopen
#include <limits>  // std::numeric_limits
#include <string>
#include <vector>

using mp::SPAdapter;

namespace {

class FileWriter {
 private:
  FILE *f_;
  FMT_DISALLOW_COPY_AND_ASSIGN(FileWriter);

 public:
  FileWriter(fmt::CStringRef filename) : f_(std::fopen(filename.c_str(), "w")) {
    if (!f_)
      throw fmt::SystemError(errno, "cannot open file '{}'", filename);
  }
  ~FileWriter() { std::fclose(f_); }

  void Write(fmt::CStringRef format, const fmt::ArgList &args) {
    fmt::print(f_, format, args);
  }
  FMT_VARIADIC(void, Write, fmt::CStringRef)
};

char GetConRHSAndType(double lb, double ub, double &rhs) {
  double inf = std::numeric_limits<double>::infinity();
  if (lb <= -inf) {
    rhs = ub;
    return ub >= inf ? 'N' : 'L';
  }
  rhs = lb;
  if (ub >= inf)
    return 'G';
  if (lb == ub)
    return 'E';
  throw mp::Error("SMPS writer doesn't support ranges");
}

void WriteTimeFile(fmt::CStringRef filename, const SPAdapter &sp) {
  FileWriter writer(filename);
  writer.Write(
    "TIME          PROBLEM\n"
    "PERIODS\n"
    "    C1        OBJ                      T1\n");
  if (sp.num_stages() > 1) {
    auto stage0 = sp.stage(0);
    writer.Write("    C{:<7}  R{:<7}                 T2\n",
                 stage0.num_vars() + 1, stage0.num_cons() + 1);
  }
  writer.Write("ENDATA\n");
}

class RHSHandler {
 private:
  std::vector<double> &rhs_;

 public:
  explicit RHSHandler(std::vector<double> &rhs) : rhs_(rhs) {}

  void OnTerm(int, int, double) {}
  void OnRHS(int con_index, double offset) { rhs_[con_index] += offset; }
};

class ScenarioWriter {
 private:
  FileWriter &writer_;

 public:
  explicit ScenarioWriter(FileWriter &w) : writer_(w) {}

  void OnTerm(int con_index, int var_index, double coef) {
    writer_.Write("    C{:<7}  R{:<7}  {}\n",
                  var_index + 1, con_index + 1, coef);
  }

  void OnRHS(int con_index, double offset) {
    writer_.Write("    RHS1      R{:<7}  {}\n", con_index + 1, offset);
  }
};

void WriteCoreFile(fmt::CStringRef filename, const SPAdapter &sp) {
  FileWriter writer(filename);
  writer.Write(
    "NAME          PROBLEM\n"
    "ROWS\n"
    " N  OBJ\n");
  int num_core_cons = sp.num_cons();
  std::vector<double> core_rhs(num_core_cons);
  for (int i = 0; i < num_core_cons; ++i) {
    auto con = sp.con(i);
    writer.Write(" {}  R{}\n",
                 GetConRHSAndType(con.lb(), con.ub(), core_rhs[i]), i + 1);
  }

  int int_var_index = 0;
  bool integer_block = false;
  writer.Write("COLUMNS\n");
  int num_core_vars = sp.num_vars();
  auto obj = sp.obj(0).linear_expr();
  auto obj_term = obj.begin(), obj_end = obj.end();
  for (int i = 0; i < num_core_vars; ++i) {
    auto var = sp.var(i);
    if (var.type() == mp::var::CONTINUOUS) {
      if (integer_block) {
        writer.Write("    INT{:<5}    'MARKER'      'INTEND'\n", int_var_index);
        integer_block = false;
      }
    } else if (!integer_block) {
      writer.Write("    INT{:<5}    'MARKER'      'INTORG'\n", ++int_var_index);
      integer_block = true;
    }

    if (i == obj_term->var_index()) {
      writer.Write("    C{:<7}  OBJ       {}\n", i + 1, obj_term->coef());
      if (obj_term != obj_end)
        ++obj_term;
    }
    auto column = sp.column(i);
    for (auto term = column.begin(), end = column.end(); term != end; ++term) {
      // TODO: merge with the first scenario
      writer.Write("    C{:<7}  R{:<7}  {}\n",
                   i + 1, term->con_index() + 1, term->coef());
    }
  }
  if (integer_block)
    writer.Write("    INT{:<5}    'MARKER'      'INTEND'\n", int_var_index);

  RHSHandler handler(core_rhs);
  sp.GetScenario(0, handler);

  writer.Write("RHS\n");
  for (int i = 0; i < num_core_cons; ++i) {
    if (auto rhs = core_rhs[i])
      writer.Write("    RHS1      R{:<7}  {}\n", i + 1, rhs);
  }

  class BoundsWriter {
   private:
    FileWriter &writer;
    bool has_bounds_;

   public:
    explicit BoundsWriter(FileWriter &w) : writer(w), has_bounds_(false) {}

    FileWriter &get() {
      if (!has_bounds_) {
        writer.Write("BOUNDS\n");
        has_bounds_ = true;
      }
      return writer;
    }
  } bw(writer);

  double inf = std::numeric_limits<double>::infinity();
  for (int i = 0; i < num_core_vars; ++i) {
    auto var = sp.var(i);
    double lb = var.lb(), ub = var.ub();
    if (lb != 0)
      bw.get().Write(" LO BOUND1      C{:<7}  {}\n", i + 1, lb);
    if (ub < inf)
      bw.get().Write(" UP BOUND1      C{:<7}  {}\n", i + 1, ub);
  }
  writer.Write("ENDATA\n");
}

void WriteDiscreteScenarios(FileWriter &writer, const SPAdapter &sp) {
  assert(sp.num_rvs() == 1);
  const auto &rv = sp.rv(0);
  writer.Write("SCENARIOS     DISCRETE\n");
  writer.Write(" SC SCEN1     'ROOT'    {:<12}   T1\n", rv.probability(0));
  for (size_t s = 1, num_scen = rv.num_realizations(); s < num_scen; ++s) {
    writer.Write(" SC SCEN{:<4}  SCEN1     {:<12}   T2\n",
                 s + 1, rv.probability(s));
    ScenarioWriter sw(writer);
    sp.GetScenario(s, sw);
  }
}

void WriteDiscreteIndep(FileWriter &writer, const SPAdapter &sp,
                        const std::vector<int> &rv2con) {
  writer.Write("INDEP         DISCRETE\n");
  std::vector<double> coefs, rhs;
  for (int i = 0, n = sp.num_rvs(); i < n; ++i) {
    const auto &rv = sp.rv(i);
    /*for (size_t s = 0, num_scen = rv.num_realizations(); s < num_scen; ++s) {
      rhs = base_rhs_;
      GetScenario(p, s, coefs, rhs);
      int con_index = rv2con[i];
      writer.Write("    RHS1      R{:<7}  {:12}   T2        {:.2}\n",
                   con_index + 1, rhs[con_index], rv.probability(s));
    }*/
  }
}

void WriteStochFile(fmt::CStringRef filename, const SPAdapter &sp) {
  FileWriter writer(filename);
  writer.Write("STOCH         PROBLEM\n");
  if (sp.num_stages() > 1) {
    int num_rvs = sp.num_rvs();
    if (num_rvs == 1) {
      WriteDiscreteScenarios(writer, sp);
    } else {
      // Get constraints where each random variable/parameter is used.
      bool indep_vars = true;
      std::vector<int> rv2con(num_rvs);
      for (int i = 0; i < num_rvs; ++i) {
        /*const auto &info = rv_info_[i];
        int var_index = info.var_index;
        int start = p.col_start(var_index), end = p.col_start(var_index + 1);
        if (start != end - 1) {
          indep_vars = false;
          break;
        }
        // TODO: handle randomness in constraint matrix
        rv2con[i] = p.row_index(start);*/
      }
      if (!indep_vars)
        throw mp::Error("unsupported RV");
      WriteDiscreteIndep(writer, sp, rv2con);
    }
  }
  writer.Write("ENDATA\n");
}
}  // namespace

namespace mp {

SMPSWriter::SMPSWriter()
  : SolverImpl<ColProblem>("smpswriter", "SMPSWriter", 20160620) {
  AddSuffix("stage", 0, suf::VAR);
}

void SMPSWriter::Solve(ColProblem &p, SolutionHandler &) {
  SPAdapter sp(p);
  std::string smps_basename = basename_;
  std::string::size_type ext_pos = smps_basename.rfind('.');
  if (ext_pos != std::string::npos)
    smps_basename.resize(ext_pos);
  WriteTimeFile(smps_basename + ".tim", sp);
  WriteCoreFile(smps_basename + ".cor", sp);
  WriteStochFile(smps_basename + ".sto", sp);
}

SolverPtr create_smpswriter(const char *) {
  return SolverPtr(new SMPSWriter());
}
}  // namespace mp
