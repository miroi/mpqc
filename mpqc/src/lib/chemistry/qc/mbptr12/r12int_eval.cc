//
// r12int_eval.cc
//
// Copyright (C) 2004 Edward Valeev
//
// Author: Edward Valeev <edward.valeev@chemistry.gatech.edu>
// Maintainer: EV
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

#ifdef __GNUG__
#pragma implementation
#endif

#include <util/misc/formio.h>
#include <util/class/scexception.h>
#include <util/ref/ref.h>
#include <util/state/state_bin.h>
#include <math/scmat/local.h>
#include <chemistry/qc/wfn/wfn.h>
#include <chemistry/qc/scf/scf.h>
#include <chemistry/qc/mbptr12/vxb_eval_info.h>
#include <chemistry/qc/mbptr12/pairiter.h>
#include <chemistry/qc/mbptr12/r12int_eval.h>
#include <chemistry/qc/mbptr12/transform_factory.h>
#include <chemistry/qc/mbptr12/utils.h>

using namespace std;
using namespace sc;

#define TEST_FOCK 0
#define NOT_INCLUDE_DIAGONAL_VXB_CONTIBUTIONS 0
#define INCLUDE_EBC_CODE 0
#define INCLUDE_GBC_CODE 0

inline int max(int a,int b) { return (a > b) ? a : b;}

/*-----------------
  R12IntEval
 -----------------*/
static ClassDesc R12IntEval_cd(
  typeid(R12IntEval),"R12IntEval",1,"virtual public SavableState",
  0, 0, 0);

R12IntEval::R12IntEval(const Ref<R12IntEvalInfo>& r12i, const Ref<LinearR12::CorrelationFactor>& corrfactor,
                       bool gbc, bool ebc,
                       LinearR12::ABSMethod abs_method,
                       LinearR12::StandardApproximation stdapprox, bool follow_ks_ebcfree) :
  r12info_(r12i), corrfactor_(corrfactor), gbc_(gbc), ebc_(ebc), abs_method_(abs_method),
  stdapprox_(stdapprox), spinadapted_(false), include_mp1_(false), evaluated_(false),
  follow_ks_ebcfree_(follow_ks_ebcfree), debug_(0)
{
  int naocc_a, naocc_b;
  int navir_a, navir_b;
  if (!spin_polarized()) {
    const int nocc_act = r12info_->refinfo()->docc_act()->rank();
    const int nvir_act = r12info_->vir_act()->rank();
    naocc_a = naocc_b = nocc_act;
    navir_a = navir_b = nvir_act;
  }
  else {
    naocc_a = r12info()->refinfo()->occ_act(Alpha)->rank();
    naocc_b = r12info()->refinfo()->occ_act(Beta)->rank();
    navir_a = r12info()->refinfo()->uocc_act(Alpha)->rank();
    navir_b = r12info()->refinfo()->uocc_act(Beta)->rank();
  }

  dim_oo_[AlphaAlpha] = new SCDimension((naocc_a*(naocc_a-1))/2);
  dim_vv_[AlphaAlpha] = new SCDimension((navir_a*(navir_a-1))/2);
  dim_oo_[AlphaBeta] = new SCDimension(naocc_a*naocc_b);
  dim_vv_[AlphaBeta] = new SCDimension(navir_a*navir_b);
  dim_oo_[BetaBeta] = new SCDimension((naocc_b*(naocc_b-1))/2);
  dim_vv_[BetaBeta] = new SCDimension((navir_b*(navir_b-1))/2);
  for(int s=0; s<NSpinCases2; s++) {
    dim_f12_[s] = new SCDimension(corrfactor_->nfunctions()*dim_oo_[s].n());
  }
  
#if 0
  dim_ij_aa_ = new SCDimension((naocc_a*(naocc_a-1))/2);
  dim_ij_ab_ = new SCDimension(naocc_a*naocc_b);
  dim_ab_aa_ = new SCDimension((navir_a*(navir_a-1))/2);
  dim_ab_ab_ = new SCDimension(navir_a*navir_b);
  if (spin_polarized()) {
    dim_ij_bb_ = new SCDimension((naocc_b*(naocc_b-1))/2);
    dim_ab_bb_ = new SCDimension((navir_b*(navir_b-1))/2);
  }
  else {
    dim_ij_s_ = new SCDimension((naocc_a*(naocc_a+1))/2);
    dim_ij_t_ = new SCDimension((naocc_a*(naocc_a-1))/2);
    dim_ij_bb_ = dim_ij_aa_;
    dim_ab_bb_ = dim_ab_aa_;
  }
#else
  if (!spin_polarized()) {
    dim_ij_s_ = new SCDimension((naocc_a*(naocc_a+1))/2);
    dim_ij_t_ = new SCDimension((naocc_a*(naocc_a-1))/2);
  }
#endif

  Ref<LocalSCMatrixKit> local_matrix_kit = new LocalSCMatrixKit();
  // Remove when done merging branches
#if 0
  Vaa_ = local_matrix_kit->matrix(dim_ij_aa_,dim_ij_aa_);
  Vab_ = local_matrix_kit->matrix(dim_ij_ab_,dim_ij_ab_);
  Xaa_ = local_matrix_kit->matrix(dim_ij_aa_,dim_ij_aa_);
  Xab_ = local_matrix_kit->matrix(dim_ij_ab_,dim_ij_ab_);
  Baa_ = local_matrix_kit->matrix(dim_ij_aa_,dim_ij_aa_);
  Bab_ = local_matrix_kit->matrix(dim_ij_ab_,dim_ij_ab_);
  if (spin_polarized()) {
    Vbb_ = local_matrix_kit->matrix(dim_ij_bb_,dim_ij_bb_);
    Xbb_ = local_matrix_kit->matrix(dim_ij_bb_,dim_ij_bb_);
    Bbb_ = local_matrix_kit->matrix(dim_ij_bb_,dim_ij_bb_);
  }
  else {
    Vbb_ = Vaa_;  Xbb_ = Xaa_;  Bbb_ = Baa_;
  }
  if (ebc_ == false) {
    Aaa_ = local_matrix_kit->matrix(dim_ij_aa_,dim_ab_aa_);
    Aab_ = local_matrix_kit->matrix(dim_ij_ab_,dim_ab_ab_);
    T2aa_ = local_matrix_kit->matrix(dim_ij_aa_,dim_ab_aa_);
    T2ab_ = local_matrix_kit->matrix(dim_ij_ab_,dim_ab_ab_);
    Raa_ = local_matrix_kit->matrix(dim_ij_aa_,dim_ab_aa_);
    Rab_ = local_matrix_kit->matrix(dim_ij_ab_,dim_ab_ab_);
    if (spin_polarized()) {
      Abb_ = local_matrix_kit->matrix(dim_ij_bb_,dim_ab_bb_);
      T2bb_ = local_matrix_kit->matrix(dim_ij_bb_,dim_ab_bb_);
    }
    else {
      Abb_ = Aaa_;  T2bb_ = T2aa_;
    }
  }
  emp2pair_aa_ = local_matrix_kit->vector(dim_ij_aa_);
  emp2pair_ab_ = local_matrix_kit->vector(dim_ij_ab_);
  if (spin_polarized())
    emp2pair_bb_ = local_matrix_kit->vector(dim_ij_bb_);
  else
    emp2pair_bb_ = emp2pair_aa_;
#endif
  
  for(int s=0; s<NSpinCases2; s++) {
    if (spin_polarized() || s != BetaBeta) {
      V_[s] = local_matrix_kit->matrix(dim_f12_[s],dim_oo_[s]);
      X_[s] = local_matrix_kit->matrix(dim_f12_[s],dim_f12_[s]);
      B_[s] = local_matrix_kit->matrix(dim_f12_[s],dim_f12_[s]);
      if (ebc == false) {
        A_[s] = local_matrix_kit->matrix(dim_f12_[s],dim_vv_[s]);
        if (follow_ks_ebcfree_) {
          Ac_[s] = local_matrix_kit->matrix(dim_f12_[s],dim_vv_[s]);
        }
        T2_[s] = local_matrix_kit->matrix(dim_oo_[s],dim_vv_[s]);
        F12_[s] = local_matrix_kit->matrix(dim_f12_[s],dim_vv_[s]);
      }
      emp2pair_[s] = local_matrix_kit->vector(dim_oo_[s]);
    }
    else {
      V_[BetaBeta] = V_[AlphaAlpha];
      X_[BetaBeta] = X_[AlphaAlpha];
      B_[BetaBeta] = B_[AlphaAlpha];
      A_[BetaBeta] = A_[AlphaAlpha];
      Ac_[BetaBeta] = Ac_[AlphaAlpha];
      T2_[BetaBeta] = T2_[AlphaAlpha];
      F12_[BetaBeta] = F12_[AlphaAlpha];
      emp2pair_[BetaBeta] = emp2pair_[AlphaAlpha];
    }
  }
  
  init_tforms_();
  // init_intermeds_ may require initialized transforms
  init_intermeds_();

  // compute canonical space of virtuals if VBS != OBS
  //if (r12info()->basis_vir()->equiv(r12info()->basis())) {
    form_canonvir_space_();
  //}
}

R12IntEval::R12IntEval(StateIn& si) : SavableState(si)
{
  int gbc; si.get(gbc); gbc_ = (bool) gbc;
  int ebc; si.get(ebc); ebc_ = (bool) ebc;
  int absmethod; si.get(absmethod); abs_method_ = (LinearR12::ABSMethod) absmethod;
  int stdapprox; si.get(stdapprox); stdapprox_ = (LinearR12::StandardApproximation) stdapprox;
  // WARNING si.get(corrfactor_)
  int follow_ks_ebcfree; si.get(follow_ks_ebcfree); follow_ks_ebcfree_ = static_cast<bool>(follow_ks_ebcfree);

  r12info_ << SavableState::restore_state(si);
#if 0
  dim_ij_aa_ << SavableState::restore_state(si);
  dim_ij_ab_ << SavableState::restore_state(si);
  dim_ij_bb_ << SavableState::restore_state(si);
  dim_ij_s_ << SavableState::restore_state(si);
  dim_ij_t_ << SavableState::restore_state(si);
  dim_ab_aa_ << SavableState::restore_state(si);
  dim_ab_ab_ << SavableState::restore_state(si);
  dim_ab_bb_ << SavableState::restore_state(si);
#endif

  Ref<LocalSCMatrixKit> local_matrix_kit = new LocalSCMatrixKit();
  // Remove when done with the merge
#if 0
  Vaa_ = local_matrix_kit->matrix(dim_ij_aa_,dim_ij_aa_);
  Vab_ = local_matrix_kit->matrix(dim_ij_ab_,dim_ij_ab_);
  Vbb_ = local_matrix_kit->matrix(dim_ij_ab_,dim_ij_ab_);
  Xaa_ = local_matrix_kit->matrix(dim_ij_aa_,dim_ij_aa_);
  Xab_ = local_matrix_kit->matrix(dim_ij_ab_,dim_ij_ab_);
  Xbb_ = local_matrix_kit->matrix(dim_ij_ab_,dim_ij_ab_);
  Baa_ = local_matrix_kit->matrix(dim_ij_aa_,dim_ij_aa_);
  Bab_ = local_matrix_kit->matrix(dim_ij_ab_,dim_ij_ab_);
  Bbb_ = local_matrix_kit->matrix(dim_ij_ab_,dim_ij_ab_);
  if (ebc_ == false) {
    Aaa_ = local_matrix_kit->matrix(dim_ij_aa_,dim_ab_aa_);
    Aab_ = local_matrix_kit->matrix(dim_ij_ab_,dim_ab_ab_);
    Abb_ = local_matrix_kit->matrix(dim_ij_ab_,dim_ab_ab_);
    if (follow_ks_ebcfree_) {
      Ac_aa_ = local_matrix_kit->matrix(dim_ij_aa_,dim_ab_aa_);
      Ac_ab_ = local_matrix_kit->matrix(dim_ij_ab_,dim_ab_ab_);
    }
    T2aa_ = local_matrix_kit->matrix(dim_ij_aa_,dim_ab_aa_);
    T2ab_ = local_matrix_kit->matrix(dim_ij_ab_,dim_ab_ab_);
    T2bb_ = local_matrix_kit->matrix(dim_ij_ab_,dim_ab_ab_);
    Raa_ = local_matrix_kit->matrix(dim_ij_aa_,dim_ab_aa_);
    Rab_ = local_matrix_kit->matrix(dim_ij_ab_,dim_ab_ab_);
  }
  emp2pair_aa_ = local_matrix_kit->vector(dim_ij_aa_);
  emp2pair_ab_ = local_matrix_kit->vector(dim_ij_ab_);
  emp2pair_bb_ = local_matrix_kit->vector(dim_ij_aa_);

  Vaa_.restore(si);
  Vab_.restore(si);
  Vbb_.restore(si);
  Xaa_.restore(si);
  Xab_.restore(si);
  Xbb_.restore(si);
  Baa_.restore(si);
  Bab_.restore(si);
  Bbb_.restore(si);
  if (ebc_ == false) {
    Aaa_.restore(si);
    Aab_.restore(si);
    Abb_.restore(si);
    Ac_aa_.restore(si);
    Ac_ab_.restore(si);
    T2aa_.restore(si);
    T2ab_.restore(si);
    T2bb_.restore(si);
    Raa_.restore(si);
    Rab_.restore(si);
  }
  emp2pair_aa_.restore(si);
  emp2pair_ab_.restore(si);
  emp2pair_bb_.restore(si);
#endif

  for(int s=0; s<NSpinCases2; s++) {
    dim_oo_[s] << SavableState::restore_state(si);
    dim_vv_[s] << SavableState::restore_state(si);
    dim_f12_[s] << SavableState::restore_state(si);
    if (!(spin_polarized() && s == BetaBeta)) {
      V_[s] = local_matrix_kit->matrix(dim_f12_[s],dim_oo_[s]);
      X_[s] = local_matrix_kit->matrix(dim_f12_[s],dim_f12_[s]);
      B_[s] = local_matrix_kit->matrix(dim_f12_[s],dim_f12_[s]);
      if (ebc == false) {
        A_[s] = local_matrix_kit->matrix(dim_f12_[s],dim_vv_[s]);
        if (follow_ks_ebcfree_) {
          Ac_[s] = local_matrix_kit->matrix(dim_f12_[s],dim_vv_[s]);
        }
        T2_[s] = local_matrix_kit->matrix(dim_oo_[s],dim_vv_[s]);
        F12_[s] = local_matrix_kit->matrix(dim_f12_[s],dim_vv_[s]);
      }
      emp2pair_[s] = local_matrix_kit->vector(dim_vv_[s]);
      
      V_[s].restore(si);
      X_[s].restore(si);
      B_[s].restore(si);
      A_[s].restore(si);
      Ac_[s].restore(si);
      T2_[s].restore(si);
      F12_[s].restore(si);
      emp2pair_[s].restore(si);
    }
    else {
      V_[BetaBeta] = V_[AlphaAlpha];
      X_[BetaBeta] = X_[AlphaAlpha];
      B_[BetaBeta] = B_[AlphaAlpha];
      A_[BetaBeta] = A_[AlphaAlpha];
      Ac_[BetaBeta] = Ac_[AlphaAlpha];
      T2_[BetaBeta] = T2_[AlphaAlpha];
      F12_[BetaBeta] = F12_[AlphaAlpha];
      emp2pair_[BetaBeta] = emp2pair_[AlphaAlpha];
    }
  }
  
  int num_tforms;
  si.get(num_tforms);
  for(int t=0; t<num_tforms; t++) {
    std::string tform_name;
    si.get(tform_name);
    Ref<TwoBodyMOIntsTransform> tform;
    tform << SavableState::restore_state(si);
    tform_map_[tform_name] = tform;
  }

  int spinadapted; si.get(spinadapted); spinadapted_ = (bool) spinadapted;
  int evaluated; si.get(evaluated); evaluated_ = (bool) evaluated;
  si.get(debug_);

  init_tforms_();
}

R12IntEval::~R12IntEval()
{
}

void
R12IntEval::save_data_state(StateOut& so)
{
  so.put((int)gbc_);
  so.put((int)ebc_);
  so.put((int)abs_method_);
  so.put((int)stdapprox_);
  so.put((int)follow_ks_ebcfree_);

  SavableState::save_state(r12info_.pointer(),so);
#if 0
  SavableState::save_state(dim_ij_aa_.pointer(),so);
  SavableState::save_state(dim_ij_ab_.pointer(),so);
  SavableState::save_state(dim_ij_bb_.pointer(),so);
  SavableState::save_state(dim_ij_s_.pointer(),so);
  SavableState::save_state(dim_ij_t_.pointer(),so);
  SavableState::save_state(dim_ab_aa_.pointer(),so);
  SavableState::save_state(dim_ab_ab_.pointer(),so);
  SavableState::save_state(dim_ab_bb_.pointer(),so);
#endif

  // Remove when done with the merge
#if 0
  Vaa_.save(so);
  Vab_.save(so);
  Vbb_.save(so);
  Xaa_.save(so);
  Xab_.save(so);
  Xbb_.save(so);
  Baa_.save(so);
  Bab_.save(so);
  Bbb_.save(so);
  if (ebc_ == false) {
    Aaa_.save(so);
    Aab_.save(so);
    Abb_.save(so);
    Ac_aa_.save(so);
    Ac_ab_.save(so);
    T2aa_.save(so);
    T2ab_.save(so);
    T2bb_.save(so);
    Raa_.save(so);
    Rab_.save(so);
  }
  emp2pair_aa_.save(so);
  emp2pair_ab_.save(so);
  emp2pair_bb_.save(so);
#endif
  
  for(int s=0; s<NSpinCases2; s++) {
    SavableState::save_state(dim_oo_[s].pointer(),so);
    SavableState::save_state(dim_vv_[s].pointer(),so);
    SavableState::save_state(dim_f12_[s].pointer(),so);
    if (!(spin_polarized() && s == BetaBeta)) {
      V_[s].save(so);
      X_[s].save(so);
      B_[s].save(so);
      A_[s].save(so);
      Ac_[s].save(so);
      T2_[s].save(so);
      F12_[s].save(so);
      emp2pair_[s].save(so);
    }
  }

  int num_tforms = tform_map_.size();
  so.put(num_tforms);
  TformMap::iterator first_tform = tform_map_.begin();
  TformMap::iterator last_tform = tform_map_.end();
  for(TformMap::iterator t=first_tform; t!=last_tform; t++) {
    so.put((*t).first);
    SavableState::save_state((*t).second.pointer(),so);
  }

  so.put((int)spinadapted_);
  so.put((int)evaluated_);
  so.put(debug_);
}

void
R12IntEval::obsolete()
{
  evaluated_ = false;

  // make all transforms obsolete
  TformMap::iterator first_tform = tform_map_.begin();
  TformMap::iterator last_tform = tform_map_.end();
  for(TformMap::iterator t=first_tform; t!=last_tform; t++) {
    (*t).second->obsolete();
  }

  init_intermeds_();
}

void R12IntEval::include_mp1(bool include_mp1) { include_mp1_ = include_mp1; };
void R12IntEval::set_debug(int debug) { if (debug >= 0) { debug_ = debug; r12info_->set_debug_level(debug_); }};
void R12IntEval::set_dynamic(bool dynamic) { r12info_->set_dynamic(dynamic); };
void R12IntEval::set_print_percent(double pp) { r12info_->set_print_percent(pp); };
void R12IntEval::set_memory(size_t nbytes) { r12info_->set_memory(nbytes); };

const Ref<R12IntEvalInfo>& R12IntEval::r12info() const { return r12info_; };
#if 0
RefSCDimension R12IntEval::dim_oo_aa() const { return dim_ij_aa_; };
RefSCDimension R12IntEval::dim_oo_ab() const { return dim_ij_ab_; };
RefSCDimension R12IntEval::dim_oo_bb() const { return dim_ij_bb_; };
#endif
RefSCDimension R12IntEval::dim_oo_s() const { return dim_ij_s_; };
RefSCDimension R12IntEval::dim_oo_t() const { return dim_ij_t_; };
#if 0
RefSCDimension R12IntEval::dim_vv_aa() const { return dim_ab_aa_; };
RefSCDimension R12IntEval::dim_vv_ab() const { return dim_ab_ab_; };
RefSCDimension R12IntEval::dim_vv_bb() const { return dim_ab_bb_; };
#endif
RefSCDimension R12IntEval::dim_oo(SpinCase2 S) const { return dim_oo_[S]; }
RefSCDimension R12IntEval::dim_vv(SpinCase2 S) const { return dim_vv_[S]; }
RefSCDimension R12IntEval::dim_f12(SpinCase2 S) const { return dim_f12_[S]; }

// Remove when done with the merge
#if 0
RefSCMatrix R12IntEval::V_aa()
{
  compute();
  return Vaa_;
}

RefSCMatrix R12IntEval::X_aa()
{
  compute();
  return Xaa_;
}

RefSymmSCMatrix R12IntEval::B_aa()
{
  compute();

  // Extract lower triangle of the matrix
  Ref<SCMatrixKit> kit = Baa_.kit();
  RefSymmSCMatrix Baa = kit->symmmatrix(Baa_.rowdim());
  int naa = Baa_.nrow();
  double* baa = new double[naa*naa];
  Baa_.convert(baa);
  const double* baa_ptr = baa;
  for(int i=0; i<naa; i++, baa_ptr += i)
    for(int j=i; j<naa; j++, baa_ptr++)
      Baa.set_element(i,j,*baa_ptr);
  delete[] baa;

  return Baa;
}

RefSCMatrix R12IntEval::A_aa()
{
  if (ebc_ == false)
    compute();
  return Aaa_;
}

RefSCMatrix R12IntEval::Ac_aa()
{
  if (ebc_ == false && follow_ks_ebcfree_) {
    compute();
    return Ac_aa_;
  }
  else
    throw ProgrammingError("R12IntEval::Ac_aa() called although the object initialized with follow_ks_ebcfree set to false",__FILE__,__LINE__);
}

RefSCMatrix R12IntEval::T2_aa()
{
  if (ebc_ == false)
    compute();
  return T2aa_;
}

RefSCMatrix R12IntEval::V_ab()
{
  compute();
  return Vab_;
}

RefSCMatrix R12IntEval::X_ab()
{
  compute();
  return Xab_;
}

RefSymmSCMatrix R12IntEval::B_ab()
{
  compute();

  // Extract lower triangle of the matrix
  Ref<SCMatrixKit> kit = Bab_.kit();
  RefSymmSCMatrix Bab = kit->symmmatrix(Bab_.rowdim());
  int nab = Bab_.nrow();
  double* bab = new double[nab*nab];
  Bab_.convert(bab);
  const double* bab_ptr = bab;
  for(int i=0; i<nab; i++, bab_ptr += i)
    for(int j=i; j<nab; j++, bab_ptr++)
      Bab.set_element(i,j,*bab_ptr);
  delete[] bab;

  return Bab;
}

RefSCMatrix R12IntEval::A_ab()
{
  if (ebc_ == false)
    compute();
  return Aab_;
}

RefSCMatrix R12IntEval::Ac_ab()
{
  if (ebc_ == false && follow_ks_ebcfree_) {
    compute();
    return Ac_ab_;
  }
  else
    throw ProgrammingError("R12IntEval::Ac_ab() called although the object initialized with follow_ks_ebcfree set to false",__FILE__,__LINE__);
}

RefSCMatrix R12IntEval::T2_ab()
{
  if (ebc_ == false)
    compute();
  return T2ab_;
}

RefSCMatrix R12IntEval::V_bb()
{
  compute();
  return Vbb_;
}

RefSCMatrix R12IntEval::X_bb()
{
  compute();
  return Xbb_;
}

RefSymmSCMatrix R12IntEval::B_bb()
{
  compute();

  // Extract lower triangle of the matrix
  Ref<SCMatrixKit> kit = Bbb_.kit();
  RefSymmSCMatrix Bbb = kit->symmmatrix(Bbb_.rowdim());
  int nbb = Bbb_.nrow();
  double* bbb = new double[nbb*nbb];
  Bbb_.convert(bbb);
  const double* bbb_ptr = bbb;
  for(int i=0; i<nbb; i++, bbb_ptr += i)
    for(int j=i; j<nbb; j++, bbb_ptr++)
      Bbb.set_element(i,j,*bbb_ptr);
  delete[] bbb;

  return Bbb;
}

RefSCMatrix R12IntEval::A_bb()
{
  if (ebc_ == false)
    compute();
  return Abb_;
}

RefSCMatrix R12IntEval::T2_bb()
{
  if (ebc_ == false)
    compute();
  return T2bb_;
}

RefSCVector R12IntEval::emp2_aa()
{
  compute();
  return emp2pair_aa_;
}

RefSCVector R12IntEval::emp2_ab()
{
  compute();
  return emp2pair_ab_;
}

RefSCVector R12IntEval::emp2_bb()
{
  compute();
  return emp2pair_bb_;
}
#endif

const RefSCMatrix&
R12IntEval::V(SpinCase2 S) {
  compute();
  if (!spin_polarized() && (S == AlphaAlpha || S == BetaBeta))
    antisymmetrize(V_[AlphaAlpha],V_[AlphaBeta],
                   occ_act(Alpha),
                   occ_act(Alpha));
  return V_[S];
}

const RefSCMatrix&
R12IntEval::X(SpinCase2 S) {
  compute();
  if (!spin_polarized() && (S == AlphaAlpha || S == BetaBeta))
    antisymmetrize(X_[AlphaAlpha],X_[AlphaBeta],
                   occ_act(Alpha),
                   occ_act(Alpha));
  return X_[S];
}

RefSymmSCMatrix
R12IntEval::B(SpinCase2 S) {
  compute();
  if (!spin_polarized() && (S == AlphaAlpha || S == BetaBeta))
    antisymmetrize(B_[AlphaAlpha],B_[AlphaBeta],
                   occ_act(Alpha),
                   occ_act(Alpha));
  
  // Extract lower triangle of the matrix
  Ref<SCMatrixKit> kit = B_[S].kit();
  RefSymmSCMatrix B = kit->symmmatrix(B_[S].rowdim());
  int n = B_[S].nrow();
  double* b = new double[n*n];
  B_[S].convert(b);
  const double* b_ptr = b;
  for(int i=0; i<n; i++, b_ptr += i)
    for(int j=i; j<n; j++, b_ptr++)
      B.set_element(i,j,*b_ptr);
  delete[] b;

  return B;
}

const RefSCMatrix&
R12IntEval::A(SpinCase2 S) {
  compute();
  if (!spin_polarized() && (S == AlphaAlpha || S == BetaBeta))
    antisymmetrize(A_[AlphaAlpha],A_[AlphaBeta],
                   occ_act(Alpha),
                   vir_act(Alpha));
  return A_[S];
}

const RefSCMatrix&
R12IntEval::Ac(SpinCase2 S) {
  compute();
  if (!spin_polarized() && (S == AlphaAlpha || S == BetaBeta))
    antisymmetrize(Ac_[AlphaAlpha],Ac_[AlphaBeta],
                   occ_act(Alpha),
                   vir_act(Alpha));
  return Ac_[S];
}

const RefSCMatrix&
R12IntEval::T2(SpinCase2 S) {
  compute();
  if (!spin_polarized() && (S == AlphaAlpha || S == BetaBeta))
    antisymmetrize(T2_[AlphaAlpha],T2_[AlphaBeta],
                   occ_act(Alpha),
                   vir_act(Alpha));
  return T2_[S];
}

const RefSCMatrix&
R12IntEval::F12(SpinCase2 S) {
  compute();
  if (!spin_polarized() && (S == AlphaAlpha || S == BetaBeta))
    antisymmetrize(F12_[AlphaAlpha],F12_[AlphaBeta],
                   occ_act(Alpha),
                   vir_act(Alpha));
  return F12_[S];
}

const RefSCVector&
R12IntEval::emp2(SpinCase2 S)
{
  compute();
  if (!spin_polarized() && S == BetaBeta)
    return emp2pair_[AlphaAlpha];
  else
    return emp2pair_[S];
}

const RefDiagSCMatrix&
R12IntEval::evals(SpinCase1 S) const {
  if (spin_polarized()) {
    if (S == Alpha)
      return r12info()->refinfo()->orbs(Alpha)->evals();
    else
      return r12info()->refinfo()->orbs(Beta)->evals();
  }
  else
    return r12info_->refinfo()->orbs()->evals();
}

RefDiagSCMatrix R12IntEval::evals() const {
  if (spin_polarized())
    throw ProgrammingError("R12IntEval::evals() called but reference determinant spin-polarized",
                           __FILE__,__LINE__);

  return r12info_->refinfo()->orbs()->evals();
};

RefDiagSCMatrix R12IntEval::evals_a() const {
  return r12info()->refinfo()->orbs(Alpha)->evals();
}

RefDiagSCMatrix R12IntEval::evals_b() const {
  return r12info()->refinfo()->orbs(Beta)->evals();
}

void
R12IntEval::checkpoint_() const
{
  int me = r12info_->msg()->me();
  Wavefunction* wfn = r12info_->wfn();

  if (me == 0 && wfn->if_to_checkpoint()) {
    StateOutBin stateout(wfn->checkpoint_file());
    SavableState::save_state(wfn,stateout);
    ExEnv::out0() << indent << "Checkpointed the wave function" << endl;
  }
}

void
R12IntEval::init_tforms_()
{
  Ref<MOIntsTransformFactory> tfactory = r12info_->tfactory();

  if (!r12info()->refinfo()->ref()->spin_polarized()) {
    Ref<MOIndexSpace> occ_space = r12info()->refinfo()->docc();
    Ref<MOIndexSpace> act_occ_space = r12info()->refinfo()->docc_act();
    Ref<MOIndexSpace> act_vir_space = r12info()->vir_act();
    Ref<MOIndexSpace> obs_space = r12info()->refinfo()->orbs();
    Ref<MOIndexSpace> ribs_space = r12info()->ribs_space();
    
    const std::string ipjq_name = "(ip|jq)";
    Ref<TwoBodyMOIntsTransform> ipjq_tform = tform_map_[ipjq_name];
    if (ipjq_tform.null()) {
      tfactory->set_spaces(act_occ_space,obs_space,
                           act_occ_space,obs_space);
      ipjq_tform = tfactory->twobody_transform_13(ipjq_name);
      tform_map_[ipjq_name] = ipjq_tform;
      }
    
    const std::string iajb_name = "(ia|jb)";
    Ref<TwoBodyMOIntsTransform> iajb_tform = tform_map_[iajb_name];
    if (iajb_tform.null()) {
      tfactory->set_spaces(act_occ_space,act_vir_space,
                           act_occ_space,act_vir_space);
      iajb_tform = tfactory->twobody_transform_13(iajb_name);
      tform_map_[iajb_name] = iajb_tform;
      }
    
    const std::string imja_name = "(im|ja)";
    Ref<TwoBodyMOIntsTransform> imja_tform = tform_map_[imja_name];
    if (imja_tform.null()) {
      tfactory->set_spaces(act_occ_space,occ_space,
                           act_occ_space,act_vir_space);
      imja_tform = tfactory->twobody_transform_13(imja_name);
      tform_map_[imja_name] = imja_tform;
      }
    
    const std::string imjn_name = "(im|jn)";
    Ref<TwoBodyMOIntsTransform> imjn_tform = tform_map_[imjn_name];
    if (imjn_tform.null()) {
      tfactory->set_spaces(act_occ_space,occ_space,
                           act_occ_space,occ_space);
      imjn_tform = tfactory->twobody_transform_13(imjn_name);
      tform_map_[imjn_name] = imjn_tform;
      }
    
    const std::string imjy_name = "(im|jy)";
    Ref<TwoBodyMOIntsTransform> imjy_tform = tform_map_[imjy_name];
    if (imjy_tform.null()) {
      tfactory->set_spaces(act_occ_space,occ_space,
                           act_occ_space,ribs_space);
      imjy_tform = tfactory->twobody_transform_13(imjy_name);
      tform_map_[imjy_name] = imjy_tform;
      }

    iajb_tform = tform_map_[iajb_name];
    imjn_tform = tform_map_[imjn_name];
    ipjq_tform = tform_map_[ipjq_name];
  }
  else {
    // Don't add any transforms yet if UHF
  }
}

Ref<TwoBodyMOIntsTransform>
R12IntEval::get_tform_(const std::string& tform_name)
{
  TformMap::const_iterator tform_iter = tform_map_.find(tform_name);
  if (tform_iter == tform_map_.end()) {
    std::string errmsg = "R12IntEval::get_tform_() -- transform " + tform_name + " is not known";
    throw TransformNotFound(errmsg.c_str(),__FILE__,__LINE__);
  }
  // Do not compute here since compute() call may take params now
  //tform->compute(tbint_params);

  TwoBodyMOIntsTransform* tform = (*tform_iter).second.pointer();
  return tform;
}

void
R12IntEval::init_intermeds_()
{
  // Remove when done with the merge
#if 0
  Vaa_.assign(0.0);
  Vab_.assign(0.0);
  Baa_.assign(0.0);
  Bab_.assign(0.0);
  Xaa_.assign(0.0);
  Xab_.assign(0.0);
  if (r12info_->msg()->me() == 0) {
#if NOT_INCLUDE_DIAGONAL_VXB_CONTIBUTIONS
    Vaa_->assign(0.0);
    Vab_->assign(0.0);
    Baa_->assign(0.0);
    Bab_->assign(0.0);
#else
    Vaa_->unit();
    Vab_->unit();
    Baa_->unit();
    Bab_->unit();
#endif
  }
  else {
    Vaa_.assign(0.0);
    Vab_.assign(0.0);
    Baa_.assign(0.0);
    Bab_.assign(0.0);
  }
  if (ebc_ == false) {
    Aaa_.assign(0.0);
    Aab_.assign(0.0);
    if (follow_ks_ebcfree_) {
      Ac_aa_.assign(0.0);
      Ac_ab_.assign(0.0);
    }
    T2aa_.assign(0.0);
    T2ab_.assign(0.0);
    Raa_.assign(0.0);
    Rab_.assign(0.0);
  }
#endif
  
  for(int s=0; s<NSpinCases2; s++) {
    V_[s].assign(0.0);
    X_[s].assign(0.0);
    B_[s].assign(0.0);
    emp2pair_[s].assign(0.0);
    if (ebc_ == false) {
      A_[s].assign(0.0);
      if (follow_ks_ebcfree_) {
        Ac_[s].assign(0.0);
      }
      T2_[s].assign(0.0);
      F12_[s].assign(0.0);
    }
  }
  
  // nothing to do if no explicit correlation
  Ref<LinearR12::NullCorrelationFactor> no12ptr; no12ptr << corrfactor_;
  if (no12ptr.nonnull())
    return;
  
  Ref<LinearR12::G12CorrelationFactor> g12ptr; g12ptr << corrfactor_;
  Ref<LinearR12::R12CorrelationFactor> r12ptr; r12ptr << corrfactor_;
  if (r12ptr.nonnull()) {
    init_intermeds_r12_();
  }
  else if (g12ptr.nonnull()) {
    for(int s=0; s<nspincases2(); s++)
      init_intermeds_g12_(static_cast<SpinCase2>(s));
  }
  
  // Remove when done with the merge
#if 0
  Xaa_.assign(0.0);
  Xab_.assign(0.0);
  //r2_contrib_to_X_orig_();
#endif

  // Remove when done with the merge
#if 0
  emp2pair_aa_.assign(0.0);
  emp2pair_ab_.assign(0.0);
#endif
}

void
R12IntEval::init_intermeds_r12_()
{
  if (r12info_->msg()->me() == 0) {
    // Remove when done with the merge
#if 0
    Vaa_->unit();
    Vab_->unit();
    Baa_->unit();
    Bab_->unit();
#endif
    
    for(int s=0; s<nspincases2(); s++) {
#if NOT_INCLUDE_DIAGONAL_VXB_CONTIBUTIONS
      V_[s]->assign(0.0);
      B_[s]->assign(0.0);
#else
      V_[s]->unit();
      B_[s]->unit();
#endif
    }
  }
#if !NOT_INCLUDE_DIAGONAL_VXB_CONTIBUTIONS
  r2_contrib_to_X_new_();
#endif
}

/// Compute <space1 space2|r_{12}^2|space3 space4>
RefSCMatrix
R12IntEval::compute_r2_(const Ref<MOIndexSpace>& space1,
                        const Ref<MOIndexSpace>& space2,
                        const Ref<MOIndexSpace>& space3,
                        const Ref<MOIndexSpace>& space4)
{
  /*-----------------------------------------------------
    Compute overlap, dipole, quadrupole moment integrals
   -----------------------------------------------------*/
  RefSCMatrix S_13, MX_13, MY_13, MZ_13, MXX_13, MYY_13, MZZ_13;
  r12info_->compute_multipole_ints(space1, space3, MX_13, MY_13, MZ_13, MXX_13, MYY_13, MZZ_13);
  r12info_->compute_overlap_ints(space1, space3, S_13);

  RefSCMatrix S_24, MX_24, MY_24, MZ_24, MXX_24, MYY_24, MZZ_24;
  if (space1 == space2 && space3 == space4) {
    S_24 = S_13;
    MX_24 = MX_13;
    MY_24 = MY_13;
    MZ_24 = MZ_13;
    MXX_24 = MXX_13;
    MYY_24 = MYY_13;
    MZZ_24 = MZZ_13;
  }
  else {
    r12info_->compute_multipole_ints(space2, space4, MX_24, MY_24, MZ_24, MXX_24, MYY_24, MZZ_24);
    r12info_->compute_overlap_ints(space2, space4, S_24);
  }
  if (debug_)
    ExEnv::out0() << indent << "Computed overlap and multipole moment integrals" << endl;

  const int nproc = r12info_->msg()->n();
  const int me = r12info_->msg()->me();

  const int n1 = space1->rank();
  const int n2 = space2->rank();
  const int n3 = space3->rank();
  const int n4 = space4->rank();
  const int n12 = n1*n2;
  const int n34 = n3*n4;
  const int n1234 = n12*n34;
  double* r2_array = new double[n1234];
  memset(r2_array,0,n1234*sizeof(double));

  int ij = 0;
  double* ijkl_ptr = r2_array;
  for(int i=0; i<n1; i++)
    for(int j=0; j<n2; j++, ij++) {

    int ij_proc = ij%nproc;
    if (ij_proc != me) {
      ijkl_ptr += n34;
      continue;
    }

    int kl=0;
    for(int k=0; k<n3; k++)
      for(int l=0; l<n4; l++, kl++, ijkl_ptr++) {

        double r2_ik = -1.0*(MXX_13->get_element(i,k) + MYY_13->get_element(i,k) + MZZ_13->get_element(i,k));
        double r2_jl = -1.0*(MXX_24->get_element(j,l) + MYY_24->get_element(j,l) + MZZ_24->get_element(j,l));
        double r11_ijkl = MX_13->get_element(i,k)*MX_24->get_element(j,l) +
          MY_13->get_element(i,k)*MY_24->get_element(j,l) +
          MZ_13->get_element(i,k)*MZ_24->get_element(j,l);
        double S_ik = S_13.get_element(i,k);
        double S_jl = S_24.get_element(j,l);
        
        double R2_ijkl = r2_ik * S_jl + r2_jl * S_ik - 2.0*r11_ijkl;
        *ijkl_ptr = R2_ijkl;
      }
    }

  r12info_->msg()->sum(r2_array,n1234);

  RefSCDimension dim_ij = new SCDimension(n12);
  RefSCDimension dim_kl = new SCDimension(n34);

  Ref<LocalSCMatrixKit> local_matrix_kit = new LocalSCMatrixKit();
  RefSCMatrix R2 = local_matrix_kit->matrix(dim_ij, dim_kl);
  R2.assign(r2_array);
  delete[] r2_array;

  return R2;
}

void
R12IntEval::r2_contrib_to_X_new_()
{
  unsigned int me = r12info_->msg()->me();

  for(int s=0; s<nspincases2(); s++) {

    const SpinCase2 spincase2 = static_cast<SpinCase2>(s);
    const Ref<MOIndexSpace>& space1 = r12info_->refinfo()->occ_act(case1(spincase2));
    const Ref<MOIndexSpace>& space2 = r12info_->refinfo()->occ_act(case2(spincase2));
    
    // compute r_{12}^2 operator in act.occ.pair/act.occ.pair basis
    RefSCMatrix R2 = compute_r2_(space1,space2,space1,space2);
    if (me != 0)
      continue;
    if (spincase2 == AlphaBeta) {
      X_[s].accumulate(R2);
    }
    else {
      // space1 == space2 because it's AlphaAlpha or BetaBeta
      antisymmetrize(X_[s],R2,space1,space1);
    }
  }
}


void
R12IntEval::form_focc_space_()
{
  // compute the Fock matrix between the complement and all occupieds and
  // create the new Fock-weighted space
  if (focc_space_.null()) {
    const Ref<MOIndexSpace>& occ_space = r12info_->refinfo()->docc();
    const Ref<MOIndexSpace>& ribs_space = r12info_->ribs_space();
    
    RefSCMatrix F_ri_o = fock_(occ_space,ribs_space,occ_space);
    if (debug_ > 1)
      F_ri_o.print("Fock matrix (RI-BS/occ.)");
    focc_space_ = new MOIndexSpace("m_F", "Fock-weighted occupied MOs sorted by energy",
                                   occ_space, ribs_space->coefs()*F_ri_o, ribs_space->basis());
  }
}

void
R12IntEval::form_factocc_space_()
{
  // compute the Fock matrix between the complement and active occupieds and
  // create the new Fock-weighted space
  if (factocc_space_.null()) {
    const Ref<MOIndexSpace>& occ_space = r12info_->refinfo()->docc();
    const Ref<MOIndexSpace>& act_occ_space = r12info_->refinfo()->docc_act();
    const Ref<MOIndexSpace>& ribs_space = r12info_->ribs_space();
    
    RefSCMatrix F_ri_ao = fock_(occ_space,ribs_space,act_occ_space);
    if (debug_ > 1)
      F_ri_ao.print("Fock matrix (RI-BS/act.occ.)");
    factocc_space_ = new MOIndexSpace("i_F", "Fock-weighted active occupied MOs sorted by energy",
                                      act_occ_space, ribs_space->coefs()*F_ri_ao, ribs_space->basis());
  }
}

void
R12IntEval::form_canonvir_space_()
{
  // Create a complement space to all occupieds
  // Fock operator is diagonal in this space
  if (r12info_->basis_vir()->equiv(r12info_->basis())) {
    return;
  }

  int nspincases = 1;
  if (r12info()->refinfo()->ref()->spin_polarized())
    nspincases = 2;

  for(int s=0; s<nspincases; s++) {
    const SpinCase1 spincase = static_cast<SpinCase1>(s);
    
    const Ref<MOIndexSpace>& mo_space = r12info_->refinfo()->orbs(spincase);
    const Ref<MOIndexSpace>& occ_space = r12info_->refinfo()->occ(spincase);
    const Ref<MOIndexSpace>& vir_space = r12info_->vir_sb(spincase);
    RefSCMatrix F_vir = fock_(occ_space,vir_space,vir_space);
    
    int nrow = vir_space->rank();
    double* F_full = new double[nrow*nrow];
    double* F_lowtri = new double [nrow*(nrow+1)/2];
    F_vir->convert(F_full);
    int ij = 0;
    for(int row=0; row<nrow; row++) {
      int rc = row*nrow;
      for(int col=0; col<=row; col++, rc++, ij++) {
        F_lowtri[ij] = F_full[rc];
        }
      }
    RefSymmSCMatrix F_vir_lt(F_vir.rowdim(),F_vir->kit());
    F_vir_lt->assign(F_lowtri);
    F_vir = 0;
    delete[] F_full;
    delete[] F_lowtri;
    
    std::string id_sb, id;
    if (r12info()->refinfo()->ref()->spin_polarized() && spincase == Alpha) {
      id_sb = "E(sym)";
      id = "E";
    }
    else {
      id_sb = "e(sym)";
      id = "e";
    }
    Ref<MOIndexSpace> canonvir_space_symblk = new MOIndexSpace(id_sb, "VBS",
                                                               vir_space, vir_space->coefs()*F_vir_lt.eigvecs(),
                                                               vir_space->basis());
    r12info()->vir_sb(spincase, canonvir_space_symblk);
    
    RefDiagSCMatrix F_vir_evals = F_vir_lt.eigvals();
    Ref<MOIndexSpace> vir_act = new MOIndexSpace(id, "VBS",
                                                 canonvir_space_symblk->coefs(), canonvir_space_symblk->basis(),
                                                 r12info()->integral(),
                                                 F_vir_evals, 0, r12info()->refinfo()->nfzv(),
                                                 MOIndexSpace::energy);
    r12info()->vir_act(spincase, vir_act);
    Ref<MOIndexSpace> vir = new MOIndexSpace(id, "VBS",
                                             canonvir_space_symblk->coefs(), canonvir_space_symblk->basis(),
                                             r12info()->integral(),
                                             F_vir_evals, 0, 0,
                                             MOIndexSpace::energy);
    r12info()->vir(spincase, vir);
  }
}

const int
R12IntEval::tasks_with_ints_(const Ref<R12IntsAcc> ints_acc, vector<int>& map_to_twi)
{
  int nproc = r12info_->msg()->n();
  
  // Compute the number of tasks that have full access to the integrals
  // and split the work among them
  int nproc_with_ints = 0;
  for(int proc=0;proc<nproc;proc++)
    if (ints_acc->has_access(proc)) nproc_with_ints++;
 
  map_to_twi.resize(nproc);
  int count = 0;
  for(int proc=0;proc<nproc;proc++)
    if (ints_acc->has_access(proc)) {
      map_to_twi[proc] = count;
      count++;
    }
      else
        map_to_twi[proc] = -1;

  ExEnv::out0() << indent << "Computing intermediates on " << nproc_with_ints
    << " processors" << endl;
    
  return nproc_with_ints;
}


void
R12IntEval::compute()
{
  if (evaluated_)
    return;

  // different expressions hence codepaths depending on relationship between OBS, VBS, and RIBS
  // compare these basis sets here
  const bool obs_eq_vbs = r12info_->basis_vir()->equiv(r12info_->basis());
  const bool obs_eq_ribs = r12info_->basis_ri()->equiv(r12info_->basis());
  
  Ref<LinearR12::NullCorrelationFactor> nocorrptr; nocorrptr << corrfactor_;
  // if no explicit correlation -- only compute MP2 energies
  if (nocorrptr.nonnull()) {
    for(int s=0; s<nspincases2(); s++) {
      const SpinCase2 spincase2 = static_cast<SpinCase2>(s);
      compute_mp2_pair_energies_(spincase2);
    }
    // Distribute the final intermediates to every node
    globally_sum_intermeds_(true);
    evaluated_ = true;
    return;
  }
  
  if (debug_ > 1) {
    for(int s=0; s<nspincases2(); s++) {
      V_[s].print(prepend_spincase(static_cast<SpinCase2>(s),"V(diag) contribution").c_str());
      X_[s].print(prepend_spincase(static_cast<SpinCase2>(s),"X(diag) contribution").c_str());
      B_[s].print(prepend_spincase(static_cast<SpinCase2>(s),"B(diag) contribution").c_str());
    }
  }
  
  if (obs_eq_vbs) {
    // Compute VXB using new code
    using LinearR12::TwoParticleContraction;
    using LinearR12::ABS_OBS_Contraction;
    using LinearR12::CABS_OBS_Contraction;
    const LinearR12::ABSMethod absmethod = r12info()->abs_method();
    for(int s=0; s<nspincases2(); s++) {
      const SpinCase2 spincase2 = static_cast<SpinCase2>(s);
      const SpinCase1 spin1 = case1(spincase2);
      const SpinCase1 spin2 = case2(spincase2);

      Ref<TwoParticleContraction> tpcontract;
      if ((absmethod == LinearR12::ABS_ABS ||
	   absmethod == LinearR12::ABS_ABSPlus) && !obs_eq_ribs)
        tpcontract = new ABS_OBS_Contraction(r12info()->refinfo()->orbs(spin1)->rank(),
                                             r12info()->refinfo()->occ(spin1)->rank(),
                                             r12info()->refinfo()->occ(spin2)->rank());
      else
        tpcontract = new CABS_OBS_Contraction(r12info()->refinfo()->orbs(spin1)->rank());
      contrib_to_VXB_a_new_(r12info()->refinfo()->occ_act(spin1),
                            r12info()->refinfo()->orbs(spin1),
                            r12info()->refinfo()->occ_act(spin2),
                            r12info()->refinfo()->orbs(spin2),
                            spincase2,tpcontract);
      compute_mp2_pair_energies_(spincase2);
      if (debug_ > 1) {
        V_[s].print(prepend_spincase(static_cast<SpinCase2>(s),"V(diag+OBS) contribution").c_str());
        X_[s].print(prepend_spincase(static_cast<SpinCase2>(s),"X(diag+OBS) contribution").c_str());
        B_[s].print(prepend_spincase(static_cast<SpinCase2>(s),"B(diag+OBS) contribution").c_str());
      }
    }
    
    if (!obs_eq_ribs) {
      // Compute VXB using new code
      using LinearR12::Direct_Contraction;
      for(int s=0; s<nspincases2(); s++) {
        const SpinCase2 spincase2 = static_cast<SpinCase2>(s);
        const SpinCase1 spin1 = case1(spincase2);
        const SpinCase1 spin2 = case2(spincase2);
        Ref<TwoParticleContraction> tpcontract = new Direct_Contraction(r12info()->refinfo()->occ(spin1)->rank(),
                                                                        r12info()->ribs_space(spin2)->rank(),-1.0);
        contrib_to_VXB_a_new_(r12info()->refinfo()->occ_act(spin1),
                              r12info()->refinfo()->occ(spin1),
                              r12info()->refinfo()->occ_act(spin2),
                              r12info()->ribs_space(spin2),
                              spincase2,tpcontract);

        if (spincase2 == AlphaBeta && r12info()->refinfo()->occ_act(spin1) != r12info()->refinfo()->occ_act(spin2)) {
          Ref<TwoParticleContraction> tpcontract = new Direct_Contraction(r12info()->ribs_space(spin1)->rank(),
                                                                          r12info()->refinfo()->occ(spin2)->rank(),-1.0);
          contrib_to_VXB_a_new_(r12info()->refinfo()->occ_act(spin1),
                                r12info()->ribs_space(spin1),
                                r12info()->refinfo()->occ_act(spin2),
                                r12info()->refinfo()->occ(spin2),
                                spincase2,tpcontract);
        }
        
        if (debug_ > 1) {
          V_[s].print(prepend_spincase(static_cast<SpinCase2>(s),"V(diag+OBS+ABS) contribution").c_str());
          X_[s].print(prepend_spincase(static_cast<SpinCase2>(s),"X(diag+OBS+ABS) contribution").c_str());
          B_[s].print(prepend_spincase(static_cast<SpinCase2>(s),"B(diag+OBS+ABS) contribution").c_str());
        }
      }
    }

  }
  else {
    contrib_to_VXB_gebc_vbsneqobs_();
  }

#if TEST_FOCK
  if (!evaluated_) {
    RefSCMatrix F = fock_(r12info_->occ(),r12info_->obs_space(),r12info_->obs_space());
    F.print("Fock matrix in OBS");
    r12info_->obs_space()->evals().print("OBS eigenvalues");

    r12info_->ribs_space()->coefs().print("Orthonormal RI-BS");
    RefSCMatrix S_ri;
    r12info_->compute_overlap_ints(r12info_->ribs_space(),r12info_->ribs_space(),S_ri);
    S_ri.print("Overlap in RI-BS");
    RefSCMatrix F_ri = fock_(r12info_->occ(),r12info_->ribs_space(),r12info_->ribs_space());
    F_ri.print("Fock matrix in RI-BS");
    RefSymmSCMatrix F_ri_symm = F_ri.kit()->symmmatrix(F_ri.rowdim());
    int nrow = F_ri.rowdim().n();
    for(int r=0; r<nrow; r++)
      for(int c=0; c<nrow; c++)
        F_ri_symm.set_element(r,c,F_ri.get_element(r,c));
    F_ri_symm.eigvals().print("Eigenvalues of the Fock matrix (RI-BS)");

    RefSCMatrix F_obs_ri = fock_(r12info_->occ(),r12info_->obs_space(),r12info_->ribs_space());
    F_obs_ri.print("Fock matrix in OBS/RI-BS");
  }
#endif

#if INCLUDE_EBC_CODE
  if (!ebc_) {
    // These functions assume that virtuals are expanded in the same basis
    // as the occupied orbitals
    if (!obs_eq_vbs)
      throw std::runtime_error("R12IntEval::compute() -- ebc=false is only supported when basis_vir == basis");

    compute_A_simple_();
    for(int s=0; s<nspincases2(); s++)
      A_[s].scale(2.0);
    
    // Remove when done with the merge
#if 0
    Aaa_.scale(2.0);
    Aab_.scale(2.0);
#endif
    if (follow_ks_ebcfree_) {
      // Does this even apply on libint2-branch?
      compute_A_via_commutator_();
      //for(int s=0; s<nspincases2(); s++)
      //  Ac_[s].scale(2.0);
      // Remove when done with the merge
#if 0
      Ac_aa_.scale(2.0);
      Ac_ab_.scale(2.0);
#endif
    }
    
    for(int s=0; s<nspincases2(); s++) {
      const SpinCase2 spincase2 = static_cast<SpinCase2>(s);
      const SpinCase1 spin1 = case1(spincase2);
      const SpinCase1 spin2 = case2(spincase2);
      Ref<MOIndexSpace> occ1_act = r12info()->refinfo()->occ_act(spin1);
      Ref<MOIndexSpace> occ2_act = r12info()->refinfo()->occ_act(spin2);
      Ref<MOIndexSpace> uocc1_act = r12info()->refinfo()->uocc_act(spin1);
      Ref<MOIndexSpace> uocc2_act = r12info()->refinfo()->uocc_act(spin2);
      compute_T2_(T2_[AlphaBeta],occ1_act, uocc1_act, occ2_act, uocc2_act);
      compute_F12_(F12_[AlphaBeta],occ1_act, uocc1_act, occ2_act, uocc2_act);
    }
    AT2_contrib_to_V_();
#if 0
    // Became compute_F12_ -- see above
    compute_R_();
#endif
    AR_contrib_to_B_();
  }
#endif

#if INCLUDE_GBC_CODE
  if (!gbc_) {
    // These functions assume that virtuals are expanded in the same basis
    // as the occupied orbitals
    if (!obs_eq_vbs)
      throw std::runtime_error("R12IntEval::compute() -- gbc=false is only supported when basis_vir == basis");

    compute_B_gbc_1_();
    // Remove when done with the merge
#if 0
    if (debug_ > 1) {
      Baa_.print("Alpha-alpha B(OBS+ABS+GBC1) contribution");
      Bab_.print("Alpha-beta B(OBS+ABS+GBC1) contribution");
    }
#endif
    compute_B_gbc_2_();
    // Remove when done with the merge
#if 0
    if (debug_ > 1) {
      Baa_.print("Alpha-alpha B(OBS+ABS+GBC1+GBC2) contribution");
      Bab_.print("Alpha-beta B(OBS+ABS+GBC1+GBC2) contribution");
    }
#endif
  }
#endif
  
  // Finally, compute MP2 energies
  const int nspincases_for_emp2pairs = (spin_polarized() ? 3: 2);
  for(int s=0; s<nspincases_for_emp2pairs; s++) {
      const SpinCase2 spincase2 = static_cast<SpinCase2>(s);
      const SpinCase1 spin1 = case1(spincase2);
      const SpinCase1 spin2 = case2(spincase2);
      Ref<MOIndexSpace> occ1_act = r12info()->refinfo()->occ_act(spin1);
      Ref<MOIndexSpace> occ2_act = r12info()->refinfo()->occ_act(spin2);
      Ref<MOIndexSpace> uocc1_act = r12info()->refinfo()->uocc_act(spin1);
      Ref<MOIndexSpace> uocc2_act = r12info()->refinfo()->uocc_act(spin2);
      // these transforms were used by VXB evaluators and should be available
      const std::string tform_name = transform_label(occ1_act,
                                                     r12info()->refinfo()->orbs(spin1),
                                                     occ2_act,
                                                     r12info()->refinfo()->orbs(spin2),0);
      const Ref<TwoBodyMOIntsTransform> tform = tform_map_[tform_name];
      compute_mp2_pair_energies_(emp2pair_[s],spincase2,
                                 occ1_act,uocc1_act,
                                 occ2_act,uocc2_act,
                                 tform);
  }
  
  // Distribute the final intermediates to every node
  globally_sum_intermeds_(true);

  evaluated_ = true;
}

void
R12IntEval::globally_sum_scmatrix_(RefSCMatrix& A, bool to_all_tasks, bool to_average)
{
  Ref<MessageGrp> msg = r12info_->msg();
  unsigned int ntasks = msg->n();
  // If there's only one task then there's nothing to do
  if (ntasks == 1)
    return;

  const int nelem = A.ncol() * A.nrow();
  double *A_array = new double[nelem];
  A.convert(A_array);
  if (to_all_tasks)
    msg->sum(A_array,nelem,0,-1);
  else
    msg->sum(A_array,nelem,0,0);
  A.assign(A_array);
  if (to_average)
    A.scale(1.0/(double)ntasks);
  if (!to_all_tasks && msg->me() != 0)
    A.assign(0.0);

  delete[] A_array;
}

void
R12IntEval::globally_sum_scvector_(RefSCVector& A, bool to_all_tasks, bool to_average)
{
  Ref<MessageGrp> msg = r12info_->msg();
  unsigned int ntasks = msg->n();
  // If there's only one task then there's nothing to do
  if (ntasks == 1)
    return;

  const int nelem = A.dim().n();
  double *A_array = new double[nelem];
  A.convert(A_array);
  if (to_all_tasks)
    msg->sum(A_array,nelem,0,-1);
  else
    msg->sum(A_array,nelem,0,0);
  A.assign(A_array);
  if (to_average)
    A.scale(1.0/(double)ntasks);
  if (!to_all_tasks && msg->me() != 0)
    A.assign(0.0);

  delete[] A_array;
}

void
R12IntEval::globally_sum_intermeds_(bool to_all_tasks)
{
  // Remove when done with the merge
#if 0
  globally_sum_scmatrix_(Vaa_,to_all_tasks);
  globally_sum_scmatrix_(Vab_,to_all_tasks);

  globally_sum_scmatrix_(Xaa_,to_all_tasks);
  globally_sum_scmatrix_(Xab_,to_all_tasks);

  globally_sum_scmatrix_(Baa_,to_all_tasks);
  globally_sum_scmatrix_(Bab_,to_all_tasks);

  if (ebc_ == false) {
    globally_sum_scmatrix_(Aaa_,to_all_tasks);
    globally_sum_scmatrix_(Aab_,to_all_tasks);

    if (follow_ks_ebcfree_) {
      globally_sum_scmatrix_(Ac_aa_,to_all_tasks);
      globally_sum_scmatrix_(Ac_ab_,to_all_tasks);
    }
    
    globally_sum_scmatrix_(T2aa_,to_all_tasks);
    globally_sum_scmatrix_(T2ab_,to_all_tasks);
    
    globally_sum_scmatrix_(Raa_,to_all_tasks);
    globally_sum_scmatrix_(Rab_,to_all_tasks);
  }

  globally_sum_scvector_(emp2pair_aa_,to_all_tasks);
  globally_sum_scvector_(emp2pair_ab_,to_all_tasks);
#endif
  
  for(int s=0; s<nspincases2(); s++) {
    globally_sum_scmatrix_(V_[s],to_all_tasks);
    globally_sum_scmatrix_(X_[s],to_all_tasks);
    globally_sum_scmatrix_(B_[s],to_all_tasks);
    if (ebc_ == false) {
      globally_sum_scmatrix_(A_[s],to_all_tasks);
      if (follow_ks_ebcfree_) {
        globally_sum_scmatrix_(Ac_[s],to_all_tasks);
      }
      globally_sum_scmatrix_(T2_[s],to_all_tasks);
      globally_sum_scmatrix_(F12_[s],to_all_tasks);
    }
    globally_sum_scvector_(emp2pair_[s],to_all_tasks);
  }

  if (debug_) {
    ExEnv::out0() << indent << "Collected contributions to the intermediates from all tasks";
    if (to_all_tasks)
      ExEnv::out0() << " and distributed to every task" << endl;
    else
      ExEnv::out0() << " on task 0" << endl;
  }
}

const Ref<MOIndexSpace>&
R12IntEval::occ_act(SpinCase1 S) const
{
  return r12info()->refinfo()->occ_act(S);
}

const Ref<MOIndexSpace>&
R12IntEval::vir_act(SpinCase1 S) const
{
  if (r12info()->basis_vir() != r12info()->refinfo()->ref()->basis())
    return r12info()->vir_act(S);
  else
    return r12info()->refinfo()->uocc_act(S);
}

std::string
R12IntEval::transform_label(const Ref<MOIndexSpace>& space1,
                            const Ref<MOIndexSpace>& space2,
                            const Ref<MOIndexSpace>& space3,
                            const Ref<MOIndexSpace>& space4,
                            unsigned int f12) const
{
  std::ostringstream oss;
  // use physicists' notation
  oss << "<" << space1->id() << " " << space3->id() << "| " << corrfactor()->label()
      << "[" << f12 << "] |" << space2->id() << " " << space4->id() << ">";
  return oss.str();
}

std::string
R12IntEval::transform_label(const Ref<MOIndexSpace>& space1,
                            const Ref<MOIndexSpace>& space2,
                            const Ref<MOIndexSpace>& space3,
                            const Ref<MOIndexSpace>& space4,
                            unsigned int f12_left,
                            unsigned int f12_right) const
{
  std::ostringstream oss;
  // use physicists' notation
  oss << "<" << space1->id() << " " << space3->id() << "| " << corrfactor()->label()
      << "[" << f12_left << "," << f12_right << "] |" << space2->id()
      << " " << space4->id() << ">";
  return oss.str();
}

/////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: c++
// c-file-style: "CLJ-CONDENSED"
// End:
