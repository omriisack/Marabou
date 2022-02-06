/*********************                                                        */
/*! \file BoundsExplainer.cpp
 ** \verbatim
 ** This file is part of the Marabou project.
 ** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved. See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** [[ Add lengthier description here ]]
 **/


#ifndef __UNSATCertificate_h__
#define __UNSATCertificate_h__

#include "BoundsExplainer.h"
#include <assert.h>
#include <utility>
#include "ReluConstraint.h"
#include "PiecewiseLinearFunctionType.h"
#include "SmtLibWriter.h"
#include "Options.h"

// Contains an explanation for a ground bound update during a run (i.e from ReLU phase-fixing)
// For now only relevant to ReLU constraints
struct PLCExplanation
{
	unsigned _causingVar;
	unsigned _affectedVar;
	double _bound;
	bool _isCausingBoundUpper;
	bool _isAffectedBoundUpper;
	double *_explanation;
	PiecewiseLinearFunctionType _constraintType;
	unsigned _decisionLevel;
};

struct Contradiction
{
	unsigned _var;
	double *_upperExplanation;
	double *_lowerExplanation;

	~Contradiction()
	{
		delete [] _upperExplanation;
		delete [] _lowerExplanation;
	}
};

struct ProblemConstraint
{
	PiecewiseLinearFunctionType _type;
	List<unsigned> _constraintVars;
	PhaseStatus _status;

	bool operator==(const ProblemConstraint other)
	{
		return _type == other._type && _constraintVars == other._constraintVars;
	}
};


class CertificateNode
{
public:

	/*
	 * Constructor for the root
	 */
	CertificateNode( std::vector<std::vector<double>> *_initialTableau, std::vector<double> &groundUBs, std::vector<double> &groundLBs );

	/*
	 * Constructor for a regular node
	 */
	CertificateNode( CertificateNode* parent, PiecewiseLinearCaseSplit split );

	~CertificateNode();

	/*
 	* Certifies the tree is indeed a proof of unsatisfiability;
 	*/
	bool certify();

	/*
 	* Adds a child to the tree
 	*/
	void addChild( CertificateNode* child );

	/*
	 * Sets the leaf certificate as input
	 */
	void setContradiction( Contradiction *certificate );

	/*
	 * Gets the leaf certificate of the node
	 */
	Contradiction* getContradiction() const;

	/*
 	* Returns the parent of a node
 	*/
	CertificateNode* getParent() const;

	/*
 	* Returns the parent of a node
 	*/
	const PiecewiseLinearCaseSplit& getSplit() const;

	/*
	 * Gets the PLC explanations of the node
	 */
	const std::list<PLCExplanation*>& getPLCExplanations() const;

	/*
	 * Certifies a contradiction
	 */
	bool certifyContradiction() const;

	/*
	 * Computes a bound according to an explanation
	 */
	double explainBound( unsigned var, bool isUpper, const std::vector<double> &expl ) const;

	/*
	 * Adds an PLC explanation to the list
	 */
	void addPLCExplanation( PLCExplanation *expl );

	/*
 	* Adds an a problem constraint to the list
 	*/
	void addProblemConstraint( PiecewiseLinearFunctionType type, List<unsigned int> constraintVars, PhaseStatus status );

	/*
 	* Return a pointer to the problem constraint representing the split
 	*/
	ProblemConstraint *getCorrespondingReLUConstraint(const List<PiecewiseLinearCaseSplit> &splits );

	/*
	 * Return true iff a list of splits represents a splits over a single variable
	 */
	bool certifySingleVarSplits( const List<PiecewiseLinearCaseSplit> &splits ) const;

	/*
	 * Return true iff the changes in the ground bounds are certified, with tolerance to errors with epsilon size at most
 	*/
	bool certifyAllPLCExplanations( double epsilon );

	/*
	 * get a pointer to a child by a head split, or NULL if not found
	 */
	CertificateNode* getChildBySplit( const PiecewiseLinearCaseSplit &split ) const;

	/*
	 * Sets value of _hasSATSolution to be true
	 */
	void hasSATSolution();

	/*
	 * Sets value of _wasVisited to be true
	 */
	void wasVisited();

	/*
	 * Sets value of _shouldDelegate to be true
	 */
	void shouldDelegate( unsigned delegationNumber );

	/*
 	* Removes all PLC explanations
 	*/
	void deletePLCExplanations();

	/*
 	* Removes all PLC explanations from a certain point
 	*/
	void resizePLCExplanationsList( unsigned newSize );

	/*
	 * Deletes all offsprings of the node and makes it a leaf
	 */
	void makeLeaf();

	/*
 	* Removes all PLCExplanations above a certain decision level WITHOUT deleting them
 	* ASSUMPTION - explanations pointers are kept elsewhere before removal
 	*/
	void removePLCExplanations( unsigned decisionLevel );

private:
	std::list<CertificateNode*> _children;
	List<ProblemConstraint> _problemConstraints;
	CertificateNode* _parent;
	std::list<PLCExplanation*> _PLCExplanations;
	Contradiction* _contradiction;
	PiecewiseLinearCaseSplit _headSplit;
	bool _hasSATSolution; // Enables certifying correctness of UNSAT certificates built before concluding SAT.
	bool _wasVisited; // Same TODO consider deleting when done
	bool _shouldDelegate;
	unsigned _delegationNumber;

	std::vector<std::vector<double>>* _initialTableau;
	std::vector<double> _groundUpperBounds;
	std::vector<double> _groundLowerBounds;

	/*
	 * Copies initial tableau and ground bounds
	 */
	void copyGB( std::vector<double> &groundUBs, std::vector<double> &groundLBs );

	/*
	 * Inherits the initialTableau and ground bounds from parent, if exists
	 */
	void passChangesToChildren( ProblemConstraint *childrenSplitConstraint );

	/*
	 * Checks if the node is a valid leaf
	 */
	bool isValidLeaf() const;

	/*
	 * Checks if the node is a valid none-leaf
	 */
	bool isValidNoneLeaf() const;

	/*
	* Write a leaf marked to delegate to a smtlib file format
	*/
	void writeLeafToFile();
};

class UNSATCertificateUtils
{
public:
	static double computeBound( unsigned var, bool isUpper, const std::vector<double> &expl,
								const std::vector<std::vector<double>> &initialTableau, const std::vector<double> &groundUBs, const std::vector<double> &groundLBs )
	{
		ASSERT( groundLBs.size() == groundUBs.size() );
		ASSERT( initialTableau.size() == expl.size() || expl.empty() );
		ASSERT( groundLBs.size() == initialTableau[0].size() );
		ASSERT( groundLBs.size() == initialTableau[initialTableau.size() - 1].size() );
		ASSERT( var < groundUBs.size() );

		double derived_bound = 0, temp;
		unsigned n = groundUBs.size();

		if ( expl.empty() )
			return isUpper ? groundUBs[var] : groundLBs[var];

		// Create linear combination of original rows implied from explanation
		std::vector<double> explRowsCombination;
		UNSATCertificateUtils::getExplanationRowCombination( var, explRowsCombination, expl, initialTableau );

		// Set the bound derived from the linear combination, using original bounds.
		for ( unsigned i = 0; i < n; ++i )
		{
			temp = explRowsCombination[i];
			if ( !FloatUtils::isZero( temp ) )
			{
				if ( isUpper )
					temp *= FloatUtils::isPositive( explRowsCombination[i] ) ? groundUBs[i] : groundLBs[i];
				else
					temp *= FloatUtils::isPositive( explRowsCombination[i] ) ? groundLBs[i] : groundUBs[i];

				if ( !FloatUtils::isZero( temp ) )
					derived_bound += temp;
			}
		}

		explRowsCombination.clear();
		return derived_bound;
	}


	static void getExplanationRowCombination( unsigned var, std::vector<double> &explRowsCombination, const std::vector<double> &expl,
											  const std::vector<std::vector<double>> &initialTableau )
	{
		explRowsCombination = std::vector<double>(initialTableau[0].size(), 0 );
		unsigned n = initialTableau[0].size(), m = expl.size();
		for ( unsigned i = 0; i < m; ++i )
			for ( unsigned j = 0; j < n; ++j )
				if ( !FloatUtils::isZero( initialTableau[i][j] ) && !FloatUtils::isZero( expl[i]) )
					explRowsCombination[j] += initialTableau[i][j] * expl[i];

		for ( unsigned i = 0; i < n; ++i )
			if ( !FloatUtils::isZero( explRowsCombination[i] ) )
				explRowsCombination[i] *= -1;
			else
				explRowsCombination[i] = 0;

		// Since: 0 = Sum (ci * xi) + c * var = Sum (ci * xi) + (c - 1) * var + var
		// We have: var = - Sum (ci * xi) - (c - 1) * var
		explRowsCombination[var] += 1;
	}
};
#endif //__UNSATCertificate_h__