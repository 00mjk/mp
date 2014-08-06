/*
 .nl file support.

 Copyright (C) 2013 AMPL Optimization Inc

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

#include "solvers/util/nl.h"

#include "solvers/asl.h"

#undef ampl_vbtol

namespace ampl {

fmt::Writer &operator<<(fmt::Writer &w, const NLHeader &h) {
  w << (h.format == NLHeader::TEXT ? 'g' : 'b') << h.num_options;
  for (int i = 0; i < h.num_options; ++i)
    w << ' ' << h.options[i];
  if (h.options[VBTOL_OPTION] == READ_VBTOL)
    w << ' ' << h.ampl_vbtol;
  w << '\n';
  w.write(" {} {} {} {} {} {}\n",
      h.num_vars, h.num_algebraic_cons, h.num_objs,
      h.num_ranges, h.num_eqns, h.num_logical_cons);
  w.write(" {} {} {} {} {} {}\n",
      h.num_nl_cons, h.num_nl_objs,
      h.num_compl_conds - h.num_nl_compl_conds,
      h.num_nl_compl_conds, h.num_compl_dbl_ineqs,
      h.num_compl_vars_with_nz_lb);
  w.write(" {} {}\n", h.num_nl_net_cons, h.num_linear_net_cons);
  w.write(" {} {} {}\n",
      h.num_nl_vars_in_cons, h.num_nl_vars_in_objs, h.num_nl_vars_in_both);
  w.write(" {} {} {} {}\n",
      h.num_linear_net_vars, h.num_funcs,
      h.format != NLHeader::BINARY_SWAPPED ? 0 : 3 - Arith_Kind_ASL, h.flags);
  w.write(" {} {} {} {} {}\n",
      h.num_linear_binary_vars, h.num_linear_integer_vars,
      h.num_nl_integer_vars_in_both, h.num_nl_integer_vars_in_cons,
      h.num_nl_integer_vars_in_objs);
  w.write(" {} {}\n", h.num_con_nonzeros, h.num_obj_nonzeros);
  w.write(" {} {}\n", h.max_con_name_len, h.max_var_name_len);
  w.write(" {} {} {} {} {}\n",
      h.num_common_exprs_in_both, h.num_common_exprs_in_cons,
      h.num_common_exprs_in_objs, h.num_common_exprs_in_single_cons,
      h.num_common_exprs_in_single_objs);
  return w;
}

TextReader::TextReader(fmt::StringRef data, fmt::StringRef name)
: ptr_(data.c_str()), end_(ptr_ + data.size()),
  line_start_(ptr_), token_(ptr_), name_(name), line_(1) {}

void TextReader::DoReportReadError(
    const char *loc, fmt::StringRef format_str, const fmt::ArgList &args) {
  int line = line_;
  const char *line_start = line_start_;
  if (loc < line_start) {
    --line;
    // Find the beginning of the previous line.
    line_start = loc - 1;
    while (*line_start != '\n')
      --line_start;
    ++line_start;
  }
  int column = static_cast<int>(loc - line_start + 1);
  fmt::Writer w;
  w.write(format_str, args);
  throw ReadError(name_, line, column,
      fmt::format("{}:{}:{}: {}", name_, line, column, w.c_str()));
}

fmt::StringRef TextReader::ReadString() {
  SkipSpace();
  const char *start = ptr_;
  while (!std::isspace(*ptr_) && *ptr_)
    ++ptr_;
  std::size_t length = ptr_ - start;
  return fmt::StringRef(length != 0 ? start : 0, length);
}

fmt::StringRef TextReader::ReadStringLiteral() {
  int length = ReadUInt();
  if (*ptr_ != ':')
    DoReportReadError(ptr_, "expected ':'");
  ++ptr_;
  const char *start = ptr_;
  for (int i = 0; i < length; ++i, ++ptr_) {
    char c = *ptr_;
    if (c == '\n') {
      line_start_ = ptr_  + 1;
      ++line_;
    } else if (c == 0 && ptr_ == end_) {
      DoReportReadError(ptr_, "unexpected end of file in string");
    }
  }
  if (*ptr_ != '\n')
    DoReportReadError(ptr_, "expected newline");
  ++ptr_;
  return fmt::StringRef(length != 0 ? start : 0, length);
}

void TextReader::ReadHeader(NLHeader &header) {
  // Read the format (text or binary).
  switch (ReadChar()) {
  case 'g':
    break;
  case 'b':
    header.format = NLHeader::BINARY;
    break;
  default:
    ReportReadError("expected format specifier");
    break;
  }

  // Read options.
  ReadOptionalUInt(header.num_options);
  if (header.num_options > MAX_NL_OPTIONS)
    ReportReadError("too many options");
  for (int i = 0; i < header.num_options; ++i) {
    if (!ReadOptionalInt(header.options[i]))
      break;
  }
  if (header.options[VBTOL_OPTION] == READ_VBTOL)
    ReadOptionalDouble(header.ampl_vbtol);
  ReadTillEndOfLine();

  // Read problem dimensions.
  header.num_vars = ReadUInt();
  header.num_algebraic_cons = ReadUInt();
  header.num_objs = ReadUInt();
  header.num_eqns = -1;
  if (ReadOptionalUInt(header.num_ranges) &&
      ReadOptionalUInt(header.num_eqns)) {
      ReadOptionalUInt(header.num_logical_cons);
  }
  ReadTillEndOfLine();

  // Read the nonlinear and complementarity information.
  header.num_nl_cons = ReadUInt();
  header.num_nl_objs = ReadUInt();
  bool all_compl =
      ReadOptionalUInt(header.num_compl_conds) &&
      ReadOptionalUInt(header.num_nl_compl_conds) &&
      ReadOptionalUInt(header.num_compl_dbl_ineqs) &&
      ReadOptionalUInt(header.num_compl_vars_with_nz_lb);
  header.num_compl_conds += header.num_nl_compl_conds;
  if (header.num_compl_conds > 0 && !all_compl)
    header.num_compl_dbl_ineqs = -1;
  ReadTillEndOfLine();

  // Read the information about network constraints.
  header.num_nl_net_cons = ReadUInt();
  header.num_linear_net_cons = ReadUInt();
  ReadTillEndOfLine();

  // Read the information about nonlinear variables.
  header.num_nl_vars_in_cons = ReadUInt();
  header.num_nl_vars_in_objs = ReadUInt();
  header.num_nl_vars_in_both = -1;
  ReadOptionalUInt(header.num_nl_vars_in_both);
  ReadTillEndOfLine();

  header.num_linear_net_vars = ReadUInt();
  header.num_funcs = ReadUInt();
  int arith = 0;
  if (ReadOptionalUInt(arith)) {
    if (arith != Arith_Kind_ASL && arith != 0) {
      bool swap_bytes = false;
#if defined(IEEE_MC68k) || defined(IEEE_8087)
      swap_bytes = arith > 0 && arith + Arith_Kind_ASL == 3;
#endif
      if (!swap_bytes)
        ReportReadError("unrecognized binary format");
      header.format = NLHeader::BINARY_SWAPPED;
      // TODO: swap bytes
    }
    ReadOptionalUInt(header.flags);
  }
  ReadTillEndOfLine();

  // Read the information about discrete variables.
  header.num_linear_binary_vars = ReadUInt();
  header.num_linear_integer_vars = ReadUInt();
  if (header.num_nl_vars_in_both >= 0) {  // ampl versions >= 19930630
    header.num_nl_integer_vars_in_both = ReadUInt();
    header.num_nl_integer_vars_in_cons = ReadUInt();
    header.num_nl_integer_vars_in_objs = ReadUInt();
  }
  ReadTillEndOfLine();

  // Read the information about nonzeros.
  header.num_con_nonzeros = ReadUInt();
  header.num_obj_nonzeros = ReadUInt();
  ReadTillEndOfLine();

  // Read the information about names.
  header.max_con_name_len = ReadUInt();
  header.max_var_name_len = ReadUInt();
  ReadTillEndOfLine();

  // Read the information about common expressions.
  header.num_common_exprs_in_both = ReadUInt();
  header.num_common_exprs_in_cons = ReadUInt();
  header.num_common_exprs_in_objs = ReadUInt();
  header.num_common_exprs_in_single_cons = ReadUInt();
  header.num_common_exprs_in_single_objs = ReadUInt();
  ReadTillEndOfLine();
}
}  // namespace ampl
