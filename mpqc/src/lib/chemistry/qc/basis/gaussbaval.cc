//
// gaussbaval.cc
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

#include <stdlib.h>
#include <math.h>

#include <util/misc/formio.h>
#include <util/keyval/keyval.h>

#include <math/topology/point.h>
#include <chemistry/qc/basis/gaussbas.h>
#include <chemistry/qc/basis/gaussshell.h>

int
GaussianBasisSet::values(const SCVector3& r, double* basis_values) const
{
  return hessian_values(r, 0, 0, basis_values);
}

int
GaussianBasisSet::grad_values(const SCVector3& r,
                              double* g_values,
                              double* basis_values) const
{
  return hessian_values(r, 0, g_values, basis_values);
}

int
GaussianBasisSet::hessian_values(const SCVector3& r,
                                 double* h_values,
                                 double* g_values,
                                 double* basis_values) const
{
    if (civec_ == 0) {
        cerr << "GaussianBasisSet::grad_values called but set_integral not"
             << endl;
        abort();
      }

    SCVector3 r_diff;
    int ishell = 0;
    int ibasis = 0;
    int nreturns;

    // for convenience
    const GaussianBasisSet& gbs = *this;

    double *b_values_i = 0;
    double *h_values_i = 0;
    double *g_values_i = 0;

    // calculate the value of each basis
    for (int icenter=0; icenter < ncenter_; icenter++) 
      {
        int nshell = center_to_nshell_[icenter];

	// Calculate r_diff
	r_diff.x()=r.x()-GaussianBasisSet::r(icenter,0);
	r_diff.y()=r.y()-GaussianBasisSet::r(icenter,1);
	r_diff.z()=r.z()-GaussianBasisSet::r(icenter,2);

#ifdef EXTRA_PRINT
        static int iflag=0;
	if (iflag)
	{
	    iflag--;
	    cout << node0 << indent
                 << scprintf("Center %d, (%lf,%lf,%lf)\n",
                             icenter,r_center(center,0),
                             r_center(center,1),r_center(center,2));
	}
#endif
	for (int ish=0; ish < nshell; ish++) {
            if (basis_values) b_values_i = &basis_values[ibasis];
            if (g_values)     g_values_i = &g_values[3*ibasis];
            if (h_values)     h_values_i = &h_values[6*ibasis];
            nreturns=gbs(ishell).hessian_values(civec_, sivec_, r_diff,
                                                h_values_i,
                                                g_values_i,
                                                b_values_i);
            ibasis += nreturns;
	    ishell++;
	}
    }

    return ibasis;
}

int
GaussianBasisSet::shell_values(const SCVector3& r,
                               int sh, double* basis_values) const
{
  return hessian_shell_values(r, sh, 0, 0, basis_values);
}

int
GaussianBasisSet::grad_shell_values(const SCVector3& r, int sh,
                              double* g_values,
                              double* basis_values) const
{
  return hessian_shell_values(r, sh, 0, g_values, basis_values);
}

int
GaussianBasisSet::hessian_shell_values(const SCVector3& r, int sh,
                                       double* h_values,
                                       double* g_values,
                                       double* basis_values) const
{
    if (civec_ == 0) {
        cout << "GaussianBasisSet::hessian_shell_values"
             << " called but set_integral not"
             << endl;
        abort();
      }

    int icenter = shell_to_center(sh);

    SCVector3 r_diff;
    for (int i=0; i<3; i++) r_diff[i] = r[i] - GaussianBasisSet::r(icenter,i);

    return operator()(sh).hessian_values(civec_, sivec_, r_diff,
                                         h_values,
                                         g_values,
                                         basis_values);
}

/////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: c++
// c-file-style: "CLJ"
// End:
