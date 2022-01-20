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

#include "BoundsExplainer.h"

/* Functions of BoundsExplainer */
BoundsExplainer::BoundsExplainer( const unsigned varsNum, const unsigned rowsNum )
	:_varsNum( varsNum )
	,_rowsNum( rowsNum )
	,_upperBoundsExplanations( _varsNum, std::vector<double>( 0 ) )
	,_lowerBoundsExplanations( _varsNum,  std::vector<double>( 0 ) )

{
}

BoundsExplainer::~BoundsExplainer()
{
	for ( unsigned i = 0; i < _varsNum; ++i )
	{
		_upperBoundsExplanations[i].clear();
		_lowerBoundsExplanations[i].clear();
	}

    _upperBoundsExplanations.clear();
	_lowerBoundsExplanations.clear();
}

unsigned BoundsExplainer::getRowsNum() const
{
	return _rowsNum;
}

unsigned BoundsExplainer::getVarsNum() const
{
	return _varsNum;
}

BoundsExplainer& BoundsExplainer::operator=( const BoundsExplainer& other )
{
	if ( this == &other )
		return *this;

	ASSERT( _rowsNum == other._rowsNum );
	ASSERT( _varsNum == other._varsNum );
	_rowsNum = other._rowsNum;
	_varsNum = other._varsNum;

	for ( unsigned i = 0; i < _varsNum; ++i )
	{
		if ( _upperBoundsExplanations[i].size() != other._upperBoundsExplanations[i].size() )
		{
			_upperBoundsExplanations[i].clear();
			_upperBoundsExplanations[i].resize( other._upperBoundsExplanations[i].size() );
			_upperBoundsExplanations[i].shrink_to_fit();
		}

		if ( _lowerBoundsExplanations[i].size() != other._lowerBoundsExplanations[i].size() )
		{
			_lowerBoundsExplanations[i].clear();
			_lowerBoundsExplanations[i].resize( other._lowerBoundsExplanations[i].size() );
			_lowerBoundsExplanations[i].shrink_to_fit();
		}

		std::copy( other._upperBoundsExplanations[i].begin(), other._upperBoundsExplanations[i].end(), _upperBoundsExplanations[i].begin() );
		std::copy( other._lowerBoundsExplanations[i].begin(), other._lowerBoundsExplanations[i].end(), _lowerBoundsExplanations[i].begin() );
	}

	return *this;
}

const std::vector<double>& BoundsExplainer::getExplanation( const unsigned var, const bool isUpper )
{
	ASSERT ( var < _varsNum );
	return isUpper ? _upperBoundsExplanations[var] : _lowerBoundsExplanations[var];
}

void BoundsExplainer::updateBoundExplanation( const TableauRow& row, const bool isUpper )
{
	if ( !row._size )
		return;
	bool tempUpper;
	unsigned var = row._lhs, tempVar;  // The var to be updated is the lhs of the row
	double curCoefficient;
	ASSERT ( var < _varsNum );
	ASSERT( row._size == _varsNum || row._size == _varsNum - _rowsNum );
	std::vector<double> rowCoefficients = std::vector<double>( _rowsNum, 0 );
	std::vector<double> sum = std::vector<double>( _rowsNum, 0 );
	std::vector<double> tempBound;

	for ( unsigned i = 0; i < row._size; ++i )
	{
		curCoefficient = row._row[i]._coefficient;
		if ( FloatUtils::isZero( curCoefficient ) ) // If coefficient is zero then nothing to add to the sum
			continue;

		tempUpper = curCoefficient < 0 ? !isUpper : isUpper; // If coefficient is negative, then replace kind of bound
		tempVar = row._row[i]._var;
		tempBound = tempUpper ? _upperBoundsExplanations[tempVar] : _lowerBoundsExplanations[tempVar];
		addVecTimesScalar( sum, tempBound, curCoefficient );
	}

	extractRowCoefficients( row, rowCoefficients ); // Update according to row coefficients
	addVecTimesScalar( sum, rowCoefficients, 1 );
	injectExplanation( sum, var, isUpper );

	rowCoefficients.clear();
	sum.clear();
}

void BoundsExplainer::updateBoundExplanation( const TableauRow& row, const bool isUpper, const unsigned var )
{
	if ( !row._size )
		return;

	ASSERT ( var < _varsNum );
	if ( var == row._lhs )
	{
		updateBoundExplanation( row, isUpper );
		return;
	}

	// Find index of variable
	int varIndex = -1;
	for ( unsigned i = 0; i < row._size; ++i )
		if ( row._row[i]._var == var )
		{
			varIndex = (int) i;
			break;
		}

	ASSERT ( varIndex >= 0 );
	double ci = row[varIndex];
	ASSERT ( ci );  // Coefficient of var cannot be zero.
	double coeff = - 1 / ci;
	// Create a new row with var as its lhs
	TableauRow equiv = TableauRow( row._size );
	equiv._lhs = var;
	equiv._scalar = FloatUtils::isZero( row._scalar ) ? 0 :  row._scalar * coeff;

	for ( unsigned i = 0; i < row._size; ++i )
	{
		equiv._row[i]._coefficient = FloatUtils::isZero( row[i] ) ? 0 :  row[i] * coeff;
		equiv._row[i]._var = row._row[i]._var;
	}

	// Since the original var is the new lhs, the new var should be replaced with original lhs
	equiv._row[varIndex]._coefficient = -coeff;
	equiv._row[varIndex]._var = row._lhs;

	updateBoundExplanation( equiv, isUpper );
}

void BoundsExplainer::updateBoundExplanationSparse( const SparseUnsortedList& row, const bool isUpper, const unsigned var )
{
	if ( row.empty() )
		return;

	ASSERT( var < _varsNum );
	bool tempUpper;
	double curCoefficient, ci = 0, realCoefficient;
	for ( const auto& entry : row )
		if ( entry._index == var )
		{
			ci = entry._value;
			break;
		}
	ASSERT( !FloatUtils::isZero( ci ) );
	std::vector<double> rowCoefficients = std::vector<double>( _rowsNum, 0 );
	std::vector<double> sum = std::vector<double>( _rowsNum, 0 );
	std::vector<double> tempBound;

	for ( const auto& entry : row )
	{
		curCoefficient = entry._value;
		if ( FloatUtils::isZero( curCoefficient ) || entry._index == var ) // If coefficient is zero then nothing to add to the sum, also skip var
			continue;
		realCoefficient = curCoefficient / -ci;
		if ( FloatUtils::isZero( realCoefficient ) )
			continue;

		tempUpper =  ( isUpper && FloatUtils::isPositive( realCoefficient ) ) ||  ( !isUpper && FloatUtils::isNegative( realCoefficient ) ); // If coefficient of lhs and var are different, use same bound
		tempBound = tempUpper ? _upperBoundsExplanations[entry._index] : _lowerBoundsExplanations[entry._index];
		addVecTimesScalar( sum, tempBound, realCoefficient );
	}

	extractSparseRowCoefficients( row, rowCoefficients, ci ); // Update according to row coefficients
	addVecTimesScalar( sum, rowCoefficients, 1 );
	injectExplanation( sum, var, isUpper );

	rowCoefficients.clear();
	sum.clear();
}

void BoundsExplainer::addVecTimesScalar( std::vector<double>& sum, const std::vector<double>& input, const double scalar ) const
{
	if ( input.empty() || FloatUtils::isZero( scalar ) )
		return;

	ASSERT( sum.size() == _rowsNum && input.size() == _rowsNum );

	for ( unsigned i = 0; i < _rowsNum; ++i )
		sum[i] += scalar * input[i];
}

void BoundsExplainer::extractRowCoefficients( const TableauRow& row, std::vector<double>& coefficients ) const
{
	ASSERT( coefficients.size() == _rowsNum && ( row._size == _varsNum  || row._size == _varsNum - _rowsNum ) );
	//The coefficients of the row m highest-indices vars are the coefficients of slack variables
	for ( unsigned i = 0; i < row._size; ++i )
		if ( row._row[i]._var >= _varsNum - _rowsNum && !FloatUtils::isZero( row._row[i]._coefficient ) )
			coefficients[row._row[i]._var - _varsNum + _rowsNum] = row._row[i]._coefficient;

	if ( row._lhs >= _varsNum - _rowsNum ) //If the lhs was part of original basis, its coefficient is -1
		coefficients[row._lhs - _varsNum + _rowsNum] = -1;
}


void BoundsExplainer::extractSparseRowCoefficients( const SparseUnsortedList& row, std::vector<double>& coefficients, double ci ) const
{
	ASSERT( coefficients.size() == _rowsNum );

	//The coefficients of the row m highest-indices vars are the coefficients of slack variables
	for ( const auto& entry : row )
		if ( entry._index >= _varsNum - _rowsNum && !FloatUtils::isZero( entry._value ) )
			coefficients[entry._index - _varsNum + _rowsNum] = - entry._value / ci;
}

void BoundsExplainer::addVariable()
{
	_rowsNum += 1;
	_varsNum += 1;
	_upperBoundsExplanations.emplace_back( std::vector<double>( 0 ) );
	_lowerBoundsExplanations.emplace_back( std::vector<double>( 0 ) );

	for ( unsigned i = 0; i < _varsNum; ++i)
	{
		_upperBoundsExplanations[i].push_back( 0 );
		_lowerBoundsExplanations[i].push_back( 0 );
	}
}

void BoundsExplainer::resetExplanation( const unsigned var, const bool isUpper )
{
	isUpper ? _upperBoundsExplanations[var].clear() : _lowerBoundsExplanations[var].clear();
}

void BoundsExplainer::injectExplanation( const std::vector<double>& expl, unsigned var, bool isUpper )
{
	std::vector<double> *temp = isUpper ? &_upperBoundsExplanations[var] : &_lowerBoundsExplanations[var];
	if ( temp->size() != expl.size() )
		temp->resize( expl.size() );

	std::copy( expl.begin(), expl.end(), temp->begin() );
}
