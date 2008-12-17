//
// moindexspace.cc
//
// Copyright (C) 2003 Edward Valeev
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

#ifdef __GNUG__
#pragma implementation
#endif

#include <stdexcept>
#include <algorithm>
#include <stdlib.h>
#include <util/misc/formio.h>
#include <util/class/scexception.h>
#include <chemistry/qc/basis/petite.h>
#include <chemistry/qc/mbptr12/moindexspace.h>

#define DEBUG_DENSE_TRANSFORM 0

using namespace std;
using namespace sc;

inline int max(int a,int b) { return (a > b) ? a : b;}

/*---------------
  MOIndexSpace
 ---------------*/
static ClassDesc MOIndexSpace_cd(
  typeid(MOIndexSpace),"MOIndexSpace",1,"virtual public SavableState",
  0, 0, create<MOIndexSpace>);

MOIndexSpace::MOIndexSpace(const MOIndexSpace& O) :
    id_(O.id_), name_(O.name_), basis_(O.basis_), integral_(O.integral_),
    nmo_(O.nmo_), moorder_(O.moorder_)
{
    init();
}

MOIndexSpace::MOIndexSpace(const std::string& id, const std::string& name,
                           const RefSCMatrix& full_coefs, const Ref<GaussianBasisSet>& basis,
                           const Ref<Integral>& integral,
                           const std::vector<unsigned int>& offsets,
                           const std::vector<unsigned int>& nmopi,
                           const IndexOrder& moorder,
                           const RefDiagSCMatrix& evals) :
  id_(id), name_(name), basis_(basis), integral_(integral), nmo_(nmopi),
  moorder_(moorder)
{
  full_coefs_to_coefs(full_coefs, evals, offsets);

  init();
}

MOIndexSpace::MOIndexSpace(const std::string& id, const std::string& name,
                           const RefSCMatrix& full_coefs, const Ref<GaussianBasisSet>& basis,
                           const Ref<Integral>& integral,
                           const RefDiagSCMatrix& evals, unsigned int nfzc, unsigned int nfzv,
                           const IndexOrder& moorder) :
  id_(id), name_(name), basis_(basis), integral_(integral), moorder_(moorder)
{
  if (evals.null())
    throw std::runtime_error("MOIndexSpace::MOIndexSpace() -- null eigenvalues matrix");
  if (nfzc < 0 || nfzc > full_coefs.coldim().n())
    throw std::runtime_error("MOIndexSpace::MOIndexSpace() -- invalid nfzc");
  if (nfzc + nfzv > full_coefs.coldim().n())
    throw std::runtime_error("MOIndexSpace::MOIndexSpace() -- invalid nfzc+nfzv");
  std::vector<unsigned int> offsets = frozen_to_blockinfo(nfzc,nfzv,evals);
  full_coefs_to_coefs(full_coefs, evals, offsets);

  init();
}

MOIndexSpace::MOIndexSpace(const std::string& id, const std::string& name,
                           const RefSCMatrix& full_coefs, const Ref<GaussianBasisSet>& basis,
                           const Ref<Integral>& integral) :
  id_(id), name_(name), basis_(basis), integral_(integral), moorder_(symmetry)
{
  Ref<SCBlockInfo> modim_blocks = full_coefs.coldim()->blocks();
  unsigned int nb = modim_blocks->nblock();
  std::vector<unsigned int> offsets(nb);
  nmo_.resize(nb);
  for(int i=0; i<nb; i++) {
    offsets[i] = 0;
    nmo_[i] = modim_blocks->size(i);
  }

  full_coefs_to_coefs(full_coefs, 0, offsets);

  init();
}

MOIndexSpace::MOIndexSpace(const std::string& id, const std::string& name,
                           const Ref<MOIndexSpace>& orig_space, const RefSCMatrix& new_coefs,
                           const Ref<GaussianBasisSet>& new_basis) :
  id_(id), name_(name), integral_(orig_space->integral()), mosym_(orig_space->mosym_), evals_(orig_space->evals_),
  rank_(orig_space->rank_), nblocks_(orig_space->nblocks_),
  nmo_(orig_space->nmo_), moorder_(orig_space->moorder_)
{
  if (rank_ != new_coefs.coldim()->n())
    throw std::runtime_error("MOIndexSpace::MOIndexSpace() -- new_coefs have different number of orbitals");
  coefs_ = new_coefs;
  basis_ = new_basis;
  init();
}

MOIndexSpace::MOIndexSpace(StateIn& si) : SavableState(si)
{
  si.get(id_);
  si.get(name_);

  coefs_.restore(si);
  evals_.restore(si);
  basis_ << SavableState::restore_state(si);
  integral_ << SavableState::restore_state(si);
  si.get(mosym_);

  si.get(rank_);
  si.get(nblocks_);
  si.get(nmo_);

  int moorder; si.get(moorder); moorder = (int) moorder_;

  init();
}

MOIndexSpace::~MOIndexSpace()
{
}

void
MOIndexSpace::save_data_state(StateOut& so)
{
  so.put(id_);
  so.put(name_);

  coefs_.save(so);
  evals_.save(so);
  modim_ = evals_.dim();
  SavableState::save_state(basis_.pointer(),so);
  SavableState::save_state(integral_.pointer(),so);
  so.put(mosym_);

  so.put(rank_);
  so.put(nblocks_);
  so.put(nmo_);

  so.put((int)moorder_);
}

const std::string&
MOIndexSpace::id() const { return id_; }
const std::string&
MOIndexSpace::name() const { return name_; }

const RefSCDimension&
MOIndexSpace::dim() const { return modim_; }

const Ref<GaussianBasisSet>&
MOIndexSpace::basis() const { return basis_; }

const Ref<Integral>&
MOIndexSpace::integral() const { return integral_; }

const RefSCMatrix&
MOIndexSpace::coefs() const { return coefs_; }

const RefDiagSCMatrix&
MOIndexSpace::evals() const { return evals_; }

const std::vector<unsigned int>&
MOIndexSpace::mosym() const { return mosym_; }

MOIndexSpace::IndexOrder
MOIndexSpace::moorder() const { return moorder_; }

unsigned int
MOIndexSpace::rank() const { return rank_; }

unsigned int
MOIndexSpace::nblocks() const { return nblocks_; }

const std::vector<unsigned int>&
MOIndexSpace::nmo() const { return nmo_; }

void
MOIndexSpace::check_mosym() const
{
  int ng = basis_->molecule()->point_group()->char_table().order();

  for(std::vector<unsigned int>::const_iterator p=mosym_.begin(); p != mosym_.end(); ++p) {
    if (*p < 0 || *p >= ng)
      throw std::runtime_error("MOIndexSpace::check_mosym() -- invalid value in the list of orbital irreps");
  }
}


std::vector<unsigned int>
MOIndexSpace::frozen_to_blockinfo(unsigned int nfzc, unsigned int nfzv,
                                  const RefDiagSCMatrix& evals)
{
  unsigned int rank = evals.dim().n();

  unsigned int nb = evals.dim()->blocks()->nblock();
  std::vector<unsigned int> offsets(nb);
  nmo_.resize(nb);
  offsets.resize(nb);
  for(unsigned int b=0; b<nb; b++) {
    nmo_[b] = evals.dim()->blocks()->size(b);
    offsets[b] = 0;
  }

  // Get the energies of the orbitals in this space
  double* energy = new double[rank];
  unsigned int* index_map = new unsigned int[rank];
  std::vector<unsigned int> blocked_index_to_irrep(rank);
  unsigned int ii = 0;     // blocked index to this space
  unsigned int offset = 0;
  for(unsigned int b=0; b<nb; b++) {
    for(unsigned int i=0; i<nmo_[b]; i++, ii++) {
      energy[ii] = evals.get_element(i+offset);
      blocked_index_to_irrep[ii] = b;
    }
    offset += nmo_[b];
  }

  // Do the sort
  dquicksort(energy,index_map,rank);
  delete[] energy;

  // Get rid of nfzc lowest orbitals
  for(unsigned int i=0; i<nfzc; i++) {
    unsigned int b = blocked_index_to_irrep[index_map[i]];
    ++offsets[b];
    --nmo_[b];
  }

  // Get rid of nfzv highest orbitals
  for(unsigned int i=rank-nfzv; i<rank; i++) {
    unsigned int b = blocked_index_to_irrep[index_map[i]];
    --nmo_[b];
  }

  delete[] index_map;
  return offsets;
}

void
MOIndexSpace::full_coefs_to_coefs(const RefSCMatrix& full_coefs, const RefDiagSCMatrix& evals,
                                  const std::vector<unsigned int>& offsets)
{
  // compute the rank of this
  rank_ = 0;
  for(vector<unsigned int>::const_iterator p=nmo_.begin(); p != nmo_.end(); ++p) {
    rank_ += *p;
  }

  mosym_.resize(rank_);
  RefSCDimension modim = full_coefs.coldim();  // the dimension of the full space

  // In general vectors are ordered differently from the original
  unsigned int* index_map = new unsigned int[rank_];               // maps index in this (sorted) space to this (blocked) space
  std::vector<unsigned int> blocked_subindex_to_full_index(rank_); // maps index from this space(in blocked form) into the full space
  std::vector<unsigned int> blocked_subindex_to_irrep(rank_);      // maps index from this space(in blocked form) to the irrep
  if (moorder_ == symmetry) {
    // coefs_ has the same number of blocks as full_coefs_
    const unsigned int nb = modim->blocks()->nblock();
    int* nfunc_per_block = new int[nb];
    for(unsigned int i=0; i<nb; i++)
      nfunc_per_block[i] = nmo_[i];
    modim_ = new SCDimension(rank_, nb, nfunc_per_block, ("MO(" + name_ + ")").c_str());
    if (rank_) {
      for(unsigned int i=0; i<nb; i++)
        modim_->blocks()->set_subdim(i, new SCDimension(nfunc_per_block[i]));
    }
    delete[] nfunc_per_block;

    // The sorted->blocked reordering array is trivial when no resorting is done
    for(unsigned int i=0; i<rank_; i++) {
      index_map[i] = i;
    }

    unsigned int ii = 0;     // blocked index to this space
    unsigned int offset = 0;
    for(unsigned int b=0; b<nb; b++) {
      for(unsigned int i=0; i<nmo_[b]; i++, ii++) {
        blocked_subindex_to_full_index[ii] = i+offsets[b]+offset;
        blocked_subindex_to_irrep[ii] = b;
      }
      offset += modim->blocks()->size(b);
    }
  }
  else if (moorder_ == energy) {
    //
    // Sort vectors by their energy
    //

    // Get the energies of the orbitals in this space
    double* energy = new double[rank_];
    const unsigned int nb = nmo_.size();
    unsigned int ii = 0;     // blocked index to this space
    unsigned int offset = 0;
    for(unsigned int b=0; b<nb; b++) {
      for(unsigned int i=0; i<nmo_[b]; i++, ii++) {
        energy[ii] = evals.get_element(i+offsets[b]+offset);
        blocked_subindex_to_full_index[ii] = i+offsets[b]+offset;
        blocked_subindex_to_irrep[ii] = b;
      }
      offset += modim->blocks()->size(b);
    }

    // Do the sort
    dquicksort(energy,index_map,rank_);

    // coefs_ has 1 block
    int* nfunc_per_block = new int[1];
    nfunc_per_block[0] = rank_;
    modim_ = new SCDimension(rank_, 1, nfunc_per_block, ("MO(" + name_ + ")").c_str());
    if (rank_)
      modim_->blocks()->set_subdim(0, new SCDimension(nfunc_per_block[0]));

    // Recompute nmo_ to conform the energy ordering
    nmo_.resize(1);
    nmo_[0] = rank_;

    delete[] energy;
    delete[] nfunc_per_block;
  }
  else
    throw std::runtime_error("MOIndexSpace::full_coefs_to_coefs() -- moorder should be either energy or symmetry");

  // Copy required columns of full_coefs_ into coefs_
  RefSCDimension aodim = full_coefs.rowdim();
  Ref<SCMatrixKit> so_matrixkit = basis_->so_matrixkit();
  coefs_ = so_matrixkit->matrix(aodim, modim_);
  evals_ = so_matrixkit->diagmatrix(modim_);
  for (unsigned int i=0; i<rank_; i++) {
    const unsigned int ii = blocked_subindex_to_full_index[index_map[i]];
    mosym_[i] = blocked_subindex_to_irrep[index_map[i]];
    for (unsigned int j=0; j<aodim.n(); j++) {
      coefs_(j,i) = full_coefs(j,ii);
    }
  }
  if (evals.nonnull())
    for (unsigned int i=0; i<rank_; i++) {
      const unsigned int ii = blocked_subindex_to_full_index[index_map[i]];
      evals_(i) = evals(ii);
    }
  else
    evals_.assign(0.0);

  nblocks_ = modim_->blocks()->nblock();

#if 0
  // Compute the map to the full space
  map_to_full_space_.resize(rank_);
  for (unsigned int i=0; i<rank_; i++) {
    map_to_full_space_[i] = blocked_subindex_to_full_index[index_map[i]];
  }
#endif

  delete[] index_map;
}

void
MOIndexSpace::init()
{
  if (id_.size() > max_id_length)
    throw ProgrammingError("MOIndexSpace constructed with id longer than allowed",__FILE__,__LINE__);
}


size_t
MOIndexSpace::memory_in_use() const
{
  size_t memory = (size_t)basis_->nbasis() * rank_ * sizeof(double);
  return memory;
}

void
MOIndexSpace::print(ostream&o) const
{
  o << indent << "MOIndexSpace \"" << name_ << "\":" << endl;
  o << incindent;
  o << indent << "Basis Set:" << endl;
  o << incindent; basis_->print(o); o << decindent << endl;
  o << decindent;
}

void
MOIndexSpace::print_detail(ostream&o) const
{
  o << indent << "MOIndexSpace \"" << name_ << "\":" << endl;
  o << incindent;
  o << indent << "Basis Set:" << endl;
  o << incindent; basis_->print(o); o << decindent << endl;
  evals_.print("Eigenvalues");
  coefs_.print("Coefficients");
  o << decindent;
}

void
MOIndexSpace::print_summary(ostream& o) const
{
  o << indent << "MOIndexSpace \"" << name_ << "\":" << endl;
  o << incindent;
  o << indent << "GaussianBasisSet \"" << basis_->name() << "\""<< endl;
  o << indent << "  rank  nbasis  nshell  nfuncmax" << endl;
  o << indent << scprintf("  %-6i %-6i  %-6i   %-6i",
                          rank_,
                          basis_->nbasis(),
                          basis_->nshell(),
                          basis_->max_nfunction_in_shell()) << endl;
  o << decindent;

}

/////////////////////////////////////////////////////////////////
// Function dquicksort performs a quick sort (smaller -> larger)
// of the double data in item by the integer indices in index;
// data in item remain unchanged
/////////////////////////////////////////////////////////////////
namespace {
  // use this to compute permutation corresponding to a sort
  class IndexedValue {
    int index_;
    double value_;
  public:
    IndexedValue(int index, double value) : index_(index), value_(value) {}
    int index() const { return index_; }
    double value() const { return value_; }

    bool operator<(const IndexedValue& a) const {
      const double small_diff = 1.0e-12;
      if (fabs(value_-a.value_) < small_diff)
        return false;
      else
        return value_ < a.value_;
    }
  };

};


void
MOIndexSpace::dquicksort(double *item,unsigned int *index,unsigned int n)
{
  typedef std::vector<IndexedValue> vectype;
  typedef std::vector<IndexedValue>::iterator iter;
  std::vector<IndexedValue> vals;
  for (unsigned int i=0; i<n; i++) {
    IndexedValue val(i,item[i]);
    vals.push_back(val);
  }
  stable_sort(vals.begin(),vals.end());
  for (unsigned int i=0; i<n; i++) {
    index[i] = vals.at(i).index();
  }
}

/////////////////////////////////////////////////////////////////////////////

