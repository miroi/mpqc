//
// uks.cc --- implementation of the unrestricted Hartree-Fock class
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

#ifdef __GNUC__
#pragma implementation
#endif

#include <math.h>

#include <util/misc/timer.h>
#include <util/misc/formio.h>

#include <math/optimize/scextrapmat.h>

#include <chemistry/qc/basis/petite.h>

#include <chemistry/qc/dft/uks.h>
#include <chemistry/qc/scf/lgbuild.h>
#include <chemistry/qc/scf/ltbgrad.h>

///////////////////////////////////////////////////////////////////////////

class LocalUKSContribution {
  private:
    double * const gmata;
    double * const gmatb;
    double * const pmata;
    double * const pmatb;
    double a0;

  public:
    LocalUKSContribution(double *ga, double *pa, double *gb, double *pb,
                         double a) :
      gmata(ga), pmata(pa), gmatb(gb), pmatb(pb), a0(a) {}
    ~LocalUKSContribution() {}

    inline void cont1(int ij, int kl, double val) {
      gmata[ij] += val*(pmata[kl]+pmatb[kl]);
      gmata[kl] += val*(pmata[ij]+pmatb[ij]);

      gmatb[ij] += val*(pmata[kl]+pmatb[kl]);
      gmatb[kl] += val*(pmata[ij]+pmatb[ij]);
    }
    
    inline void cont2(int ij, int kl, double val) {
      val *= a0*0.5;
      gmata[ij] -= val*pmata[kl];
      gmata[kl] -= val*pmata[ij];

      gmatb[ij] -= val*pmatb[kl];
      gmatb[kl] -= val*pmatb[ij];
    }
    
    inline void cont3(int ij, int kl, double val) {
      val *= a0;
      gmata[ij] -= val*pmata[kl];
      gmata[kl] -= val*pmata[ij];

      gmatb[ij] -= val*pmatb[kl];
      gmatb[kl] -= val*pmatb[ij];
    }
    
    inline void cont4(int ij, int kl, double val) {
      cont1(ij,kl,val);
      cont2(ij,kl,val);
    }
    
    inline void cont5(int ij, int kl, double val) {
      cont1(ij,kl,val);
      cont3(ij,kl,val);
    }
};

class LocalUKSEnergyContribution {
  private:
    double * const pmata;
    double * const pmatb;
    double a0;

  public:
    double ec;
    double ex;
    
    LocalUKSEnergyContribution(double *a, double *b, double an) :
      pmata(a), pmatb(b), a0(an) {
      ec=ex=0;
    }

    ~LocalUKSEnergyContribution() {}

    inline void cont1(int ij, int kl, double val) {
      ec += val*(pmata[ij]+pmatb[ij])*(pmata[kl]+pmatb[kl]);
    }
    
    inline void cont2(int ij, int kl, double val) {
      ex -= a0*0.5*val*(pmata[ij]*pmata[kl]+pmatb[ij]*pmatb[kl]);
    }
    
    inline void cont3(int ij, int kl, double val) {
      ex -= a0*val*(pmata[ij]*pmata[kl]+pmatb[ij]*pmatb[kl]);
    }
    
    inline void cont4(int ij, int kl, double val) {
      cont1(ij,kl,val);
      cont2(ij,kl,val);
    }
    
    inline void cont5(int ij, int kl, double val) {
      cont1(ij,kl,val);
      cont3(ij,kl,val);
    }
};

class LocalUKSGradContribution {
  private:
    double * const pmata;
    double * const pmatb;

  public:
    LocalUKSGradContribution(double *a, double *b) : pmata(a), pmatb(b) {}
    ~LocalUKSGradContribution() {}

    inline double cont1(int ij, int kl) {
      return (pmata[ij]*pmata[kl])+(pmatb[ij]*pmatb[kl]) +
             (pmata[ij]*pmatb[kl])+(pmatb[ij]*pmata[kl]);
    }

    inline double cont2(int ij, int kl) {
      return 2*((pmata[ij]*pmata[kl])+(pmatb[ij]*pmatb[kl]));
    }
};

#ifdef __GNUC__
template class GBuild<LocalUKSContribution>;
template class GBuild<LocalUKSEnergyContribution>;
template class LocalGBuild<LocalUKSContribution>;
template class LocalGBuild<LocalUKSEnergyContribution>;

template class TBGrad<LocalUKSGradContribution>;
template class LocalTBGrad<LocalUKSGradContribution>;
#endif

///////////////////////////////////////////////////////////////////////////
// UKS

#define CLASSNAME UKS
#define HAVE_STATEIN_CTOR
#define HAVE_KEYVAL_CTOR
#define PARENTS public UnrestrictedSCF
#include <util/class/classi.h>
void *
UKS::_castdown(const ClassDesc*cd)
{
  void* casts[1];
  casts[0] = UnrestrictedSCF::_castdown(cd);
  return do_castdowns(casts,cd);
}

UKS::UKS(StateIn& s) :
  UnrestrictedSCF(s)
  maybe_SavableState(s)
{
  exc_=0;
  integrator_ = new Murray93Integrator();
  functional_ = new LSDAXFunctional();
}

UKS::UKS(const RefKeyVal& keyval) :
  UnrestrictedSCF(keyval)
{
  exc_=0;
  integrator_ = keyval->describedclassvalue("integrator");
  if (integrator_.null()) integrator_ = new Murray93Integrator();

  functional_ = keyval->describedclassvalue("functional");
  if (functional_.null()) functional_ = new LSDAXFunctional();
}

UKS::~UKS()
{
}

void
UKS::save_data_state(StateOut& s)
{
  UnrestrictedSCF::save_data_state(s);
}

int
UKS::value_implemented()
{
  return 1;
}

int
UKS::gradient_implemented()
{
  return 0;
}

int
UKS::hessian_implemented()
{
  return 0;
}

double
UKS::scf_energy()
{
  RefSymmSCMatrix mva = vaxc_.copy();
  mva.scale(-1.0);
  focka_.result_noupdate().accumulate(mva);
  RefSymmSCMatrix mvb = vbxc_.copy();
  mvb.scale(-1.0);
  fockb_.result_noupdate().accumulate(mvb);
  double ehf = UnrestrictedSCF::scf_energy();
  focka_.result_noupdate().accumulate(vaxc_);
  fockb_.result_noupdate().accumulate(vbxc_);
  return ehf + exc_;
}

RefSCExtrapData
UKS::extrap_data()
{
  RefSymmSCMatrix *m = new RefSymmSCMatrix[4];
  m[0] = focka_.result_noupdate();
  m[1] = fockb_.result_noupdate();
  m[2] = vaxc_;
  m[3] = vbxc_;
  
  RefSCExtrapData data = new SymmSCMatrixNSCExtrapData(4, m);
  delete[] m;
  
  return data;
}

void
UKS::print(ostream&o)
{
  UnrestrictedSCF::print(o);
}

//////////////////////////////////////////////////////////////////////////////

void
UKS::two_body_energy(double &ec, double &ex)
{
  tim_enter("uks e2");
  ec = 0.0;
  ex = 0.0;
  if (local_ || local_dens_) {
    // grab the data pointers from the G and P matrices
    double *apmat;
    double *bpmat;
    tim_enter("local data");
    RefSymmSCMatrix adens = alpha_ao_density();
    RefSymmSCMatrix bdens = beta_ao_density();
    adens->scale(2.0);
    adens->scale_diagonal(0.5);
    bdens->scale(2.0);
    bdens->scale_diagonal(0.5);
    RefSymmSCMatrix aptmp = get_local_data(adens, apmat, SCF::Read);
    RefSymmSCMatrix bptmp = get_local_data(bdens, bpmat, SCF::Read);
    tim_exit("local data");

    // initialize the two electron integral classes
    tbi_ = integral()->electron_repulsion();
    tbi_->set_integral_storage(0);

    signed char * pmax = init_pmax(apmat);
  
    LocalUKSEnergyContribution lclc(apmat, bpmat, 0);
    LocalGBuild<LocalUKSEnergyContribution>
      gb(lclc, tbi_, integral(), basis(), scf_grp_, pmax);
    gb.build_gmat(desired_value_accuracy()/100.0);

    delete[] pmax;

    tbi_ = 0;

    ec = lclc.ec;
    ex = lclc.ex;
  }
  else {
    cerr << node0 << indent << "Cannot yet use anything but Local matrices\n";
    abort();
  }
  tim_exit("uks e2");
}

//////////////////////////////////////////////////////////////////////////////

void
UKS::ao_fock()
{
  RefPetiteList pl = integral()->petite_list(basis());
  
  // calculate G.  First transform diff_densa_ to the AO basis, then
  // scale the off-diagonal elements by 2.0
  RefSymmSCMatrix dda = diff_densa_;
  diff_densa_ = pl->to_AO_basis(dda);
  diff_densa_->scale(2.0);
  diff_densa_->scale_diagonal(0.5);

  RefSymmSCMatrix ddb = diff_densb_;
  diff_densb_ = pl->to_AO_basis(ddb);
  diff_densb_->scale(2.0);
  diff_densb_->scale_diagonal(0.5);

  // now try to figure out the matrix specialization we're dealing with
  // if we're using Local matrices, then there's just one subblock, or
  // see if we can convert G and P to local matrices
  if (local_ || local_dens_) {
    double *gmat, *gmato, *pmat, *pmato;
    
    // grab the data pointers from the G and P matrices
    RefSymmSCMatrix gtmp = get_local_data(gmata_, gmat, SCF::Accum);
    RefSymmSCMatrix ptmp = get_local_data(diff_densa_, pmat, SCF::Read);
    RefSymmSCMatrix gotmp = get_local_data(gmatb_, gmato, SCF::Accum);
    RefSymmSCMatrix potmp = get_local_data(diff_densb_, pmato, SCF::Read);

    signed char * pmax = init_pmax(pmat);
  
    LocalUKSContribution lclc(gmat, pmat, gmato, pmato, 0);
    LocalGBuild<LocalUKSContribution>
      gb(lclc, tbi_, integral(), basis(), scf_grp_, pmax);
    gb.build_gmat(desired_value_accuracy()/100.0);

    delete[] pmax;

    // if we're running on multiple processors, then sum the G matrices
    if (scf_grp_->n() > 1) {
      scf_grp_->sum(gmat, i_offset(basis()->nbasis()));
      scf_grp_->sum(gmato, i_offset(basis()->nbasis()));
    }
    
    // if we're running on multiple processors, or we don't have local
    // matrices, then accumulate gtmp back into G
    if (!local_ || scf_grp_->n() > 1) {
      gmata_->convert_accumulate(gtmp);
      gmatb_->convert_accumulate(gotmp);
    }
  }

  // for now quit
  else {
    cerr << node0 << indent << "Cannot yet use anything but Local matrices\n";
    abort();
  }
  
  tim_enter("integrate");
  diff_densa_ = pl->to_AO_basis(densa_);
  diff_densb_ = pl->to_AO_basis(densb_);
  integrator_->set_wavefunction(this);
  integrator_->set_compute_potential_integrals(1);
  integrator_->integrate(functional_, diff_densa_, diff_densb_);
  exc_ = integrator_->value();
  RefSymmSCMatrix vxa = gmata_.clone();
  RefSymmSCMatrix vxb = gmatb_.clone();
  vxa->assign((double*)integrator_->alpha_vmat());
  vxb->assign((double*)integrator_->beta_vmat());
  vxa = pl->to_SO_basis(vxa);
  vxb = pl->to_SO_basis(vxb);
  vaxc_ = vxa;
  vbxc_ = vxb;
  tim_exit("integrate");

  // get rid of AO delta P
  diff_densa_ = dda;
  dda = diff_densa_.clone();

  diff_densb_ = ddb;
  ddb = diff_densb_.clone();

  // now symmetrize the skeleton G matrix, placing the result in dda
  RefSymmSCMatrix skel_gmat = gmata_.copy();
  skel_gmat.scale(1.0/(double)pl->order());
  pl->symmetrize(skel_gmat,dda);

  skel_gmat = gmatb_.copy();
  skel_gmat.scale(1.0/(double)pl->order());
  pl->symmetrize(skel_gmat,ddb);
  
  // Fa = H+Ga
  focka_.result_noupdate().assign(hcore_);
  focka_.result_noupdate().accumulate(dda);
  focka_.result_noupdate().accumulate(vaxc_);

  // Fb = H+Gb
  fockb_.result_noupdate().assign(hcore_);
  fockb_.result_noupdate().accumulate(ddb);
  fockb_.result_noupdate().accumulate(vbxc_);

  dda.assign(0.0);
  accumddh_->accum(dda);
  focka_.result_noupdate().accumulate(dda);
  fockb_.result_noupdate().accumulate(dda);

  focka_.computed()=1;
  fockb_.computed()=1;
}

/////////////////////////////////////////////////////////////////////////////

void
UKS::two_body_deriv(double * tbgrad)
{
  RefSCElementMaxAbs m = new SCElementMaxAbs;
  densa_.element_op(m);
  double pmax = m->result();
  m=0;

  // now try to figure out the matrix specialization we're dealing with.
  // if we're using Local matrices, then there's just one subblock, or
  // see if we can convert P to local matrices

  if (local_ || local_dens_) {
    // grab the data pointers from the P matrices
    double *pmata, *pmatb;
    RefSymmSCMatrix ptmpa = get_local_data(densa_, pmata, SCF::Read);
    RefSymmSCMatrix ptmpb = get_local_data(densb_, pmatb, SCF::Read);
  
    LocalUKSGradContribution l(pmata,pmatb);
    LocalTBGrad<LocalUKSGradContribution> tb(l, integral(), basis(),scf_grp_);
    tb.build_tbgrad(tbgrad, pmax, desired_gradient_accuracy());
  }

  // for now quit
  else {
    cerr << node0 << indent
         << "UKS::two_body_deriv: can't do gradient yet\n";
    abort();
  }
}

/////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: c++
// c-file-style: "ETS"
// End:
