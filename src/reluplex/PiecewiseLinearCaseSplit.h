/*********************                                                        */
/*! \file PiecewiseLinearCaseSplit.h
** \verbatim
** Top contributors (to current version):
**   Guy Katz
** This file is part of the Marabou project.
** Copyright (c) 2016-2017 by the authors listed in the file AUTHORS
** in the top-level source directory) and their institutional affiliations.
** All rights reserved. See the file COPYING in the top-level source
** directory for licensing information.\endverbatim
**/

#ifndef __PiecewiseLinearCaseSplit_h__
#define __PiecewiseLinearCaseSplit_h__

#include "Equation.h"
#include "IEngine.h"
#include "Tightening.h"

class PiecewiseLinearCaseSplit
{
public:

    /*
      Store information regarding a bound tightening.
    */
    void storeBoundTightening( const Tightening &tightening );
    List<Tightening> getBoundTightenings() const;

    /*
      Store information regarding an auxiliary variable bound tightening.
    */
    void storeAuxBoundTightening( const Tightening &tightening );
    List<Tightening> getAuxBoundTightenings() const;

    /*
      Store information regarding a new equation to be added.
    */
    void addEquation( const Equation &equation );
  	List<Equation> getEquations() const;

    // /*
    //   Checks that this case has no conflicts with the already-given
    //   lower and upper bounds.
    // */
    // bool valid( const Map<unsigned, double> &lowerBounds, const Map<unsigned, double> &upperBounds ) const;

    /*
      Dump the case split - for debugging purposes.
    */
    void dump() const;

private:
    /*
      Bound tightening information.
    */
    List<Tightening> _bounds;

    /*
      Auxiliary variable bound tightening information.
    */
    List<Tightening> _auxBounds;

    /*
      The equation that needs to be added.
    */
    List<Equation> _equations;
};

#endif // __PiecewiseLinearCaseSplit_h__

//
// Local Variables:
// compile-command: "make -C .. "
// tags-file-name: "../TAGS"
// c-basic-offset: 4
// End:
//
