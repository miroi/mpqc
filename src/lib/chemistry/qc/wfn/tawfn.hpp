//
// tawfn.hpp
//
// Copyright (C) 2013 Drew Lewis
//
// Authors: Drew Lewis
// Maintainer: Drew Lewis and Edward Valeev
//
// This file is part of the MPQC Toolkit.
//
// The MPQC Toolkit is free software; you can redistribute it and/or modify
// it under the terms of the GNU Library General Public License as published by
// the Free Software Foundation; either version 2, or (at your option)
// any later version.
//
// The MPQC Toolkit is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public License
// along with the MPQC Toolkit; see the file COPYING.LIB.  If not, write to
// the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
//
// The U.S. Government is granted a limited license as per AL 91-7.
//

#ifndef _MPQC_CHEMISTRY_WFN_TAWFN_HPP_
#define _MPQC_CHEMISTRY_WFN_TAWFN_HPP_

#include <tiledarray_fwd.h>
#include <util/madness/world.h>
#include <chemistry/qc/basis/integral.h>
#include <chemistry/qc/basis/tiledbasisset.hpp>
#include <chemistry/molecule/energy.h>
#include <chemistry/qc/wfn/spin.h>

// will happen: namespace sc -> namespace mpqc

namespace mpqc {
  namespace TA {

    /// @addtogroup TAWFN
    /// @{
    
    /** Wavefunction represents an electronic wave function expressed in terms of
     *  a basis set of atomic orbitals.
     */
    class Wavefunction: public sc::MolecularEnergy {

    public:

      typedef TiledArray::TArray1D TAVector; //!< Vector of reals
      typedef TiledArray::TArray2D TAMatrix; //!< Matrix of reals
      typedef sc::Result<TAVector> ResultVector;
      typedef sc::Result<TAMatrix> ResultMatrix;
      typedef sc::AccResult<TAVector> AccResultVector;
      typedef sc::AccResult<TAMatrix> AccResultMatrix;
      typedef TiledArray::TensorD TATensor;


      /** The KeyVal constructor.
       *
       */
      Wavefunction(const sc::Ref<sc::KeyVal> &kval);
      //Wavefunction(sc::StateIn &s);
      virtual ~Wavefunction();
      //void save_data_state(sc::StateOut &s);

      /// @return the basis set
      const sc::Ref<TiledBasisSet>& basis() const {
        return tbs_;
      }
      /// @return the Integral factory used to make the basis set object "concrete"
      const sc::Ref<sc::Integral>& integral() const {
        return integral_;
      }

      /// @return the Molecule object
      sc::Ref<sc::Molecule> molecule() const override {
        return tbs_->molecule();
      }

      /// @return the total charge of system
      double total_charge() const{
        return molecule()->total_charge() - nelectron();
      }

      /// @return the number of electrons in the system
      virtual size_t nelectron() const = 0;

      /** Computes the S (or J) magnetic moment
       * of the target state(s), in units of \f$ \hbar/2 \f$.
       * Can be evaluated from density and overlap, as;
       * \code
       *  (this->alpha_density() * this-> overlap()).trace() -
       *  (this->beta_density() * this-> overlap()).trace()
       * \endcode
       * but derived Wavefunction may have this value as user input.
       * @return the magnetic moment
       * */
      virtual double magnetic_moment() const;
      /// @return true if the magnetic moment != 0
      bool spin_polarized() {
        return magnetic_moment() != 0.0;
      }

      /// Returns electron 1-body reduced density matrix (1-RDM) in AO basis.
      virtual const TAMatrix& rdm1() = 0;

      /** Returns expression to the AO density matrix.
       * If the matrix has not been computed, then it will be computed by the
       * calling class.
       * */
      //virtual TAMatrixExpr rdm1_expr(std::string);

      /// Return electron 1-body reduced density matrix of spin \c s in AO basis.
      virtual const TAMatrix& rdm1(sc::SpinCase1 s) = 0;
      /// Returns the AO overlap.
      virtual const TAMatrix& ao_overlap();

      /** Returns expression to the AO overlap matrix.
       * If the matrix has not been computed, then it will be computed by the
       * calling class.
       * */
      //virtual TAMatrixExpr ao_overlap_expr(std::string);

      /// Returns the AO overlap.
      virtual const TAMatrix& ao_hcore();

      /** Returns expression to the AO hcore matrix.
       * If the matrix has not been computed, then it will be computed by the
       * calling class.
       * */
      //virtual TAMatrixExpr ao_hcore_expr(std::string);

      /// Returns debugging flag
      unsigned debug() const {
        return debug_;
      }


      /// makes this object obsolete, next call to compute() will recompute
      void obsolete();

      void print(std::ostream& os = sc::ExEnv::out0()) const;

      // functions for internal access
    protected:

      const sc::Ref<mpqc::World> world() const {
        return world_;
      }

      /// Returns reference to rdm1_.result_noupdate(),
      /// but guarantees nothing about its computed status
      virtual TAMatrix& ao_density(){
        return rdm1_.result_noupdate();
      }

      ResultMatrix rdm1_; // Result density abreviated D
      ResultMatrix rdm1_alpha_; // Result alpha density abreviated Da
      ResultMatrix rdm1_beta_; // Result beta density abreviated Db

    private:

      sc::Ref<mpqc::World> world_;
      sc::Ref<TiledBasisSet> tbs_;
      sc::Ref<sc::Integral> integral_;
      ResultMatrix overlap_; // Overlap Matrix abreviated S
      ResultMatrix hcore_; // Hcore Matrix abreviated H

      mutable double magnetic_moment_; //!< caches the value returned by magnetic_moment()
      /// Wavefunction (reluctantly) supports calculations in finite electric fields in c1 symmetry
      /// general support is coming in the future.
      bool nonzero_efield_supported() const;

      unsigned debug_;

    private:
      static sc::ClassDesc class_desc_;

    };

/// @}

  }// namespace mpqc::TA
} // namespace mpqc

#endif /* CHEMISTRY_WFN_TAWFN_HPP */
