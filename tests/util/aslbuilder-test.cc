/*
 Tests of the ASL problem builder.

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

#include "gtest/gtest.h"

#include "solvers/util/aslbuilder.h"
#include "solvers/util/nl.h"
#include "tests/util.h"

using ampl::NLHeader;
using ampl::LogicalExpr;
using ampl::NumericExpr;
using ampl::internal::ASLBuilder;

bool operator==(const cde &lhs, const cde &rhs) {
  return lhs.e == rhs.e && lhs.d == rhs.d && lhs.zaplen == rhs.zaplen;
}

bool operator==(const expr_v &lhs, const expr_v &rhs) {
  return lhs.op == rhs.op && lhs.a == rhs.a && lhs.v == rhs.v;
}

bool operator==(const cexp &lhs, const cexp &rhs) {
  return lhs.e == rhs.e && lhs.nlin == rhs.nlin && lhs.L == rhs.L &&
      lhs.funneled == rhs.funneled && lhs.cref == rhs.cref &&
      lhs.z.e == rhs.z.e && lhs.zlen == rhs.zlen && lhs.d == rhs.d &&
      lhs.vref == rhs.vref;
}

bool operator==(const cexp1 &lhs, const cexp1 &rhs) {
  return lhs.e == rhs.e && lhs.nlin == rhs.nlin && lhs.L == rhs.L;
}

namespace {

// Searches the ASL list for asl backwards from prev and forward from next
// returning true if it is found, false otherwise.
bool FindASL(ASLhead *prev, ASLhead *next, const ASL &asl) {
  while (prev && prev != &asl.p.h)
    prev = prev->prev;
  if (prev)
    return true;
  while (next && next != &asl.p.h)
    next = next->next;
  if (next)
    return true;
  return false;
}

std::size_t CountBlocks(void *start) {
  std::size_t num_blocks = 0;
  struct Mblock {
    struct Mblock *next;
    void *m[31];
  };
  for (Mblock *mb = reinterpret_cast<Mblock*>(start); mb; mb = mb->next)
    ++num_blocks;
  return num_blocks;
}

template <typename T>
T *GetPtr(const ASL &asl, T *Edaginfo::*ptr) { return asl.i.*ptr; }

template <typename T>
T *GetPtr(const ASL &asl, T *Edag1info::*ptr) {
  return reinterpret_cast<const ASL_fg&>(asl).I.*ptr;
}

// Checks if the array pointed to by ptr in the actual ASL object is the
// same as the one in the expected object. In particular, they should have
// the same offset from start and have the same elements.
template <typename StartInfo, typename Info, typename StartT, typename T>
void CheckArray(const ASL &expected, const ASL &actual,
    StartT *StartInfo::*start, T *Info::*ptr, std::size_t size,
    const char *str) {
  // Get start pointers.
  const char *expected_start =
      reinterpret_cast<const char*>(GetPtr(expected, start));
  const char *actual_start =
      reinterpret_cast<const char*>(GetPtr(actual, start));

  // Get array pointers and compare.
  const T *expected_ptr = GetPtr(expected, ptr);
  const T *actual_ptr = GetPtr(actual, ptr);
  EXPECT_EQ(reinterpret_cast<const char*>(expected_ptr) - expected_start,
      reinterpret_cast<const char*>(actual_ptr) - actual_start) << str;
  if (expected_ptr) {
    for (std::size_t i = 0; i < size; ++i)
      EXPECT_EQ(expected_ptr[i], actual_ptr[i]) << str << ' ' << i;
  }
}

#define CHECK_ARRAY(expected, actual, ptr, size) \
  CheckArray(expected, actual, &Edag1info::var_e_, ptr, size, #ptr)

template <typename T>
void ExpectArrayEqual(const T *expected,
    const T *actual, std::size_t size, const char *str) {
  if (!expected) {
    EXPECT_EQ(expected, actual);
    return;
  }
  for (std::size_t i = 0; i < size; ++i)
    EXPECT_EQ(expected[i], actual[i]) << str << ' ' << i;
}

// Compare two arrays for equality.
#define EXPECT_ARRAY_EQ(expected, actual, size) \
  ExpectArrayEqual(expected, actual, size, #expected)

// Compare two ASL objects for equality.
void CheckASL(const ASL &expected, const ASL &actual, bool complete = true) {
  // Compare Edagpars.
  EXPECT_TRUE(FindASL(expected.p.h.prev, expected.p.h.next, actual));
  EXPECT_TRUE(FindASL(actual.p.h.prev, actual.p.h.next, expected));
  EXPECT_EQ(expected.p.hffactor, actual.p.hffactor);
  EXPECT_EQ(expected.p.FUNNEL_MIN_, actual.p.FUNNEL_MIN_);
  EXPECT_EQ(expected.p.maxfwd_, actual.p.maxfwd_);
  EXPECT_EQ(expected.p.need_funcadd_, actual.p.need_funcadd_);
  EXPECT_EQ(expected.p.vrefGulp_, actual.p.vrefGulp_);
  EXPECT_EQ(expected.p.want_derivs_, actual.p.want_derivs_);
  EXPECT_EQ(expected.p.ihd_limit_, actual.p.ihd_limit_);
  EXPECT_EQ(expected.p.solve_code_, actual.p.solve_code_);
  EXPECT_EQ(expected.p.Objval, actual.p.Objval);
  EXPECT_EQ(expected.p.Objval_nomap, actual.p.Objval_nomap);
  EXPECT_EQ(expected.p.Objgrd, actual.p.Objgrd);
  EXPECT_EQ(expected.p.Objgrd_nomap, actual.p.Objgrd_nomap);
  EXPECT_EQ(expected.p.Conval, actual.p.Conval);
  EXPECT_EQ(expected.p.Jacval, actual.p.Jacval);
  EXPECT_EQ(expected.p.Conival, actual.p.Conival);
  EXPECT_EQ(expected.p.Conival_nomap, actual.p.Conival_nomap);
  EXPECT_EQ(expected.p.Congrd, actual.p.Congrd);
  EXPECT_EQ(expected.p.Congrd_nomap, actual.p.Congrd_nomap);
  EXPECT_EQ(expected.p.Hvcomp, actual.p.Hvcomp);
  EXPECT_EQ(expected.p.Hvcomp_nomap, actual.p.Hvcomp_nomap);
  EXPECT_EQ(expected.p.Hvinit, actual.p.Hvinit);
  EXPECT_EQ(expected.p.Hvinit_nomap, actual.p.Hvinit_nomap);
  EXPECT_EQ(expected.p.Hesset, actual.p.Hesset);
  EXPECT_EQ(expected.p.Lconval, actual.p.Lconval);
  EXPECT_EQ(expected.p.Xknown, actual.p.Xknown);
  EXPECT_EQ(expected.p.Duthes, actual.p.Duthes);
  EXPECT_EQ(expected.p.Duthes_nomap, actual.p.Duthes_nomap);
  EXPECT_EQ(expected.p.Fulhes, actual.p.Fulhes);
  EXPECT_EQ(expected.p.Fulhes_nomap, actual.p.Fulhes_nomap);
  EXPECT_EQ(expected.p.Sphes, actual.p.Sphes);
  EXPECT_EQ(expected.p.Sphes_nomap, actual.p.Sphes_nomap);
  EXPECT_EQ(expected.p.Sphset, actual.p.Sphset);
  EXPECT_EQ(expected.p.Sphset_nomap, actual.p.Sphset_nomap);

  // Compare Edaginfo.
  EXPECT_EQ(expected.i.ASLtype, actual.i.ASLtype);
  EXPECT_EQ(expected.i.amplflag_, actual.i.amplflag_);
  EXPECT_EQ(expected.i.need_nl_, actual.i.need_nl_);
  if (expected.i.ASLtype != ASL_read_f)
    CHECK_ARRAY(expected, actual, &Edaginfo::funcs_, expected.i.nfunc_);
  else
    EXPECT_EQ(expected.i.funcs_, actual.i.funcs_);
  EXPECT_EQ(expected.i.funcsfirst_, actual.i.funcsfirst_);
  EXPECT_EQ(expected.i.funcslast_, actual.i.funcslast_);
  EXPECT_EQ(expected.i.xscanf_, actual.i.xscanf_);
  for (int i = 0; i < NFHASH; ++i)
    EXPECT_EQ(expected.i.fhash_[i], actual.i.fhash_[i]);

  EXPECT_ARRAY_EQ(expected.i.adjoints_, actual.i.adjoints_, expected.i.amax_);
  EXPECT_EQ(expected.i.adjoints_nv1_ - expected.i.adjoints_,
      actual.i.adjoints_nv1_ - actual.i.adjoints_);
  EXPECT_ARRAY_EQ(expected.i.LUrhs_, actual.i.LUrhs_,
      2 * (expected.i.n_con_ + expected.i.nsufext[ASL_Sufkind_con]));
  EXPECT_EQ(expected.i.Urhsx_, actual.i.Urhsx_);
  EXPECT_EQ(expected.i.X0_, actual.i.X0_);

  // The number of variables and variable suffixes.
  std::size_t num_vars_suf = complete ?
      2 * (expected.i.n_var_ + expected.i.nsufext[ASL_Sufkind_var]) : 0;
  EXPECT_ARRAY_EQ(expected.i.LUv_, actual.i.LUv_, num_vars_suf);
  EXPECT_EQ(expected.i.Uvx_, actual.i.Uvx_);
  EXPECT_ARRAY_EQ(expected.i.Lastx_, actual.i.Lastx_, num_vars_suf);
  EXPECT_EQ(expected.i.pi0_, actual.i.pi0_);

  CHECK_ARRAY(expected, actual, &Edaginfo::objtype_, expected.i.n_obj_);
  EXPECT_EQ(expected.i.havex0_, actual.i.havex0_);
  EXPECT_EQ(expected.i.havepi0_, actual.i.havepi0_);
  EXPECT_EQ(expected.i.A_vals_, actual.i.A_vals_);
  EXPECT_EQ(expected.i.A_rownos_, actual.i.A_rownos_);
  EXPECT_EQ(expected.i.A_colstarts_, actual.i.A_colstarts_);

  EXPECT_EQ(expected.i.Cgrad_, actual.i.Cgrad_);
  CHECK_ARRAY(expected, actual, &Edaginfo::Ograd_, expected.i.n_obj_);
  EXPECT_EQ(expected.i.Cgrad0, actual.i.Cgrad0);

  EXPECT_EQ(expected.i.Fortran_, actual.i.Fortran_);
  EXPECT_EQ(expected.i.amax_, actual.i.amax_);

  EXPECT_EQ(expected.i.c_vars_, actual.i.c_vars_);
  EXPECT_EQ(expected.i.comb_, actual.i.comb_);
  EXPECT_EQ(expected.i.combc_, actual.i.combc_);
  EXPECT_EQ(expected.i.comc1_, actual.i.comc1_);
  EXPECT_EQ(expected.i.comc_, actual.i.comc_);
  EXPECT_EQ(expected.i.como1_, actual.i.como1_);
  EXPECT_EQ(expected.i.como_, actual.i.como_);

  EXPECT_EQ(expected.i.lnc_, actual.i.lnc_);
  EXPECT_EQ(expected.i.nbv_, actual.i.nbv_);
  EXPECT_EQ(expected.i.niv_, actual.i.niv_);
  EXPECT_EQ(expected.i.nlc_, actual.i.nlc_);
  EXPECT_EQ(expected.i.n_eqn_, actual.i.n_eqn_);
  EXPECT_EQ(expected.i.n_cc_, actual.i.n_cc_);
  EXPECT_EQ(expected.i.nlcc_, actual.i.nlcc_);
  EXPECT_EQ(expected.i.ndcc_, actual.i.ndcc_);

  EXPECT_EQ(expected.i.nzlb_, actual.i.nzlb_);
  EXPECT_EQ(expected.i.nlnc_, actual.i.nlnc_);
  EXPECT_EQ(expected.i.nlo_, actual.i.nlo_);
  EXPECT_EQ(expected.i.nlvb_, actual.i.nlvb_);
  EXPECT_EQ(expected.i.nlvc_, actual.i.nlvc_);
  EXPECT_EQ(expected.i.nlvo_, actual.i.nlvo_);
  EXPECT_EQ(expected.i.nlvbi_, actual.i.nlvbi_);
  EXPECT_EQ(expected.i.nlvci_, actual.i.nlvci_);
  EXPECT_EQ(expected.i.nlvoi_, actual.i.nlvoi_);
  EXPECT_EQ(expected.i.nwv_, actual.i.nwv_);
  EXPECT_EQ(expected.i.nzc_, actual.i.nzc_);
  EXPECT_EQ(expected.i.nzo_, actual.i.nzo_);
  EXPECT_EQ(expected.i.n_var_, actual.i.n_var_);
  EXPECT_EQ(expected.i.n_con_, actual.i.n_con_);
  EXPECT_EQ(expected.i.n_obj_, actual.i.n_obj_);
  EXPECT_EQ(expected.i.n_prob, actual.i.n_prob);
  EXPECT_EQ(expected.i.n_lcon_, actual.i.n_lcon_);
  EXPECT_EQ(expected.i.flags, actual.i.flags);
  EXPECT_EQ(expected.i.n_conjac_[0], actual.i.n_conjac_[0]);
  EXPECT_EQ(expected.i.n_conjac_[1], actual.i.n_conjac_[1]);

  EXPECT_EQ(expected.i.nclcon_, actual.i.nclcon_);
  EXPECT_EQ(expected.i.ncom0_, actual.i.ncom0_);
  EXPECT_EQ(expected.i.ncom1_, actual.i.ncom1_);
  EXPECT_EQ(expected.i.nderps_, actual.i.nderps_);
  EXPECT_EQ(expected.i.nfunc_, actual.i.nfunc_);
  EXPECT_EQ(expected.i.nzjac_, actual.i.nzjac_);
  EXPECT_EQ(expected.i.o_vars_, actual.i.o_vars_);
  EXPECT_EQ(expected.i.want_deriv_, actual.i.want_deriv_);
  EXPECT_EQ(expected.i.x0kind_, actual.i.x0kind_);
  EXPECT_EQ(expected.i.rflags, actual.i.rflags);
  EXPECT_EQ(expected.i.x0len_, actual.i.x0len_);

  EXPECT_STREQ(expected.i.filename_, actual.i.filename_);
  EXPECT_EQ(
      expected.i.stub_end_ - expected.i.filename_,
      actual.i.stub_end_ - actual.i.filename_);
  EXPECT_EQ(expected.i.archan_, actual.i.archan_);
  EXPECT_EQ(expected.i.awchan_, actual.i.awchan_);
  EXPECT_EQ(expected.i.binary_nl_, actual.i.binary_nl_);
  EXPECT_EQ(expected.i.return_nofile_, actual.i.return_nofile_);
  EXPECT_EQ(expected.i.plterms_, actual.i.plterms_);
  EXPECT_EQ(expected.i.maxrownamelen_, actual.i.maxrownamelen_);
  EXPECT_EQ(expected.i.maxcolnamelen_, actual.i.maxcolnamelen_);
  EXPECT_EQ(expected.i.co_index_, actual.i.co_index_);
  EXPECT_EQ(expected.i.cv_index_, actual.i.cv_index_);
  // Edaginfo::err_jmp_ is ignored.
  EXPECT_EQ(expected.i.err_jmp1_, actual.i.err_jmp1_);
  for (int i = 0; i < ampl::MAX_NL_OPTIONS + 1; ++i)
    EXPECT_EQ(expected.i.ampl_options_[i], actual.i.ampl_options_[i]);
  EXPECT_EQ(expected.i.obj_no_, actual.i.obj_no_);
  EXPECT_EQ(expected.i.nranges_, actual.i.nranges_);
  EXPECT_EQ(expected.i.want_xpi0_, actual.i.want_xpi0_);

  EXPECT_EQ(expected.i.c_cexp1st_, actual.i.c_cexp1st_);
  EXPECT_EQ(expected.i.o_cexp1st_, actual.i.o_cexp1st_);

  EXPECT_EQ(expected.i.cvar_, actual.i.cvar_);
  EXPECT_EQ(expected.i.ccind1, actual.i.ccind1);
  EXPECT_EQ(expected.i.ccind2, actual.i.ccind2);

  EXPECT_EQ(expected.i.size_expr_n_, actual.i.size_expr_n_);
  EXPECT_EQ(expected.i.ampl_vbtol_, actual.i.ampl_vbtol_);

  EXPECT_EQ(expected.i.zaC_, actual.i.zaC_);
  EXPECT_EQ(expected.i.zac_, actual.i.zac_);
  EXPECT_EQ(expected.i.zao_, actual.i.zao_);

  EXPECT_EQ(expected.i.skip_int_derivs_, actual.i.skip_int_derivs_);

  EXPECT_EQ(expected.i.nsuffixes, actual.i.nsuffixes);
  for (int i = 0; i < 4; ++i) {
    EXPECT_EQ(expected.i.nsufext[i], actual.i.nsufext[i]);
    EXPECT_EQ(expected.i.nsuff[i], actual.i.nsuff[i]);
    EXPECT_EQ(expected.i.suffixes[i], actual.i.suffixes[i]);
  }

  if (!expected.i.zerograds_) {
    EXPECT_EQ(expected.i.zerograds_, actual.i.zerograds_);
  } else {
    const char *expected_start =
        reinterpret_cast<const char*>(expected.i.zerograds_);
    const char *actual_start =
        reinterpret_cast<const char*>(actual.i.zerograds_);
    for (int i = 0; i < expected.i.n_obj_; ++i) {
      EXPECT_EQ(
          reinterpret_cast<const char*>(expected.i.zerograds_[i]) -
          expected_start,
          reinterpret_cast<const char*>(actual.i.zerograds_[i]) -
          actual_start);

      // Compare values pointed to by zerograds_[i].
      int nx = expected.i.nsufext[ASL_Sufkind_var];
      int nv = expected.i.nlvog;
      if (nv == 0) {
        nv = expected.i.n_var_;
        if (nv > expected.i.n_var0)
          nx -= nv - expected.i.n_var0;
      }
      int size = expected.i.n_obj_;
      for (ograd **ogp = expected.i.Ograd_, **ogpe = ogp + size;
          ogp < ogpe; ++ogp) {
        ograd *og = *ogp;
        int n = 0;
        while (og) {
          size += og->varno - n;
          n = og->varno + 1;
          if (n >= nv)
            break;
          og = og->next;
        }
        if (n < nv)
          size += nv - n;
      }
      size += expected.i.n_obj_ * nx;
      EXPECT_ARRAY_EQ(reinterpret_cast<const int*>(
          expected.i.zerograds_ + expected.i.n_obj_),
          reinterpret_cast<const int*>(actual.i.zerograds_ + expected.i.n_obj_),
          size);
    }
  }
  EXPECT_EQ(expected.i.congrd_mode, actual.i.congrd_mode);
  EXPECT_EQ(expected.i.x_known, actual.i.x_known);
  EXPECT_EQ(expected.i.xknown_ignore, actual.i.xknown_ignore);
  EXPECT_EQ(expected.i.zap_J, actual.i.zap_J);
  EXPECT_EQ(expected.i.nxval, actual.i.nxval);
  EXPECT_EQ(expected.i.nlvog, actual.i.nlvog);
  EXPECT_EQ(expected.i.ncxval, actual.i.ncxval);
  EXPECT_EQ(expected.i.noxval, actual.i.noxval);
  EXPECT_EQ(expected.i.sputinfo_, actual.i.sputinfo_);

  EXPECT_EQ(expected.i.Mblast - expected.i.Mbnext,
            actual.i.Mblast - actual.i.Mbnext);
  EXPECT_EQ(CountBlocks(expected.i.Mb), CountBlocks(actual.i.Mb));
  EXPECT_EQ(expected.i.memLast - expected.i.memNext,
      actual.i.memLast - actual.i.memNext);
  EXPECT_EQ(expected.i.ae, actual.i.ae);

  EXPECT_EQ(expected.i.connames, actual.i.connames);
  EXPECT_EQ(expected.i.lconnames, actual.i.lconnames);
  EXPECT_EQ(expected.i.objnames, actual.i.objnames);
  EXPECT_EQ(expected.i.varnames, actual.i.varnames);
  EXPECT_EQ(expected.i.vcochecked, actual.i.vcochecked);

  EXPECT_EQ(expected.i.uinfo, actual.i.uinfo);

  EXPECT_EQ(expected.i.iadjfcn, actual.i.iadjfcn);
  EXPECT_EQ(expected.i.dadjfcn, actual.i.dadjfcn);

  EXPECT_EQ(expected.i.cscale, actual.i.cscale);
  EXPECT_EQ(expected.i.vscale, actual.i.vscale);
  EXPECT_EQ(expected.i.lscale, actual.i.lscale);

  EXPECT_EQ(expected.i.arlast, actual.i.arlast);
  EXPECT_EQ(expected.i.arnext, actual.i.arnext);
  EXPECT_EQ(expected.i.arprev, actual.i.arprev);

  EXPECT_EQ(expected.i.csd, actual.i.csd);
  EXPECT_EQ(expected.i.rsd, actual.i.rsd);
  EXPECT_EQ(expected.i.n_var0, actual.i.n_var0);
  EXPECT_EQ(expected.i.n_con0, actual.i.n_con0);
  EXPECT_EQ(expected.i.n_var1, actual.i.n_var1);
  EXPECT_EQ(expected.i.n_con1, actual.i.n_con1);
  EXPECT_EQ(expected.i.vmap, actual.i.vmap);
  EXPECT_EQ(expected.i.cmap, actual.i.cmap);
  EXPECT_EQ(expected.i.vzap, actual.i.vzap);
  EXPECT_EQ(expected.i.czap, actual.i.czap);
  EXPECT_EQ(expected.i.vminv, actual.i.vminv);

  EXPECT_EQ(expected.i.Or, actual.i.Or);
  EXPECT_EQ(expected.i.orscratch, actual.i.orscratch);

  EXPECT_EQ(expected.i.mpa, actual.i.mpa);

  EXPECT_EQ(expected.i.Derrs, actual.i.Derrs);
  EXPECT_EQ(expected.i.Derrs0, actual.i.Derrs0);

  // Compare Edag1info.
  const ASL_fg &expected_fg = reinterpret_cast<const ASL_fg&>(expected);
  const ASL_fg &actual_fg = reinterpret_cast<const ASL_fg&>(actual);
  CHECK_ARRAY(expected, actual, &Edag1info::con_de_,
      expected.i.n_con_ + expected.i.nsufext[ASL_Sufkind_con]);
  CHECK_ARRAY(expected, actual, &Edag1info::lcon_de_, expected.i.n_lcon_);
  CHECK_ARRAY(expected, actual, &Edag1info::obj_de_, expected.i.n_obj_);
  CHECK_ARRAY(expected, actual, &Edag1info::var_e_,
      expected.i.n_var_ + expected.i.nsufext[ASL_Sufkind_var]);

  EXPECT_EQ(expected_fg.I.f_b_, actual_fg.I.f_b_);
  EXPECT_EQ(expected_fg.I.f_c_, actual_fg.I.f_c_);
  EXPECT_EQ(expected_fg.I.f_o_, actual_fg.I.f_o_);

  if (expected.i.ASLtype != ASL_read_f) {
    CHECK_ARRAY(expected, actual, &Edag1info::var_ex_, expected.i.ncom0_);
    CHECK_ARRAY(expected, actual, &Edag1info::var_ex1_,
        expected.i.comc1_ + expected.i.como1_);
    CHECK_ARRAY(expected, actual, &Edag1info::cexps_, expected.i.ncom0_);
    CHECK_ARRAY(expected, actual, &Edag1info::cexps1_, expected.i.ncom1_);
  } else {
    EXPECT_EQ(expected_fg.I.var_ex_, actual_fg.I.var_ex_);
    EXPECT_EQ(expected_fg.I.var_ex1_, actual_fg.I.var_ex1_);
    EXPECT_EQ(expected_fg.I.cexps_, actual_fg.I.cexps_);
    EXPECT_EQ(expected_fg.I.cexps1_, actual_fg.I.cexps1_);
  }

  EXPECT_EQ(expected_fg.I.r_ops_, actual_fg.I.r_ops_);
  EXPECT_EQ(expected_fg.I.c_class, actual_fg.I.c_class);
  EXPECT_EQ(expected_fg.I.o_class, actual_fg.I.o_class);
  EXPECT_EQ(expected_fg.I.v_class, actual_fg.I.v_class);
  EXPECT_EQ(expected_fg.I.c_class_max, actual_fg.I.c_class_max);
  EXPECT_EQ(expected_fg.I.o_class_max, actual_fg.I.o_class_max);
}

class ASLPtr : ampl::Noncopyable {
 private:
  ASL *asl_;

 public:
  explicit ASLPtr(int type = ASL_read_fg) : asl_(ASL_alloc(type)) {}
  ~ASLPtr() { ASL_free(&asl_); }

  ASL *get() const { return asl_; }
  ASL &operator*() const { return *asl_; }
  ASL *operator->() const { return asl_; }
};

// Reads ASL header from a file with the specified header and body.
FILE *ReadHeader(ASL &asl, const NLHeader &h, const char *body) {
  fmt::Writer w;
  w << h << body;
  WriteFile("test"
  ".nl", w.str());
  char stub[] = "test";
  return jac0dim_ASL(&asl, stub, sizeof(stub) - 1);
}

// Check that ASLBuilder creates an ASL object compatible with the one
// created by jac0dim.
void CheckInitASL(const NLHeader &h) {
  ASLPtr expected, actual;
  fclose(ReadHeader(*expected, h, ""));
  ASLBuilder(*actual).InitASL("test", h);
  CheckASL(*expected, *actual);
}

TEST(ASLBuilderTest, Ctor) {
  ASLPtr asl;
  ASLBuilder builder(*asl);
}

TEST(ASLBuilderTest, InitASLTrivial) {
  NLHeader header = {};
  header.num_vars = 1;  // jac0dim can't handle problems with 0 vars
  CheckInitASL(header);
}

TEST(ASLBuilderTest, InitASLFull) {
  NLHeader header = {
    NLHeader::BINARY,
    9, {2, 3, 5, 7, 11, 13, 17, 19, 23}, 1.23,
    29, 47, 37, 41, 43, 31,
    53, 59, 67, 61, 71, 73,
    79, 83,
    89, 97, 101,
    103, 107, 109,
    113, 127, 131, 137, 139,
    149, 151,
    157, 163,
    167, 173, 179, 181, 191
  };
  CheckInitASL(header);
}

// Check that iadjfcn & dadjfcn are set properly when format is
// NLHeader::BINARY_SWAPPED.
TEST(ASLBuilderTest, ASLBuilderAdjFcn) {
  NLHeader header = {NLHeader::BINARY_SWAPPED};
  header.num_vars = 1;
  CheckInitASL(header);  // iadjfcn & dadjfcn are checked here.
}

#define CHECK_THROW_ASL_ERROR(code, expected_error_code, expected_message) { \
  ampl::internal::ASLError error(0, ""); \
  try { \
    code; \
  } catch (const ampl::internal::ASLError &e) { \
    error = e; \
  } \
  EXPECT_EQ(expected_error_code, error.error_code()); \
  EXPECT_STREQ(expected_message, error.what()); \
}

TEST(ASLBuilderTest, ASLBuilderInvalidProblemDim) {
  NLHeader header = {};
  CHECK_THROW_ASL_ERROR(ASLBuilder(*ASLPtr()).InitASL("test", header),
      ASL_readerr_corrupt, "invalid problem dimensions: M = 0, N = 0, NO = 0");
  header.num_vars = 1;
  ASLBuilder(*ASLPtr()).InitASL("test", header);
  header.num_algebraic_cons = -1;
  CHECK_THROW_ASL_ERROR(ASLBuilder(*ASLPtr()).InitASL("test", header),
      ASL_readerr_corrupt, "invalid problem dimensions: M = -1, N = 1, NO = 0");
  header.num_objs = -1;
  header.num_algebraic_cons = 0;
  CHECK_THROW_ASL_ERROR(ASLBuilder(*ASLPtr()).InitASL("test", header),
      ASL_readerr_corrupt, "invalid problem dimensions: M = 0, N = 1, NO = -1");
}

// Check that x0len_ is set properly for different values of
// num_nl_vars_in_cons & num_nl_vars_in_objs.
TEST(ASLBuilderTest, ASLBuilderX0Len) {
  NLHeader header = {};
  header.num_vars = 1;
  header.num_nl_vars_in_cons = 5;
  header.num_nl_vars_in_objs = 10;
  CheckInitASL(header);
  std::swap(header.num_nl_vars_in_cons, header.num_nl_vars_in_objs);
  CheckInitASL(header);
}

int ReadASL(ASL &asl, const NLHeader &h, const char *body, int flags) {
  return fg_read_ASL(&asl, ReadHeader(asl, h, body), flags);
}

NLHeader MakeHeader() {
  NLHeader header = {};
  header.num_vars = header.num_objs = 1;
  return header;
}

TEST(ASLBuilderTest, ASLBuilderLinear) {
  NLHeader header = MakeHeader();
  ASLPtr actual(ASL_read_f);
  ASLBuilder builder(*actual);
  builder.BeginBuild("test", header, 0);
  builder.EndBuild();
  ASLPtr expected(ASL_read_f);
  EXPECT_EQ(0,
      f_read_ASL(expected.get(), ReadHeader(*expected, header, ""), 0));
  CheckASL(*expected, *actual, false);
}

TEST(ASLBuilderTest, ASLBuilderTrivialProblem) {
  NLHeader header = MakeHeader();
  ASLPtr actual;
  ASLBuilder builder(*actual);
  builder.BeginBuild("test", header, 0);
  builder.EndBuild();
  ASLPtr expected;
  EXPECT_EQ(0, ReadASL(*expected, header, "", 0));
  CheckASL(*expected, *actual, false);
}

TEST(ASLBuilderTest, ASLBuilderDisallowCLPByDefault) {
  NLHeader header = MakeHeader();
  header.num_logical_cons = 1;
  ASLPtr actual;
  ASLBuilder builder(*actual);
  CHECK_THROW_ASL_ERROR(builder.BeginBuild("test", header, ASL_return_read_err),
      ASL_readerr_CLP, "cannot handle logical constraints");
  ASLPtr expected;
  EXPECT_EQ(ASL_readerr_CLP,
      ReadASL(*expected, header, "", ASL_return_read_err));
  CheckASL(*expected, *actual, false);
}

TEST(ASLBuilderTest, ASLBuilderAllowCLP) {
  NLHeader header = MakeHeader();
  header.num_logical_cons = 1;
  ASLPtr actual;
  ASLBuilder builder(*actual);
  ampl::internal::ASLError error(0, "");
  builder.BeginBuild("test", header, ASL_return_read_err | ASL_allow_CLP);
  builder.EndBuild();
  ASLPtr expected;
  ReadASL(*expected, header, "", ASL_return_read_err | ASL_allow_CLP);
  CheckASL(*expected, *actual, false);
}

class TestASLBuilder : private ASLPtr, public ASLBuilder {
 public:
  explicit TestASLBuilder(int num_vars = 1) : ASLBuilder(*get()) {
    NLHeader header = MakeHeader();
    header.num_vars = num_vars;
    BeginBuild("", header, ampl::internal::ASL_STANDARD_OPCODES);
  }
};

TEST(ASLBuilderTest, MakeUnary) {
  const int opcodes[] = {
      FLOOR, CEIL, ABS, OPUMINUS, OP_tanh, OP_tan, OP_sqrt,
      OP_sinh, OP_sin, OP_log10, OP_log, OP_exp, OP_cosh, OP_cos,
      OP_atanh, OP_atan, OP_asinh, OP_asin, OP_acosh, OP_acos, OP2POW
  };
  TestASLBuilder builder;
  NumericExpr arg = builder.MakeNumericConstant(42);
  for (size_t i = 0, n = sizeof(opcodes) / sizeof(*opcodes); i < n; ++i) {
    ampl::UnaryExpr expr = builder.MakeUnary(opcodes[i], arg);
    EXPECT_EQ(opcodes[i], expr.opcode());
    EXPECT_EQ(arg, expr.arg());
  }
  EXPECT_THROW_MSG(builder.MakeUnary(OPPLUS, arg), ampl::Error,
    fmt::format("invalid unary expression code {}", OPPLUS));
}

TEST(ASLBuilderTest, MakeBinary) {
  const int opcodes[] = {
      OPPLUS, OPMINUS, OPMULT, OPDIV, OPREM, OPPOW, OPLESS, OP_atan2,
      OPintDIV, OPprecision, OPround, OPtrunc, OP1POW, OPCPOW
  };
  TestASLBuilder builder;
  NumericExpr lhs = builder.MakeNumericConstant(1);
  NumericExpr rhs = builder.MakeNumericConstant(2);
  for (size_t i = 0, n = sizeof(opcodes) / sizeof(*opcodes); i < n; ++i) {
    ampl::BinaryExpr expr = builder.MakeBinary(opcodes[i], lhs, rhs);
    EXPECT_EQ(opcodes[i], expr.opcode());
    EXPECT_EQ(lhs, expr.lhs());
    EXPECT_EQ(rhs, expr.rhs());
  }
  EXPECT_THROW_MSG(builder.MakeBinary(OPUMINUS, lhs, rhs), ampl::Error,
    fmt::format("invalid binary expression code {}", OPUMINUS));
}

TEST(ASLBuilderTest, MakeVarArg) {
  const int opcodes[] = {MINLIST, MAXLIST};
  TestASLBuilder builder;
  enum {NUM_ARGS = 3};
  NumericExpr args[NUM_ARGS] = {
      builder.MakeNumericConstant(1),
      builder.MakeNumericConstant(2),
      builder.MakeNumericConstant(3)
  };
  for (size_t i = 0, n = sizeof(opcodes) / sizeof(*opcodes); i < n; ++i) {
    ampl::VarArgExpr expr = builder.MakeVarArg(opcodes[i], NUM_ARGS, args);
    EXPECT_EQ(opcodes[i], expr.opcode());
    int arg_index = 0;
    for (ampl::VarArgExpr::iterator
        i = expr.begin(), end = expr.end(); i != end; ++i, ++arg_index) {
      EXPECT_EQ(args[arg_index], *i);
    }
  }
  EXPECT_THROW_MSG(builder.MakeVarArg(OPUMINUS, NUM_ARGS, args), ampl::Error,
      fmt::format("invalid vararg expression code {}", OPUMINUS));
#ifndef NDEBUG
  EXPECT_DEBUG_DEATH(
      builder.MakeVarArg(MINLIST, -1, args);, "Assertion");  // NOLINT(*)
#endif
}

TEST(ASLBuilderTest, MakeSum) {
  TestASLBuilder builder;
  enum {NUM_ARGS = 3};
  NumericExpr args[NUM_ARGS] = {
      builder.MakeNumericConstant(1),
      builder.MakeNumericConstant(2),
      builder.MakeNumericConstant(3)
  };
  ampl::SumExpr expr = builder.MakeSum(NUM_ARGS, args);
  EXPECT_EQ(OPSUMLIST, expr.opcode());
  int arg_index = 0;
  for (ampl::SumExpr::iterator
      i = expr.begin(), end = expr.end(); i != end; ++i, ++arg_index) {
    EXPECT_EQ(args[arg_index], *i);
  }
#ifndef NDEBUG
  EXPECT_DEBUG_DEATH(builder.MakeSum(-1, args);, "Assertion");  // NOLINT(*)
#endif
}

TEST(ASLBuilderTest, MakeCount) {
  TestASLBuilder builder;
  enum {NUM_ARGS = 3};
  LogicalExpr args[NUM_ARGS] = {
      builder.MakeLogicalConstant(1),
      builder.MakeLogicalConstant(2),
      builder.MakeLogicalConstant(3)
  };
  ampl::CountExpr expr = builder.MakeCount(NUM_ARGS, args);
  EXPECT_EQ(OPCOUNT, expr.opcode());
  int arg_index = 0;
  for (ampl::CountExpr::iterator
      i = expr.begin(), end = expr.end(); i != end; ++i, ++arg_index) {
    EXPECT_EQ(args[arg_index], *i);
  }
#ifndef NDEBUG
  EXPECT_DEBUG_DEATH(builder.MakeCount(-1, args);, "Assertion");  // NOLINT(*)
#endif
}

TEST(ASLBuilderTest, MakeIf) {
  TestASLBuilder builder;
  LogicalExpr condition = builder.MakeLogicalConstant(true);
  NumericExpr true_expr = builder.MakeNumericConstant(1);
  NumericExpr false_expr = builder.MakeNumericConstant(2);
  ampl::IfExpr expr = builder.MakeIf(condition, true_expr, false_expr);
  EXPECT_EQ(OPIFnl, expr.opcode());
  EXPECT_EQ(condition, expr.condition());
  EXPECT_EQ(true_expr, expr.true_expr());
  EXPECT_EQ(false_expr, expr.false_expr());
}

TEST(ASLBuilderTest, MakePiecewiseLinear) {
  enum { NUM_BREAKPOINTS = 2 };
  double breakpoints[NUM_BREAKPOINTS] = { 11, 22 };
  double slopes[NUM_BREAKPOINTS + 1] = {33, 44, 55};
  TestASLBuilder builder(3);
  ampl::Variable var = builder.MakeVariable(2);
  ampl::PiecewiseLinearExpr expr = builder.MakePiecewiseLinear(
      NUM_BREAKPOINTS, breakpoints, slopes, var);
  EXPECT_EQ(OPPLTERM, expr.opcode());
  EXPECT_EQ(NUM_BREAKPOINTS, expr.num_breakpoints());
  EXPECT_EQ(NUM_BREAKPOINTS + 1, expr.num_slopes());
  for (int i = 0; i < NUM_BREAKPOINTS; ++i) {
    EXPECT_EQ(breakpoints[i], expr.breakpoint(i));
    EXPECT_EQ(slopes[i], expr.slope(i));
  }
  EXPECT_EQ(slopes[NUM_BREAKPOINTS], expr.slope(NUM_BREAKPOINTS));
  EXPECT_EQ(2, expr.var_index());
#ifndef NDEBUG
  EXPECT_DEBUG_DEATH(
      builder.MakePiecewiseLinear(-1, breakpoints, slopes, var);,
      "Assertion");  // NOLINT(*)
#endif
}

TEST(ASLBuilderTest, MakeVariable) {
  TestASLBuilder builder(10);
  ampl::Variable var = builder.MakeVariable(0);
  EXPECT_EQ(OPVARVAL, var.opcode());
  EXPECT_EQ(0, var.index());
  var = builder.MakeVariable(9);
  EXPECT_EQ(9, var.index());
  EXPECT_DEBUG_DEATH(builder.MakeVariable(-1);, "Assertion");  // NOLINT(*)
  EXPECT_DEBUG_DEATH(builder.MakeVariable(10);, "Assertion");  // NOLINT(*)
}

TEST(ASLBuilderTest, MakeNumberOf) {
  TestASLBuilder builder;
  NumericExpr value = builder.MakeNumericConstant(1);
  enum {NUM_ARGS = 2};
  NumericExpr args[NUM_ARGS] = {
      builder.MakeNumericConstant(2), builder.MakeNumericConstant(3)
  };
  ampl::NumberOfExpr expr = builder.MakeNumberOf(value, NUM_ARGS, args);
  EXPECT_EQ(OPNUMBEROF, expr.opcode());
  EXPECT_EQ(value, expr.value());
  int arg_index = 0;
  for (ampl::NumberOfExpr::iterator
      i = expr.begin(), end = expr.end(); i != end; ++i, ++arg_index) {
    EXPECT_EQ(args[arg_index], *i);
  }
#ifndef NDEBUG
  EXPECT_DEBUG_DEATH(
      builder.MakeNumberOf(value, -1, args);, "Assertion");  // NOLINT(*)
#endif
}

TEST(ASLBuilderTest, MakeCall) {
  TestASLBuilder builder;
  //builder.MakeCall();
  // TODO: where to get function?
}

TEST(ASLBuilderTest, MakeNumericConstant) {
  TestASLBuilder builder;
  ampl::NumericConstant expr = builder.MakeNumericConstant(42);
  EXPECT_EQ(OPNUM, expr.opcode());
  EXPECT_EQ(42.0, expr.value());
}

// TODO: test StringLiteral

// TODO: test f_read

// TODO: more tests
}
