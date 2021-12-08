/*********************                                                        */
/*! \file ConstraintBoundTightener.cpp
 ** \verbatim
 ** Top contributors (to current version):
 **   Guy Katz
 ** This file is part of the Marabou project.
 ** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved. See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** [[ Add lengthier description here ]]

 **/

#include "ConstraintBoundTightener.h"
#include "FloatUtils.h"
#include "MarabouError.h"
#include "Statistics.h"
#include "UNSATCertificate.h"


ConstraintBoundTightener::ConstraintBoundTightener( ITableau &tableau, IEngine &engine )
    : _tableau( tableau )
    , _lowerBounds( NULL )
    , _upperBounds( NULL )
    , _tightenedLower( NULL )
    , _tightenedUpper( NULL )
    , _statistics( NULL )
    , _engine (engine)
{
}

void ConstraintBoundTightener::setDimensions()
{
    freeMemoryIfNeeded();

    _n = _tableau.getN();
    _m = _tableau.getM();

    _lowerBounds = new double[_n];
    if ( !_lowerBounds )
        throw MarabouError( MarabouError::ALLOCATION_FAILED, "ConstraintBoundTightener::lowerBounds" );

    _upperBounds = new double[_n];
    if ( !_upperBounds )
        throw MarabouError( MarabouError::ALLOCATION_FAILED, "ConstraintBoundTightener::upperBounds" );

    _tightenedLower = new bool[_n];
    if ( !_tightenedLower )
        throw MarabouError( MarabouError::ALLOCATION_FAILED, "ConstraintBoundTightener::tightenedLower" );

    _tightenedUpper = new bool[_n];
    if ( !_tightenedUpper )
		throw MarabouError( MarabouError::ALLOCATION_FAILED, "ConstraintBoundTightener::tightenedUpper" );

	resetBounds();
}

void ConstraintBoundTightener::resetBounds()
{
    std::fill( _tightenedLower, _tightenedLower + _n, false );
    std::fill( _tightenedUpper, _tightenedUpper + _n, false );

    for ( unsigned i = 0; i < _n; ++i )
    {
        _lowerBounds[i] = _tableau.getLowerBound( i );
        _upperBounds[i] = _tableau.getUpperBound( i );
    }
}

ConstraintBoundTightener::~ConstraintBoundTightener()
{
    freeMemoryIfNeeded();
}

void ConstraintBoundTightener::freeMemoryIfNeeded()
{
    if ( _lowerBounds )
    {
        delete[] _lowerBounds;
        _lowerBounds = NULL;
    }

    if ( _upperBounds )
    {
        delete[] _upperBounds;
        _upperBounds = NULL;
    }

    if ( _tightenedLower )
    {
        delete[] _tightenedLower;
        _tightenedLower = NULL;
    }

    if ( _tightenedUpper )
    {
        delete[] _tightenedUpper;
        _tightenedUpper = NULL;
    }

}

void ConstraintBoundTightener::setStatistics( Statistics *statistics )
{
    _statistics = statistics;
}

void ConstraintBoundTightener::notifyLowerBound( unsigned variable, double bound )
{
    if ( bound > _lowerBounds[variable] )
    {
        _lowerBounds[variable] = bound;
        _tightenedLower[variable] = false;
    }
}

void ConstraintBoundTightener::notifyUpperBound( unsigned variable, double bound )
{
    if ( bound < _upperBounds[variable] )
    {
        _upperBounds[variable] = bound;
        _tightenedUpper[variable] = false;
    }
}

void ConstraintBoundTightener::notifyDimensionChange( unsigned /* m */ , unsigned /* n */ )
{
    setDimensions();
}

void ConstraintBoundTightener::registerTighterLowerBound( unsigned variable, double bound )
{
    if ( bound > _lowerBounds[variable] )
    {
        _lowerBounds[variable] = bound;
        _tightenedLower[variable] = true;
    }
}

void ConstraintBoundTightener::registerTighterUpperBound( unsigned variable, double bound )
{
    if ( bound < _upperBounds[variable] )
    {
        _upperBounds[variable] = bound;
        _tightenedUpper[variable] = true;
    }
}

void ConstraintBoundTightener::registerTighterLowerBound( unsigned variable, double bound, const SparseUnsortedList& row )
{

	if ( bound > _lowerBounds[variable] )
	{
		if ( GlobalConfiguration::PROOF_CERTIFICATE && _engine.isBoundTightest( variable, bound, false ) )
			_tableau.updateExplanation( row, false, variable );

		registerTighterLowerBound( variable, bound );
	}
}

void ConstraintBoundTightener::registerTighterUpperBound( unsigned variable, double bound, const SparseUnsortedList& row )
{
	if ( bound < _upperBounds[variable] )
	{
		if ( GlobalConfiguration::PROOF_CERTIFICATE && _engine.isBoundTightest( variable, bound, true ) )
			_tableau.updateExplanation( row, true, variable );

		registerTighterUpperBound( variable, bound );
	}
}

void ConstraintBoundTightener::ConstraintBoundTightener::getConstraintTightenings( List<Tightening> &tightenings ) const
{
    for ( unsigned i = 0; i < _n; ++i )
    {
        if ( _tightenedLower[i] )
            tightenings.append( Tightening( i, _lowerBounds[i], Tightening::LB ) );

        if ( _tightenedUpper[i] )
            tightenings.append( Tightening( i, _upperBounds[i], Tightening::UB ) );
    }
}

void ConstraintBoundTightener::externalExplanationUpdate( const unsigned var, const double value,
														 const bool isAffectedBoundUpper,
														 const unsigned causingVar, bool isCausingBoundUpper,
														 const List<unsigned int> constraintVars,
														 const PiecewiseLinearFunctionType constraintType )
{
	if ( !GlobalConfiguration::PROOF_CERTIFICATE || !_engine.isBoundTightest( var, value, isAffectedBoundUpper ))
		return;

	// TODO re-consider design of this function
	// Register new ground bound, update certificate, and reset explanation
	auto* PLCExpl = new PLCExplanation();

	PLCExpl->_causingVar = causingVar;
	PLCExpl->_explanation = _engine.getVarCurrentBoundExplanation( causingVar, isCausingBoundUpper );
	PLCExpl->_constraintVars = constraintVars;
	PLCExpl->_affectedVar = var;
	PLCExpl->_bound = value;
	PLCExpl->_isAffectedBoundUpper = isAffectedBoundUpper;
	PLCExpl->_isCausingBoundUpper = isCausingBoundUpper;
	PLCExpl->_constraintType = constraintType;

	_engine.getUNSATCertificateCurrentPointer()->addPLCExplanation( PLCExpl );

	isAffectedBoundUpper ? _engine.updateGroundUpperBound( var, value ) : _engine.updateGroundLowerBound( var, value ); // Function resets explanation as well
	isAffectedBoundUpper ? registerTighterUpperBound( var, value ) : registerTighterLowerBound( var, value );
}

double ConstraintBoundTightener::getUpperBound( unsigned var ) const
{
	ASSERT( var < _n );
	return _upperBounds[var];
}

double ConstraintBoundTightener::getLowerBound( unsigned var ) const
{
	ASSERT( var < _n );
	return _lowerBounds[var];
}

//
// Local Variables:
// compile-command: "make -C ../.. "
// tags-file-name: "../../TAGS"
// c-basic-offset: 4
// End:
//
