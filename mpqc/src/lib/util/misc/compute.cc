//
// compute.cc
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

#include <float.h>
#include <util/misc/compute.h>
#include <util/state/state.h>

#ifdef __GNUC__
template class Result<int>;
template class Result<double>;
template class NCAccResult<double>;
#endif

ARRAY_def(ResultInfoP);
SET_def(ResultInfoP);

Compute::Compute()
{
}

Compute::~Compute()
{
}

void
Compute::add(ResultInfo*r)
{
  _results.add(r);
}

void
Compute::obsolete()
{
  // go thru all of the results and mark them as obsolete
  for (Pix i = _results.first(); i; _results.next(i)) {
      _results(i)->computed() = 0;
    }
}

////////////////////////////////////////////////////////////////////////

ResultInfo::ResultInfo(Compute*c):
  _compute(0),_computed(0),_c(c)
{
  c->add(this);
}

void
ResultInfo::update() {
  if (!computed()) {
      int oldcompute = compute(1);
      _c->compute();
      compute() = oldcompute;
      if (!computed()) {
          cerr << "Result::compute: nothing was computed" << endl;
          abort();
        }
    }
}

ResultInfo::~ResultInfo()
{
}

ResultInfo::ResultInfo(StateIn&s,Compute*c):
  _c(c)
{
  s.get(_compute);
  s.get(_computed);

  c->add(this);
}

ResultInfo::ResultInfo(const ResultInfo&r, Compute*c) :
  _c(c)
{
  _compute=r._compute;
  _computed=r._computed;
  
  c->add(this);
}

void
ResultInfo::restore_state(StateIn&s)
{
  s.get(_compute);
  s.get(_computed);
}

void
ResultInfo::save_data_state(StateOut&s)
{
  s.put(_compute);
  s.put(_computed);
}

ResultInfo&
ResultInfo::operator=(const ResultInfo&r)
{
  _compute=r._compute;
  _computed=r._computed;
  return *this;
}

/////////////////////////////////////////////////////////////////////////

AccResultInfo::AccResultInfo(Compute*c):
  ResultInfo(c),
  _actual_accuracy(0.0),
  _desired_accuracy(0.01)
{
}

AccResultInfo::~AccResultInfo()
{
}

double
AccResultInfo::actual_accuracy() const
{
  return _actual_accuracy;
}

double
AccResultInfo::desired_accuracy() const
{
  return _desired_accuracy;
}

void
AccResultInfo::set_desired_accuracy(double a)
{
  _desired_accuracy = a;
  if (_desired_accuracy < _actual_accuracy &&
      (fabs(_actual_accuracy)-fabs(_desired_accuracy)) > DBL_EPSILON) {
      computed() = 0;
    }
}

void
AccResultInfo::set_actual_accuracy(double a)
{
  _actual_accuracy = a;
  if (_desired_accuracy < _actual_accuracy) {
      cerr << "AccResult: setting actual accuracy greater than "
           << "desired accuracy" << endl;
      abort();
    }
  computed() = 1;
}

AccResultInfo::AccResultInfo(StateIn&s,Compute*c):
  ResultInfo(s,c)
{
  s.get(_actual_accuracy);
  s.get(_desired_accuracy);
}

AccResultInfo::AccResultInfo(const AccResultInfo&a, Compute*c) :
  ResultInfo(a,c)
{
  _actual_accuracy=a._actual_accuracy;
  _desired_accuracy=a._desired_accuracy;
}

void
AccResultInfo::restore_state(StateIn&s)
{
  ResultInfo::restore_state(s);
  s.get(_actual_accuracy);
  s.get(_desired_accuracy);
}

void
AccResultInfo::save_data_state(StateOut&s)
{
  ResultInfo::save_data_state(s);
  s.put(_actual_accuracy);
  s.put(_desired_accuracy);
}

AccResultInfo&
AccResultInfo::operator=(const AccResultInfo&a)
{
  ResultInfo::operator=(a);
  _actual_accuracy=a._actual_accuracy;
  _desired_accuracy=a._desired_accuracy;
  return *this;
}

/////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: c++
// c-file-style: "CLJ"
// End:
