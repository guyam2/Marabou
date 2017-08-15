/*********************                                                        */
/*! \file MockEngine.h
** \verbatim
** Top contributors (to current version):
**   Guy Katz
** This file is part of the Marabou project.
** Copyright (c) 2016-2017 by the authors listed in the file AUTHORS
** in the top-level source directory) and their institutional affiliations.
** All rights reserved. See the file COPYING in the top-level source
** directory for licensing information.\endverbatim
**/

#ifndef __MockEngine_h__
#define __MockEngine_h__

#include "IEngine.h"
#include "List.h"
#include "FreshVariables.h"
#include "PiecewiseLinearCaseSplit.h"

class MockEngine : public IEngine
{
public:
	MockEngine()
	{
		wasCreated = false;
		wasDiscarded = false;
	}

    ~MockEngine()
    {
    }

	bool wasCreated;
	bool wasDiscarded;

	void mockConstructor()
	{
		TS_ASSERT( !wasCreated );
		wasCreated = true;
	}

	void mockDestructor()
	{
		TS_ASSERT( wasCreated );
		TS_ASSERT( !wasDiscarded );
		wasDiscarded = true;
	}

    struct Bound
    {
        Bound( unsigned variable, double bound )
        {
            _variable = variable;
            _bound = bound;
        }

        unsigned _variable;
        double _bound;
    };

    List<Bound> lastLowerBounds;
    List<Bound> lastUpperBounds;
    List<Equation> lastEquations;
    void applySplit( const PiecewiseLinearCaseSplit &split )
    {
        unsigned auxVariable = FreshVariables::getNextVariable();
        for ( auto &equation : split.getEquations() )
        {
            equation.markAuxiliaryVariable( auxVariable );
            lastEquations.append( equation );
        }
    
        List<Tightening> bounds = split.getBoundTightenings();
        List<Tightening> auxBounds = split.getAuxBoundTightenings();
        for ( auto &bound : auxBounds )
        {
            bound._variable = auxVariable;
            bounds.append( bound );
        }
    
        for ( auto &bound : bounds )
        {
            if ( bound._type == Tightening::LB )
                lastLowerBounds.append( Bound( bound._variable, bound._value ) );
            else
                lastUpperBounds.append( Bound( bound._variable, bound._value ) );
        }
    }

    mutable EngineState *lastStoredState;
    void storeState( EngineState &state ) const
    {
        lastStoredState = &state;
    }

    const EngineState *lastRestoredState;
    void restoreState( const EngineState &state )
    {
        lastRestoredState = &state;
    }
};

#endif // __MockEngine_h__

//
// Local Variables:
// compile-command: "make -C ../.. "
// tags-file-name: "../../TAGS"
// c-basic-offset: 4
// End:
//
