//
// atominfo.h
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

#ifndef _chemistry_molecule_atominfo_h
#define _chemistry_molecule_atominfo_h

#include <util/class/class.h>
#include <util/keyval/keyval.h>

SavableState_REF_fwddec(Units);

class AtomInfo: public SavableState {
#   define CLASSNAME AtomInfo
#   define HAVE_KEYVAL_CTOR
#   define HAVE_STATEIN_CTOR
#   include <util/state/stated.h>
#   include <util/class/classd.h>
  private:
    enum { MaxZ = 107+1 };

    struct atomname
    {
      char *name;
      char *symbol;
    };

    static struct atomname names_[MaxZ];
    double  mass_[MaxZ];
    double  atomic_radius_[MaxZ];
    double  vdw_radius_[MaxZ];
    double  bragg_radius_[MaxZ];
    double  rgb_[MaxZ][3];

    char *overridden_values_;

    void load_library_values();
    void override_library_values(const RefKeyVal &keyval);
    void load_values(const RefKeyVal& keyval, int override);
    void load_values(double *array, const char *keyword,
                     const RefKeyVal &keyval, int override,
                     const RefUnits &);
    void load_values(double array[][3], const char *keyword,
                     const RefKeyVal &keyval, int override);
    void add_overridden_value(const char *assignment);
  public:
    AtomInfo();
    AtomInfo(const RefKeyVal&);
    AtomInfo(StateIn&);
    ~AtomInfo();
    void save_data_state(StateOut& s);

    // (vdw_radius used to be the radius member)
    double vdw_radius(int Z) const { return vdw_radius_[Z]; }
    double bragg_radius(int Z) const { return bragg_radius_[Z]; }
    double atomic_radius(int Z) const { return atomic_radius_[Z]; }

    double rgb(int Z, int color) const { return rgb_[Z][color]; }
    double red(int Z) const { return rgb_[Z][0]; }
    double green(int Z) const { return rgb_[Z][1]; }
    double blue(int Z) const { return rgb_[Z][2]; }

    double mass(int Z) const { return mass_[Z]; }

    static const char *name(int Z) { return names_[Z].name; }
    static const char *symbol(int Z) { return names_[Z].symbol; }

    static int string_to_Z(const char *);
};
SavableState_REF_dec(AtomInfo);

#endif

// Local Variables:
// mode: c++
// c-file-style: "CLJ"
// End:
