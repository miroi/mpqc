//
// hsosks.cc --- implementation of restricted open shell Kohn-Sham SCF
// derived from clks.cc
//
// Copyright (C) 1997 Limit Point Systems, Inc.
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
#include <util/state/stateio.h>

#include <math/optimize/scextrapmat.h>

#include <chemistry/qc/basis/petite.h>

#include <chemistry/qc/dft/hsosks.h>
#include <chemistry/qc/scf/lgbuild.h>
#include <chemistry/qc/scf/ltbgrad.h>
#include <chemistry/qc/scf/effh.h>
#include <chemistry/qc/scf/scfops.h>

#include <chemistry/qc/dft/hsoskstmpl.h>

///////////////////////////////////////////////////////////////////////////
// HSOSKS

#define CLASSNAME HSOSKS
#define HAVE_STATEIN_CTOR
#define HAVE_KEYVAL_CTOR
#define PARENTS public SCF
#include <util/class/classi.h>
void *
HSOSKS::_castdown(const ClassDesc*cd)
{
  void* casts[1];
  casts[0] = HSOSSCF::_castdown(cd);
  return do_castdowns(casts,cd);
}

HSOSKS::HSOSKS(StateIn& s) :
  SavableState(s),
  HSOSSCF(s)
{
  exc_=0;
  integrator_ = new Murray93Integrator();
  functional_ = new SlaterXFunctional();
}

HSOSKS::HSOSKS(const RefKeyVal& keyval) :
  HSOSSCF(keyval)
{
  exc_=0;
  integrator_ = keyval->describedclassvalue("integrator");
  if (integrator_.null()) integrator_ = new Murray93Integrator();

  functional_ = keyval->describedclassvalue("functional");
  if (functional_.null()) functional_ = new SlaterXFunctional();
}

HSOSKS::~HSOSKS()
{
}

void
HSOSKS::save_data_state(StateOut& s)
{
  HSOSSCF::save_data_state(s);
}

int
HSOSKS::value_implemented() const
{
  return 1;
}

int
HSOSKS::gradient_implemented() const
{
  return 1;
}

void
HSOSKS::print(ostream&o) const
{
  o << node0 << indent
    << "Restricted Open Shell Kohn-Sham (HSOSKS) Parameters:" << endl;
  o << incindent;
  HSOSSCF::print(o);
  o << node0 << indent << "Functional:" << endl;
  o << incindent;
  functional_->print(o);
  o << decindent;
  o << node0 << indent << "Integrator:" << endl;
  o << incindent;
  integrator_->print(o);
  o << decindent;
  o << decindent;
}

double
HSOSKS::scf_energy()
{
  double ehf = HSOSSCF::scf_energy();

  return ehf+exc_;
}

RefSymmSCMatrix
HSOSKS::effective_fock()
{
  RefSymmSCMatrix mofock(oso_dimension(), basis_matrixkit());
  mofock.assign(0.0);

  RefSymmSCMatrix mofocko(oso_dimension(), basis_matrixkit());
  mofocko.assign(0.0);

  // use eigenvectors if oso_scf_vector_ is null
  if (oso_scf_vector_.null()) {
    mofock.accumulate_transform(eigenvectors(), fock(0)+cl_vxc(),
                                SCMatrix::TransposeTransform);
    mofocko.accumulate_transform(eigenvectors(), fock(1)+op_vxc(),
                                 SCMatrix::TransposeTransform);
  } else {
    RefSCMatrix so_to_oso_tr = so_to_orthog_so().t();
    mofock.accumulate_transform(so_to_oso_tr * oso_scf_vector_,
                                fock(0)+cl_vxc(),
                                SCMatrix::TransposeTransform);
    mofocko.accumulate_transform(so_to_oso_tr * oso_scf_vector_,
                                 fock(1)+op_vxc(),
                                 SCMatrix::TransposeTransform);
  }

  RefSCElementOp2 op = new GSGeneralEffH(this);
  mofock.element_op(op, mofocko);

  return mofock;
}

RefSymmSCMatrix
HSOSKS::lagrangian()
{
  RefSCMatrix so_to_oso_tr = so_to_orthog_so().t();

  RefSymmSCMatrix mofock(oso_dimension(), basis_matrixkit());
  mofock.assign(0.0);
  mofock.accumulate_transform(so_to_oso_tr * oso_scf_vector_,
                              cl_fock_.result_noupdate()+cl_vxc(),
                              SCMatrix::TransposeTransform);

  RefSymmSCMatrix mofocko(oso_dimension(), basis_matrixkit());
  mofocko.assign(0.0);
  mofocko.accumulate_transform(so_to_oso_tr * oso_scf_vector_,
                               op_fock_.result_noupdate()+op_vxc(),
                               SCMatrix::TransposeTransform);

  mofock.scale(2.0);
  
  RefSCElementOp2 op = new MOLagrangian(this);
  mofock.element_op(op, mofocko);
  mofocko=0;

  // transform MO lagrangian to SO basis
  RefSymmSCMatrix so_lag(so_dimension(), basis_matrixkit());
  so_lag.assign(0.0);
  so_lag.accumulate_transform(so_to_oso_tr * oso_scf_vector_, mofock);
  
  // and then from SO to AO
  RefPetiteList pl = integral()->petite_list();
  RefSymmSCMatrix ao_lag = pl->to_AO_basis(so_lag);

  ao_lag.scale(-1.0);

  return ao_lag;
}

RefSCExtrapData
HSOSKS::extrap_data()
{
  RefSCExtrapData data =
    new SymmSCMatrix4SCExtrapData(cl_fock_.result_noupdate(),
                                  op_fock_.result_noupdate(),
                                  vxc_a_, vxc_b_);
  return data;
}

//////////////////////////////////////////////////////////////////////////////

void
HSOSKS::ao_fock(double accuracy)
{
  RefPetiteList pl = integral()->petite_list(basis());
  
  // calculate G.  First transform cl_dens_diff_ to the AO basis, then
  // scale the off-diagonal elements by 2.0
  RefSymmSCMatrix dd = cl_dens_diff_;
  cl_dens_diff_ = pl->to_AO_basis(dd);
  cl_dens_diff_->scale(2.0);
  cl_dens_diff_->scale_diagonal(0.5);

  RefSymmSCMatrix ddo = op_dens_diff_;
  op_dens_diff_ = pl->to_AO_basis(ddo);
  op_dens_diff_->scale(2.0);
  op_dens_diff_->scale_diagonal(0.5);
  
  // now try to figure out the matrix specialization we're dealing with
  // if we're using Local matrices, then there's just one subblock, or
  // see if we can convert G and P to local matrices
  if (local_ || local_dens_) {
    double *gmat, *gmato, *pmat, *pmato;
    
    // grab the data pointers from the G and P matrices
    RefSymmSCMatrix gtmp = get_local_data(cl_gmat_, gmat, SCF::Accum);
    RefSymmSCMatrix ptmp = get_local_data(cl_dens_diff_, pmat, SCF::Read);
    RefSymmSCMatrix gotmp = get_local_data(op_gmat_, gmato, SCF::Accum);
    RefSymmSCMatrix potmp = get_local_data(op_dens_diff_, pmato, SCF::Read);

    signed char * pmax = init_pmax(pmat);
  
    LocalHSOSKSContribution lclc(gmat, pmat, gmato, pmato, functional_->a0());
    LocalGBuild<LocalHSOSKSContribution>
      gb(lclc, tbi_, pl, basis(), scf_grp_, pmax, desired_value_accuracy()/100.0);
    gb.run();

    delete[] pmax;

    // if we're running on multiple processors, then sum the G matrices
    if (scf_grp_->n() > 1) {
      scf_grp_->sum(gmat, i_offset(basis()->nbasis()));
      scf_grp_->sum(gmato, i_offset(basis()->nbasis()));
    }
    
    // if we're running on multiple processors, or we don't have local
    // matrices, then accumulate gtmp back into G
    if (!local_ || scf_grp_->n() > 1) {
      cl_gmat_->convert_accumulate(gtmp);
      op_gmat_->convert_accumulate(gotmp);
    }
  }

  // for now quit
  else {
    cerr << node0 << indent << "Cannot yet use anything but Local matrices\n";
    abort();
  }

  RefSymmSCMatrix dens_a = alpha_ao_density();
  RefSymmSCMatrix dens_b = beta_ao_density();
  integrator_->set_compute_potential_integrals(1);
  integrator_->set_accuracy(0.00001*accuracy);
  integrator_->integrate(functional_, dens_a, dens_b);
  exc_ = integrator_->value();
  vxc_a_ = dens_a.clone();
  vxc_a_->assign((double*)integrator_->alpha_vmat());
  vxc_a_ = pl->to_SO_basis(vxc_a_);
  vxc_b_ = dens_b.clone();
  vxc_b_->assign((double*)integrator_->beta_vmat());
  vxc_b_ = pl->to_SO_basis(vxc_b_);
  
  // get rid of AO delta P
  cl_dens_diff_ = dd;
  dd = cl_dens_diff_.clone();

  op_dens_diff_ = ddo;
  ddo = op_dens_diff_.clone();

  // now symmetrize the skeleton G matrix, placing the result in dd
  RefSymmSCMatrix skel_gmat = cl_gmat_.copy();
  skel_gmat.scale(1.0/(double)pl->order());
  pl->symmetrize(skel_gmat,dd);

  skel_gmat = op_gmat_.copy();
  skel_gmat.scale(1.0/(double)pl->order());
  pl->symmetrize(skel_gmat,ddo);
  
  // F = H+G
  cl_fock_.result_noupdate().assign(hcore_);
  cl_fock_.result_noupdate().accumulate(dd);

  // Fo = H+G-Go
  op_fock_.result_noupdate().assign(cl_fock_.result_noupdate());
  ddo.scale(-1.0);
  op_fock_.result_noupdate().accumulate(ddo);
  ddo=0;

  dd.assign(0.0);
  accumddh_->accum(dd);
  cl_fock_.result_noupdate().accumulate(dd);
  op_fock_.result_noupdate().accumulate(dd);
  dd=0;

  cl_fock_.computed()=1;
  op_fock_.computed()=1;
}

/////////////////////////////////////////////////////////////////////////////

void
HSOSKS::two_body_energy(double &ec, double &ex)
{
  tim_enter("hsosks e2");
  ec = 0.0;
  ex = 0.0;
  if (local_ || local_dens_) {
    // grab the data pointers from the G and P matrices
    double *dpmat;
    double *spmat;
    tim_enter("local data");
    RefSymmSCMatrix ddens = beta_ao_density();
    RefSymmSCMatrix sdens = alpha_ao_density() - ddens;
    ddens->scale(2.0);
    ddens->accumulate(sdens);
    ddens->scale(2.0);
    ddens->scale_diagonal(0.5);
    sdens->scale(2.0);
    sdens->scale_diagonal(0.5);
    RefSymmSCMatrix dptmp = get_local_data(ddens, dpmat, SCF::Read);
    RefSymmSCMatrix sptmp = get_local_data(sdens, spmat, SCF::Read);
    tim_exit("local data");

    // initialize the two electron integral classes
    tbi_ = integral()->electron_repulsion();
    tbi_->set_integral_storage(0);

    signed char * pmax = init_pmax(dpmat);
  
    LocalHSOSKSEnergyContribution lclc(dpmat, spmat, functional_->a0());
    RefPetiteList pl = integral()->petite_list();
    LocalGBuild<LocalHSOSKSEnergyContribution>
      gb(lclc, tbi_, pl, basis(), scf_grp_, pmax,
         desired_value_accuracy()/100.0);
    gb.run();

    delete[] pmax;

    tbi_ = 0;

    ec = lclc.ec;
    ex = lclc.ex;
  }
  else {
    cerr << node0 << indent << "Cannot yet use anything but Local matrices\n";
    abort();
  }
  tim_exit("hsoshf e2");
}

/////////////////////////////////////////////////////////////////////////////

void
HSOSKS::two_body_deriv(double * tbgrad)
{
  tim_enter("grad");

  int natom3 = 3*molecule()->natom();

  tim_enter("two-body");
  double *hfgrad = new double[natom3];
  memset(hfgrad,0,sizeof(double)*natom3);
  two_body_deriv_hf(hfgrad,functional_->a0());
  print_natom_3(hfgrad, "Two-body contribution to DFT gradient");
  tim_exit("two-body");

  double *dftgrad = new double[natom3];
  memset(dftgrad,0,sizeof(double)*natom3);
  RefSymmSCMatrix dens_a = alpha_ao_density();
  RefSymmSCMatrix dens_b = beta_ao_density();
  integrator_->init(this);
  integrator_->set_compute_potential_integrals(0);
  integrator_->set_accuracy(0.00001*desired_gradient_accuracy());
  integrator_->integrate(functional_, dens_a, dens_b, dftgrad);
  // must unset the wavefunction so we don't have a circular list that
  // will not be freed with the reference counting memory manager
  integrator_->done();
  print_natom_3(dftgrad, "E-X contribution to DFT gradient");

  for (int i=0; i<natom3; i++) tbgrad[i] += dftgrad[i] + hfgrad[i];
  delete[] dftgrad;
  delete[] hfgrad;

  tim_exit("grad");
}

RefSymmSCMatrix
HSOSKS::cl_vxc()
{
  RefSymmSCMatrix r = vxc_a_+vxc_b_;
  r.scale(0.5);
  return r;
}

RefSymmSCMatrix
HSOSKS::op_vxc()
{
  RefSymmSCMatrix r = vxc_a_.copy();
  return r;
}

/////////////////////////////////////////////////////////////////////////////

void
HSOSKS::init_vector()
{
  integrator_->init(this);
  HSOSSCF::init_vector();
}

void
HSOSKS::done_vector()
{
  integrator_->done();
  HSOSSCF::done_vector();
}

/////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: c++
// c-file-style: "ETS"
// End:
