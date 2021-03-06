//
// tbint.h
//
// Copyright (C) 1996 Limit Point Systems, Inc.
//
// Author: Edward Seidl <seidl@janed.com>
// Maintainer: LPS
//
// This file is part of the SC Toolkit.
//
// The SC Toolkit is free software; you can redistribute it and/or modify
// it under the terms of the GNU Library General Public License as published by
// the Free Software Foundation; either version 2, or (at your option)
// any later version.
//
// The SC Toolkit is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public License
// along with the SC Toolkit; see the file COPYING.LIB.  If not, write to
// the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
//
// The U.S. Government is granted a limited license as per AL 91-7.
//

#ifndef _chemistry_qc_basis_tbint_h
#define _chemistry_qc_basis_tbint_h

#include <util/ref/ref.h>
#include <util/group/message.h>
#include <util/container/stdarray.h>
#include <chemistry/qc/basis/gaussbas.h>
#include <chemistry/qc/basis/dercent.h>
#include <chemistry/qc/basis/operator.h>
#include <chemistry/qc/basis/obint.h>

namespace sc {

// //////////////////////////////////////////////////////////////////////////

class Integral;

/**
 * Describes types of integrals of 2-body operators
 */
struct TwoBodyIntShape {
    enum value {
      /// 4-center integral in chemistry convention
      11_O_22,
      /// 3-center integral
      11_O_2,
      /// 2-center integral
      1_O_2 };
};

/** This is an abstract base type for classes that
    compute integrals of general N-body operators described by OperDescr
 */
class NBodyIntEval : public RefCount {

  private:
    double *log2_to_double_;

  protected:
    // this is who created me
    Integral *integral_;

    std::vector< Ref<GaussianBasisSet> > bs_;

    double *buffer_;

    int redundant_;

    NBodyInt(Integral *integral,
             const std::vector< Ref<GaussianBasisSet> >& bs);
  public:
    virtual ~NBodyInt();

    /// Return the number of basis functions on center \c center
    Ref<GaussianBasisSet> basis(size_t center = 0) { return bs_.at(center); }

    /** Returns the type of the operator set that this object computes.
        this function is necessary to describe the computed integrals
        (their number, symmetries, etc.) and/or to implement cloning. */
    virtual NBodyOperSet::type type() const =0;
    /// return the operator set descriptor
    virtual const Ref<OperSetDescr>& descr() const =0;

    /** The computed shell integrals will be put in the buffer returned
        by this member.  Some TwoBodyInt specializations have more than
	one buffer:  The type arguments selects which buffer is returned.
	If the requested type is not supported, then 0 is returned. */
    virtual const double * buffer(NBodyOper::type type = NBodyOper::coulomb) const;

    /** Given four shell indices, integrals will be computed and placed in
        the buffer.  The first two indices correspond to electron 1 and the
        second two indices correspond to electron 2.*/
    virtual void compute_shell(int,int,int,int) = 0;

    /** Given four shell indices, supported two body integral types
        are computed and returned.  The first two indices correspond
        to electron 1 and the second two indices correspond to
        electron 2. This is used in the python interface where the
        return type is automatically converted to a map of numpy
        arrays. */
    std::pair<std::map<TwoBodyOper::type,const double*>,std::array<unsigned long,4> >
    compute_shell_arrays(int,int,int,int);

    /** Return log base 2 of the maximum magnitude of any integral in a
        shell block obtained from compute_shell.  An index of -1 for any
        argument indicates any shell.  */
    virtual int log2_shell_bound(int= -1,int= -1,int= -1,int= -1) = 0;

    /** Return the maximum magnitude of any integral in a
        shell block obtained from compute_shell.  An index of -1 for any
        argument indicates any shell.  */
    double shell_bound(int= -1,int= -1,int= -1,int= -1);

    /** If redundant is true, then keep redundant integrals in the buffer.
        The default is true. */
    virtual int redundant() const { return redundant_; }
    /// See redundant().
    virtual void set_redundant(int i) { redundant_ = i; }

    /// This storage is used to cache computed integrals.
    virtual void set_integral_storage(size_t storage);

    /** Return true if the clone member can be called.  The default
     * implementation returns false. */
    virtual bool cloneable() const;

    /** Returns a clone of this.  The default implementation throws an
     * exception. */
    virtual Ref<TwoBodyInt> clone();

    /// Return the integral factory that was used to create this object.
    Integral *integral() const { return integral_; }

};


class ShellQuartetIter {
  protected:
    const double * buf;
    double scale_;

    int redund_;

    int e12;
    int e34;
    int e13e24;

    int index;

    int istart;
    int jstart;
    int kstart;
    int lstart;

    int iend;
    int jend;
    int kend;
    int lend;

    int icur;
    int jcur;
    int kcur;
    int lcur;

    int i_;
    int j_;
    int k_;
    int l_;

  public:
    ShellQuartetIter();
    virtual ~ShellQuartetIter();

    virtual void init(const double *,
                      int, int, int, int,
                      int, int, int, int,
                      int, int, int, int,
                      double, int);

    virtual void start();
    virtual void next();

    int ready() const { return icur < iend; }

    int i() const { return i_; }
    int j() const { return j_; }
    int k() const { return k_; }
    int l() const { return l_; }

    int nint() const { return iend*jend*kend*lend; }

    double val() const { return buf[index]*scale_; }
};

class NBodyIntIter {
  protected:
    Ref<TwoBodyInt> tbi;
    ShellQuartetIter sqi;

    int iend;

    int icur;
    int jcur;
    int kcur;
    int lcur;

  public:
    TwoBodyIntIter();
    TwoBodyIntIter(const Ref<TwoBodyInt>&);

    virtual ~TwoBodyIntIter();

    virtual void start();
    virtual void next();

    int ready() const { return (icur < iend); }

    int ishell() const { return icur; }
    int jshell() const { return jcur; }
    int kshell() const { return kcur; }
    int lshell() const { return lcur; }

    virtual double scale() const;

    ShellQuartetIter& current_quartet();
};

// //////////////////////////////////////////////////////////////////////////

class TwoBodyTwoCenterIntIter : public RefCount {
  protected:
    Ref<TwoBodyTwoCenterInt> tbi; // help me obi wan
    TwoBodyOper::type type;
    ShellPairIter spi;

    int redund;

    int istart;
    int jstart;

    int iend;
    int jend;

    int icur;
    int jcur;

    int ij;

  public:
    TwoBodyTwoCenterIntIter();
    TwoBodyTwoCenterIntIter(const Ref<TwoBodyTwoCenterInt>& tbinteval,
                            TwoBodyOper::type t = TwoBodyOper::eri);
    virtual ~TwoBodyTwoCenterIntIter();

    virtual void start(int ist=0, int jst=0, int ien=0, int jen=0);
    virtual void next();

    int ready() const { return (icur < iend); }

    int ishell() const { return icur; }
    int jshell() const { return jcur; }

    int ijshell() const { return ij; }

    int redundant() const { return redund; }
    void set_redundant(int i) { redund=i; }

    virtual double scale() const;

    Ref<TwoBodyTwoCenterInt> two_body_int() { return tbi; }

    ShellPairIter& current_pair();

    virtual bool cloneable() const;
    virtual Ref<TwoBodyTwoCenterIntIter> clone();
};

/////////////

/// The 2-body analog of OneBodyIntOp
class TwoBodyTwoCenterIntOp: public SCElementOp {
  protected:
    Ref<TwoBodyTwoCenterIntIter> iter;

  public:
    TwoBodyTwoCenterIntOp(const Ref<TwoBodyTwoCenterInt>&);
    TwoBodyTwoCenterIntOp(const Ref<TwoBodyTwoCenterIntIter>&);
    virtual ~TwoBodyTwoCenterIntOp();

    void process(SCMatrixBlockIter&);
    void process_spec_rect(SCMatrixRectBlock*);
    void process_spec_ltri(SCMatrixLTriBlock*);
    void process_spec_rectsub(SCMatrixRectSubBlock*);
    void process_spec_ltrisub(SCMatrixLTriSubBlock*);

    bool cloneable() const;
    Ref<SCElementOp> clone();

    int has_side_effects();
};


}

#endif

// Local Variables:
// mode: c++
// c-file-style: "ETS"
// End:
