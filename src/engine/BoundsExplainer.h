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

/*
  A class which encapsulates the bounds explanations of all variables of a tableau
*/
class BoundsExplainer {
public:
	BoundsExplainer( unsigned varsNum, unsigned rowsNum );

	~BoundsExplainer();

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
	const std::vector<double>& returnWholeVarExplanation( unsigned var, bool isUpper );

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
	 * Adds a zero explanation at the end
	 */
	void addVariable();

	/*
	 * Resets an explanation
	 */
	void resetExplanation ( unsigned var, bool isUpper );

	/*
	 * Artificially updates an explanation, without using the recursive rule
	 */
	void injectExplanation( const std::vector<double>& expl, unsigned var, bool isUpper );

private:
	unsigned _varsNum;
	unsigned _rowsNum;
	std::vector<std::vector<double>> _upperBoundsExplanations;
	std::vector<std::vector<double>> _lowerBoundsExplanations;

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