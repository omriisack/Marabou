/*********************                                                        */
/*! \file Contradiction.h
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

#ifndef __Contradiction_h__
#define __Contradiction_h__

#include "Vector.h"

/*
  Contains all info relevant for a simple Marabou contradiction - i.e. explanations of contradicting bounds of a variable
*/
class Contradiction
{
public:
    Contradiction( const Vector<double> &contradictionVec );
    Contradiction( unsigned var );

    ~Contradiction();

    /*
      Getters for all fields
     */
    unsigned getVar() const;
    const double *getContradictionVec() const;

private:
    unsigned _var;
    double *_contradictionVec;
};

#endif //__Contradiction_h__