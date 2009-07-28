//
// written by Toru Shiozaki <shiozaki@qtp.ufl.edu>
// Dec 20, 2008
//

#include <chemistry/qc/scf/clscf.h>
#include <chemistry/qc/scf/hsosscf.h>
#include <chemistry/qc/mbptr12/orbitalspace.h>
#include <string>
#include <cassert>
#include <vector>
#include <algorithm>
#include <math.h>
#include <chemistry/qc/mbptr12/blas.h>

#include <chemistry/qc/ccr12/ccr12_info.h>
#include <chemistry/qc/ccr12/mtensor.h>

using namespace sc;


void CCR12_Info::fill_in_iiii() {

  // only RHF is handled right now
  assert(restricted_);

  // fill in X, B and P intermediates, as well as geminal T amplitudes
  assert(need_w1());

  // intermediates are computed in occ_act(Alpha) space
  // map corr_space_ to it
  // compute map from indices in full spin-orbital space to indices in the respective spin spaces
  vector<long> amap;
  {
    vector<int> intmap = sc::map(*(r12evalinfo_->refinfo()->occ_act(Alpha)), *corr_space_, false);
    amap.resize(intmap.size());
    std::copy(intmap.begin(), intmap.end(), amap.begin());
  }

  // assuming RHF!
  const int nocc_act = this->naoa();
  MTensor<4>::tile_ranges iiii(4, MTensor<4>::tile_range(0, this->noab()));
  MTensor<4>::element_ranges iiii_erange(4, MTensor<4>::element_range(0, nocc_act) );
  {
    MTensor<4> X(this,d_xs2.pointer(),iiii);
    X.convert(r12int_eval_->X(AlphaBeta), nocc_act, nocc_act, false, false,
              amap, amap, amap, amap, &iiii_erange);
  }

  {
    MTensor<4> B(this,d_bs2.pointer(),iiii);
    B.convert(r12int_eval_->B(AlphaBeta), nocc_act, nocc_act, false, false,
              amap, amap, amap, amap, &iiii_erange);
  }

 if (need_gt2()) { // (2)R12 methods do not need gt2 tensors (but need all others).
    MTensor<4> GT2(this,d_gt2.pointer(),iiii);

    // compute fixed geminal coefficients
    Ref<R12EnergyIntermediates> r12intermeds = new R12EnergyIntermediates(r12int_eval_, r12evalinfo_->stdapprox());
    Ref<MP2R12Energy_SpinOrbital_new> mp2r12_energy = new MP2R12Energy_SpinOrbital_new(r12intermeds, 0);
    RefSCMatrix gt2 = mp2r12_energy->C(AlphaBeta);
    GT2.convert(gt2, nocc_act, nocc_act, false, false,
                amap, amap, amap, amap, &iiii_erange);

#if 0
    ExEnv::out0() << "TEST: mp2-r12 energy = " << mp2r12_energy->energy() << endl;
    {
      double C_dot_V_tr = 0.0;

#if 1
      {
        SpinCase2 spin = AlphaAlpha;
        RefSCMatrix C_dot_V = mp2r12_energy->C(spin) * r12int_eval_->V(spin);
        C_dot_V.print("C.V(AA)");
        const int noo = r12int_eval_->dim_oo(spin).n();
        for(int ij=0; ij<noo; ++ij) {
          C_dot_V_tr += C_dot_V(ij,ij);
        }
        C_dot_V_tr *= 2.0;
      }
#endif
#if 1
      {
        SpinCase2 spin = AlphaBeta;
        RefSCMatrix C_dot_V = mp2r12_energy->C(spin) * r12int_eval_->V(spin);
        C_dot_V.print("C.V(AB)");
        const int noo = r12int_eval_->dim_oo(spin).n();
        for(int ij=0; ij<noo; ++ij) {
          C_dot_V_tr += C_dot_V(ij,ij);
        }
      }
#endif

      ExEnv::out0() << "TEST: tr(C.V) = " << C_dot_V_tr << endl;
    }
#endif

  } // GT2

}


void CCR12_Info::fill_in_vr_and_vd() {

  // only RHF is handled so far
  assert(restricted_);

  // for V intermediate (denoted as d_vr2 in smith; its inverse is d_vd2)
  assert(need_w1());

  // intermediates are computed using i indices from occ_act(Alpha) space
  // map corr_space_ to it
  vector<long> aimap;
  {
    vector<int> intmap = sc::map(*(r12evalinfo_->refinfo()->occ_act(Alpha)), *corr_space_, false);
    aimap.resize(intmap.size());
    std::copy(intmap.begin(), intmap.end(), aimap.begin());
  }
  // g indices are in aobs_space_, map to it also
  vector<long> agmap;
  {
    vector<int> intmap = sc::map(*aobs_space_, *corr_space_, false);
    agmap.resize(intmap.size());
    std::copy(intmap.begin(), intmap.end(), agmap.begin());
  }


  // assuming RHF!
  const int nocc_act = this->naoa();
  const int norbs_act = this->naoa() + this->nava();

  // d_vr2 = ggii
  {
    MTensor<4>::tile_ranges ggii(4);
    ggii[0] = MTensor<4>::tile_range(0, this->nab());
    ggii[1] = MTensor<4>::tile_range(0, this->nab());
    ggii[2] = MTensor<4>::tile_range(0, this->noab());
    ggii[3] = MTensor<4>::tile_range(0, this->noab());
    MTensor<4> V(this,d_vr2.pointer(),ggii);
    MTensor<4>::element_ranges ggii_erange(4);
    ggii_erange[0] = MTensor<4>::element_range(0, norbs_act);
    ggii_erange[1] = MTensor<4>::element_range(0, norbs_act);
    ggii_erange[2] = MTensor<4>::element_range(0, nocc_act);
    ggii_erange[3] = MTensor<4>::element_range(0, nocc_act);
    V.convert<RefSCMatrix>(Vgg_[AlphaBeta].t(), norbs_act, nocc_act, false, false,
              agmap, agmap, aimap, aimap, &ggii_erange);
  }

  // d_vd2 = iigg
  {
    MTensor<4>::tile_ranges iigg(4);
    iigg[0] = MTensor<4>::tile_range(0, this->noab());
    iigg[1] = MTensor<4>::tile_range(0, this->noab());
    iigg[2] = MTensor<4>::tile_range(0, this->nab());
    iigg[3] = MTensor<4>::tile_range(0, this->nab());
    MTensor<4> V(this,d_vd2.pointer(),iigg);
    MTensor<4>::element_ranges iigg_erange(4);
    iigg_erange[0] = MTensor<4>::element_range(0, nocc_act);
    iigg_erange[1] = MTensor<4>::element_range(0, nocc_act);
    iigg_erange[2] = MTensor<4>::element_range(0, norbs_act);
    iigg_erange[3] = MTensor<4>::element_range(0, norbs_act);
    V.convert<RefSCMatrix>(Vgg_[AlphaBeta], nocc_act, norbs_act, false, false,
              aimap, aimap, agmap, agmap, &iigg_erange);
  }

}


void CCR12_Info::fill_in_fr_and_fd() {


  // only RHF is handled so far
  assert(restricted_);
  assert(need_w1());

  // intermediates are computed using i indices from occ_act(Alpha) space
  // map corr_space_ to it
  vector<long> aimap;
  {
    vector<int> intmap = sc::map(*(r12evalinfo_->refinfo()->occ_act(Alpha)), *corr_space_, false);
    aimap.resize(intmap.size());
    std::copy(intmap.begin(), intmap.end(), aimap.begin());
  }
  // a indices are in vir_act(Alpha), map to it also
  vector<long> aamap;
  {
    vector<int> intmap = sc::map(*(r12evalinfo_->vir_act(Alpha)), *corr_space_, false);
    aamap.resize(intmap.size());
    std::copy(intmap.begin(), intmap.end(), aamap.begin());
  }
  // A indices are in ribs_space(Alpha), map to it also
  vector<long> aAmap;
  {
    vector<int> intmap = sc::map(*(r12evalinfo_->ribs_space(Alpha)), *corr_space_, false);
    aAmap.resize(intmap.size());
    std::copy(intmap.begin(), intmap.end(), aAmap.begin());
  }

  // assuming RHF!
  const int nocc_act = r12evalinfo_->refinfo()->occ_act(Alpha)->rank();
  const int nvir_act = r12evalinfo_->vir_act(Alpha)->rank();
  const int ncabs = r12evalinfo_->ribs_space(Alpha)->rank();

  // d_fr2 = aAii
  {
    MTensor<4>::tile_ranges aAii(4);
    aAii[0] = MTensor<4>::tile_range(this->noab(), this->noab() + this->nvab());
    aAii[1] = MTensor<4>::tile_range(this->noab(), this->nab());
    aAii[2] = MTensor<4>::tile_range(0, this->noab());
    aAii[3] = MTensor<4>::tile_range(0, this->noab());
    MTensor<4> F(this,d_fr2.pointer(),aAii);
    MTensor<4>::element_ranges aAii_erange(4);
    aAii_erange[0] = MTensor<4>::element_range(0, nvir_act);
    aAii_erange[1] = MTensor<4>::element_range(0, ncabs);
    aAii_erange[2] = MTensor<4>::element_range(0, nocc_act);
    aAii_erange[3] = MTensor<4>::element_range(0, nocc_act);
    const bool transpose_23_01 = true;
    F.convert(iiaA_acc_[AlphaBeta], r12evalinfo_->corrfactor()->tbint_type_f12(),
              aamap, aAmap, aimap, aimap, &aAii_erange, transpose_23_01);
  }

  // d_fd2 = iiaA
  {
    MTensor<4>::tile_ranges iiaA(4);
    iiaA[0] = MTensor<4>::tile_range(0, this->noab());
    iiaA[1] = MTensor<4>::tile_range(0, this->noab());
    iiaA[2] = MTensor<4>::tile_range(this->noab(), this->noab() + this->nvab());
    iiaA[3] = MTensor<4>::tile_range(this->noab(), this->nab());
    MTensor<4> F(this,d_fd2.pointer(),iiaA);
    MTensor<4>::element_ranges iiaA_erange(4);
    iiaA_erange[0] = MTensor<4>::element_range(0, nocc_act);
    iiaA_erange[1] = MTensor<4>::element_range(0, nocc_act);
    iiaA_erange[2] = MTensor<4>::element_range(0, nvir_act);
    iiaA_erange[3] = MTensor<4>::element_range(0, ncabs);
    F.convert(iiaA_acc_[AlphaBeta], r12evalinfo_->corrfactor()->tbint_type_f12(),
              aimap, aimap, aamap, aAmap, &iiaA_erange);
  }

}

