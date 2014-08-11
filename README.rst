AMPL/MP
=======

An open-source library for mathematical programming.

Components:

* Reusable high-performance `.nl <https://en.wikipedia.org/wiki/Nl_(format)>`__ reader

* Efficient type-safe C++ API for writing AMPL solvers:
  `source <https://github.com/ampl/mp/tree/master/src/asl>`__

* AMPL bindings for the GNU Scientific Library: `docs <http://ampl.github.io/amplgsl/>`__,
  `source <https://github.com/ampl/mp/tree/master/src/gsl>`__

* Interfaces to solvers supporting
  `AMPL extensions for logic and constraint programming <http://ampl.com/resources/logic-and-constraint-programming-extensions/>`__:

  - IBM ILOG CPLEX and CPLEX CP Optimizer (`ilogcp <https://github.com/ampl/mp/tree/master/solvers/ilogcp>`__)

  - `Gecode <https://github.com/ampl/mp/tree/master/solvers/gecode>`__

  - `JaCoP <https://github.com/ampl/mp/tree/master/solvers/jacop>`__

* Interfaces to the following solvers:

  - `LocalSolver <https://github.com/ampl/mp/tree/master/solvers/localsolver>`__
  - `Sulum <https://github.com/ampl/mp/tree/master/solvers/sulum>`__

* Interfaces to other solvers via AMPL Solver Library
