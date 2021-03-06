//
// fjt.cc
//
// Copyright (C) 2001 Edward Valeev
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

#include <cmath>
#include <iostream>
#include <util/misc/math.h>
#include <util/misc/formio.h>
#include <util/misc/exenv.h>
#include <util/misc/consumableresources.h>
#include <chemistry/qc/basis/fjt.h>

using namespace sc;
using namespace std;

#define SOFT_ZERO 1e-6

namespace {
#include "static.h"
};

Fjt::Fjt() {}
Fjt::~Fjt() {}

double Taylor_Fjt::relative_zero_(1e-15);

/*------------------------------------------------------
  Initialize Taylor_Fm_Eval object (computes incomplete
  gamma function via Taylor interpolation)
 ------------------------------------------------------*/
Taylor_Fjt::Taylor_Fjt(unsigned int mmax, double accuracy) :
    cutoff_(accuracy), interp_order_(TAYLOR_INTERPOLATION_ORDER),
    ExpMath_(interp_order_+1,2*(mmax + interp_order_ - 1)),
    F_(allocate<double>(mmax+1))
{
    const double sqrt_pi = std::sqrt(M_PI);

  /*---------------------------------------
    We are doing Taylor interpolation with
    n=TAYLOR_ORDER terms here:
    error <= delT^n/(n+1)!
   ---------------------------------------*/
  delT_ = 2.0*std::pow(cutoff_*ExpMath_.fac[interp_order_+1],
		       1.0/interp_order_);
  oodelT_ = 1.0/delT_;
  max_m_ = mmax + interp_order_ - 1;

  T_crit_ = allocate<double>(max_m_ + 1);   /*--- m=0 is included! ---*/
  max_T_ = 0;
  /*--- Figure out T_crit for each m and put into the T_crit ---*/
  for(int m=max_m_; m>=0; --m) {
    /*------------------------------------------
      Damped Newton-Raphson method to solve
      T^{m-0.5}*exp(-T) = epsilon*Gamma(m+0.5)
      The solution is the max T for which to do
      the interpolation
     ------------------------------------------*/
    double T = -log(cutoff_);
    const double egamma = cutoff_ * sqrt_pi * ExpMath_.df[2*m]/std::pow(2.0,m);
    double T_new = T;
    double func;
    do {
      const double damping_factor = 0.2;
      T = T_new;
      /* f(T) = the difference between LHS and RHS of the equation above */
      func = std::pow(T,m-0.5) * std::exp(-T) - egamma;
      const double dfuncdT = ((m-0.5) * std::pow(T,m-1.5) - std::pow(T,m-0.5)) * std::exp(-T);
      /* f(T) has 2 roots and has a maximum in between. If f'(T) > 0 we are to the left of the hump. Make a big step to the right. */
      if (dfuncdT > 0.0) {
	T_new *= 2.0;
      }
      else {
	/* damp the step */
	double deltaT = -func/dfuncdT;
	const double sign_deltaT = (deltaT > 0.0) ? 1.0 : -1.0;
	const double max_deltaT = damping_factor * T;
	if (std::fabs(deltaT) > max_deltaT)
	  deltaT = sign_deltaT * max_deltaT;
	T_new = T + deltaT;
      }
      if ( T_new <= 0.0 ) {
	T_new = T / 2.0;
      }
    } while (std::fabs(func/egamma) >= SOFT_ZERO);
    T_crit_[m] = T_new;
    const int T_idx = (int)std::floor(T_new/delT_);
    max_T_ = std::max(max_T_,T_idx);
  }

  // allocate the grid (see the comments below)
  {
      const int nrow = max_T_+1;
      const int ncol = max_m_+1;
      grid_ = allocate<double*>(nrow);
      grid_[0] = allocate<double>(nrow*ncol);
      for(int r=1; r<nrow; ++r)
	  grid_[r] = grid_[r-1] + ncol;
  }

  /*-------------------------------------------------------
    Tabulate the gamma function from t=delT to T_crit[m]:
    1) include T=0 though the table is empty for T=0 since
       Fm(0) is simple to compute
    2) modified MacLaurin series converges fastest for
       the largest m -> use it to compute Fmmax(T)
       see JPC 94, 5564 (1990).
    3) then either use the series to compute the rest
       of the row or maybe use downward recursion
   -------------------------------------------------------*/
  /*--- do the mmax first ---*/
  const double cutoff_o_10 = 0.1 * cutoff_;
  for(int m=0; m<=max_m_; ++m) {
      for(int T_idx = max_T_;
	  T_idx >= 0;
	  --T_idx) {
	  const double T = T_idx * delT_;
	  double denom = (m+0.5);
	  double term = 0.5*std::exp(-T)/denom;
	  double sum = term;
	  double rel_error;
	  double epsilon;
	  do {
	      denom += 1.0;
	      term *= T/denom;
	      sum += term;
	      rel_error = term/sum;
	      // stop if adding a term smaller or equal to cutoff_/10 and smaller than relative_zero * sum
	      // When sum is small in absolute value, the second threshold is more important
	      epsilon = std::min(cutoff_o_10, sum*relative_zero_);
	  } while (term >epsilon);

	  grid_[T_idx][m] = sum;
      }
  }
}

Taylor_Fjt::~Taylor_Fjt()
{
  deallocate(F_);
  deallocate(T_crit_);
  deallocate(grid_[0]);
  deallocate(grid_);
}

/* Using the tabulated incomplete gamma function in gtable, compute
 * the incomplete gamma function for a particular wval for all 0<=j<=J.
 * The result is placed in the global intermediate int_fjttable.
 */
double *
Taylor_Fjt::values(int l, double T)
{
  static const double sqrt_pio2 = std::sqrt(M_PI/2);
  const double two_T = 2.0*T;

  // since Tcrit grows with l, this condition only needs to be determined once
  const bool T_gt_Tcrit = T > T_crit_[l];
  // start recursion at j=jrecur
  const int jrecur = TAYLOR_INTERPOLATION_AND_RECURSION ? l : 0;
  /*-------------------------------------
     Compute Fj(T) from l down to jrecur
   -------------------------------------*/
  if (T_gt_Tcrit) {
     double pow_two_T_to_minusjp05 = std::pow(two_T,-l-0.5);
     for(int j=l; j>=jrecur; --j) {
         /*--- Asymptotic formula ---*/
          F_[j] = ExpMath_.df[2*j] * sqrt_pio2 * pow_two_T_to_minusjp05;
          pow_two_T_to_minusjp05 *= two_T;
     }
  }
  else {
      const int T_ind = (int)std::floor(0.5+T*oodelT_);
      const double h = T_ind * delT_ - T;
      const double* F_row = grid_[T_ind] + l;

      for(int j=l; j>=jrecur; --j, --F_row) {

	  /*--- Taylor interpolation ---*/
	  F_[j] =          F_row[0]
#if TAYLOR_INTERPOLATION_ORDER > 0
	    +       h*(F_row[1]
#endif
#if TAYLOR_INTERPOLATION_ORDER > 1
	    +oon[2]*h*(F_row[2]
#endif
#if TAYLOR_INTERPOLATION_ORDER > 2
	    +oon[3]*h*(F_row[3]
#endif
#if TAYLOR_INTERPOLATION_ORDER > 3
	    +oon[4]*h*(F_row[4]
#endif
#if TAYLOR_INTERPOLATION_ORDER > 4
	    +oon[5]*h*(F_row[5]
#endif
#if TAYLOR_INTERPOLATION_ORDER > 5
	    +oon[6]*h*(F_row[6]
#endif
#if TAYLOR_INTERPOLATION_ORDER > 6
	    +oon[7]*h*(F_row[7]
#endif
#if TAYLOR_INTERPOLATION_ORDER > 7
	    +oon[8]*h*(F_row[8])
#endif
#if TAYLOR_INTERPOLATION_ORDER > 6
		 )
#endif
#if TAYLOR_INTERPOLATION_ORDER > 5
		 )
#endif
#if TAYLOR_INTERPOLATION_ORDER > 4
		 )
#endif
#if TAYLOR_INTERPOLATION_ORDER > 3
		 )
#endif
#if TAYLOR_INTERPOLATION_ORDER > 2
		 )
#endif
#if TAYLOR_INTERPOLATION_ORDER > 1
		 )
#endif
#if TAYLOR_INTERPOLATION_ORDER > 0
		 )
#endif
	  ;
     } // interpolation for F_j(T), jrecur<=j<=l
  } // if T < T_crit

  /*------------------------------------
    And then do downward recursion in j
   ------------------------------------*/
#if TAYLOR_INTERPOLATION_AND_RECURSION
  if (l > 0 && jrecur > 0) {
    double F_jp1 = F_[jrecur];
    const double exp_jT = std::exp(-T);
    for(int j=jrecur-1; j>=0; --j) {
      const double F_j = (exp_jT + two_T*F_jp1)*oo2np1[j];
      F_[j] = F_j;
      F_jp1 = F_j;
    }
  }
#endif

  return F_;
}

Taylor_Fjt::ExpensiveMath::ExpensiveMath(int ifac, int idf)
{
  if (ifac >= 0) {
      fac = allocate<double>(ifac+1);
      fac[0] = 1.0;
      for(int i=1; i<=ifac; i++) {
	  fac[i] = i*fac[i-1];
      }
  }

  if (idf >= 0) {
      df = allocate<double>(idf+1);
      /* df[i] gives (i-1)!!, so that (-1)!! is defined... */
      df[0] = 1.0;
      if (idf >= 1) df[1] = 1.0;
      if (idf >= 2) df[2] = 1.0;
      for(int i=3; i<=idf; i++){
	  df[i] = (i-1)*df[i-2];
      }
  }

}

Taylor_Fjt::ExpensiveMath::~ExpensiveMath()
{
  deallocate(fac);
  deallocate(df);
}

/////////////////////////////////////////////////////////////////////////////

 /* Tablesize should always be at least 121. */
#define TABLESIZE 121

/* Tabulate the incomplete gamma function and put in gtable. */
/*
 *     For J = JMAX a power series expansion is used, see for
 *     example Eq.(39) given by V. Saunders in "Computational
 *     Techniques in Quantum Chemistry and Molecular Physics",
 *     Reidel 1975.  For J < JMAX the values are calculated
 *     using downward recursion in J.
 */
FJT::FJT(int max)
{
  int i,j;
  double denom,d2jmax1,r2jmax1,wval,d2wval,sum,term,rexpw;

  maxj = max;

  /* Allocate storage for gtable and int_fjttable. */
  int_fjttable = allocate<double>(maxj+1);
  gtable = allocate<double*>(ngtable());
  for (i=0; i<ngtable(); i++) {
      gtable[i] = allocate<double>(TABLESIZE);
    }

  /* Tabulate the gamma function for t(=wval)=0.0. */
  denom = 1.0;
  for (i=0; i<ngtable(); i++) {
    gtable[i][0] = 1.0/denom;
    denom += 2.0;
    }

  /* Tabulate the gamma function from t(=wval)=0.1, to 12.0. */
  d2jmax1 = 2.0*(ngtable()-1) + 1.0;
  r2jmax1 = 1.0/d2jmax1;
  for (i=1; i<TABLESIZE; i++) {
    wval = 0.1 * i;
    d2wval = 2.0 * wval;
    term = r2jmax1;
    sum = term;
    denom = d2jmax1;
    for (j=2; j<=200; j++) {
      denom = denom + 2.0;
      term = term * d2wval / denom;
      sum = sum + term;
      if (term <= 1.0e-15) break;
      }
    rexpw = exp(-wval);

    /* Fill in the values for the highest j gtable entries (top row). */
    gtable[ngtable()-1][i] = rexpw * sum;

    /* Work down the table filling in the rest of the column. */
    denom = d2jmax1;
    for (j=ngtable() - 2; j>=0; j--) {
      denom = denom - 2.0;
      gtable[j][i] = (gtable[j+1][i]*d2wval + rexpw)/denom;
      }
    }

  /* Form some denominators, so divisions can be eliminated below. */
  denomarray = allocate<double>(max+1);
  denomarray[0] = 0.0;
  for (i=1; i<=max; i++) {
    denomarray[i] = 1.0/(2*i - 1);
    }

  wval_infinity = 2*max + 37.0;
  itable_infinity = (int) (10 * wval_infinity);

  }

FJT::~FJT()
{
  deallocate(int_fjttable);
  for (int i=0; i<maxj+7; i++) {
    deallocate(gtable[i]);
  }
  deallocate(gtable);
  deallocate(denomarray);
  }

/* Using the tabulated incomplete gamma function in gtable, compute
 * the incomplete gamma function for a particular wval for all 0<=j<=J.
 * The result is placed in the global intermediate int_fjttable.
 */
double *
FJT::values(int J,double wval)
{
  const double sqrpih =  0.886226925452758;
  const double coef2 =  0.5000000000000000;
  const double coef3 = -0.1666666666666667;
  const double coef4 =  0.0416666666666667;
  const double coef5 = -0.0083333333333333;
  const double coef6 =  0.0013888888888889;
  const double gfac30 =  0.4999489092;
  const double gfac31 = -0.2473631686;
  const double gfac32 =  0.321180909;
  const double gfac33 = -0.3811559346;
  const double gfac20 =  0.4998436875;
  const double gfac21 = -0.24249438;
  const double gfac22 =  0.24642845;
  const double gfac10 =  0.499093162;
  const double gfac11 = -0.2152832;
  const double gfac00 = -0.490;

  double wdif, d2wal, rexpw, /* denom, */ gval, factor, rwval, term;
  int i, itable, irange;

  if (J>maxj) {
    ExEnv::errn()
      << scprintf("the int_fjt routine has been incorrectly used")
      << endl;
    ExEnv::errn()
      << scprintf("J = %d but maxj = %d",J,maxj)
      << endl;
    abort();
    }

  /* Compute an index into the table. */
  /* The test is needed to avoid floating point exceptions for
   * large values of wval. */
  if (wval > wval_infinity) {
    itable = itable_infinity;
    }
  else {
    itable = (int) (10.0 * wval);
    }

  /* If itable is small enough use the table to compute int_fjttable. */
  if (itable < TABLESIZE) {

    wdif = wval - 0.1 * itable;

    /* Compute fjt for J. */
    int_fjttable[J] = (((((coef6 * gtable[J+6][itable]*wdif
                            + coef5 * gtable[J+5][itable])*wdif
                             + coef4 * gtable[J+4][itable])*wdif
                              + coef3 * gtable[J+3][itable])*wdif
                               + coef2 * gtable[J+2][itable])*wdif
                                -  gtable[J+1][itable])*wdif
                         +  gtable[J][itable];

    /* Compute the rest of the fjt. */
    d2wal = 2.0 * wval;
    rexpw = exp(-wval);
    /* denom = 2*J + 1; */
    for (i=J; i>0; i--) {
      /* denom = denom - 2.0; */
      int_fjttable[i-1] = (d2wal*int_fjttable[i] + rexpw)*denomarray[i];
      }
    }
  /* If wval <= 2*J + 36.0, use the following formula. */
  else if (itable <= 20*J + 360) {
    rwval = 1.0/wval;
    rexpw = exp(-wval);

    /* Subdivide wval into 6 ranges. */
    irange = itable/30 - 3;
    if (irange == 1) {
      gval = gfac30 + rwval*(gfac31 + rwval*(gfac32 + rwval*gfac33));
      int_fjttable[0] = sqrpih*sqrt(rwval) - rexpw*gval*rwval;
      }
    else if (irange == 2) {
      gval = gfac20 + rwval*(gfac21 + rwval*gfac22);
      int_fjttable[0] = sqrpih*sqrt(rwval) - rexpw*gval*rwval;
      }
    else if (irange == 3 || irange == 4) {
      gval = gfac10 + rwval*gfac11;
      int_fjttable[0] = sqrpih*sqrt(rwval) - rexpw*gval*rwval;
      }
    else if (irange == 5 || irange == 6) {
      gval = gfac00;
      int_fjttable[0] = sqrpih*sqrt(rwval) - rexpw*gval*rwval;
      }
    else {
      int_fjttable[0] = sqrpih*sqrt(rwval);
      }

    /* Compute the rest of the int_fjttable from int_fjttable[0]. */
    factor = 0.5 * rwval;
    term = factor * rexpw;
    for (i=1; i<=J; i++) {
      int_fjttable[i] = factor * int_fjttable[i-1] - term;
      factor = rwval + factor;
      }
    }
  /* For large values of wval use this algorithm: */
  else {
    rwval = 1.0/wval;
    int_fjttable[0] = sqrpih*sqrt(rwval);
    factor = 0.5 * rwval;
    for (i=1; i<=J; i++) {
      int_fjttable[i] = factor * int_fjttable[i-1];
      factor = rwval + factor;
      }
    }
  /* printf(" %2d %12.8f %4d %12.8f\n",J,wval,itable,int_fjttable[0]); */

  return int_fjttable;
  }

/////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: c++
// c-file-style: "CLJ-CONDENSED"
// End:
