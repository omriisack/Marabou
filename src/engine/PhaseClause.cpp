/*********************                                                        */
/*! \file PhaseClause.cpp
 ** \verbatim
 ** Top contributors (to current version):
 **   Omri Isac, Guy Katz
 ** This file is part of the Marabou project.
 ** Copyright (c) 2017-2022 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved. See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** [[ Add lengthier description here ]]
 **/

#include "PhaseClause.h"

PhaseClause::PhaseClause( Map<unsigned, PhaseStatus> clause, Contradiction *contradiction )
	: _clause( clause )
	, _contradiction( contradiction )
	, _watchVarA( -1 )
	, _watchVarB( -1 )
{
	ASSERT( isClauseValid() );
}

PhaseClause::~PhaseClause()
{
	if ( _contradiction )
	{
		delete _contradiction;
		_contradiction = NULL;
	}

	_clause.clear();
}

Contradiction *PhaseClause::getContradiction()
{
	return _contradiction;
}

PhaseStatus PhaseClause::getConstraintStatus( unsigned constraintAbstraction )
{
	if ( _clause.exists( constraintAbstraction ) )
		return _clause.at( constraintAbstraction );
	return PhaseStatus::PHASE_NOT_FIXED;
}

int PhaseClause::isUnitPropagationCandidate( Map<unsigned, PhaseStatus > &assignment )
{
	ASSERT( isClauseValid() );
	if ((_watchVarA == -1 && _watchVarB == -1 ) || (_watchVarB >= 0 && _watchVarA >= 0 ) )
		return -1;

	for ( auto key : _clause.keys() )
		if ( ( int ) key != _watchVarA && ( int ) key != _watchVarB )
			if ( assignment.at( key ) == PHASE_NOT_FIXED || assignment.at( key ) == _clause.at( key ) )
				return -1;

	return _watchVarA == -1 ? _watchVarB : _watchVarA;
}

bool PhaseClause::isAWatchVariable( unsigned var )
{
	return _watchVarA == ( int ) var || _watchVarB == ( int ) var;
}

void PhaseClause::setAWatchVariable( int value )
{
	if ( isAWatchVariable( value ) )
		return;

	_watchVarB = _watchVarA;
	_watchVarA = value;
}

void PhaseClause::updateWatchVariables( Map<unsigned, PhaseStatus > &assignment )
{
	if ( !assignment.exists( _watchVarA ) || (assignment.exists( _watchVarA ) && assignment.at( _watchVarA ) != PhaseStatus::PHASE_NOT_FIXED ) )
		_watchVarA = -1;

	if ( !assignment.exists( _watchVarB ) || (assignment.exists( _watchVarB ) && assignment.at( _watchVarB ) != PhaseStatus::PHASE_NOT_FIXED ) )
		_watchVarB = -1;

	if ( _watchVarA >= 0 && _watchVarB >= 0 ) // If there are still at least two watch variables, no update required
		return;

	for ( auto key : assignment.keys() )
	{
		if ( assignment.at( key ) == PHASE_NOT_FIXED && _clause.exists( key ) )
			_watchVarA == -1 ? _watchVarA = key : _watchVarB = key;

		if ( _watchVarA >= 0 && _watchVarB >= 0 )
			break;
	}
}

bool PhaseClause::isClauseValid()
{
	for ( auto literal : _clause )
		if ( literal.second == PhaseStatus::PHASE_NOT_FIXED )
			return false;

	return true;
}

PhaseClausesManager::PhaseClausesManager(unsigned sizeLimit)
	: _clauses( )
	,_sizeLimit( sizeLimit )
{
}

void PhaseClausesManager::addClause( PhaseClause *clause )
{
	ASSERT( _clauses.size() <= _sizeLimit );
	if ( _clauses.size() == _sizeLimit )
	{
		delete _clauses.front();
		_clauses.front() = NULL;
		_clauses.erase( _clauses.begin() );
		_clauses.append( clause );
	}
	else
		_clauses.append( clause );
}

void PhaseClausesManager::updateAssignment( std::pair<unsigned, PhaseStatus> assignment )
{
	unsigned constraintAbstraction = assignment.first;
	PhaseStatus status = assignment.second;

	// If assignment is already part of the current assignment, do nothing
	if (_currentAssignment.exists( constraintAbstraction ) && _currentAssignment.at(constraintAbstraction) == status )
		return;

	// Update current assignment
	if (  _currentAssignment.exists( constraintAbstraction ) )
		_currentAssignment.at(constraintAbstraction ) = status;
	else
		_currentAssignment.insert( constraintAbstraction, status );

	// Update watch variables of clauses
	for ( auto clause : _clauses )
		if ( clause->isAWatchVariable( constraintAbstraction ) )
			clause->updateWatchVariables( _currentAssignment );

}

std::pair<int, PhaseStatus> PhaseClausesManager::getUnitPropagationCandidate()
{
	for ( auto clause : _clauses )
	{
		int var = clause->isUnitPropagationCandidate( _currentAssignment );
		if ( var != -1 )
		{
			ASSERT( _currentAssignment.at( var ) == PHASE_NOT_FIXED );
			return std::pair<int, PhaseStatus>( var, clause->_clause.at( var ) );
		}
	}

	return std::pair<int, PhaseStatus>( -1, PHASE_NOT_FIXED );
}