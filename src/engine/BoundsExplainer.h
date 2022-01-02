/*********************                                                        */
/*! \file BoundsExplainer.h
 ** \verbatim
 ** This file is part of the Marabou project.
 ** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved. See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** [[ Add lengthier description here ]]
 **/

#ifndef __BoundsExplainer_h__
#define __BoundsExplainer_h__
#include "TableauRow.h"
#include "vector"
#include "SparseUnsortedList.h"
#include "assert.h"


/*
 * A class which encapsulates the bounds explanations of a single variable
*/
class SingleVarBoundsExplainer {
public:
	explicit SingleVarBoundsExplainer( unsigned length );

	/*
	* Puts the values of a bound explanation in the array bound.
	*/
	void getVarBoundExplanation( std::vector<double>& bound,  bool isUpper ) const;

	/*
	 * Returns the length of the explanation
	 */
	unsigned getLength() const;

	/*
	 * Updates the values of the bound explanation according to newBound
	 */
	void updateVarBoundExplanation( const std::vector<double>& newBound,  bool isUpper );

	/*
	 * Deep copy of SingleVarBoundsExplainer
	 */
	SingleVarBoundsExplainer& operator=( const SingleVarBoundsExplainer& other );

	/*
	 * Adds an entry for an explanation with given coefficient
	 */
	void addEntry( double coefficient );

	/*
	 * Asserts that all lengths are consistent
	 */
	void assertLengthConsistency() const;

	unsigned _upperRecLevel; // For debugging purpose, TODO delete upon completing
	unsigned _lowerRecLevel;

private:
	unsigned _length;
	std::vector<double> _lower;
	std::vector<double> _upper;
};


/*
  A class which encapsulates the bounds explanations of all variables of a tableau
*/
class BoundsExplainer {
public:
	BoundsExplainer( unsigned varsNum, unsigned rowsNum );

	/*
	 * Returns the number of rows
	 */
	unsigned getRowsNum() const;

	/*
 	* Returns the number of variables
 	*/
	unsigned getVarsNum() const;
	/*
	  Puts the values of a bound explanation in the array bound.
	*/
	void getOneBoundExplanation ( std::vector<double>& bound, unsigned var, bool isUpper ) const;

	/*
	  Puts the values of a bound explanation in the array bound.
	*/
	SingleVarBoundsExplainer& returnWholeVarExplanation( unsigned var );

	/*
	  Given a row, updates the values of the bound explanations of its lhs according to the row
	*/
	void updateBoundExplanation( const TableauRow& row, bool isUpper );

	/*
	  Given a row, updates the values of the bound explanations of a var according to the row
	*/
	void updateBoundExplanation( const TableauRow& row, bool isUpper, unsigned varIndex );

	/*
	Given a row as SparseUnsortedList, updates the values of the bound explanations of a var according to the row
	*/
	void updateBoundExplanationSparse( const SparseUnsortedList& row, bool isUpper, unsigned var );

	/*
	 * Copies all elements of other BoundsExplainer
	 */
	BoundsExplainer& operator=( const BoundsExplainer& other );

	/*
	 * Get the explanations vector
	 */
	std::vector<SingleVarBoundsExplainer>& getExplanations();

	/*
	 * Adds a zero explanation at the end
	 */
	void addZeroExplanation();

	/*
	 * Resets an explanation
	 */
	void resetExplanation ( unsigned var, bool isUpper );

	/*
	 * Artificially updates an explanation, without using the recursive rule
	 */
	void injectExplanation( unsigned var, const std::vector<double>& expl, bool isUpper );

private:
	unsigned _varsNum;
	unsigned _rowsNum;
	std::vector<SingleVarBoundsExplainer> _bounds;

	/*
	  A helper function which adds a multiplication of an array by scalar to another array
	*/
	void addVecTimesScalar( std::vector<double>& sum, const std::vector<double>& input, double scalar ) const;

	/*
	  Upon receiving a row, extract coefficients of the original tableau's equations that creates the row
	  It is merely the coefficients of the slack variables.
	  Assumption - the slack variables indices are always the last m.
	*/
	void extractRowCoefficients( const TableauRow& row, std::vector<double>& coefficients ) const;

	/*
	Upon receiving a row given as a SparseUnsortedList, extract coefficients of the original tableau's equations that creates the row
	It is merely the coefficients of the slack variables.
	Assumption - the slack variables indices are always the last m.
	All coefficients are divided by -ci, the coefficient of the lhs in the row, for normalization.   
	*/
	void extractSparseRowCoefficients( const SparseUnsortedList& row, std::vector<double>& coefficients, double ci ) const;

};
#endif // __BoundsExplainer_h__