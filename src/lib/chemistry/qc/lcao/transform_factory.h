//
// transform_factory.h
//
// Copyright (C) 2004 Edward Valeev
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

#ifndef _chemistry_qc_lcao_transformfactory_h
#define _chemistry_qc_lcao_transformfactory_h

#include <string>
#include <util/ref/ref.h>
#include <util/group/memory.h>
#include <chemistry/molecule/energy.h>
#include <chemistry/qc/basis/integral.h>
#include <chemistry/qc/basis/intdescr.h>
#include <chemistry/qc/wfn/orbitalspace.h>
#include <chemistry/qc/lcao/transform.h>

namespace sc {

  /** Provides hints to the constructors of a Transform class that help configure
      its implementation.

      The default is to assume that data will not be needed persistently.
   */
  class CreateTransformHints {
    public:
      CreateTransformHints();
      CreateTransformHints(const CreateTransformHints&);
      CreateTransformHints& operator=(const CreateTransformHints& other);

      /// will this transform's data need to be used multiple times?
      bool data_persistent() const
      {
          return data_persistent_;
      }
      void data_persistent(bool data_persistent_)
      {
          this->data_persistent_ = data_persistent_;
      }

    private:
      bool data_persistent_;
  };

class TwoBodyMOIntsTransform;
class TwoBodyThreeCenterMOIntsTransform;
class DensityFittingInfo;
namespace detail {
  template <bool DF> struct MakeTwoBodyTransform;
};

  /** MOIntsTransformFactory is a factory that produces MOIntsTransform objects. */

class MOIntsTransformFactory : virtual public SavableState {

  public:
    typedef MOIntsTransform::StorageType StorageType;

private:

  /// The default integral descriptor will compute ERIs
  typedef TwoBodyIntDescrERI DefaultTwoBodyIntDescr;

  Ref<MolecularEnergy> top_mole_;   // Top-level molecular energy to enable checkpointing

  Ref<Integral> integral_;
  Ref<MessageGrp> msg_;
  Ref<MemoryGrp> mem_;
  Ref<ThreadGrp> thr_;

  Ref<TwoBodyIntDescr> tbintdescr_;

  Ref<OrbitalSpaceRegistry> oreg_;
  Ref<AOSpaceRegistry> aoreg_;
  Ref<OrbitalSpace> space1_;
  Ref<OrbitalSpace> space2_;
  Ref<OrbitalSpace> space3_;
  Ref<OrbitalSpace> space4_;
  DensityFittingInfo* df_info_;

  CreateTransformHints hints_;
  bool dynamic_;
  double print_percent_;
  int debug_;
  MOIntsTransform::StoreMethod::type ints_method_;
  std::string file_prefix_;
  double log2_precision_; //< numerical precision of the integrals

  template <typename TransformType> Ref<TwoBodyMOIntsTransform>
    twobody_transform(const std::string& name,
                      const Ref<TwoBodyIntDescr>& descrarg);
  template <typename TransformType> Ref<TwoBodyThreeCenterMOIntsTransform>
    twobody_transform(const std::string& name,
                      const Ref<TwoBodyThreeCenterIntDescr>& descrarg);

  // this class helps to form correct transform constructor calls
  template <bool DF> friend struct sc::detail::MakeTwoBodyTransform;

public:

  MOIntsTransformFactory(StateIn&);
  MOIntsTransformFactory(const Ref<Integral>& integral);
  ~MOIntsTransformFactory();

  void save_data_state(StateOut&);

  /// obsolete this object
  void obsolete();

  /// Sets the orbital spaces
  void set_spaces(const Ref<OrbitalSpace>& space1, const Ref<OrbitalSpace>& space2 = 0,
                  const Ref<OrbitalSpace>& space3 = 0, const Ref<OrbitalSpace>& space4 = 0);
  /// provides the DensityFittingInfo object
  const DensityFittingInfo* df_info() const { return const_cast<const DensityFittingInfo*>(df_info_); }
  void df_info(const DensityFittingInfo* i) { df_info_ = const_cast<DensityFittingInfo*>(i); }

  /// Specifies the top-level MolecularEnergy object to use for checkpointing
  void set_top_mole(const Ref<MolecularEnergy>& top_mole) { top_mole_ = top_mole; }
  /// Changes the default TwoBodyIntDescr used to produce integrals
  void tbintdescr(const Ref<TwoBodyIntDescr>& descr) { tbintdescr_ = descr; }
  /// Sets the method of storing transformed MO integrals. Default method is mem_posix.
  void set_ints_method(const MOIntsTransform::StoreMethod::type method) { ints_method_ = method; }
  /// Sets the name of the file to hold the integrals.
  void set_file_prefix(const std::string& prefix) { file_prefix_ = prefix; }
  void set_debug(int debug);
  void set_print_percent(double print_percent);
  void set_dynamic(bool dynamic) { dynamic_ = dynamic; }
  /**
   * @param prec \f$ \log_2(\epsilon) \f$, where \f$ \epsilon \f$ is the absolute numerical precision of the integrals
   *        requested from the produced MOIntsTransform objects. The default is -50 ( \f$ 2^{-50} \approx 10^{-15} \f$ ).
   */
  void set_log2_precision(double prec) { log2_precision_ = prec; }

  /// Returns the MemoryGrp object
  Ref<MemoryGrp> mem() const { return mem_; }
  /// Returns the MessageGrp object
  Ref<MessageGrp> msg() const { return msg_; }
  /// Returns the Integral factory
  const Ref<Integral>& integral() const { return integral_; }
  /// returns the OrbitalSpaceRegistry
  const Ref<OrbitalSpaceRegistry>& orbital_registry() const { return oreg_; }
  /// returns the AOSpaceRegistry
  const Ref<AOSpaceRegistry>& ao_registry() const { return aoreg_; }
  /// Returns the default TwoBodyIntDescr used to produce integrals
  Ref<TwoBodyIntDescr> tbintdescr() const { return tbintdescr_; }
  /// Returns the method of storing transformed MO integrals.
  MOIntsTransform::StoreMethod::type ints_method() const { return ints_method_; }
  const CreateTransformHints& hints() const { return hints_; }
  CreateTransformHints& hints() { return hints_; }
  /// Sets the name of the file to hold the integrals.
  std::string file_prefix() const { return file_prefix_; }
  int debug() const { return debug_; }
  double print_percent() const { return print_percent_; }
  bool dynamic() const { return dynamic_; }
  /**
   * @return prec \f$ \log_2(\epsilon) \f$, where \f$ \epsilon \f$ is the abolute numerical precision of the integrals
   *         requested from the produced MOIntsTransform objects
   */
  double log2_precision() const { return log2_precision_; }

  /// Returns OrbitalSpace object 1
  Ref<OrbitalSpace> space1() const;
  /// Returns OrbitalSpace object 2
  Ref<OrbitalSpace> space2() const;
  /// Returns OrbitalSpace object 3
  Ref<OrbitalSpace> space3() const;
  /// Returns OrbitalSpace object 4
  Ref<OrbitalSpace> space4() const;

  /** Creates an TwoBodyMOIntsTransform object that will compute (pq|rs) integrals
      stored in qs blocks for each pr */
  Ref<TwoBodyMOIntsTransform>
  twobody_transform_13(const std::string& id, const Ref<TwoBodyIntDescr>& descr = 0);

  /** Creates an TwoBodyMOIntsTransform object that will compute (pq|rs) integrals
    stored in rs blocks for each pq */
  Ref<TwoBodyMOIntsTransform>
  twobody_transform_12(const std::string& id, const Ref<TwoBodyIntDescr>& descr = 0);

#if 1
  /** Creates an TwoBodyMOIntsTransform object that will compute (pq|rs) integrals
    stored according to storage */
  Ref<TwoBodyMOIntsTransform>
  twobody_transform(StorageType storage, const std::string& id,
                    const Ref<TwoBodyIntDescr>& descr = 0);
#endif

  /// Creates an TwoBodyMOIntsTransform object of type T
  Ref<TwoBodyMOIntsTransform> twobody_transform(MOIntsTransform::TwoBodyTransformType T,
                                                const std::string& name,
                                                const Ref<TwoBodyIntDescr>& descrarg);
  /// Creates an TwoBodyThreeCenterMOIntsTransform object of type T
  Ref<TwoBodyThreeCenterMOIntsTransform> twobody_transform(MOIntsTransform::TwoBodyTransformType T,
                                                const std::string& name,
                                                const Ref<TwoBodyThreeCenterIntDescr>& descrarg);

};

}

#include <chemistry/qc/lcao/transform_tbint.h>

#endif

// Local Variables:
// mode: c++
// c-file-style: "CLJ"
// End:


