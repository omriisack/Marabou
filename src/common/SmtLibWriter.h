/*********************                                                        */
/*! \file SmtLibWriter.h
 ** \verbatim
 ** This file is part of the Marabou project.
 ** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved. See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** [[ Add lengthier description here ]]
 **/

#ifndef MARABOU_SMTLIBWRITER_H
#define MARABOU_SMTLIBWRITER_H
#include "List.h"
#include "MString.h"
#include "SparseUnsortedList.h"
#include "vector"
#include <fstream>


/*
 * A class responsible for writing instances of LP+PLC into SMTLIB format
 */
class SmtLibWriter
{
public:
	SmtLibWriter();
	~SmtLibWriter();

	/*
	 * Add a new instance to be written in SMTLIB format
	 */
	void addInstance();

	/*
 	* Adds a SMTLIB header to the current SMTLIB instance
	* n is used to declare all variables
 	*/
	void addHeader( unsigned n );

	/*
 	* Adds a SMTLIB footer to the current SMTLIB instance
 	*/
	void addFooter();

	/*
	 * Adds a line representing ReLU constraint, in SMTLIB format, to the current SMTLIB instance
	 */
	void addReLUConstraint( unsigned b, unsigned f );

	/*
 	* Adds a line representing a Tableau Row, in SMTLIB format, to the current SMTLIB instance
 	*/
	void addTableauRow( const SparseUnsortedList &row );

	/*
 	* Adds lines representing a the ground upper bounds, in SMTLIB format, to the current SMTLIB instance
 	*/
	void addGroundUpperBounds( const std::vector<double> &bounds );

	/*
 	* Adds lines representing a the ground lower bounds, in SMTLIB format, to the current SMTLIB instance
 	*/
	void addGroundLowerBounds( const std::vector<double> &bounds );

	/*
	 * Write the instances to files
	 */
	void writeInstancesToFiles( const std::string &directory );

private:
	List<List<String>> _instances;
};

#endif //MARABOU_SMTLIBWRITER_H
