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

// Contains an explanation for a ground bound update during a run (i.e from ReLU phase-fixing)
// For now only relevant to ReLU constraints
struct PLCExplanation
{
	void copyContent( PLCExplanation *other )
	{
		_causingVar = other->_causingVar;
		_affectedVar = other->_affectedVar;
		_bound = other->_bound;
		_isCausingBoundUpper = other->_isCausingBoundUpper;
		_isAffectedBoundUpper = other->_isAffectedBoundUpper;
		_constraintType = other->_constraintType;
		_decisionLevel = other->_decisionLevel;

		_explanation.clear();
		_explanation.resize( other->_explanation.size() );
		std::copy( other->_explanation.begin(), other->_explanation.end(), _explanation.begin() );

		_constraintVars.clear();
		for( auto var : other->_constraintVars )
			_constraintVars.append( var );

	}

	unsigned _causingVar;
	unsigned _affectedVar;
	double _bound;
	bool _isCausingBoundUpper;
	bool _isAffectedBoundUpper;
	std::vector<double> _explanation;
	PiecewiseLinearFunctionType _constraintType;
	List<unsigned> _constraintVars;
	unsigned _decisionLevel;
};

struct Contradiction
{
	void copyContent( Contradiction* other)
	{
		_var = other->_var;
		*_explanation = *( other->_explanation );
	}
	unsigned _var;
	SingleVarBoundsExplainer* _explanation;
};

struct ProblemConstraint
{
	PiecewiseLinearFunctionType _type;
	List<unsigned> _constraintVars;
};


class CertificateNode
{
public:

	/*
	 * Constructor for the root
	 */
	CertificateNode( const std::vector<std::vector<double>> &_initialTableau, std::vector<double> &groundUBs, std::vector<double> &groundLBs );

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
	void addProblemConstraint( PiecewiseLinearFunctionType type, List<unsigned int> constraintVars );

	/*
 	* Return true iff the splits are created from a valid PLC
 	*/
	bool certifyReLUSplits( const List<PiecewiseLinearCaseSplit> &splits ) const;

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
	void shouldDelegate();

	/*
 	* Removes all PLC explanations
 	*/
	void removePLCExplanations();

	/*
	 * Deletes all offsprings of the node and makes it a leaf
	 */
	void makeLeaf();

private:

	std::list<CertificateNode*> _children;
	List<ProblemConstraint> _problemConstraints;
	CertificateNode* _parent;
	std::list<PLCExplanation*> _PLCExplanations;
	Contradiction* _contradiction;
	PiecewiseLinearCaseSplit _headSplit;
	bool _hasSATSolution; // Enables certifying correctness of UNSAT certificates built before concluding SAT.
	bool _wasVisited; // Same TODO consider deleting when done
	bool _shouldDelegate; // TODO replace with delegated proof

	std::vector<std::vector<double>> _initialTableau;
	std::vector<double> _groundUpperBounds;
	std::vector<double> _groundLowerBounds;

	/*
	 * Copies initial tableau and ground bounds
	 */
	void copyInitials ( const std::vector<std::vector<double>> &_initialTableau, std::vector<double> &groundUBs, std::vector<double> &groundLBs );

	/*
	 * Inherits the initialTableau and ground bounds from parent, if exists
	 */
	void passChangesToChildren();

	/*
	 * Checks if the node is a valid leaf
	 */
	bool isValidLeaf() const;

	/*
	 * Checks if the node is a valid none-leaf
	 */
	bool isValidNoneLeaf() const;

	/*
	 * Clear initial tableau and ground bounds
	 */
	void clearInitials();

};

class UNSATCertificateUtils
{
public:
	static double computeBound( unsigned var, bool isUpper, const  std::vector<double> &expl,
							   const std::vector<std::vector<double>> &initialTableau, const std::vector<double> &groundUBs, const std::vector<double> &groundLBs )
	{
		ASSERT( groundLBs.size() == groundUBs.size() );
		ASSERT( initialTableau.size() == expl.size() );
		ASSERT( groundLBs.size() == initialTableau[0].size() - 1 );
		ASSERT( groundLBs.size() == initialTableau[initialTableau.size() - 1].size() - 1 );

		double derived_bound = 0, scalar = 0, temp;
		unsigned n = groundUBs.size();
		std::vector<double> explCopy ( expl );

		// If explanation is all zeros, return original bound
		//TODO maybe add an allZero bit to explanations to improve efficiency
		bool allZeros = std::all_of( expl.begin(), expl.end(), [] ( double x ) { return FloatUtils::isZero( x ); } );

		if ( allZeros )
			return isUpper ? groundUBs[var] : groundLBs[var];

		// Create linear combination of original rows implied from explanation
		std::vector<double> explRowsCombination = std::vector<double>( n, 0 );
		UNSATCertificateUtils::getExplanationRowCombination( explRowsCombination, expl, initialTableau );
		explRowsCombination[var] = 0;

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

		derived_bound += scalar;
		explRowsCombination.clear();
		explCopy.clear();
		return derived_bound;
	}


	static void getExplanationRowCombination( std::vector<double> &explRowsCombination, const std::vector<double> &expl,
											   const std::vector<std::vector<double>> &initialTableau )
	{
		ASSERT( explRowsCombination.size() == initialTableau[0].size() - 1 );
		ASSERT( explRowsCombination.size() == initialTableau[initialTableau.size() - 1].size() - 1 );

		unsigned n = explRowsCombination.size(), m = expl.size();
		for ( unsigned i = 0; i < m; ++i )
		{
			for ( unsigned j = 0; j < n; ++j )
				if ( !FloatUtils::isZero( initialTableau[i][j] ) && !FloatUtils::isZero( expl[i]) )
					explRowsCombination[j] += initialTableau[i][j] * expl[i];

		}

		// Since: 0 = Sum (ci * xi) + c * var = Sum (ci * xi) + (c - 1) * var + var
		// We have: var = - Sum (ci * xi) - (c - 1) * var
		for ( unsigned i = 0; i < n; ++i )
			if ( !FloatUtils::isZero( explRowsCombination[i] ) )
				explRowsCombination[i] *= -1;
			else
				explRowsCombination[i] = 0;


	}
};
#endif //__UNSATCertificate_h__