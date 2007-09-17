//
// psiwfn.cc
//
// Copyright (C) 2002 Edward Valeev
//
// Author: Edward Valeev <evaleev@vt.edu>
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

#ifdef __GNUC__
#pragma implementation
#endif

#include <ccfiles.h>
#include <chemistry/qc/mbptr12/utils.h>
#include <chemistry/qc/mbptr12/print.h>
#include <chemistry/qc/psi/psicc_pt2r12.h>

using namespace sc;

//////////////////////////////////////////////////////////////////////////

static ClassDesc PsiCCSD_PT2R12_cd(typeid(PsiCCSD_PT2R12), "PsiCCSD_PT2R12", 1,
                                   "public PsiCC", 0, create<PsiCCSD_PT2R12>,
                                   create<PsiCCSD_PT2R12>);

PsiCCSD_PT2R12::PsiCCSD_PT2R12(const Ref<KeyVal>&keyval) :
  PsiCC(keyval), eccsd_(1.0) {
  if (!replace_Lambda_with_T_)
    throw FeatureNotImplemented("PsiCCSD_PT2R12::PsiCCSD_PT2R12() -- cannot properly use Lambdas yet",__FILE__,__LINE__);
  
  if (mp2_only_)
    PsiCC::do_test_t2_phases();
  
  mbptr12_ = require_dynamic_cast<MBPT2_R12*>(keyval->describedclassvalue("mbpt2r12").pointer(), "PsiCCSD_PT2R12::PsiCCSD_PT2R12\n");
  
  const Ref<R12IntEvalInfo> r12info = mbptr12_->r12evalinfo();
  const Ref<R12Technology> r12tech = r12info->r12tech();
  // cannot do gbc = false yet
  if (!r12tech->gbc())
    throw FeatureNotImplemented("PsiCCSD_PT2R12::PsiCCSD_PT2R12() -- gbc = false is not yet implemented",__FILE__,__LINE__);
  // cannot do ebc=false either
  if (!r12tech->ebc())
    throw FeatureNotImplemented("PsiCCSD_PT2R12::PsiCCSD_PT2R12() -- ebc = false is not yet implemented",__FILE__,__LINE__);
  
}

PsiCCSD_PT2R12::~PsiCCSD_PT2R12() {
}

PsiCCSD_PT2R12::PsiCCSD_PT2R12(StateIn&s) :
  PsiCC(s) {
  mbptr12_ << SavableState::restore_state(s);
  s.get(eccsd_);
}

int PsiCCSD_PT2R12::gradient_implemented() const {
  return 0;
}

void PsiCCSD_PT2R12::save_data_state(StateOut&s) {
  PsiCC::save_data_state(s);
  SavableState::save_state(mbptr12_.pointer(), s);
  s.put(eccsd_);
}

void PsiCCSD_PT2R12::write_input(int convergence) {
  Ref<PsiInput> input = get_psi_input();
  input->open();
  PsiCorrWavefunction::write_input(convergence);
  // if testing T2 transform, obtain MP1 amplitudes
  if (test_t2_phases_)
    input->write_keyword("psi:wfn", "mp2");
  else
    input->write_keyword("psi:wfn", "ccsd");
  input->close();
}

void PsiCCSD_PT2R12::compute() {
  const Ref<R12IntEvalInfo> r12info = mbptr12_->r12evalinfo();
  const Ref<R12Technology> r12tech = r12info->r12tech();
  // params
  const bool ebc = r12tech->ebc();
  
  // compute CCSD wave function
  PsiWavefunction::compute();
  // read CCSD energy
  psi::PSIO& psio = exenv()->psio();
  psio.open(CC_INFO, PSIO_OPEN_OLD);
  psio.read_entry(CC_INFO, "CCSD Energy", reinterpret_cast<char*>(&eccsd_),
                  sizeof(double));
  psio.close(CC_INFO, 1);
  
  // grab amplitudes
  RefSCMatrix T1_psi = T(1);
  RefSCMatrix T2_psi = T(2);
  RefSCMatrix Tau2_psi;
  // Tau = T2 + T1*T1 has same 1st through 3rd order contributions as T2
  if (mp2_only_ || completeness_order_for_intermediates_ < 4) {
    Tau2_psi = T2_psi.clone();
    Tau2_psi.assign(T2_psi);
  } else
    Tau2_psi = Tau2();
  
  // Compute intermediates
  const double mp2r12_energy = mbptr12_->value();
  Ref<R12IntEval> r12eval = mbptr12_->r12eval();
  RefSCMatrix Vpq[NSpinCases2];
  RefSCMatrix Vab[NSpinCases2];
  RefSCMatrix Via[NSpinCases2];
  RefSCMatrix Vij[NSpinCases2];
  RefSymmSCMatrix B[NSpinCases2];
  RefSymmSCMatrix X[NSpinCases2];
  RefSCMatrix T2_MP1[NSpinCases2];
  RefSCMatrix A[NSpinCases2];
  RefSCMatrix T1[NSpinCases2];
  RefSCMatrix T2[NSpinCases2];
  RefSCMatrix Tau2[NSpinCases2];
  
  const int nspincases2 = r12eval->nspincases2();
  for (int s=0; s<nspincases2; s++) {
    const SpinCase2 spincase2 = static_cast<SpinCase2>(s);
    const SpinCase1 spin1 = case1(spincase2);
    const SpinCase1 spin2 = case2(spincase2);
    
    Ref<R12IntEvalInfo> r12info = r12eval->r12info();
    const Ref<MOIndexSpace>& p1 = r12info->refinfo()->orbs(spin1);
    const Ref<MOIndexSpace>& p2 = r12info->refinfo()->orbs(spin2);
    const unsigned int np1 = p1->rank();
    const unsigned int np2 = p2->rank();
    
    const Ref<MOIndexSpace>& occ1_act = r12eval->occ_act(spin1);
    const Ref<MOIndexSpace>& occ2_act = r12eval->occ_act(spin2);
    const Ref<MOIndexSpace>& vir1_act = r12eval->vir_act(spin1);
    const Ref<MOIndexSpace>& vir2_act = r12eval->vir_act(spin2);
    
    Vpq[s] = r12eval->V(spincase2, p1, p2);
    Vij[s] = r12eval->V(spincase2);
    X[s] = r12eval->X(spincase2);
    B[s] = r12eval->B(spincase2);
    if (test_t2_phases_)
      T2_MP1[s] = r12eval->T2(spincase2);
    if (!ebc)
      A[s] = r12eval->A(spincase2);
    
    const Ref<MOIndexSpace>& v1 = r12eval->vir_act(spin1);
    const Ref<MOIndexSpace>& v2 = r12eval->vir_act(spin2);
    const unsigned int nv1 = v1->rank();
    const unsigned int nv2 = v2->rank();
    const Ref<MOIndexSpace>& o1 = r12eval->occ_act(spin1);
    const Ref<MOIndexSpace>& o2 = r12eval->occ_act(spin2);
    const unsigned int no1 = o1->rank();
    const unsigned int no2 = o2->rank();
    const unsigned int no1v2 = no1*nv2;
    const unsigned int nv1v2 = nv1*nv2;
    const unsigned int nxy = Vpq[s].rowdim().n();
    
    // extract Vab and Via from Vpq
    Vab[s] = Vpq[s].kit()->matrix(Vpq[s].rowdim(), new SCDimension(nv1v2));
    Via[s] = Vpq[s].kit()->matrix(Vpq[s].rowdim(), new SCDimension(no1v2));
    
    typedef MOIndexMap OrbMap;
    OrbMap v1_to_p1(*p1<<*v1);
    OrbMap v2_to_p2(*p2<<*v2);
    OrbMap o1_to_p1(*p1<<*o1);
    
    for (unsigned int xy=0; xy<nxy; ++xy) {
      unsigned int ab = 0;
      for (unsigned int a=0; a<nv1; ++a) {
        const unsigned int p = v1_to_p1[a];
        for (unsigned int b=0; b<nv1; ++b, ++ab) {
          const unsigned int q = v2_to_p2[b];
          const unsigned int pq = p*np2 + q;
          const double elem = Vpq[s].get_element(xy, pq);
          Vab[s].set_element(xy, ab, elem);
        }
      }
    }
    
    for (unsigned int xy=0; xy<nxy; ++xy) {
      unsigned int ia = 0;
      for (unsigned int i=0; i<no1; ++i) {
        const unsigned int p = o1_to_p1[i];
        for (unsigned int a=0; a<nv2; ++a, ++ia) {
          const unsigned int q = v1_to_p1[a];
          const unsigned int pq = p*np2 + q;
          const double elem = Vpq[s].get_element(xy, pq);
          Via[s].set_element(xy, ia, elem);
        }
      }
    }
    
    if (debug() >= DefaultPrintThresholds::mostO2N2) {
      Vpq[s].print("Vpq matrix");
      Vab[s].print("Vab matrix");
      Via[s].print("Via matrix");
      if (test_t2_phases_)
        T2_MP1[s].print("MP1 T2 amplitudes");
    }
    if (debug() >= DefaultPrintThresholds::mostO4) {
      Vij[s].print("Vij matrix");
    }
#if TEST_V
    RefSCMatrix Vab_test = r12eval->V(spincase2,vir1_act,vir2_act);
    Vab_test.print("Vab matrix (test)");
    (Vab[s] - Vab_test).print("Vab - Vab (test): should be 0");
#endif
#if TEST_V
    RefSCMatrix Via_test = r12eval->V(spincase2,occ1_act,vir2_act);
    Via_test.print("Via matrix (test)");
    (Via[s] - Via_test).print("Via - Via (test): should be 0");
#endif
  } // end of spincase2 loop
  Ref<SCMatrixKit> localkit = Vpq[AlphaBeta].kit();
  
  // print out MPQC orbitals to compare to Psi orbitals below;
  Ref<MOIndexSpace> orbs_sb_mpqc = r12eval->r12info()->refinfo()->orbs_sb(Alpha);
  if (debug() >= DefaultPrintThresholds::mostN2) {
    orbs_sb_mpqc->coefs().print("MPQC eigenvector");
    orbs_sb_mpqc->evals().print("MPQC eigenvalues");
  }
  Ref<MOIndexSpace> occ_act = r12eval->occ_act(Alpha);
  Ref<MOIndexSpace> vir_act = r12eval->vir_act(Alpha);
  
  // Psi stores amplitudes in Pitzer (symmetry-blocked) order. Construct such spaces for active occupied and virtual spaces
  RefSCMatrix psi_coefs = reference_->coefs();
  RefDiagSCMatrix psi_evals = reference_->evals();
  Ref<MOIndexSpace> occ_act_sb_psi = occ_act_sb(Alpha);
  Ref<MOIndexSpace> vir_act_sb_psi = vir_act_sb(Alpha);
  
  // map MPQC (energy-ordered) orbitals to Psi (symmetry-ordered) orbitals
  SparseMOIndexMap MPQC2PSI_occ_act;
  SparseMOIndexMap MPQC2PSI_vir_act;
  bool can_map_occ_act = true;
  bool can_map_vir_act = true;
  try {
    SparseMOIndexMap tmpmap_o(sparsemap(*occ_act_sb_psi,*occ_act,1e-9));
    std::swap(MPQC2PSI_occ_act,tmpmap_o);
  }
  catch (CannotConstructMap& e) {
    can_map_occ_act = false;
  }
  try {
    SparseMOIndexMap tmpmap_v(sparsemap(*vir_act_sb_psi,*vir_act,1e-9));
    std::swap(MPQC2PSI_vir_act,tmpmap_v);
  }
  catch (CannotConstructMap& e) {
    can_map_vir_act = false;
  }
  
  if (use_sparsemap_only_) {
    if (!can_map_occ_act)
      throw ProgrammingError("PsiCCSD_PT2R12::compute() -- cannot map MPQC occ act. orbitals to Psi occ act. orbitals",__FILE__,__LINE__);
    if (!can_map_vir_act)
      throw ProgrammingError("PsiCCSD_PT2R12::compute() -- cannot map MPQC vir act. orbitals to Psi vir act. orbitals",__FILE__,__LINE__);
  }
  
  const Ref<MOIndexSpace>& occ1_act = r12eval->occ_act(Alpha);
  const Ref<MOIndexSpace>& occ2_act = r12eval->occ_act(Beta);
  const Ref<MOIndexSpace>& vir1_act = r12eval->vir_act(Alpha);
  const Ref<MOIndexSpace>& vir2_act = r12eval->vir_act(Beta);
  const bool use_sparsemap = can_map_occ_act && can_map_vir_act;
  if (use_sparsemap) {
    T1[AlphaBeta] = transform_T1(MPQC2PSI_occ_act, MPQC2PSI_vir_act, T1_psi,
                                 localkit);
    T2[AlphaBeta] = transform_T2(MPQC2PSI_occ_act, MPQC2PSI_occ_act,
                                 MPQC2PSI_vir_act, MPQC2PSI_vir_act, T2_psi,
                                 localkit);
    Tau2[AlphaBeta] = transform_T2(MPQC2PSI_occ_act, MPQC2PSI_occ_act,
                                   MPQC2PSI_vir_act, MPQC2PSI_vir_act,
                                   Tau2_psi, localkit);
    if (test_t2_phases_) {
      compare_T2(T2[AlphaBeta], T2_MP1[AlphaBeta], occ1_act->rank(),
                 occ2_act->rank(), vir1_act->rank(), vir2_act->rank());
    }
    if (debug() >= DefaultPrintThresholds::mostO2N2) {
      T1[AlphaBeta].print("CCSD T1 amplitudes from Psi3 (in MPQC orbitals):");
      T2[AlphaBeta].print("CCSD T2 amplitudes from Psi3 (in MPQC orbitals):");
      Tau2[AlphaBeta].print("CCSD Tau2 amplitudes from Psi3 (in MPQC orbitals):");
    }
  } else {
    RefSCMatrix MPQC2PSI_tform_oa = transform(*occ_act_sb_psi, *occ_act,
                                              localkit);
    RefSCMatrix MPQC2PSI_tform_va = transform(*vir_act_sb_psi, *vir_act,
                                              localkit);
    T1[AlphaBeta] = transform_T1(MPQC2PSI_tform_oa, MPQC2PSI_tform_va, T1_psi,
                                 localkit);
    T2[AlphaBeta] = transform_T2(MPQC2PSI_tform_oa, MPQC2PSI_tform_oa,
                                 MPQC2PSI_tform_va, MPQC2PSI_tform_va, T2_psi,
                                 localkit);
    Tau2[AlphaBeta] = transform_T2(MPQC2PSI_tform_oa, MPQC2PSI_tform_oa,
                                   MPQC2PSI_tform_va, MPQC2PSI_tform_va,
                                   Tau2_psi, localkit);
    if (test_t2_phases_) {
      compare_T2(T2[AlphaBeta], T2_MP1[AlphaBeta], occ1_act->rank(),
                 occ2_act->rank(), vir1_act->rank(), vir2_act->rank());
    }
    if (debug() >= DefaultPrintThresholds::mostO2N2) {
      T1[AlphaBeta].print("CCSD T1 amplitudes from Psi3 (in MPQC orbitals, obtained by transform):");
      T2[AlphaBeta].print("CCSD T2 amplitudes from Psi3 (in MPQC orbitals, obtained by transform):");
      Tau2[AlphaBeta].print("CCSD Tau2 amplitudes from Psi3 (in MPQC orbitals, obtained by transform):");
    }
  }
  
  // Compute Hamiltonian matrix elements
  RefSCMatrix H1_0R[NSpinCases2];
  RefSCMatrix H1_R0[NSpinCases2];
  RefSymmSCMatrix H0_RR[NSpinCases2];
  for (int s=0; s<nspincases2; s++) {
    const SpinCase2 spincase2 = static_cast<SpinCase2>(s);
    const SpinCase1 spin1 = case1(spincase2);
    const SpinCase1 spin2 = case2(spincase2);
    
    const Ref<MOIndexSpace>& occ1_act = r12eval->occ_act(spin1);
    const Ref<MOIndexSpace>& occ2_act = r12eval->occ_act(spin2);
    const Ref<MOIndexSpace>& vir1_act = r12eval->vir_act(spin1);
    const Ref<MOIndexSpace>& vir2_act = r12eval->vir_act(spin2);
    
    // H1_0R is just Vij
    H1_0R[s] = Vij[s].clone();
    H1_0R[s].assign(Vij[s]);
    if (debug() >= DefaultPrintThresholds::O2N2) {
      H1_0R[s].print(prepend_spincase(spincase2,"<0|Hb|R>").c_str());
    }
    
    // Vij is also the leading term in H1_R0
    H1_R0[s] = Vij[s].clone();
    H1_R0[s].assign(Vij[s]);
    
    // if not testing MP1, compute other terms in <R|Hb|0>
    if (!mp2_only_) {
      
      // the leading term in <R|(HT)|0> is Tau2.Vab
      RefSCMatrix HT = Vab[s] * Tau2[s].t();
      // If using the symmetric form of the correction, Lambdas are replaced with T
      if (replace_Lambda_with_T_)
        HT.scale(2.0);
      H1_R0[s].accumulate(HT);
      
      // the next term is T1.Vai
      if (completeness_order_for_intermediates_ >= 3) {
        // store V_xy_ia as V_xyi_a
        double* tmp = new double[Via[s].rowdim().n() * Via[s].coldim().n()];
        Via[s].convert(tmp);
        RefSCMatrix V_xyi_a = Via[s].kit()->matrix(new SCDimension(Via[s].rowdim().n() * T1[s].rowdim().n()), T1[s].coldim());
        V_xyi_a.assign(tmp);
        delete[] tmp;
        
        RefSCMatrix V_xyi_J = V_xyi_a * T1[s].t();
        // store V_xyi_J as V_xy_iJ
        tmp = new double[HT.rowdim().n() * HT.coldim().n()];
        V_xyi_J.convert(tmp);
        HT.assign(tmp);
        symmetrize<false>(HT, HT, r12eval->xspace(spin1), occ1_act);
        if (debug() >= DefaultPrintThresholds::mostO2N2) {
          HT.print("Via.T1");
        }
        delete[] tmp;
        // NOTE: V_{xy}^{ia} T_a^j + V_{xy}^{aj} T_a^i = 2 * symm(V_{xy}^{ia} T_a^j
        HT.scale(2.0);
        H1_R0[s].accumulate(HT);
        
#if TEST_ViaT1
        // test against V evaluator
        RefSCMatrix vir2_act_coefs = vir2_act->coefs();
        RefSCMatrix to_t1;
        {
          RefSCDimension rowdim = vir2_act->dim();
          RefSCDimension coldim = occ2_act->dim();
          to_t1 = vir2_act_coefs.kit()->matrix(rowdim,coldim);
          double* tmp = new double[rowdim.n() * coldim.n()];
          T1[s].t().convert(tmp);
          to_t1.assign(tmp);
          delete[] tmp;
        }
        RefSCMatrix Tocc2_act_coefs = vir2_act_coefs * to_t1;
        Ref<MOIndexSpace> Tocc2_act = new MOIndexSpace("i(t1)", "active occupied orbitals (weighted by T1)",
            occ2_act, Tocc2_act_coefs, occ2_act->basis());
        RefSCMatrix ViJ = r12eval->V(spincase2,occ1_act,Tocc2_act);
        symmetrize<false>(ViJ,ViJ,r12eval->xspace(spin1),occ1_act);
        ViJ.print("Via.T1 computed with R12IntEvalInfo::V()");
        (HT - ViJ).print("Via.T1 - Via.T1 (test): should be zero");
#endif
      }
      
      // ebc term is Tau2.A
      if (!ebc)
        HT.accumulate_product(A[s], Tau2[s].t());
      
      if (debug() >= DefaultPrintThresholds::mostO2N2) {
        HT.print(prepend_spincase(spincase2,"<R|(H*T)|0>").c_str());
      }
    }
    
    if (debug() >= DefaultPrintThresholds::O2N2) {
      H1_R0[s].print(prepend_spincase(spincase2,"<R|Hb|0>").c_str());
    }
    
    // H0_RR is just B
    H0_RR[s] = B[s].clone();
    H0_RR[s].assign(B[s]);
  }
  // Make H1_R0[AlphaAlpha]
  {
    H1_R0[AlphaAlpha] = r12eval->V(AlphaAlpha).clone();
    antisymmetrize(H1_R0[AlphaAlpha], H1_R0[AlphaBeta], r12eval->xspace(Alpha),
                   r12eval->occ_act(Alpha));
  }
  
  // compute the second-order correction: E2 = - H1_0R . H0_RR^{-1} . H1_R0 = C_MP1 . H1_R0
  const int debug = 0;
  Ref<MP2R12Energy> r12energy = new MP2R12Energy(r12eval,r12eval->r12info()->r12tech()->stdapprox(),debug);
  double E2[NSpinCases2];
  // WARNING only RHF and UHF are considered
  const int num_unique_spincases2 = (reference_->spin_polarized() ? 3 : 2);
  for (int s=0; s<num_unique_spincases2; s++) {
    const SpinCase2 spincase2 = static_cast<SpinCase2>(s);
    
    RefSCMatrix C_MP1 = r12energy->C(spincase2);
    RefSCMatrix E2_mat = C_MP1.t() * H1_R0[s];
    E2[s] = E2_mat.trace();
  }
  ExEnv::out0() << "E2(AB) = "<< E2[AlphaBeta]<< endl;
  ExEnv::out0() << "E2(AA) = "<< 2.0*E2[AlphaAlpha]<< endl;
  ExEnv::out0() << "E2(s) = "<< E2[AlphaBeta] - E2[AlphaAlpha]<< endl;
  ExEnv::out0() << "E2(t) = "<< 3.0*E2[AlphaAlpha]<< endl;
  
  const double e2 = E2[AlphaBeta] + 2.0*E2[AlphaAlpha];
  ExEnv::out0() << "E2           = "<< e2 << endl;
  ExEnv::out0() << "ECCSD        = "<< eccsd_ << endl;
  ExEnv::out0() << "ECCSD_PT2R12 = "<< e2 + eccsd_ << endl;
  ExEnv::out0() << "E2(MP2)      = "<< mbptr12_->r12_corr_energy() << endl;
  ExEnv::out0() << "EMP2         = "<< mbptr12_->corr_energy()
      - mbptr12_->r12_corr_energy() << endl;
  ExEnv::out0() << "EMP2R12      = "<< mbptr12_->corr_energy() << endl;
}

