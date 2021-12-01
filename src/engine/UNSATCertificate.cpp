/*********************                                                        */
/*! \file BoundsExplanator.cpp
 ** \verbatim
 ** This file is part of the Marabou project.
 ** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved. See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** [[ Add lengthier description here ]]

 **/

#include "UNSATCertificate.h"
#include <utility>


CertificateNode::CertificateNode( const std::vector<std::vector<double>> &initialTableau, std::vector<double> &groundUBs, std::vector<double> &groundLBs )
	: _children ( 0 )
	, _problemConstraints()
	, _parent( NULL )
	, _PLCExplanations( 0 )
	, _contradiction ( NULL )
	, _headSplit( )
	, _hasSATSolution( false )
	, _wasVisited ( false )
	, _shouldDelegate(false )
	, _initialTableau( initialTableau )
	, _groundUpperBounds( groundUBs )
	, _groundLowerBounds( groundLBs )
{
}

CertificateNode::CertificateNode( CertificateNode* parent, PiecewiseLinearCaseSplit split )
	: _children ( 0 )
	, _problemConstraints( parent->_problemConstraints )
	, _parent( parent )
	, _PLCExplanations( 0 )
	, _contradiction ( NULL )
	, _headSplit( std::move( split ) )
	, _hasSATSolution( false )
	, _wasVisited ( false )
	, _shouldDelegate (false)
	, _initialTableau( 0 )
	, _groundUpperBounds( 0 )
	, _groundLowerBounds( 0 )
{
}

CertificateNode::~CertificateNode()
{
	for ( auto child : _children )
		if ( child )
		{
			delete child;
			child = NULL;
		}

	removePLCExplanations();

	if ( _contradiction )
	{
		if ( _contradiction->explanation )
		{
			delete _contradiction->explanation;
			_contradiction->explanation = NULL;
		}

		delete _contradiction;
		_contradiction = NULL;
	}

	clearInitials();
}

void CertificateNode::setContradiction( Contradiction* contradiction )
{
	_contradiction = contradiction;
}


void CertificateNode::addChild( CertificateNode* child )
{
	_children.push_back( child );
}

Contradiction* CertificateNode::getContradiction() const
{
	return _contradiction;
}

CertificateNode* CertificateNode::getParent() const
{
	return _parent;
}

const PiecewiseLinearCaseSplit& CertificateNode::getSplit() const
{
	return _headSplit;
}

void CertificateNode::passChangesToChildren()
{
	for ( auto* child : _children )
	{
		child->copyInitials( _initialTableau, _groundUpperBounds, _groundLowerBounds );
	}
}

bool CertificateNode::certify()
{
	// Update ground bounds according to head split
	for ( auto& tightening : _headSplit.getBoundTightenings() )
	{
		auto& temp = tightening._type == Tightening::UB ? _groundUpperBounds : _groundLowerBounds;
		temp[tightening._variable] = tightening._value;
	}

	if ( !certifyAllPLCExplanations( 0.01 ) )
		return false;

	// Check if it is a leaf, and if so use contradiction to certify
	// return true iff it is certified
	if ( _hasSATSolution || _shouldDelegate )
		return true;

	if ( isValidLeaf() )
		return certifyContradiction();

	if ( !_wasVisited && !_contradiction && _children.empty() )
		return true;

	ASSERT( isValidNoneLeaf() );
	// Otherwise, assert there is a constraint and children
	// validate the constraint is ok
	// Certify all children
	// return true iff all children are certified
	bool answer = true;
	List<PiecewiseLinearCaseSplit> childrenSplits;
	for ( auto& child : _children )
		childrenSplits.append( child->_headSplit );

	if ( !certifySingleVarSplits( childrenSplits ) && !certifyReLUSplits( childrenSplits ) )
		return false;

	passChangesToChildren();

	for ( auto child : _children )
		if ( !child->certify() )
			answer = false;

	return answer;
}

bool CertificateNode::certifyContradiction() const
{
	ASSERT( isValidLeaf() && !_hasSATSolution );
	unsigned var = _contradiction->var;
	SingleVarBoundsExplanator& varExpl = *_contradiction->explanation;

	std::vector<double> ubExpl ( varExpl.getLength() );
	std::vector<double> lbExpl ( varExpl.getLength() );

	varExpl.getVarBoundExplanation( ubExpl, true );
	varExpl.getVarBoundExplanation( lbExpl, false );

	double computedUpper = explainBound( var, true, ubExpl ), computedLower = explainBound( var, false, lbExpl );

	if ( computedUpper >= computedLower ) //TODO delete when completing
		printf("(Global) Certification error for var %d. ub is %.5lf, lb is %.5lf \n", var, computedUpper, computedLower );

	return (  computedUpper < computedLower );
}

double CertificateNode::explainBound( unsigned var, bool isUpper, const std::vector<double>& expl ) const
{
	return UNSATCertificateUtils::computeBound( var, isUpper, expl, _initialTableau, _groundUpperBounds, _groundLowerBounds );
}


void CertificateNode::copyInitials( const std::vector<std::vector<double>> &initialTableau, std::vector<double> &groundUBs, std::vector<double> &groundLBs )
{
	clearInitials();
	for ( auto& row : initialTableau )
	{
		auto rowCopy = std::vector<double> ( row.size() );
		std::copy( row.begin(), row.end(), rowCopy.begin() );
		_initialTableau.push_back( rowCopy );
	}

	_groundUpperBounds.resize( groundUBs.size() );
	_groundLowerBounds.resize( groundLBs.size() );

	std::copy( groundUBs.begin(), groundUBs.end(), _groundUpperBounds.begin() );
	std::copy( groundLBs.begin(), groundLBs.end(), _groundLowerBounds.begin() );

}

bool CertificateNode::isValidLeaf() const
{
	return _contradiction && _children.empty();
}

bool CertificateNode::isValidNoneLeaf() const
{
	return !_contradiction && !_children.empty();
}

void CertificateNode::clearInitials()
{
	for ( auto& row : _initialTableau )
		row.clear();
	_initialTableau.clear();

	_groundUpperBounds.clear();
	_groundLowerBounds.clear();
}

void CertificateNode::addPLCExplanation( PLCExplanation* expl )
{
	_PLCExplanations.push_back( expl );
}


void CertificateNode::addProblemConstraint( PiecewiseLinearFunctionType type, List<unsigned int> constraintVars )
{
	_problemConstraints.append( { type, constraintVars } );
}

bool CertificateNode::certifyReLUSplits( const List<PiecewiseLinearCaseSplit> &splits ) const
{

	if ( splits.size() != 2 )
		return false;
	auto firstSplitTightenings = splits.front().getBoundTightenings(), secondSplitTightenings = splits.back().getBoundTightenings();
	if ( firstSplitTightenings.size() != 2 || secondSplitTightenings.size() != 2 )
		return false;

	// find the LB, it is b
	auto &activeSplit = firstSplitTightenings.front()._type == Tightening::LB ? firstSplitTightenings : secondSplitTightenings;
	auto &inactiveSplit = firstSplitTightenings.front()._type == Tightening::LB ? secondSplitTightenings : firstSplitTightenings;

	unsigned b = activeSplit.front()._variable;
	unsigned aux = activeSplit.back()._variable;
	unsigned f = inactiveSplit.back()._variable;

	if ( inactiveSplit.front()._variable != b || inactiveSplit.back()._type == Tightening::LB || activeSplit.back()._type == Tightening::LB )
		return false;
	if ( FloatUtils::areDisequal( inactiveSplit.back()._value, 0.0 ) || FloatUtils::areDisequal( inactiveSplit.front()._value, 0.0 ) || FloatUtils::areDisequal( activeSplit.back()._value, 0.0 ) || FloatUtils::areDisequal( activeSplit.front()._value, 0.0 ) )
		return false;

	// Certify that f = relu(b) + aux is in problem constraints
	bool foundConstraint = false;

	for ( auto& con : _problemConstraints )
		if ( con._type == PiecewiseLinearFunctionType::RELU && con._constraintVars.front() == b && con._constraintVars.exists( f ) && con._constraintVars.back() == aux )
			foundConstraint = true;

	return foundConstraint;
}

bool CertificateNode::certifyAllPLCExplanations( double epsilon )
{
	// Create copies of the gb, check for their validity, and pass these changes to all the children
	// Assuming the splits of the children are ok.
	for ( auto* expl : _PLCExplanations )
	{
		bool constraintMatched = false, tighteningMatched = false;
		double explainedBound = UNSATCertificateUtils::computeBound( expl->_causingVar, expl->_isCausingBoundUpper, expl->_explanation, _initialTableau, _groundUpperBounds, _groundLowerBounds );
		unsigned b = 0, f = 0, aux = 0;
		// Make sure it is a problem constraint
		for ( auto& con : _problemConstraints )
		{
			if ( expl->_constraintType == PiecewiseLinearFunctionType::RELU && expl->_constraintVars == con._constraintVars )
			{
				std::vector<unsigned> conVec( con._constraintVars.begin(), con._constraintVars.end() );
				b = conVec[0], f = conVec[1], aux = conVec[2];
				conVec.clear();
				constraintMatched = true;
			}
		}

		if ( !constraintMatched )
			return false;

		if ( expl->_causingVar != b && expl->_causingVar != f && expl->_causingVar != aux )
			return false;

		// Make sure the explanation is explained using a ReLU bound tightening. Cases are matching each rule in ReluConstraint.cpp
		// Cases allow explained bound to be tighter than the ones recorded (since an explanation can explain looser bounds), and an epsilon sized error is tolerated.

		// If lb of b is non negative, then ub of aux is 0
		if ( expl->_causingVar == b && !expl->_isCausingBoundUpper && expl->_affectedVar == aux && expl->_isAffectedBoundUpper && FloatUtils::isZero( expl->_bound ) && !FloatUtils::isNegative( explainedBound + epsilon ) )
			tighteningMatched = true;

		// If lb of f is positive, then ub if aux is 0
		else if ( expl->_causingVar == f && !expl->_isCausingBoundUpper && expl->_affectedVar == aux && expl->_isAffectedBoundUpper && FloatUtils::isZero( expl->_bound ) && FloatUtils::isPositive( explainedBound + epsilon ) )
			tighteningMatched = true;

		// If lb of b is positive x, then lb of aux is -x
		else if ( expl->_causingVar == b && !expl->_isCausingBoundUpper && expl->_affectedVar == aux && expl->_isAffectedBoundUpper && FloatUtils::gte( explainedBound, - expl->_bound - epsilon ) && expl->_bound > 0 )
			tighteningMatched = true;

		// If lb of aux is positive, then ub of f is 0
		else if ( expl->_causingVar == aux && !expl->_isCausingBoundUpper && expl->_affectedVar == f && expl->_isAffectedBoundUpper && FloatUtils::isZero( expl->_bound ) && FloatUtils::isPositive( explainedBound + epsilon ) )
			tighteningMatched = true;

		// If lb of f is negative, then it is 0
		else if ( expl->_causingVar == f && !expl->_isCausingBoundUpper && expl->_affectedVar == f && !expl->_isAffectedBoundUpper && FloatUtils::isZero( expl->_bound ) && FloatUtils::isNegative( explainedBound - epsilon ) )
			tighteningMatched = true;

		// Propagate ub from f to b
		else if ( expl->_causingVar == f && expl->_isCausingBoundUpper && expl->_affectedVar == b && expl->_isAffectedBoundUpper && FloatUtils::lte( explainedBound, expl->_bound + epsilon ) )
			tighteningMatched = true;

		// If ub of b is non positive, then ub of f is 0
		else if ( expl->_causingVar == b && expl->_isCausingBoundUpper && expl->_affectedVar == f && expl->_isAffectedBoundUpper && FloatUtils::isZero( expl->_bound ) && !FloatUtils::isPositive( explainedBound - epsilon ) )
			tighteningMatched = true;

		// If ub of b is non positive x, then lb of aux is -x
		else if ( expl->_causingVar == b && expl->_isCausingBoundUpper && expl->_affectedVar == aux && !expl->_isAffectedBoundUpper && expl->_bound > 0 && !FloatUtils::isPositive( explainedBound - epsilon ) && FloatUtils::lte( explainedBound, -expl->_bound + epsilon) )
			tighteningMatched = true;

		// If ub of b is positive, then propagate to f ( positivity of explained bound is not checked since negative explained ub can always explain positive bound )
		else if ( expl->_causingVar == b && expl->_isCausingBoundUpper && expl->_affectedVar == f && expl->_isAffectedBoundUpper && FloatUtils::isPositive( expl->_bound )  && FloatUtils::lte( explainedBound, expl->_bound + epsilon ) )
			tighteningMatched = true;

		// If ub of aux is x, then lb of b is -x
		else if ( expl->_causingVar == aux && expl->_isCausingBoundUpper && expl->_affectedVar == b && !expl->_isAffectedBoundUpper && FloatUtils::lte( explainedBound, -expl->_bound + epsilon ) )
			tighteningMatched = true;

		if ( !tighteningMatched )
		{
			printf( "bound %.5lf. explained bound is %.5lf\n", expl->_bound, explainedBound ); //TODO delete when completing
			return false;
		}

		// If so, update the ground bounds and continue
		std::vector<double>& temp = expl->_isAffectedBoundUpper ? _groundUpperBounds : _groundLowerBounds;
		bool isTighter =  expl->_isAffectedBoundUpper ? FloatUtils::lt( expl->_bound, temp[expl->_affectedVar] ) : FloatUtils::gt( expl->_bound, temp[expl->_affectedVar] );
		if ( isTighter )
			temp[expl->_affectedVar] = expl->_bound;
	}
	return true;
}

/*
 * get a pointer to a child by a head split, or NULL if not found
 */
CertificateNode* CertificateNode::getChildBySplit( PiecewiseLinearCaseSplit& split) const
{
	for ( CertificateNode* child : _children )
		if ( child->_headSplit == split )
			return child;

	return NULL;
}

void CertificateNode::hasSATSolution()
{
	_hasSATSolution = true;
}

void CertificateNode::wasVisited()
{
	_wasVisited = true;
}

void CertificateNode::shouldDelegate()
{
	_shouldDelegate = true;
}

bool CertificateNode::certifySingleVarSplits( const List<PiecewiseLinearCaseSplit> &splits) const
{
	if ( splits.size() != 2 )
		return false;

	auto &frontSplitTightenings = splits.front().getBoundTightenings();
	auto &backSplitTightenings = splits.back().getBoundTightenings();

	if ( frontSplitTightenings.size() != 1 || backSplitTightenings.size() != 1 )
		return false;

	auto &frontSplitOnlyTightening = frontSplitTightenings.front();
	auto &backSplitOnlyTightening = backSplitTightenings.front();

	if ( frontSplitOnlyTightening._variable != backSplitOnlyTightening._variable )
		return false;

	if ( FloatUtils::areDisequal( frontSplitOnlyTightening._value, backSplitOnlyTightening._value ) )
		return false;

	if ( frontSplitOnlyTightening._type == backSplitOnlyTightening._type )
		return false;

	return true;
}

void CertificateNode::removePLCExplanations()
{
	if ( !_PLCExplanations.empty() )
	{
		for ( auto expl : _PLCExplanations )
		{
			expl->_explanation.clear();
			delete expl;
		}

		_PLCExplanations.clear();
	}
}