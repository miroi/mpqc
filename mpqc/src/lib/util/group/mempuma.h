//
// mempuma.h
//
// Copyright (C) 1996 Limit Point Systems, Inc.
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
#pragma interface
#endif

#ifndef _util_group_mempuma_h
#define _util_group_mempuma_h

#include <util/group/memamsg.h>

#include <mpi.h>

/** The PumaMemoryGrp concrete class specializes the ActiveMsgMemoryGrp
class for the Intel Paragon running the Puma OS.  This implementation
is buggy.  */
class PumaMemoryGrp: public ActiveMsgMemoryGrp {
#define CLASSNAME PumaMemoryGrp
#define HAVE_KEYVAL_CTOR
#include <util/class/classd.h>
  private:
    MPI_Comm rma_comm_;
    
    void retrieve_data(void *, int node, int offset, int size);
    void replace_data(void *, int node, int offset, int size);
    void sum_data(double *data, int node, int doffset, int dsize);

  public:
    PumaMemoryGrp(const RefMessageGrp& msg);
    PumaMemoryGrp(const RefKeyVal&);
    ~PumaMemoryGrp();

    void set_localsize(int);
};

#endif

// Local Variables:
// mode: c++
// c-file-style: "ETS"
// End:
