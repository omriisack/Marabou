/*********************                                                        */
/*! \file PhaseClause.h
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

#include "Map.h"
#include "PiecewiseLinearConstraint.h"
#include "UNSATCertificate.h"

#ifndef __PhaseClause_h__
#define __PhaseClause_h__

/*
 * A clause of PLC phases, s.t. if the negation of the clause is asserted, then query is UNSAT
 * Currently supports only PLC with two possible phases ( ReLU, Sign, ABS, etc. ).
 */
class PhaseClause
{
public:
	PhaseClause( Map<unsigned, PhaseStatus> clause, Contradiction* contradiction );
	~PhaseClause();

	/*
	 * Returns the contradiction of the clause
	 */
	Contradiction *getContradiction();

	/*
	 * Returns the status of a constraint in the clause
	 * If the phase is not fixed in the clause, then the constraint is not participating
	 */
	PhaseStatus getConstraintStatus( unsigned constraintAbstraction );

	/*
	 * Returns a constraint abstraction if the clause is a unit clause
	 * Otherwise returns -1
	 */
	int isUnitPropagationCandidate( Map<unsigned, PhaseStatus > &assignment );

private:
	friend class PhaseClausesManager;

	Map<unsigned, PhaseStatus> _clause;
	Contradiction* _contradiction; // A proof that negation of the clause is false
	int _watchVarA; //Represent variables in clause that are yet unassigned, used instead of literals for simplicity in case of costraints with multiple possible phases.
	int _watchVarB;

	/*
 	* Checks whether a variable is a watch variable of the clause
 	*/
	bool isAWatchVariable( unsigned var );

	/*
	 * Sets a watch variable of the clause
	 */
	void setAWatchVariable( int value );

	/*
	 * Updates the watch variables according to an assignment
	 */
	void updateWatchVariables( Map<unsigned, PhaseStatus> &assignment );

	/*
	 * Returns true iff the clause is valid i.e all statuses are fixed
	 */
	bool isClauseValid();

};

/*
 *  A class that manages the phase clauses
 */
class PhaseClausesManager
{
public:
	PhaseClausesManager( unsigned sizeLimit );

	/*
	 * Adds a clause to the manager
	 * Clause is added iff it is certified
	 */
	void addClause( PhaseClause *clause );

	/*
	 * Update the assignment of the manager according to the assignment
	 * Includes update of watch literals
	 */
	void updateAssignment( std::pair<unsigned, PhaseStatus> assignment );

	/*
	 * Gets a candidate for unit propagation
	 * Returns <-1, PHASE_NOT_FIXED> if no candidate is found
	 */
	std::pair<int, PhaseStatus> getUnitPropagationCandidate();

private:
	List<PhaseClause*> _clauses;
	unsigned _sizeLimit;
	Map<unsigned, PhaseStatus> _currentAssignment;
};
#endif // __PhaseClause_h__
