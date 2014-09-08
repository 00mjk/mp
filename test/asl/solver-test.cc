/*
 Solver tests.

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

#include <gtest/gtest.h>
#include "asl/aslsolver.h"
#include "../args.h"
#include "../solution-handler.h"
#include "../util.h"
#include "../gtest-extra.h"
#include "stderr-redirect.h"

#include <cstdio>

#ifdef _WIN32
# define putenv _putenv
#endif

#ifndef MP_TEST_DATA_DIR
# define MP_TEST_DATA_DIR "../data"
#endif

using mp::InvalidOptionValue;
using mp::OptionError;
using mp::Problem;
using mp::Solver;
using mp::SolverOption;
using mp::internal::OptionHelper;

namespace {

typedef Solver::OptionPtr SolverOptionPtr;

struct TestSolver : mp::ASLSolver {
  TestSolver(const char *name = "testsolver",
      const char *long_name = 0, long date = 0)
  : ASLSolver(name, long_name, date) {}

  using Solver::set_long_name;
  using Solver::set_version;
  using Solver::AddOption;
  using Solver::AddSuffix;
  using Solver::HandleSolution;

  bool ParseOptions(char **argv,
      unsigned flags = Solver::NO_OPTION_ECHO, const Problem *p = 0) {
    return Solver::ParseOptions(argv, flags, p);
  }

  void DoSolve(Problem &) {}
};

void CheckObjPrecision(int precision) {
  double value = 12.3456789123456789;
  TestSolver solver;
  EXPECT_EQ(fmt::format("{:.{}}", value, precision),
            fmt::format("{}", solver.FormatObjValue(value)));
}

TEST(SolverTest, FormatObjValue) {
  CheckObjPrecision(Solver::DEFAULT_PRECISION);
  static char options[] = "objective_precision=0";
  putenv(options);
  CheckObjPrecision(Solver::DEFAULT_PRECISION);
  static char options2[] = "objective_precision=7";
  putenv(options2);
  CheckObjPrecision(7);
}

TEST(SolverTest, EmptyValueArrayRef) {
  mp::ValueArrayRef r;
  EXPECT_EQ(0, r.size());
  EXPECT_EQ(r.begin(), r.end());
}

TEST(SolverTest, ValueArrayRef) {
  const mp::OptionValueInfo values[] = {
      {"val1", "description of val1", 0},
      {"val2", "description of val2", 0}
  };
  mp::ValueArrayRef r(values);
  EXPECT_EQ(2, r.size());
  mp::ValueArrayRef::iterator i = r.begin();
  EXPECT_NE(i, r.end());
  EXPECT_STREQ("val1", i->value);
  ++i;
  EXPECT_STREQ("val2", i->value);
  ++i;
  EXPECT_EQ(i, r.end());
}

TEST(SolverTest, ValueArrayRefOffset) {
  const mp::OptionValueInfo values[] = {
      {"val1", 0, 0}, {"val2", 0, 0}
  };
  mp::ValueArrayRef r(values, 1);
  EXPECT_EQ(1, r.size());
  mp::ValueArrayRef::iterator i = r.begin();
  EXPECT_STREQ("val2", i->value);
  EXPECT_EQ(r.end(), ++i);
}

TEST(SolverTest, ValueArrayRefInvalidOffset) {
  const mp::OptionValueInfo values[] = {
      {"val1", 0, 0}, {"val2", 0, 0}
  };
  EXPECT_DEBUG_DEATH(
      mp::ValueArrayRef r(values, -1);, "Assertion");  // NOLINT(*)
  EXPECT_DEBUG_DEATH(
      mp::ValueArrayRef r(values, 2);, "Assertion");  // NOLINT(*)
}

// A wrapper around mp::internal::FormatRST used to simplify testing.
std::string FormatRST(fmt::StringRef s,
    int indent = 0, mp::ValueArrayRef values = mp::ValueArrayRef()) {
  fmt::Writer w;
  mp::internal::FormatRST(w, s, indent, values);
  return w.str();
}

TEST(FormatRSTTest, IndentAndWrapText) {
  EXPECT_EQ(
    "     This is a very long option description that should be indented and\n"
    "     wrapped.\n",
    FormatRST(
          "This is a very long option description "
          "that should be indented and wrapped.", 5));
}

TEST(FormatRSTTest, RemoveLeadingWhitespace) {
  EXPECT_EQ(
    "Leading whitespace should be removed.\n",
    FormatRST(" \t\v\fLeading whitespace should be removed."));
}

TEST(FormatRSTTest, FormatParagraph) {
  EXPECT_EQ(
    "This is the first paragraph.\n"
    "\n"
    "This is the second paragraph.\n",
    FormatRST("This is the first paragraph.\n\nThis is the second paragraph."));
}

TEST(FormatRSTTest, FormatBulletList) {
  EXPECT_EQ(
    "* item1\n"
    "\n"
    "* item2\n",
    FormatRST("* item1\n* item2"));
}

TEST(FormatRSTTest, FormatLiteralBlock) {
  EXPECT_EQ(
    "   line1\n"
    "   line2\n",
    FormatRST(
        "::\n\n  line1\n  line2"));
}

TEST(FormatRSTTest, FormatLineBlock) {
  EXPECT_EQ(
    "line1\n"
    "line2\n",
    FormatRST("| line1\n| line2"));
}

TEST(FormatRSTTest, FormatRSTValueTable) {
  const mp::OptionValueInfo values[] = {
      {"val1", "description of val1", 0},
      {"val2", "description of val2", 0}
  };
  EXPECT_EQ(
    "  val1 - description of val1\n"
    "  val2 - description of val2\n",
    FormatRST(".. value-table::", 2, values));
}

TEST(FormatRSTTest, FormatRSTValueList) {
  const mp::OptionValueInfo values[] = {
      {"val1", 0, 0},
      {"val2", 0, 0}
  };
  EXPECT_EQ(
    "  val1\n"
    "  val2\n",
    FormatRST(".. value-table::", 0, values));
}

TEST(SolverTest, BasicSolverCtor) {
  TestSolver s;
  EXPECT_STREQ("testsolver", s.name());
  EXPECT_STREQ("testsolver", s.long_name());
  EXPECT_STREQ("testsolver", s.version());
  EXPECT_EQ(0, s.date());
  EXPECT_EQ(0, s.flags());
  EXPECT_EQ(0, s.wantsol());
}

TEST(SolverTest, BasicSolverVirtualDtor) {
  bool destroyed = false;
  class DtorTestSolver : public Solver {
   private:
    bool &destroyed_;

   public:
    DtorTestSolver(bool &destroyed)
    : Solver("test", 0, 0), destroyed_(destroyed) {}
    ~DtorTestSolver() { destroyed_ = true; }
    void DoSolve(Problem &) {}
    void ReadNL(fmt::StringRef) {}
  };
  (DtorTestSolver(destroyed));
  EXPECT_TRUE(destroyed);
}

TEST(SolverTest, NameInUsage) {
  TestSolver s("solver-name", "long-solver-name");
  s.set_version("solver-version");
  Problem p;
  OutputRedirect redirect(stderr);
  s.ProcessArgs(Args("program-name"), p);
  std::string usage = "usage: solver-name ";
  EXPECT_EQ(usage, redirect.restore_and_read().substr(0, usage.size()));
}

TEST(SolverTest, LongName) {
  EXPECT_STREQ("solver-name", TestSolver("solver-name").long_name());
  EXPECT_STREQ("long-solver-name",
      TestSolver("solver-name", "long-solver-name").long_name());
  TestSolver s("solver-name");
  s.set_long_name("another-name");
  EXPECT_STREQ("another-name", s.long_name());
}

TEST(SolverTest, Version) {
  TestSolver s("testsolver", "Test Solver");
  EXPECT_EXIT({
    FILE *f = freopen("out", "w", stdout);
    Problem p;
    s.ProcessArgs(Args("program-name", "-v"), p);
    fclose(f);
  }, ::testing::ExitedWithCode(0), "");
  fmt::Writer w;
  w.write("Test Solver ({}), ASL({})\n", MP_SYSINFO, MP_DATE);
  EXPECT_EQ(w.str(), ReadFile("out"));
}

TEST(SolverTest, VersionWithDate) {
  TestSolver s("testsolver", "Test Solver", 20121227);
  EXPECT_EXIT({
    FILE *f = freopen("out", "w", stdout);
    Problem p;
    s.ProcessArgs(Args("program-name", "-v"), p);
    fclose(f);
  }, ::testing::ExitedWithCode(0), "");
  fmt::Writer w;
  w.write("Test Solver ({}), driver(20121227), ASL({})\n",
    MP_SYSINFO, MP_DATE);
  EXPECT_EQ(w.str(), ReadFile("out"));
}

TEST(SolverTest, SetVersion) {
  TestSolver s("testsolver", "Test Solver");
  const char *VERSION = "Solver Version 3.0";
  s.set_version(VERSION);
  EXPECT_STREQ(VERSION, s.version());
  EXPECT_EXIT({
    FILE *f = freopen("out", "w", stdout);
    Problem p;
    s.ProcessArgs(Args("program-name", "-v"), p);
    fclose(f);
  }, ::testing::ExitedWithCode(0), "");
  fmt::Writer w;
  w.write("{} ({}), ASL({})\n", VERSION, MP_SYSINFO, MP_DATE);
  EXPECT_EQ(w.str(), ReadFile("out"));
}

TEST(SolverTest, ErrorHandler) {
  struct TestErrorHandler : mp::ErrorHandler {
    std::string message;

    virtual ~TestErrorHandler() {}
    void HandleError(fmt::StringRef message) {
      this->message = message;
    }
  };

  TestErrorHandler eh;
  TestSolver s("test");
  s.set_error_handler(&eh);
  EXPECT_TRUE(&eh == s.error_handler());
  s.ReportError("test message");
  EXPECT_EQ("test message", eh.message);
}

TEST(SolverTest, OutputHandler) {
  struct TestOutputHandler : mp::OutputHandler {
    std::string output;

    virtual ~TestOutputHandler() {}
    void HandleOutput(fmt::StringRef output) {
      this->output += output;
    }
  };

  TestOutputHandler oh;
  TestSolver s("test");
  s.set_output_handler(&oh);
  EXPECT_TRUE(&oh == s.output_handler());
  s.Print("line {}\n", 1);
  s.Print("line {}\n", 2);
  EXPECT_EQ("line 1\nline 2\n", oh.output);
}

TEST(SolverTest, SolutionHandler) {
  TestSolutionHandler sh;
  TestSolver s("test");
  s.set_solution_handler(&sh);
  EXPECT_TRUE(&sh == s.solution_handler());
  Problem p;
  double primal = 0, dual = 0, obj = 42;
  s.HandleSolution(p, "test message", &primal, &dual, obj);
  EXPECT_EQ(&p, sh.problem());
  EXPECT_EQ("test message", sh.message());
  EXPECT_EQ(&primal, sh.primal());
  EXPECT_EQ(&dual, sh.dual());
  EXPECT_EQ(42.0, sh.obj_value());
}

TEST(SolverTest, ReadProblem) {
  TestSolver s("test");
  Problem p;
  EXPECT_TRUE(s.ProcessArgs(
                Args("testprogram", MP_TEST_DATA_DIR "/objconst.nl"), p));
  EXPECT_EQ(1, p.num_vars());
}

TEST(SolverTest, ReadProblemNoStub) {
  StderrRedirect redirect("out");
  TestSolver s("test");
  Problem p;
  EXPECT_FALSE(s.ProcessArgs(Args("testprogram"), p));
  EXPECT_EQ(0, p.num_vars());
}

TEST(SolverTest, ReadingMinOrMaxWithZeroArgsFails) {
  const char *names[] = {"min", "max"};
  for (size_t i = 0, n = sizeof(names) / sizeof(*names); i < n; ++i) {
    std::string stub =
        fmt::format(MP_TEST_DATA_DIR "/{}-with-zero-args", names[i]);
    Problem p;
    EXPECT_THROW_MSG(p.Read(stub), mp::Error,
                     fmt::format("{}.nl:13:1: too few arguments", stub));
  }
}

TEST(SolverTest, ReportError) {
  TestSolver s("test");
  EXPECT_EXIT({
    s.ReportError("File not found: {}", "somefile");
    exit(0);
  }, ::testing::ExitedWithCode(0), "File not found: somefile");
}

TEST(SolverTest, ProcessArgsReadsProblem) {
  TestSolver s;
  Problem p;
  EXPECT_TRUE(s.ProcessArgs(
                Args("testprogram", MP_TEST_DATA_DIR "/objconst.nl"), p));
  EXPECT_EQ(1, p.num_vars());
}

TEST(SolverTest, ProcessArgsParsesSolverOptions) {
  TestSolver s;
  Problem p;
  EXPECT_TRUE(s.ProcessArgs(
      Args("testprogram", MP_TEST_DATA_DIR "/objconst.nl", "wantsol=5"),
      p, Solver::NO_OPTION_ECHO));
  EXPECT_EQ(5, s.wantsol());
}

TEST(SolverTest, ProcessArgsWithouStub) {
  StderrRedirect redirect("out");
  TestSolver s;
  Problem p;
  EXPECT_FALSE(s.ProcessArgs(Args("testprogram"), p));
  EXPECT_EQ(0, p.num_vars());
}

TEST(SolverTest, ProcessArgsError) {
  TestSolver s;
  Problem p;
  std::string message;
  try {
    s.ProcessArgs(Args("testprogram", "nonexistent"), p);
  } catch (const fmt::SystemError &e) {
    message = e.what();
  }
  EXPECT_EQ(message.find("cannot open file nonexistent.nl"), 0);
}

TEST(SolverTest, SignalHandler) {
  std::signal(SIGINT, SIG_DFL);
  TestSolver s;
  EXPECT_EXIT({
    FILE *f = freopen("out", "w", stdout);
    mp::SignalHandler sh(s);
    fmt::print("{}", mp::SignalHandler::stop());
    std::fflush(stdout);
    std::raise(SIGINT);
    fmt::print("{}", mp::SignalHandler::stop());
    fclose(f);
    exit(0);
  }, ::testing::ExitedWithCode(0), "");
  EXPECT_EQ("0\n<BREAK> (testsolver)\n1", ReadFile("out"));
}

TEST(SolverTest, SignalHandlerExitOnTwoSIGINTs) {
  std::signal(SIGINT, SIG_DFL);
  TestSolver s;
  EXPECT_EXIT({
    mp::SignalHandler sh(s);
    FILE *f = freopen("out", "w", stdout);
    std::raise(SIGINT);
    std::raise(SIGINT);
    std::fclose(f); // Unreachable, but silences a warning.
  }, ::testing::ExitedWithCode(1), "");
  EXPECT_EQ("\n<BREAK> (testsolver)\n\n<BREAK> (testsolver)\n",
      ReadFile("out"));
}

// ----------------------------------------------------------------------------
// Option tests

TEST(SolverTest, SolverOption) {
  struct TestOption : SolverOption {
    bool formatted, parsed;
    TestOption(const char *name, const char *description)
    : SolverOption(name, description), formatted(false), parsed(false) {}
    TestOption(const char *name, const char *description,
        mp::ValueArrayRef values, bool is_flag)
    : SolverOption(name, description, values, is_flag),
      formatted(false), parsed(false) {}
    void Write(fmt::Writer &) { formatted = true; }
    void Parse(const char *&) { parsed = true; }
  };
  {
    TestOption opt("abc", "def");
    EXPECT_STREQ("abc", opt.name());
    EXPECT_STREQ("def", opt.description());
    EXPECT_EQ(0, opt.values().size());
    EXPECT_FALSE(opt.is_flag());
  }
  {
    const mp::OptionValueInfo VALUES[] = {
        {"value1", "description1", 0},
        {"value2", "description2", 0},
    };
    TestOption opt("", "", VALUES, true);
    EXPECT_TRUE(opt.is_flag());
    EXPECT_EQ(2, opt.values().size());
    EXPECT_EQ(VALUES, opt.values().begin());
    EXPECT_EQ(VALUES + 1, opt.values().begin() + 1);
  }
  {
    TestOption opt("", "");
    EXPECT_FALSE(opt.formatted);
    EXPECT_FALSE(opt.parsed);
    SolverOption &so = opt;
    fmt::Writer w;
    so.Write(w);
    EXPECT_TRUE(opt.formatted);
    const char *s = 0;
    so.Parse(s);
    EXPECT_TRUE(opt.parsed);
  }
}

TEST(SolverTest, IntOptionHelper) {
  fmt::Writer w;
  OptionHelper<int>::Write(w, 42);
  EXPECT_EQ("42", w.str());
  const char *start = "123 ";
  const char *s = start;
  EXPECT_EQ(123, OptionHelper<int>::Parse(s));
  EXPECT_EQ(start + 3, s);
  EXPECT_EQ(42, OptionHelper<int>::CastArg(42));
}

TEST(SolverTest, DoubleOptionHelper) {
  fmt::Writer w;
  OptionHelper<double>::Write(w, 4.2);
  EXPECT_EQ("4.2", w.str());
  const char *start = "1.23 ";
  const char *s = start;
  EXPECT_EQ(1.23, OptionHelper<double>::Parse(s));
  EXPECT_EQ(start + 4, s);
  EXPECT_EQ(4.2, OptionHelper<double>::CastArg(4.2));
}

TEST(SolverTest, StringOptionHelper) {
  fmt::Writer w;
  OptionHelper<std::string>::Write(w, "abc");
  EXPECT_EQ("abc", w.str());
  const char *start = "def ";
  const char *s = start;
  EXPECT_EQ("def", OptionHelper<std::string>::Parse(s));
  EXPECT_EQ(start + 3, s);
  EXPECT_STREQ("abc", OptionHelper<std::string>::CastArg(std::string("abc")));
}

TEST(SolverTest, TypedSolverOption) {
  struct TestOption : mp::TypedSolverOption<int> {
    int value;
    TestOption(const char *name, const char *description)
    : mp::TypedSolverOption<int>(name, description), value(0) {}
    int GetValue() const { return value; }
    void SetValue(int value) { this->value = value; }
  };
  TestOption opt("abc", "def");
  EXPECT_STREQ("abc", opt.name());
  EXPECT_STREQ("def", opt.description());
  EXPECT_FALSE(opt.is_flag());
  const char *start = "42";
  const char *s = start;
  opt.Parse(s);
  EXPECT_EQ(start + 2, s);
  EXPECT_EQ(42, opt.value);
  fmt::Writer w;
  opt.Write(w);
  EXPECT_EQ("42", w.str());
}

enum Info { INFO = 0xcafe };

struct TestSolverWithOptions : Solver {
  int intopt1;
  int intopt2;
  double dblopt1;
  double dblopt2;
  std::string stropt1;
  std::string stropt2;

  int GetIntOption(const SolverOption &) const { return intopt1; }
  void SetIntOption(const SolverOption &opt, int value) {
    EXPECT_STREQ("intopt1", opt.name());
    intopt1 = value;
  }

  int GetIntOptionWithInfo(const SolverOption &, Info) const { return 0; }
  void SetIntOptionWithInfo(const SolverOption &opt, int value, Info info) {
    EXPECT_STREQ("intopt2", opt.name());
    intopt2 = value;
    EXPECT_EQ(INFO, info);
  }

  double GetDblOption(const SolverOption &) const { return dblopt1; }
  void SetDblOption(const SolverOption &opt, double value) {
    EXPECT_STREQ("dblopt1", opt.name());
    dblopt1 = value;
  }

  double GetDblOptionWithInfo(const SolverOption &, Info) const { return 0; }
  void SetDblOptionWithInfo(const SolverOption &opt, double value, Info info) {
    EXPECT_STREQ("dblopt2", opt.name());
    dblopt2 = value;
    EXPECT_EQ(INFO, info);
  }

  std::string GetStrOption(const SolverOption &) const { return stropt1; }
  void SetStrOption(const SolverOption &opt, const char *value) {
    EXPECT_STREQ("stropt1", opt.name());
    stropt1 = value;
  }

  std::string GetStrOptionWithInfo(const SolverOption &, Info) const {
    return "";
  }
  void SetStrOptionWithInfo(
      const SolverOption &opt, const char *value, Info info) {
    EXPECT_STREQ("stropt2", opt.name());
    stropt2 = value;
    EXPECT_EQ(INFO, info);
  }

  TestSolverWithOptions() : Solver("testsolver"),
    intopt1(0), intopt2(0), dblopt1(0), dblopt2(0) {
    AddIntOption("intopt1", "Integer option 1",
        &TestSolverWithOptions::GetIntOption,
        &TestSolverWithOptions::SetIntOption);
    AddIntOption("intopt2", "Integer option 2",
        &TestSolverWithOptions::GetIntOptionWithInfo,
        &TestSolverWithOptions::SetIntOptionWithInfo, INFO);
    AddDblOption("dblopt1", "Double option 1",
        &TestSolverWithOptions::GetDblOption,
        &TestSolverWithOptions::SetDblOption);
    AddDblOption("dblopt2", "Double option 2",
        &TestSolverWithOptions::GetDblOptionWithInfo,
        &TestSolverWithOptions::SetDblOptionWithInfo, INFO);
    AddStrOption("stropt1", "Double option 1",
        &TestSolverWithOptions::GetStrOption,
        &TestSolverWithOptions::SetStrOption);
    AddStrOption("stropt2", "Double option 2",
        &TestSolverWithOptions::GetStrOptionWithInfo,
        &TestSolverWithOptions::SetStrOptionWithInfo, INFO);
  }

  bool ParseOptions(char **argv,
      unsigned flags = Solver::NO_OPTION_ECHO, const Problem *p = 0) {
    return Solver::ParseOptions(argv, flags, p);
  }

  void DoSolve(Problem &) {}
  void ReadNL(fmt::StringRef) {}
};

TEST(SolverTest, AddOption) {
  struct TestOption : SolverOption {
    int value;
    TestOption() : SolverOption("testopt", "A test option."), value(0) {}

    void Write(fmt::Writer &) {}
    void Parse(const char *&s) {
      char *end = 0;
      value = std::strtol(s, &end, 10);
      s = end;
    }
  };
  TestSolver s;
  TestOption *opt = 0;
  s.AddOption(SolverOptionPtr(opt = new TestOption()));
  EXPECT_TRUE(s.ParseOptions(Args("testopt=42"), Solver::NO_OPTION_ECHO));
  EXPECT_EQ(42, opt->value);
}

TEST(SolverTest, OptionHeader) {
  struct OptionTestSolver : Solver {
    OptionTestSolver() : Solver("testsolver") {}
    void set_option_header() {
      Solver::set_option_header("test header");
    }
    void DoSolve(Problem &) {}
    void ReadNL(fmt::StringRef) {}
  } s;
  EXPECT_STREQ("", s.option_header());
  s.set_option_header();
  EXPECT_STREQ("test header", s.option_header());
}

TEST(SolverTest, NumOptions) {
  int num_std_options = TestSolver().num_options();
  EXPECT_EQ(num_std_options + 6, TestSolverWithOptions().num_options());
}

TEST(SolverTest, OptionIterator) {
  TestSolverWithOptions s;
  Solver::option_iterator i = s.option_begin();
  EXPECT_NE(i, s.option_end());
  EXPECT_STREQ("dblopt1", i->name());
  Solver::option_iterator i2 = i++;
  EXPECT_STREQ("dblopt2", i->name());
  EXPECT_NE(i, i2);
  ++i2;
  EXPECT_EQ(i, i2);
  Solver::option_iterator i3 = ++i;
  EXPECT_EQ(i, i3);
  EXPECT_NE(i, i2);
  int count = 0;
  for (Solver::option_iterator
      j = s.option_begin(), e = s.option_end(); j != e; ++j) {
    ++count;
  }
  EXPECT_EQ(TestSolver().num_options() + 6, count);
}

TEST(SolverTest, ParseOptionsFromArgs) {
  TestSolverWithOptions s;
  EXPECT_TRUE(s.ParseOptions(Args("intopt1=5 intopt2=7")));
  EXPECT_EQ(5, s.intopt1);
  EXPECT_EQ(7, s.intopt2);
}

TEST(SolverTest, ParseOptionsFromEnvVar) {
  TestSolverWithOptions s;
  static char options[] = "testsolver_options=intopt1=9 intopt2=11";
  putenv(options);
  EXPECT_TRUE(s.ParseOptions(Args(0)));
  EXPECT_EQ(9, s.intopt1);
  EXPECT_EQ(11, s.intopt2);
  static char reset_options[] = "testsolver_options=";
  putenv(reset_options);
}

TEST(SolverTest, ParseOptionsNoArgs) {
  TestSolver s;
  EXPECT_TRUE(s.ParseOptions(Args(0)));
}

TEST(SolverTest, ParseOptionsSkipsWhitespace) {
  TestSolverWithOptions s;
  EXPECT_TRUE(s.ParseOptions(Args(
      " \t\r\n\vintopt1 \t\r\n\v= \t\r\n\v5"
      " \t\r\n\vintopt2 \t\r\n\v7 \t\r\n\v")));
  EXPECT_EQ(5, s.intopt1);
  EXPECT_EQ(7, s.intopt2);
}

TEST(SolverTest, ParseOptionsCaseInsensitiveName) {
  TestSolverWithOptions s;
  EXPECT_TRUE(s.ParseOptions(Args("IntOpt1=42")));
  EXPECT_EQ(42, s.intopt1);
  EXPECT_TRUE(s.ParseOptions(Args("INTOPT1=21")));
  EXPECT_EQ(21, s.intopt1);
}

TEST(SolverTest, ParseOptionsNoEqualSign) {
  TestSolverWithOptions s;
  EXPECT_TRUE(s.ParseOptions(Args("stropt1 abc")));
  EXPECT_EQ("abc", s.stropt1);
}

struct TestErrorHandler : mp::ErrorHandler {
  std::vector<std::string> errors;

  virtual ~TestErrorHandler() {}
  void HandleError(fmt::StringRef message) {
    errors.push_back(message);
  }
};

TEST(SolverTest, UnknownOption) {
  TestSolverWithOptions s;
  TestErrorHandler handler;
  s.set_error_handler(&handler);
  EXPECT_FALSE(s.ParseOptions(Args("badopt1=3 badopt2 intopt1=42 badopt3")));
  EXPECT_EQ(3u, handler.errors.size());
  EXPECT_EQ("Unknown option \"badopt1\"", handler.errors[0]);
  EXPECT_EQ("Unknown option \"badopt2\"", handler.errors[1]);
  EXPECT_EQ("Unknown option \"badopt3\"", handler.errors[2]);
  EXPECT_EQ(42, s.intopt1);
}

TEST(SolverTest, HandleUnknownOption) {
  struct TestSolver : Solver {
    std::string option_name;
    TestSolver() : Solver("test", 0, 0) {}
    void DoSolve(Problem &) {}
    void ReadNL(fmt::StringRef) {}
    void HandleUnknownOption(const char *name) { option_name = name; }
  };
  TestSolver s;
  s.ParseOptions(Args("BadOption"));
  EXPECT_EQ("BadOption", s.option_name);
}

TEST(SolverTest, ParseOptionRecovery) {
  TestSolverWithOptions s;
  TestErrorHandler handler;
  s.set_error_handler(&handler);
  // After encountering an unknown option without "=" parsing should skip
  // everything till the next known option.
  EXPECT_FALSE(s.ParseOptions(Args("badopt1 3 badopt2=1 intopt1=42 badopt3")));
  EXPECT_EQ(2u, handler.errors.size());
  EXPECT_EQ("Unknown option \"badopt1\"", handler.errors[0]);
  EXPECT_EQ("Unknown option \"badopt3\"", handler.errors[1]);
  EXPECT_EQ(42, s.intopt1);
}

struct FormatOption : SolverOption {
  int format_count;
  FormatOption() : SolverOption("fmtopt", ""), format_count(0) {}

  void Write(fmt::Writer &w) {
    w << "1";
    ++format_count;
  }
  void Parse(const char *&) {}
};

TEST(SolverTest, FormatOption) {
  EXPECT_EXIT({
    TestSolver s;
    FILE *f = freopen("out", "w", stdout);
    s.AddOption(SolverOptionPtr(new FormatOption()));
    s.ParseOptions(Args("fmtopt=?"), 0);
    printf("---\n");
    s.ParseOptions(Args("fmtopt=?", "fmtopt=?"), 0);
    fclose(f);
    exit(0);
  }, ::testing::ExitedWithCode(0), "");
  EXPECT_EQ("fmtopt=1\n---\nfmtopt=1\nfmtopt=1\n", ReadFile("out"));
}

TEST(SolverTest, OptionNotPrintedWhenEchoOff) {
  TestSolver s;
  FormatOption *opt = 0;
  s.AddOption(SolverOptionPtr(opt = new FormatOption()));
  EXPECT_EQ(0, opt->format_count);
  s.ParseOptions(Args("fmtopt=?"));
  EXPECT_EQ(0, opt->format_count);
}

TEST(SolverTest, NoEchoWhenPrintingOption) {
  EXPECT_EXIT({
    TestSolver s;
    FILE *f = freopen("out", "w", stdout);
    s.AddOption(SolverOptionPtr(new FormatOption()));
    s.ParseOptions(Args("fmtopt=?"));
    fclose(f);
    exit(0);
  }, ::testing::ExitedWithCode(0), "");
  EXPECT_EQ("", ReadFile("out"));
}

TEST(SolverTest, QuestionMarkInOptionValue) {
  TestSolverWithOptions s;
  s.ParseOptions(Args("stropt1=?x"));
  EXPECT_EQ("?x", s.stropt1);
}

TEST(SolverTest, ErrorOnKeywordOptionValue) {
  struct KeywordOption : SolverOption {
    bool parsed;
    KeywordOption()
    : SolverOption("kwopt", "", mp::ValueArrayRef(), true), parsed(false) {}
    void Write(fmt::Writer &) {}
    void Parse(const char *&) { parsed = true; }
  };
  TestSolver s;
  TestErrorHandler handler;
  s.set_error_handler(&handler);
  KeywordOption *opt = 0;
  s.AddOption(SolverOptionPtr(opt = new KeywordOption()));
  s.ParseOptions(Args("kwopt=42"));
  EXPECT_EQ(1u, handler.errors.size());
  EXPECT_EQ("Option \"kwopt\" doesn't accept argument", handler.errors[0]);
  EXPECT_FALSE(opt->parsed);
}

TEST(SolverTest, ParseOptionsHandlesOptionErrorsInParse) {
  struct TestOption : SolverOption {
    TestOption() : SolverOption("testopt", "") {}
    void Write(fmt::Writer &) {}
    void Parse(const char *&s) {
      while (*s && !std::isspace(*s))
        ++s;
      throw OptionError("test message");
    }
  };
  TestSolver s;
  TestErrorHandler handler;
  s.set_error_handler(&handler);
  s.AddOption(SolverOptionPtr(new TestOption()));
  s.ParseOptions(Args("testopt=1 testopt=2"));
  EXPECT_EQ(2u, handler.errors.size());
  EXPECT_EQ("test message", handler.errors[0]);
  EXPECT_EQ("test message", handler.errors[1]);
}

TEST(SolverTest, NoEchoOnErrors) {
  EXPECT_EXIT({
    TestSolver s;
    FILE *f = freopen("out", "w", stdout);
    s.ParseOptions(Args("badopt=1 version=2"));
    fclose(f);
    exit(0);
  }, ::testing::ExitedWithCode(0), "");
  EXPECT_EQ("", ReadFile("out"));
}

TEST(SolverTest, OptionEcho) {
  EXPECT_EXIT({
    TestSolver s;
    FILE *f = freopen("out", "w", stdout);
    s.ParseOptions(Args("wantsol=3"));
    s.ParseOptions(Args("wantsol=5"), 0);
    s.ParseOptions(Args("wantsol=9"));
    fclose(f);
    exit(0);
  }, ::testing::ExitedWithCode(0), "");
  EXPECT_EQ("wantsol=5\n", ReadFile("out"));
}

class TestException {};

struct ExceptionTestSolver : public Solver {
  int GetIntOption(const SolverOption &) const { return 0; }
  void Throw(const SolverOption &, int) { throw TestException(); }
  ExceptionTestSolver() : Solver("") {
    AddIntOption("throw", "",
        &ExceptionTestSolver::GetIntOption, &ExceptionTestSolver::Throw);
  }
  void DoSolve(Problem &) {}
  void ReadNL(fmt::StringRef) {}
};

TEST(SolverTest, ExceptionInOptionHandler) {
  ExceptionTestSolver s;
  EXPECT_THROW(s.ParseOptions(Args("throw=1")), TestException);
}

TEST(SolverTest, IntOptions) {
  TestSolverWithOptions s;
  EXPECT_TRUE(s.ParseOptions(Args("intopt1=3", "intopt2=7")));
  EXPECT_EQ(3, s.intopt1);
  EXPECT_EQ(7, s.intopt2);
}

TEST(SolverTest, GetIntOption) {
  TestSolverWithOptions test_solver;
  Solver &s = test_solver;
  EXPECT_EQ(0, s.GetIntOption("intopt1"));
  test_solver.intopt1 = 42;
  EXPECT_EQ(42, s.GetIntOption("intopt1"));
  EXPECT_THROW(s.GetDblOption("intopt1"), OptionError);
  EXPECT_THROW(s.GetStrOption("intopt1"), OptionError);
  EXPECT_THROW(s.GetIntOption("badopt"), OptionError);
}

TEST(SolverTest, SetIntOption) {
  TestSolverWithOptions test_solver;
  Solver &s = test_solver;
  s.SetIntOption("intopt1", 11);
  EXPECT_EQ(11, test_solver.intopt1);
  s.SetIntOption("intopt1", 42);
  EXPECT_EQ(42, test_solver.intopt1);
  EXPECT_THROW(s.SetDblOption("intopt1", 0), OptionError);
  EXPECT_THROW(s.SetStrOption("intopt1", ""), OptionError);
  EXPECT_THROW(s.SetIntOption("badopt", 0), OptionError);
}

TEST(SolverTest, DblOptions) {
  TestSolverWithOptions s;
  EXPECT_TRUE(s.ParseOptions(Args("dblopt2=1.3", "dblopt1=5.4")));
  EXPECT_EQ(5.4, s.dblopt1);
  EXPECT_EQ(1.3, s.dblopt2);
}

TEST(SolverTest, GetDblOption) {
  TestSolverWithOptions test_solver;
  Solver &s = test_solver;
  EXPECT_EQ(0, s.GetDblOption("dblopt1"));
  test_solver.dblopt1 = 42;
  EXPECT_EQ(42, s.GetDblOption("dblopt1"));
  EXPECT_THROW(s.GetIntOption("dblopt1"), OptionError);
  EXPECT_THROW(s.GetStrOption("dblopt1"), OptionError);
  EXPECT_THROW(s.GetDblOption("badopt"), OptionError);
}

TEST(SolverTest, SetDblOption) {
  TestSolverWithOptions test_solver;
  Solver &s = test_solver;
  s.SetDblOption("dblopt1", 1.1);
  EXPECT_EQ(1.1, test_solver.dblopt1);
  s.SetDblOption("dblopt1", 4.2);
  EXPECT_EQ(4.2, test_solver.dblopt1);
  EXPECT_THROW(s.SetIntOption("dblopt1", 0), OptionError);
  EXPECT_THROW(s.SetStrOption("dblopt1", ""), OptionError);
  EXPECT_THROW(s.SetDblOption("badopt", 0), OptionError);
}

TEST(SolverTest, StrOptions) {
  TestSolverWithOptions s;
  EXPECT_TRUE(s.ParseOptions(Args("stropt1=abc", "stropt2=def")));
  EXPECT_EQ("abc", s.stropt1);
  EXPECT_EQ("def", s.stropt2);
}

TEST(SolverTest, GetStrOption) {
  TestSolverWithOptions test_solver;
  Solver &s = test_solver;
  EXPECT_EQ("", s.GetStrOption("stropt1"));
  test_solver.stropt1 = "abc";
  EXPECT_EQ("abc", s.GetStrOption("stropt1"));
  EXPECT_THROW(s.GetIntOption("stropt1"), OptionError);
  EXPECT_THROW(s.GetDblOption("stropt1"), OptionError);
  EXPECT_THROW(s.GetStrOption("badopt"), OptionError);
}

TEST(SolverTest, SetStrOption) {
  TestSolverWithOptions test_solver;
  Solver &s = test_solver;
  s.SetStrOption("stropt1", "abc");
  EXPECT_EQ("abc", test_solver.stropt1);
  s.SetStrOption("stropt1", "def");
  EXPECT_EQ("def", test_solver.stropt1);
  EXPECT_THROW(s.SetIntOption("stropt1", 0), OptionError);
  EXPECT_THROW(s.SetDblOption("stropt1", 0), OptionError);
  EXPECT_THROW(s.SetStrOption("badopt", ""), OptionError);
}

TEST(SolverTest, VersionOption) {
  TestSolver s("testsolver", "Test Solver");
  EXPECT_EXIT({
    FILE *f = freopen("out", "w", stdout);
    s.ParseOptions(Args("version"));
    fclose(f);
    exit(0);
  }, ::testing::ExitedWithCode(0), "");
  fmt::Writer w;
  w.write("Test Solver ({}), ASL({})\n", MP_SYSINFO, MP_DATE);
  EXPECT_EQ(w.str(), ReadFile("out"));
}

TEST(SolverTest, VersionOptionReset) {
  TestSolver s("testsolver", "Test Solver");
  EXPECT_EXIT({
    FILE *f = freopen("out", "w", stdout);
    s.ParseOptions(Args("version"));
    printf("end\n");
    s.ParseOptions(Args(0));
    fclose(f);
    exit(0);
  }, ::testing::ExitedWithCode(0), "");
  fmt::Writer w;
  w.write("Test Solver ({}), ASL({})\nend\n", MP_SYSINFO, MP_DATE);
  EXPECT_EQ(w.str(), ReadFile("out"));
}

TEST(SolverTest, WantsolOption) {
  TestSolver s("");
  EXPECT_EQ(0, s.wantsol());
  s.SetIntOption("wantsol", 1);
  EXPECT_EQ(1, s.wantsol());
  s.SetIntOption("wantsol", 5);
  EXPECT_EQ(5, s.wantsol());
  EXPECT_THROW(s.SetIntOption("wantsol", -1), InvalidOptionValue);
  EXPECT_THROW(s.SetIntOption("wantsol", 16), InvalidOptionValue);
}

TEST(SolverTest, TimingOption) {
  TestSolver s("");
  EXPECT_FALSE(s.timing());
  s.SetIntOption("timing", 1);
  EXPECT_TRUE(s.timing());
  EXPECT_EQ(1, s.GetIntOption("timing"));
  EXPECT_THROW(s.SetIntOption("timing", -1), InvalidOptionValue);
  EXPECT_THROW(s.SetIntOption("timing", 2), InvalidOptionValue);
}

TEST(SolverTest, InputTiming) {
  struct TestOutputHandler : mp::OutputHandler {
    std::string output;

    virtual ~TestOutputHandler() {}
    void HandleOutput(fmt::StringRef output) {
      this->output += output;
    }
  };
  TestOutputHandler oh;
  TestSolver s("");
  s.set_output_handler(&oh);

  s.SetIntOption("timing", 0);
  {
    Problem p;
    s.ProcessArgs(Args("test", MP_TEST_DATA_DIR "/objconst.nl"), p);
    EXPECT_TRUE(oh.output.find("Input time = ") == std::string::npos);
  }

  {
    s.SetIntOption("timing", 1);
    Problem p;
    s.ProcessArgs(Args("test", MP_TEST_DATA_DIR "/objconst.nl"), p);
    EXPECT_TRUE(oh.output.find("Input time = ") != std::string::npos);
  }
}

TEST(SolverTest, InputSuffix) {
  TestSolver s("");
  s.AddSuffix("answer", 0, ASL_Sufkind_var, 0);
  Problem p;
  s.ProcessArgs(Args("program-name", MP_TEST_DATA_DIR "/suffix.nl"), p);
  mp::Suffix suffix = p.suffix("answer", ASL_Sufkind_var);
  EXPECT_EQ(42, suffix.int_value(0));
}

TEST(SolverTest, OutputSuffix) {
  TestSolver s("");
  s.AddSuffix("answer", 0, ASL_Sufkind_var | ASL_Sufkind_outonly, 0);
  Problem p;
  s.ProcessArgs(Args("program-name"), p);
  mp::Suffix suffix = p.suffix("answer", ASL_Sufkind_var);
  int value = 42;
  suffix.set_values(&value);
  EXPECT_EQ(42, suffix.int_value(0));
}

const int NUM_SOLUTIONS = 3;

struct SolCountingSolver : mp::ASLSolver {
  explicit SolCountingSolver(bool multiple_sol)
  : ASLSolver("", "", 0, multiple_sol ? MULTIPLE_SOL : 0) {}
  void DoSolve(Problem &p) {
    for (int i = 0; i < NUM_SOLUTIONS; ++i)
      HandleFeasibleSolution(p, "", 0, 0, 0);
    HandleSolution(p, "", 0, 0, 0);
  }
};

TEST(SolverTest, CountSolutionsOption) {
  SolCountingSolver s1(false);
  EXPECT_THROW(s1.GetIntOption("countsolutions"), OptionError);
  SolCountingSolver s2(true);
  EXPECT_EQ(0, s2.GetIntOption("countsolutions"));
  s2.SetIntOption("countsolutions", 1);
  EXPECT_EQ(1, s2.GetIntOption("countsolutions"));
}

TEST(SolverTest, SolutionsAreNotCountedByDefault) {
  SolCountingSolver s(true);
  s.SetIntOption("wantsol", 1);
  WriteFile("test.nl", ReadFile(MP_TEST_DATA_DIR "/objconst.nl"));
  Problem p;
  p.Read("test.nl");
  s.Solve(p);
  EXPECT_TRUE(ReadFile("test.sol").find(
      fmt::format("nsol\n0 {}\n", NUM_SOLUTIONS)) == std::string::npos);
}

TEST(SolverTest, CountSolutions) {
  SolCountingSolver s(true);
  s.SetIntOption("countsolutions", 1);
  s.SetIntOption("wantsol", 1);
  WriteFile("test.nl", ReadFile(MP_TEST_DATA_DIR "/objconst.nl"));
  Problem p;
  p.Read("test.nl");
  s.Solve(p);
  EXPECT_TRUE(ReadFile("test.sol").find(
      fmt::format("nsol\n0 {}\n", NUM_SOLUTIONS)) != std::string::npos);
}

TEST(SolverTest, SolutionStubOption) {
  SolCountingSolver s1(false);
  EXPECT_THROW(s1.GetStrOption("solutionstub"), OptionError);
  SolCountingSolver s2(true);
  EXPECT_EQ("", s2.GetStrOption("solutionstub"));
  s2.SetStrOption("solutionstub", "abc");
  EXPECT_EQ("abc", s2.GetStrOption("solutionstub"));
}

bool Exists(fmt::StringRef filename) {
  std::FILE *f = std::fopen(filename.c_str(), "r");
  if (f)
    std::fclose(f);
  return f != 0;
}

TEST(SolverTest, SolutionsAreNotWrittenByDefault) {
  SolCountingSolver s(true);
  s.SetIntOption("wantsol", 1);
  WriteFile("test.nl", ReadFile(MP_TEST_DATA_DIR "/objconst.nl"));
  Problem p;
  p.Read("test.nl");
  s.Solve(p);
  EXPECT_TRUE(!Exists("1.sol"));
  EXPECT_TRUE(ReadFile("test.sol").find(
      fmt::format("nsol\n0 {}\n", NUM_SOLUTIONS)) == std::string::npos);
}

TEST(SolverTest, WriteSolutions) {
  SolCountingSolver s(true);
  s.SetStrOption("solutionstub", "abc");
  s.SetIntOption("wantsol", 1);
  WriteFile("test.nl", ReadFile(MP_TEST_DATA_DIR "/objconst.nl"));
  Problem p;
  p.Read("test.nl");
  for (int i = 1; i <= NUM_SOLUTIONS; ++i)
    std::remove(fmt::format("abc{}.sol", i).c_str());
  s.Solve(p);
  for (int i = 1; i <= NUM_SOLUTIONS; ++i)
    EXPECT_TRUE(Exists(fmt::format("abc{}.sol", i)));
  EXPECT_TRUE(ReadFile("test.sol").find(
      fmt::format("nsol\n0 {}\n", NUM_SOLUTIONS)) != std::string::npos);
}
}
