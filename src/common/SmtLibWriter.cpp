/*********************                                                        */
/*! \file SmtLibWriter.cpp
 ** \verbatim
 ** This file is part of the Marabou project.
 ** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved. See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** [[ Add lengthier description here ]]
 **/

#include "SmtLibWriter.h"
SmtLibWriter::SmtLibWriter()
	:_instances( 0, List<String>( 0, "" ) )
{
}

SmtLibWriter::~SmtLibWriter()
{
	for ( auto instance : _instances )
		instance.clear();

	_instances.clear();
}

void SmtLibWriter::addInstance()
{
	_instances.append( List<String>( 0, "" ) );
}

void SmtLibWriter::addHeader( unsigned n )
{
	auto &instance = _instances.back();
	instance.append( "(set-logic QF_LRA)\n" );
	for ( unsigned i = 0; i < n; ++i )
		instance.append( "(declare-fun x" + std::to_string( i ) + " () Real)\n" );
}

void SmtLibWriter::addFooter()
{
	auto &instance = _instances.back();
	instance.append(  "(check-sat)\n" );
	instance.append(  "(exit)\n" );
}

void SmtLibWriter::addReLUConstraint( unsigned b, unsigned f )
{
	auto &instance = _instances.back();
	instance.append(  "(assert (= x" + std::to_string( f ) + " (ite (>= x" + std::to_string( b ) + " 0) x" + std::to_string( b )+ " 0 ) ) )\n" );
}

void SmtLibWriter::addTableauRow( const SparseUnsortedList &row )
{
	auto &instance = _instances.back();
	unsigned size = row.getSize(), counter = 0;
	String assertRowLine = "(assert ( = 0";
	for ( auto &entry : row )
	{
		if (counter != size - 1)
			assertRowLine += " ( + ( * " + std::to_string( entry._value ) + " x" + std::to_string( entry._index ) + " )";
		else
			assertRowLine += " ( * " + std::to_string( entry._value ) + " x" + std::to_string( entry._index ) + " )";
		++counter;
	}

	assertRowLine += std::string( counter + 2, ')' );
	instance.append( assertRowLine + "\n" );
}

void SmtLibWriter::addGroundUpperBounds( const std::vector<double> &bounds )
{
	auto &instance = _instances.back();
	unsigned n = bounds.size();
	for ( unsigned i = 0; i < n; ++i)
		instance.append( " (assert ( <= x" + std::to_string( i ) + " " + std::to_string( bounds[i] ) + " ) )\n" );
}

void SmtLibWriter::addGroundLowerBounds( const std::vector<double> &bounds )
{
	auto &instance = _instances.back();
	unsigned n = bounds.size();
	for ( unsigned i = 0; i < n; ++i)
		instance.append( " (assert ( >= x" + std::to_string( i ) + " " + std::to_string( bounds[i] ) + " ) )\n" );
}

void SmtLibWriter::writeInstancesToFiles( const std::string &directory )
{
	unsigned counter = 0;
	for( auto &instance : _instances )
	{
		std::ofstream file ( directory + "Delegated" + std::to_string( counter ) + ".smtlib"); //TODO be exact
		for ( String &s : instance )
			file << s;
		file.close();
		++counter;
	}
}