MOIndexMap
sc::operator<<(const MOIndexSpace& s2, const MOIndexSpace& s1)
{
  const unsigned int rank1 = s1.rank();
  const unsigned int rank2 = s2.rank();
  if (rank1 > rank2 ||
      s1.basis() != s2.basis() ||
      s1.integral()->class_desc() != s2.integral()->class_desc())
    throw CannotConstructMap();

  const RefSCMatrix& c1 = s1.coefs().t();
  const RefSCMatrix& c2 = s2.coefs().t();
#if 0
  c1.print("operator<<(MOIndexSpace,MOIndexSpace): c1");
  c2.print("operator<<(MOIndexSpace,MOIndexSpace): c2");
#endif
  const unsigned int nao = c1.coldim().n();

  typedef std::vector<unsigned int> maptype;
  maptype map(rank1);

  // if objects are the same, map is trivial
  if (&s1 == &s2) {
    for(unsigned int mo1=0; mo1<rank1; mo1++)
      map[mo1] = mo1;
    return map;
  }

  std::vector<int> has_been_mapped(rank2,-1);
  // for each MO in 1 find vector in 2
  for(unsigned int mo1=0; mo1<rank1; mo1++) {
    bool found_match = false;
    for(unsigned int mo2=0; mo2<rank2; mo2++) {
      // if mo2 is not yet mapped by one of MOs in 1
      if (has_been_mapped[mo2] != -1)
        continue;
      bool vectors_do_not_match = false;
      // compare vectors
      for(unsigned int ao=0; ao<nao; ao++) {
        if ( fabs(c1.get_element(mo1,ao)-c2.get_element(mo2,ao)) > 1.0e-12 ) {
          vectors_do_not_match = true;
#if 0
          ExEnv::out0() << "operator<<(MOIndexSpace,MOIndexSpace): (mo1,mo2,ao) = "
                        << mo1 << "," << mo2 << "," << ao << "  delta = "
                        << fabs(c1.get_element(mo1,ao)-c2.get_element(mo2,ao)) << std::endl;
#endif
          break;
        }
      }
      // go to next mo1 if found match
      if (!vectors_do_not_match) {
        found_match = true;
#if 0
        ExEnv::out0() << "operator<<(MOIndexSpace,MOIndexSpace): found match (mo1,mo2) = "
                      << mo1 << "," << mo2 << std::endl;
#endif
        map[mo1] = mo2;
        has_been_mapped[mo2] = 1;
        mo2 = rank2;
      }
    }
    // if this mo1 doesn't match any mo2 -- punt
    if (!found_match)
      throw CannotConstructMap();
  }

  return map;
}

sc::SparseMOIndexMap
sc::sparsemap(const MOIndexSpace& s2, const MOIndexSpace& s1, double hardzero)
{
  const unsigned int rank1 = s1.rank();
  const unsigned int rank2 = s2.rank();
  if (rank1 > rank2 ||
      s1.basis() != s2.basis() ||
      s1.integral()->class_desc() != s2.integral()->class_desc())
    throw CannotConstructMap();

  const RefSCMatrix& c1 = s1.coefs().t();
  const RefSCMatrix& c2 = s2.coefs().t();
#if 0
  c1.print("sparsemap(MOIndexSpace,MOIndexSpace): c1");
  c2.print("sparsemap(MOIndexSpace,MOIndexSpace): c2");
#endif
  const unsigned int nao = c1.coldim().n();

  typedef SparseMOIndexMap maptype;
  maptype map(rank1);

  // if objects are the same, map is trivial
  if (&s1 == &s2) {
    for(unsigned int mo1=0; mo1<rank1; mo1++)
      map[mo1] = make_pair(mo1,1.0);
    return map;
  }

  std::vector<int> has_been_mapped(rank2,-1);
  // for each MO in 1 find vector in 2
  for(unsigned int mo1=0; mo1<rank1; mo1++) {
    bool found_match = false;
    for(unsigned int mo2=0; mo2<rank2; mo2++) {
      // if mo2 is not yet mapped by one of MOs in 1
      if (has_been_mapped[mo2] != -1)
        continue;

      // compare vectors assuming same phase
      bool vectors_do_not_match = false;
      double phase = 1.0;
      for(unsigned int ao=0; ao<nao; ao++) {
        if ( fabs(c1.get_element(mo1,ao) - phase*c2.get_element(mo2,ao)) > hardzero ) {
          vectors_do_not_match = true;
#if 0
          ExEnv::out0() << "sparsemap(MOIndexSpace,MOIndexSpace): (mo1,mo2,ao,phase) = "
                        << mo1 << "," << mo2 << "," << ao << "," << phase << "  delta = "
                        << fabs(c1.get_element(mo1,ao)-phase*c2.get_element(mo2,ao)) << std::endl;
#endif
          break;
        }
      }

      // compare vectors assuming opposite phase
      if (vectors_do_not_match) {
	vectors_do_not_match = false;
	phase = -1.0;
	for(unsigned int ao=0; ao<nao; ao++) {
	  if ( fabs(c1.get_element(mo1,ao) - phase*c2.get_element(mo2,ao)) > hardzero ) {
	    vectors_do_not_match = true;
#if 0
	    ExEnv::out0() << "sparsemap(MOIndexSpace,MOIndexSpace): (mo1,mo2,ao,phase) = "
			  << mo1 << "," << mo2 << "," << ao << "," << phase << "  delta = "
			  << fabs(c1.get_element(mo1,ao)-phase*c2.get_element(mo2,ao)) << std::endl;
#endif
	    break;
	  }
	}
      }

      // go to next mo1 if found match
      if (!vectors_do_not_match) {
        found_match = true;
#if 0
        ExEnv::out0() << "sparsemap(MOIndexSpace,MOIndexSpace): found match (mo1,mo2,phase) = "
                      << mo1 << "," << mo2 << "," << phase << std::endl;
#endif
        map[mo1] = make_pair(mo2,phase);
        has_been_mapped[mo2] = 1;
        mo2 = rank2;
      }
    }
    // if this mo1 doesn't match any mo2 -- punt
    if (!found_match)
      throw CannotConstructMap();
  }

  return map;
}

RefSCMatrix
sc::transform(const MOIndexSpace& s2, const MOIndexSpace& s1, const Ref<SCMatrixKit>& kit)
{
  const unsigned int rank1 = s1.rank();
  const unsigned int rank2 = s2.rank();
  // simple tests for whether the transform can be constructed
  if (rank1 < rank2 ||
      s1.integral()->class_desc() != s2.integral()->class_desc())
    throw CannotConstructMap();
  Ref<Integral> integral = s1.integral()->clone();
  const Ref<GaussianBasisSet>& bs1 = s1.basis();
  const Ref<GaussianBasisSet>& bs2 = s2.basis();

  const RefSCMatrix& c1 = s1.coefs().t();
  const RefSCMatrix& c2 = s2.coefs().t();
#if DEBUG_DENSE_TRANSFORM
  c1.print("transform(MOIndexSpace,MOIndexSpace): c1");
  c2.print("transform(MOIndexSpace,MOIndexSpace): c2");
#endif

  RefSCMatrix tform(c2.rowdim(),c1.rowdim(),kit);
  tform.assign(0.0);

  // if objects are the same, the transform is trivial
  if (&s1 == &s2) {
      RefDiagSCMatrix unit(c2.rowdim(),kit);
      unit.assign(1.0);
      tform.accumulate(unit);
      return tform;
  }

  // compute the overlap matrix in AO basis
  RefSCMatrix S21 = overlap(s2,s1,kit);

  // the transform matrix is the inverse of S21
  RefSCMatrix U21 = S21.gi();

#if DEBUG_DENSE_TRANSFORM
  // test
  (S21).print("S21");
  (U21).print("U21");
  (U21 * S21).print("U21 * S21");
  (S21 * U21).print("S21 * U21");
#endif

  return U21;
}

RefSCMatrix
sc::overlap(const MOIndexSpace& space1, const MOIndexSpace& space2,
	    const Ref<SCMatrixKit>& kit)
{
  if (!space1.integral()->equiv(space2.integral()))
    throw ProgrammingError("sc::overlap(s1,s2) -- s1 and s2 are supported by incompatible Integral factories");
  const Ref<GaussianBasisSet> bs1 = space1.basis();
  const Ref<GaussianBasisSet> bs2 = space2.basis();
  const bool bs1_eq_bs2 = (bs1 == bs2);
  int nshell1 = bs1->nshell();
  int nshell2 = bs2->nshell();

  RefSCMatrix vec1t = space1.coefs().t();
  RefSCMatrix vec2 = space2.coefs();

  Ref<Integral> localints = space1.integral()->clone();
  localints->set_basis(bs1,bs2);
  Ref<OneBodyInt> ov_ints = localints->overlap();

  // form AO moment matrices
  RefSCDimension aodim1 = vec1t.coldim();
  RefSCDimension aodim2 = vec2.rowdim();
  RefSCMatrix s(aodim1, aodim2, vec1t.kit());
  s.assign(0.0);

  for(int sh1=0; sh1<nshell1; sh1++) {
    int bf1_offset = bs1->shell_to_function(sh1);
    int nbf1 = bs1->shell(sh1).nfunction();

    int sh2max;
    if (bs1_eq_bs2)
      sh2max = sh1;
    else
      sh2max = nshell2-1;

    for(int sh2=0; sh2<=sh2max; sh2++) {
      int bf2_offset = bs2->shell_to_function(sh2);
      int nbf2 = bs2->shell(sh2).nfunction();

      ov_ints->compute_shell(sh1,sh2);
      const double *ovintsptr = ov_ints->buffer();

      int bf1_index = bf1_offset;
      for(int bf1=0; bf1<nbf1; bf1++, bf1_index++, ovintsptr+=nbf2) {
        int bf2_index = bf2_offset;
        const double *ptr = ovintsptr;
        int bf2max;
        if (bs1_eq_bs2 && sh1 == sh2)
          bf2max = bf1;
        else
          bf2max = nbf2-1;
        for(int bf2=0; bf2<=bf2max; bf2++, bf2_index++) {

          s.set_element(bf1_index, bf2_index, *(ptr++));

        }
      }
    }
  }

  // and clean up a bit
  ov_ints = 0;

  // Symmetrize matrices, if necessary
  if (bs1_eq_bs2) {

    const int nbasis = bs1->nbasis();

    for(int bf1=0; bf1<nbasis; bf1++)
      for(int bf2=0; bf2<=bf1; bf2++) {
        s(bf2,bf1) = s(bf1,bf2);
      }

  }

  // finally, transform
  RefSCMatrix s12 = vec1t * s * vec2;
  // and convert to S (kits may differ!)
  double* ss = new double[s12.rowdim().n() * s12.coldim().n()];
  s12.convert(ss);
  s12 = 0;
  RefSCMatrix S(vec1t.rowdim(), vec2.coldim(), kit);
  S.assign(ss);

  // and clean up a bit
  s = 0;

  return S;
}

bool
sc::in(const MOIndexSpace& s1, const MOIndexSpace& s2)
{
    bool result = true;
    try {
	s2<<s1;
    }
    catch(CannotConstructMap& e) { result = false; }
    return result;
}

/////////////////////////////////////////////////////////////////////////////

const Ref<MOIndexSpaceRegistry>&
MOIndexSpaceRegistry::instance()
{
  return instance_;
}

Ref<MOIndexSpaceRegistry>
MOIndexSpaceRegistry::instance_ = new MOIndexSpaceRegistry;

MOIndexSpaceRegistry::MOIndexSpaceRegistry() :
  lock_(ThreadGrp::get_default_threadgrp()->new_lock())
{
}

Ref<MOIndexSpace>
MOIndexSpaceRegistry::find(const std::string& key) const
{
  // although this does not modify the map, cannot search map while someone else is changing it
  lock_->lock();

  typedef MOIndexSpaceMap::const_iterator citer;
  citer v = space_map_.find(key);
  Ref<MOIndexSpace> result;
  if (v != space_map_.end())
    result = v->second;

  lock_->unlock();
  return result;
}

void
MOIndexSpaceRegistry::add(const Ref<MOIndexSpace>& space)
{
  lock_->lock();
  const std::string key = space->id();

  // check if this key already exists
  typedef MOIndexSpaceMap::const_iterator citer;
  citer v = space_map_.find(key);
  Ref<MOIndexSpace> result;
  if (v != space_map_.end())
    throw ProgrammingError("MOIndexSpaceRegistry::add -- MOIndexSpace with this key already exists",__FILE__,__LINE__);

  space_map_[key] = space;

  lock_->unlock();
}

/////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: c++
// c-file-style: "CLJ-CONDENSED"
// End:
