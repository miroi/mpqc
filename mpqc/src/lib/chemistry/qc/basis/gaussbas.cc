//
// gaussbas.cc
//
// Copyright (C) 1996 Limit Point Systems, Inc.
//
// Author: Curtis Janssen <cljanss@limitpt.com>
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

#include <stdio.h>
#include <stdexcept>

#include <scconfig.h>
#ifdef HAVE_SSTREAM
#  include <sstream>
#else
#  include <strstream.h>
#endif

#include <util/keyval/keyval.h>
#include <util/misc/formio.h>
#include <util/misc/newstring.h>
#include <util/state/stateio.h>
#include <math/scmat/blocked.h>
#include <chemistry/molecule/molecule.h>
#include <chemistry/qc/basis/gaussshell.h>
#include <chemistry/qc/basis/gaussbas.h>
#include <chemistry/qc/basis/files.h>
#include <chemistry/qc/basis/cartiter.h>
#include <chemistry/qc/basis/transform.h>
#include <chemistry/qc/basis/integral.h>

using namespace std;
using namespace sc;

static ClassDesc GaussianBasisSet_cd(
  typeid(GaussianBasisSet),"GaussianBasisSet",3,"public SavableState",
  0, create<GaussianBasisSet>, create<GaussianBasisSet>);

GaussianBasisSet::GaussianBasisSet(const Ref<KeyVal>&topkeyval)
{
  molecule_ << topkeyval->describedclassvalue("molecule");
  if (molecule_.null()) {
      ExEnv::err0() << indent << "GaussianBasisSet: no \"molecule\"\n";
      abort();
    }

  // see if the user requests pure am or cartesian functions
  int pure;
  pure = topkeyval->booleanvalue("puream");
  if (topkeyval->error() != KeyVal::OK) pure = -1;

  // construct a keyval that contains the basis library
  Ref<KeyVal> keyval;

  if (topkeyval->exists("basisfiles")) {
      Ref<MessageGrp> grp = MessageGrp::get_default_messagegrp();
      Ref<ParsedKeyVal> parsedkv = new ParsedKeyVal();
      char *in_char_array;
      if (grp->me() == 0) {
#ifdef HAVE_SSTREAM
          ostringstream ostrs;
#else
          ostrstream ostrs;
#endif
          // Look at the basisdir and basisfiles variables to find out what
          // basis set files are to be read in.  The files are read on node
          // 0 only.
          ParsedKeyVal::cat_files("basis",topkeyval,ostrs);
#ifdef HAVE_SSTREAM
          int n = 1 + strlen(ostrs.str().c_str());
          in_char_array = strcpy(new char[n],ostrs.str().c_str());
#else
          ostrs << ends;
          in_char_array = ostrs.str();
          int n = ostrs.pcount();
#endif
          grp->bcast(n);
          grp->bcast(in_char_array, n);
        }
      else {
          int n;
          grp->bcast(n);
          in_char_array = new char[n];
          grp->bcast(in_char_array, n);
        }
      parsedkv->parse_string(in_char_array);
      delete[] in_char_array;
      Ref<KeyVal> libkeyval = parsedkv.pointer();
      keyval = new AggregateKeyVal(topkeyval,libkeyval);
    }
  else {
      keyval = topkeyval;
    }

  // if there isn't a matrixkit in the input, init2() will assign the
  // default matrixkit
  matrixkit_ << keyval->describedclassvalue("matrixkit");
  
  // Bases keeps track of what basis set data bases have already
  // been read in.  It also handles the conversion of basis
  // names to file names.
  BasisFileSet bases(keyval);
  init(molecule_,keyval,bases,1,pure);
}

GaussianBasisSet::GaussianBasisSet(const GaussianBasisSet& gbs) :
  molecule_(gbs.molecule_),
  matrixkit_(gbs.matrixkit_),
  basisdim_(gbs.basisdim_),
  ncenter_(gbs.ncenter_),
  nshell_(gbs.nshell_)
{
  int i,j;
  
  name_ = new_string(gbs.name_);

  center_to_nshell_.resize(ncenter_);
  for (i=0; i < ncenter_; i++) {
      center_to_nshell_[i] = gbs.center_to_nshell_[i];
    }
  
  shell_ = new GaussianShell*[nshell_];
  for (i=0; i<nshell_; i++) {
      const GaussianShell& gsi = gbs(i);

      int nc=gsi.ncontraction();
      int np=gsi.nprimitive();
      
      int *ams = new int[nc];
      int *pure = new int[nc];
      double *exps = new double[np];
      double **coefs = new double*[nc];

      for (j=0; j < nc; j++) {
          ams[j] = gsi.am(j);
          pure[j] = gsi.is_pure(j);
          coefs[j] = new double[np];
          for (int k=0; k < np; k++)
              coefs[j][k] = gsi.coefficient_unnorm(j,k);
        }

      for (j=0; j < np; j++)
          exps[j] = gsi.exponent(j);
      
      shell_[i] = new GaussianShell(nc, np, exps, ams, pure, coefs,
                                   GaussianShell::Unnormalized);
    }

  init2();
}

GaussianBasisSet::GaussianBasisSet(const char* name,
                                   const Ref<Molecule>& molecule,
                                   const Ref<SCMatrixKit>& matrixkit,
                                   const RefSCDimension& basisdim,
				   const int ncenter,
                                   const int nshell,
                                   GaussianShell** shell,
                                   const std::vector<int>& center_to_nshell) :
  molecule_(molecule),
  matrixkit_(matrixkit),
  basisdim_(basisdim),
  ncenter_(ncenter),
  nshell_(nshell),
  shell_(shell),
  center_to_nshell_(center_to_nshell)
{
  name_ = new_string(name);
  
  init2();
}

Ref<GaussianBasisSet>
GaussianBasisSet::concatenate(const Ref<GaussianBasisSet>& B)
{
  GaussianBasisSet* b = B.pointer();
  if (molecule_.pointer() != b->molecule_.pointer())
    throw std::runtime_error("GaussianBasisSet::concatenate -- cannot concatenate basis sets, molecules are different");

  Ref<Molecule> molecule = molecule_;
  Ref<SCMatrixKit> matrixkit = matrixkit_;
  const int ncenter = ncenter_;
  const int nshell = nshell_ + b->nshell_;
  std::vector<int> center_to_nshell(ncenter);

  GaussianShell** shell = new GaussianShell*[nshell];
  int* func_per_shell = new int[nshell];

  for(int c=0; c<ncenter; c++) {

    int ns1 = center_to_nshell_[c];
    int ns2 = b->center_to_nshell_[c];
    int ns = ns1+ns2;
    int s1off = center_to_shell_[c];
    int s2off = b->center_to_shell_[c];
    int soff = s1off + s2off;
    center_to_nshell[c] = ns;

    for (int i=0; i<ns; i++) {
      const GaussianShell* gsi;
      if (i < ns1)
        gsi = shell_[s1off + i];
      else
        gsi = b->shell_[s2off + i - ns1];

      int nc=gsi->ncontraction();
      int np=gsi->nprimitive();
      func_per_shell[soff + i] = gsi->nfunction();

      int *ams = new int[nc];
      int *pure = new int[nc];
      double *exps = new double[np];
      double **coefs = new double*[nc];
      
      for (int j=0; j < nc; j++) {
        ams[j] = gsi->am(j);
        pure[j] = gsi->is_pure(j);
        coefs[j] = new double[np];
        for (int k=0; k < np; k++)
          coefs[j][k] = gsi->coefficient_unnorm(j,k);
      }
      
      for (int j=0; j < np; j++)
        exps[j] = gsi->exponent(j);
      
      shell[soff + i] = new GaussianShell(nc, np, exps, ams, pure, coefs,
                                          GaussianShell::Unnormalized);
    }
  }

  int nbas = nbasis() + b->nbasis();
  RefSCDimension basisdim
      = new SCDimension(nbas, nshell, func_per_shell, "basis set dimension");

  std::string AplusB_name;
  AplusB_name += "[";
  AplusB_name += name();
  AplusB_name += "]+[";
  AplusB_name += B->name();
  AplusB_name += "]";
  Ref<GaussianBasisSet> AplusB
      = new GaussianBasisSet(AplusB_name.c_str(), molecule,
                             matrixkit, basisdim, ncenter,
                             nshell, shell, center_to_nshell);

  delete[] func_per_shell;

  return AplusB;
}

GaussianBasisSet::GaussianBasisSet(StateIn&s):
  SavableState(s)
{
  matrixkit_ = SCMatrixKit::default_matrixkit();

  if (s.version(::class_desc<GaussianBasisSet>()) < 3) {
      // read the duplicate length saved in older versions
      int junk;
      s.get(junk);
    }
  s.get(center_to_nshell_);

  molecule_ << SavableState::restore_state(s);
  basisdim_ << SavableState::restore_state(s);


  ncenter_ = center_to_nshell_.size();
  s.getstring(name_);

  nshell_ = 0;
  int i;
  for (i=0; i<ncenter_; i++) {
      nshell_ += center_to_nshell_[i];
    }
  
  shell_ = new GaussianShell*[nshell_];
  for (i=0; i<nshell_; i++) {
      shell_[i] = new GaussianShell(s);
    }

  init2();
}

void
GaussianBasisSet::save_data_state(StateOut&s)
{
  s.put(center_to_nshell_);

  SavableState::save_state(molecule_.pointer(),s);
  SavableState::save_state(basisdim_.pointer(),s);
  
  s.putstring(name_);
  for (int i=0; i<nshell_; i++) {
      shell_[i]->save_object_state(s);
    }
}

void
GaussianBasisSet::init(Ref<Molecule>&molecule,
                       Ref<KeyVal>&keyval,
                       BasisFileSet& bases,
                       int have_userkeyval,
                       int pur)
{
  int pure, havepure;
  pure = pur;
  if (pur == -1) {
      havepure = 0;
    }
  else {
      havepure = 1;
    }

  int skip_ghosts = keyval->booleanvalue("skip_ghosts");

  name_ = keyval->pcharvalue("name");
  int have_custom, nelement;

  if (keyval->exists("basis")) {
      have_custom = 1;
      nelement = keyval->count("element");
    }
  else {
      have_custom = 0;
      nelement = 0;
      if (!name_) {
          ExEnv::err0() << indent
               << "GaussianBasisSet: No name given for basis set\n";
          abort();
        }
    }

  // construct prefixes for each atom: :basis:<atom>:<basisname>:<shell#>
  // and read in the shell
  nbasis_ = 0;
  int ishell = 0;
  ncenter_ = molecule->natom();
  int iatom;
  for (iatom=0; iatom < ncenter_; iatom++) {
      if (skip_ghosts && molecule->charge(iatom) == 0.0) continue;
      int Z = molecule->Z(iatom);
      // see if there is a specific basisname for this atom
      char* sbasisname = 0;
      if (have_custom && !nelement) {
          sbasisname = keyval->pcharvalue("basis",iatom);
        }
      else if (nelement) {
          int i;
          for (i=0; i<nelement; i++) {
              char *tmpelementname = keyval->pcharvalue("element", i);
              int tmpZ = AtomInfo::string_to_Z(tmpelementname);
              if (tmpZ == Z) {
                  sbasisname = keyval->pcharvalue("basis", i);
                  break;
                }
              delete[] tmpelementname;
            }
        }
      if (!sbasisname) {
          if (!name_) {
              ExEnv::err0() << indent << "GaussianBasisSet: "
                   << scprintf("no basis name for atom %d (%s)\n",
                               iatom, AtomInfo::name(Z));
              abort();
            }
          sbasisname = strcpy(new char[strlen(name_)+1],name_);
        }
      recursively_get_shell(ishell,keyval,
                            AtomInfo::name(Z),
                            sbasisname,bases,havepure,pure,0);
      delete[] sbasisname;
    }
  nshell_ = ishell;
  shell_ = new GaussianShell*[nshell_];
  ishell = 0;
  center_to_nshell_.resize(ncenter_);
  for (iatom=0; iatom<ncenter_; iatom++) {
      if (skip_ghosts && molecule->charge(iatom) == 0.0) {
          center_to_nshell_[iatom] = 0;
          continue;
        }
      int Z = molecule->Z(iatom);
      // see if there is a specific basisname for this atom
      char* sbasisname = 0;
      if (have_custom && !nelement) {
          sbasisname = keyval->pcharvalue("basis",iatom);
        }
      else if (nelement) {
          int i;
          for (i=0; i<nelement; i++) {
              char *tmpelementname = keyval->pcharvalue("element", i);
              int tmpZ = AtomInfo::string_to_Z(tmpelementname);
              if (tmpZ == Z) {
                  sbasisname = keyval->pcharvalue("basis", i);
                  break;
                }
              delete[] tmpelementname;
            }
        }
      if (!sbasisname) {
          if (!name_) {
              ExEnv::err0() << indent << "GaussianBasisSet: "
                   << scprintf("no basis name for atom %d (%s)\n",
                               iatom, AtomInfo::name(Z));
              abort();
            }
          sbasisname = strcpy(new char[strlen(name_)+1],name_);
        }

      int ishell_old = ishell;
      recursively_get_shell(ishell,keyval,
                            AtomInfo::name(Z),
                            sbasisname,bases,havepure,pure,1);

      center_to_nshell_[iatom] = ishell - ishell_old;

      delete[] sbasisname;
     }

  // delete the name_ if the basis set is customized
  if (have_custom) {
      delete[] name_;
      name_ = 0;
    }

  // finish with the initialization steps that don't require any
  // external information
  init2(skip_ghosts);
}

double
GaussianBasisSet::r(int icenter, int xyz) const
{
  return molecule_->r(icenter,xyz);
}

void
GaussianBasisSet::init2(int skip_ghosts)
{
  // center_to_shell_ and shell_to_center_
  shell_to_center_.resize(nshell_);
  center_to_shell_.resize(ncenter_);
  center_to_nbasis_.resize(ncenter_);
  int ishell = 0;
  for (int icenter=0; icenter<ncenter_; icenter++) {
      if (skip_ghosts && molecule()->charge(icenter) == 0.0) {
          center_to_shell_[icenter] = -1;
          center_to_nbasis_[icenter] = 0;
          continue;
        }
      int j;
      center_to_shell_[icenter] = ishell;
      center_to_nbasis_[icenter] = 0;
      for (j = 0; j<center_to_nshell_[icenter]; j++) {
          center_to_nbasis_[icenter] += shell_[ishell+j]->nfunction();
        }
      ishell += center_to_nshell_[icenter];
      for (j = center_to_shell_[icenter]; j<ishell; j++) {
	  shell_to_center_[j] = icenter;
	}
     }

  // compute nbasis_ and shell_to_function_[]
  shell_to_function_.resize(nshell_);
  shell_to_primitive_.resize(nshell_);
  nbasis_ = 0;
  nprim_ = 0;
  for (ishell=0; ishell<nshell_; ishell++) {
      shell_to_function_[ishell] = nbasis_;
      shell_to_primitive_[ishell] = nprim_;
      nbasis_ += shell_[ishell]->nfunction();
      nprim_ += shell_[ishell]->nprimitive();
    }

  // would like to do this in function_to_shell(), but it is const
  int n = nbasis();
  int nsh = nshell();
  function_to_shell_.resize(n);
  int ifunc = 0;
  for (int i=0; i<nsh; i++) {
      int nfun = operator()(i).nfunction();
      for (int j=0; j<nfun; j++) {
          function_to_shell_[ifunc] = i;
          ifunc++;
        }
    }

  // figure out if any shells are spherical harmonics
  has_pure_ = false;
  for(int i=0; i<nsh; i++)
    has_pure_ = has_pure_ || shell_[i]->has_pure();

  if (matrixkit_.null())
    matrixkit_ = SCMatrixKit::default_matrixkit();

  so_matrixkit_ = new BlockedSCMatrixKit(matrixkit_);
  
  if (basisdim_.null()) {
    int nb = nshell();
    int *bs = new int[nb];
    for (int s=0; s < nb; s++)
      bs[s] = shell(s).nfunction();
    basisdim_ = new SCDimension(nbasis(), nb, bs, "basis set dimension");
    delete[] bs;
  }
}

void
GaussianBasisSet::set_matrixkit(const Ref<SCMatrixKit>& mk)
{
  matrixkit_ = mk;
  so_matrixkit_ = new BlockedSCMatrixKit(matrixkit_);
}

void
GaussianBasisSet::
  recursively_get_shell(int&ishell,Ref<KeyVal>&keyval,
			const char*element,
			const char*basisname,
                        BasisFileSet &bases,
			int havepure,int pure,
			int get)
{
  char keyword[KeyVal::MaxKeywordLength],prefix[KeyVal::MaxKeywordLength];

  sprintf(keyword,":basis:%s:%s",
	  element,basisname);
  int count = keyval->count(keyword);
  if (keyval->error() != KeyVal::OK) {
      keyval = bases.keyval(keyval, basisname);
    }
  count = keyval->count(keyword);
  if (keyval->error() != KeyVal::OK) {
      ExEnv::err0() << indent
           << scprintf("GaussianBasisSet:: couldn't find \"%s\":\n", keyword);
      keyval->errortrace(ExEnv::err0());
      exit(1);
    }
  if (!count) return;
  for (int j=0; j<count; j++) {
      sprintf(prefix,":basis:%s:%s",
	      element,basisname);
      Ref<KeyVal> prefixkeyval = new PrefixKeyVal(keyval,prefix,j);
      if (prefixkeyval->exists("get")) {
          char* newbasis = prefixkeyval->pcharvalue("get");
          if (!newbasis) {
	      ExEnv::err0() << indent << "GaussianBasisSet: "
                   << scprintf("error processing get for \"%s\"\n", prefix);
              keyval->errortrace(ExEnv::err0());
	      exit(1);
	    }
	  recursively_get_shell(ishell,keyval,element,newbasis,bases,
                                havepure,pure,get);
          delete[] newbasis;
	}
      else {
          if (get) {
	      if (havepure) shell_[ishell] = new GaussianShell(prefixkeyval,pure);
	      else shell_[ishell] = new GaussianShell(prefixkeyval);
	    }
	  ishell++;
	}
    }
}

GaussianBasisSet::~GaussianBasisSet()
{
  delete[] name_;

  int ii;
  for (ii=0; ii<nshell_; ii++) {
      delete shell_[ii];
    }
  delete[] shell_;
}

int
GaussianBasisSet::max_nfunction_in_shell() const
{
  int i;
  int max = 0;
  for (i=0; i<nshell_; i++) {
      if (max < shell_[i]->nfunction()) max = shell_[i]->nfunction();
    }
  return max;
}

int
GaussianBasisSet::max_ncontraction() const
{
  int i;
  int max = 0;
  for (i=0; i<nshell_; i++) {
      if (max < shell_[i]->ncontraction()) max = shell_[i]->ncontraction();
    }
  return max;
}

int
GaussianBasisSet::max_angular_momentum() const
{
  int i;
  int max = 0;
  for (i=0; i<nshell_; i++) {
      int maxshi = shell_[i]->max_angular_momentum();
      if (max < maxshi) max = maxshi;
    }
  return max;
}

int
GaussianBasisSet::max_cartesian() const
{
  int i;
  int max = 0;
  for (i=0; i<nshell_; i++) {
      int maxshi = shell_[i]->max_cartesian();
      if (max < maxshi) max = maxshi;
    }
  return max;
}

int
GaussianBasisSet::max_ncartesian_in_shell(int aminc) const
{
  int i;
  int max = 0;
  for (i=0; i<nshell_; i++) {
      int maxshi = shell_[i]->ncartesian_with_aminc(aminc);
      if (max < maxshi) max = maxshi;
    }
  return max;
}

int
GaussianBasisSet::max_nprimitive_in_shell() const
{
  int i;
  int max = 0;
  for (i=0; i<nshell_; i++) {
      if (max < shell_[i]->nprimitive()) max = shell_[i]->nprimitive();
    }
  return max;
}

int
GaussianBasisSet::max_am_for_contraction(int con) const
{
  int i;
  int max = 0;
  for (i=0; i<nshell_; i++) {
      if (shell_[i]->ncontraction() <= con) continue;
      int maxshi = shell_[i]->am(con);
      if (max < maxshi) max = maxshi;
    }
  return max;
}

int
GaussianBasisSet::function_to_shell(int func) const
{
  return function_to_shell_[func];
}

int
GaussianBasisSet::ncenter() const
{
  return ncenter_;
}

int
GaussianBasisSet::nshell_on_center(int icenter) const
{
  return center_to_nshell_[icenter];
}

int
GaussianBasisSet::nbasis_on_center(int icenter) const
{
  return center_to_nbasis_[icenter];
}

int
GaussianBasisSet::shell_on_center(int icenter, int ishell) const
{
  return center_to_shell_[icenter] + ishell;
}

const GaussianShell&
GaussianBasisSet::operator()(int icenter,int ishell) const
{
  return *shell_[center_to_shell_[icenter] + ishell];
}

GaussianShell&
GaussianBasisSet::operator()(int icenter,int ishell)
{
  return *shell_[center_to_shell_[icenter] + ishell];
}

int
GaussianBasisSet::equiv(const Ref<GaussianBasisSet> &b)
{
  if (nshell() != b->nshell()) return 0;
  for (int i=0; i<nshell(); i++) {
      if (!shell_[i]->equiv(b->shell_[i])) return 0;
    }
  return 1;
}

void
GaussianBasisSet::print_brief(ostream& os) const
{
  os << indent
     << "GaussianBasisSet:" << endl << incindent
     << indent << "nbasis = " << nbasis_ << endl
     << indent << "nshell = " << nshell_ << endl
     << indent << "nprim  = " << nprim_ << endl;
  if (name_) {
      os << indent
         << "name = \"" << name_ << "\"" << endl;
    }
  os << decindent;
}

void
GaussianBasisSet::print(ostream& os) const
{
  print_brief(os);
  if (!SCFormIO::getverbose(os)) return;

  os << incindent;

  // Loop over centers
  int icenter = 0;
  int ioshell = 0;
  for (icenter=0; icenter < ncenter_; icenter++) {
      os << endl << indent
         << scprintf(
             "center %d: %12.8f %12.8f %12.8f, nshell = %d, shellnum = %d\n",
             icenter,
             r(icenter,0),
             r(icenter,1),
             r(icenter,2),
             center_to_nshell_[icenter],
             center_to_shell_[icenter]);
      for (int ishell=0; ishell < center_to_nshell_[icenter]; ishell++) {
	  os << indent
             << scprintf("Shell %d: functionnum = %d, primnum = %d\n",
                         ishell,shell_to_function_[ioshell],shell_to_primitive_[ioshell]);
          os << incindent;
	  operator()(icenter,ishell).print(os);
          os << decindent;
          ioshell++;
	}
    }

  os << decindent;
}

/////////////////////////////////////////////////////////////////////////////
// GaussianBasisSet::ValueData

GaussianBasisSet::ValueData::ValueData(
    const Ref<GaussianBasisSet> &basis,
    const Ref<Integral> &integral)
{
  maxam_ = basis->max_angular_momentum();

  civec_ = new CartesianIter *[maxam_+1];
  sivec_ = new SphericalTransformIter *[maxam_+1];
  for (int i=0; i<=maxam_; i++) {
      civec_[i] = integral->new_cartesian_iter(i);
      if (i>1) sivec_[i] = integral->new_spherical_transform_iter(i);
      else sivec_[i] = 0;
    }
}

GaussianBasisSet::ValueData::~ValueData()
{
  for (int i=0; i<=maxam_; i++) {
      delete civec_[i];
      delete sivec_[i];
    }
  delete[] civec_;
  delete[] sivec_;
}

/////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: c++
// c-file-style: "CLJ"
// End:
