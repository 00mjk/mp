/*
 AMPL suffix support
 Suffixes are values associated with model components.
 See http://www.ampl.com/NEW/suffixes.html

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
 whatsoever resulting from loss of use	, data or profits, whether in an
 action of contract, negligence or other tortious action, arising out
 of or in connection with the use or performance of this software.

 Author: Victor Zverovich
 */

#ifndef MP_SUFFIX_H_
#define MP_SUFFIX_H_

#include <cstddef>  // for std::size_t
#include <iterator>
#include <set>
#include <string>  // for std::char_traits

#include "mp/common.h"
#include "mp/format.h"

namespace mp {

template <typename T>
class BasicSuffix;

namespace internal {

// A suffix.
// Suffixes are metadata that can be attached to variables, objectives,
// constraints and problems.
class SuffixBase {
 protected:
  struct Impl {
    // Name is stored as a StringRef rather than std::string to avoid
    // dynamic memory allocation when using set::find.
    fmt::StringRef name;
    int kind;
    int num_values;
    union {
      void *values;
      int *int_values;
      double *dbl_values;
    };

    explicit Impl(fmt::StringRef name, int kind = 0, int num_values = 0)
      : name(name), kind(kind), num_values(num_values), int_values(0) {}
  };

  template <typename SuffixType>
  explicit SuffixBase(SuffixType s) : impl_(s.impl_) {}

  void get_value(int index, int &value) const {
    value = impl_->int_values[index];
  }
  void set_value(int index, int value) { impl_->int_values[index] = value; }

  void get_value(int index, double &value) const {
    value = impl_->dbl_values[index];
  }
  void set_value(int index, double value) { impl_->dbl_values[index] = value; }

  const Impl *impl() const { return impl_; }

  explicit SuffixBase(const Impl *impl) : impl_(impl) {}

  // Safe bool type.
  typedef void (SuffixBase::*SafeBool)() const;

 private:
  const Impl *impl_;

  friend class SuffixSet;

  // A member function representing the true value of SafeBool.
  void True() const {}

 public:
  // Constructs a Suffix object representing a null reference to a
  // suffix. The only operation permitted for such object is copying,
  // assignment and check whether it is null using operator SafeBool.
  SuffixBase() : impl_() {}

  // Returns the suffix name.
  const char *name() const { return impl_->name.c_str(); }

  // Returns the suffix kind.
  int kind() const { return impl_->kind; }

  int num_values() const { return impl_->num_values; }

  // Returns a value convertible to bool that can be used in conditions but not
  // in comparisons and evaluates to "true" if this suffix is not null
  // and "false" otherwise.
  // Example:
  //   if (s) {
  //     // Do something if s is not null.
  //   }
  operator SafeBool() const { return impl_ != 0 ? &SuffixBase::True : 0; }
};
}  // namespace internal

class Suffix : private internal::SuffixBase {
 private:
  // SuffixBase is a friend because it needs access to SuffixBase::impl_ via
  // a private base class.
  friend class SuffixBase;
  friend class SuffixSet;

  explicit Suffix(const Impl *impl) : SuffixBase(impl) {}

 public:
  Suffix() {}

  template <typename T>
  Suffix(BasicSuffix<T> other) : SuffixBase(other) {}

  using SuffixBase::name;
  using SuffixBase::kind;
  using SuffixBase::num_values;
  using SuffixBase::operator SafeBool;

  // Iterates over nonzero suffix values and sends them to the visitor.
  template <typename Visitor>
  void VisitValues(Visitor &visitor) const;
};

template <typename SuffixType>
SuffixType Cast(Suffix s);

template <typename T>
class BasicSuffix : private internal::SuffixBase {
 private:
  // SuffixBase is a friend because it needs access to SuffixBase::impl_ via
  // a private base class.
  friend class SuffixBase;
  friend class SuffixSet;

  friend BasicSuffix<T> Cast< BasicSuffix<T> >(Suffix s);

  explicit BasicSuffix(const Impl *impl) : SuffixBase(impl) {}
  explicit BasicSuffix(Suffix other) : SuffixBase(other) {}

 public:
  BasicSuffix() {}

  using SuffixBase::operator SafeBool;

  T value(int index) const {
    assert(index < impl()->num_values);
    T result = T();
    get_value(index, result);
    return result;
  }

  void set_value(int index, T value) {
    assert(index < impl()->num_values);
    SuffixBase::set_value(index, value);
  }

  template <typename Visitor>
  void VisitValues(Visitor &visitor) const {
    for (int i = 0, n = num_values(); i < n; ++i) {
      T value = T();
      get_value(i, value);
      if (value != 0)
        visitor.Visit(i, value);
    }
  }
};

typedef BasicSuffix<int> IntSuffix;
typedef BasicSuffix<double> DoubleSuffix;

namespace internal {

// Returns true if s is of type SuffixType.
template <typename SuffixType>
bool Is(Suffix s);

template <>
inline bool Is<IntSuffix>(Suffix s) { return (s.kind() & suf::FLOAT) == 0; }

template <>
inline bool Is<DoubleSuffix>(Suffix s) { return (s.kind() & suf::FLOAT) != 0; }
}

// Casts a suffix to type SuffixType which must be a valid suffix type.
// Returns a null suffix if s is not convertible to SuffixType.
template <typename SuffixType>
inline SuffixType Cast(Suffix s) {
  return internal::Is<SuffixType>(s) ? SuffixType(s) : SuffixType();
}

template <typename Visitor>
inline void Suffix::VisitValues(Visitor &visitor) const {
  IntSuffix int_suffix = Cast<IntSuffix>(*this);
  if (int_suffix)
    int_suffix.VisitValues(visitor);
  else
    Cast<DoubleSuffix>(*this).VisitValues(visitor);
}

// A set of suffixes.
class SuffixSet {
 private:
  struct SuffixNameLess {
    bool operator()(const Suffix::Impl &lhs, const Suffix::Impl &rhs) const {
      std::size_t lhs_size = lhs.name.size(), rhs_size = rhs.name.size();
      if (lhs_size != rhs_size)
        return lhs_size < rhs_size;
      return std::char_traits<char>::compare(
            lhs.name.c_str(), rhs.name.c_str(), lhs_size) < 0;
    }
  };

  typedef std::set<Suffix::Impl, SuffixNameLess> Set;
  Set set_;

  FMT_DISALLOW_COPY_AND_ASSIGN(SuffixSet);

  template <typename Alloc>
  friend class BasicProblem;

  Suffix::Impl *DoAdd(fmt::StringRef name, int kind, int num_values);

 public:
  SuffixSet() {}
  ~SuffixSet();

  template <typename T>
  BasicSuffix<T> Add(fmt::StringRef name, int kind, int num_values) {
    // TODO: check if suffix kind agrees with type
    Suffix::Impl *impl = DoAdd(name, kind, num_values);
    impl->values = new T[num_values];
    return BasicSuffix<T>(impl);
  }

  // Finds a suffix with the specified name.
  Suffix Find(fmt::StringRef name) const {
    Set::iterator i = set_.find(Suffix::Impl(name));
    return Suffix(i != set_.end() ? &*i : 0);
  }

  class iterator : public std::iterator<std::forward_iterator_tag, Suffix> {
   private:
    Set::const_iterator it_;

    // A suffix proxy used for implementing operator->.
    class Proxy {
     private:
      Suffix suffix_;

     public:
      explicit Proxy(const Suffix::Impl *impl) : suffix_(impl) {}

      const Suffix *operator->() const { return &suffix_; }
    };

   public:
    iterator(Set::const_iterator it) : it_(it) {}

    Suffix operator*() const { return Suffix(&*it_); }

    Proxy operator->() const { return Proxy(&*it_); }

    iterator &operator++() {
      ++it_;
      return *this;
    }

    iterator operator++(int ) {
      iterator it(*this);
      ++it_;
      return it;
    }

    bool operator==(iterator other) const { return it_ == other.it_; }
    bool operator!=(iterator other) const { return it_ != other.it_; }
  };

  iterator begin() const { return iterator(set_.begin()); }
  iterator end() const { return iterator(set_.end()); }
};

class SuffixManager {
 private:
  mp::SuffixSet suffixes_[suf::NUM_KINDS];

 public:
  virtual ~SuffixManager() {}

  typedef mp::Suffix Suffix;
  typedef mp::IntSuffix IntSuffix;
  typedef mp::SuffixSet SuffixSet;

  // Returns a set of suffixes.
  SuffixSet &suffixes(int kind) {
    assert(kind < suf::NUM_KINDS);
    return suffixes_[kind];
  }
};

}  // namespace mp

#endif  // MP_SUFFIX_H_
