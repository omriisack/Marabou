/*********************                                                        */
/*! \file Checker.cpp
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

#include "Checker.h"

Checker::Checker( const UnsatCertificateNode *root,
                  unsigned proofSize,
                  const SparseMatrix *initialTableau,
                  const Vector<double> &groundUpperBounds,
                  const Vector<double> &groundLowerBounds,
                  const List<PiecewiseLinearConstraint *> &problemConstraints )
    : _root( root )
    , _proofSize( proofSize )
    , _initialTableau( initialTableau )
    , _groundUpperBounds( groundUpperBounds )
    , _groundLowerBounds( groundLowerBounds )
    , _problemConstraints( problemConstraints )
    , _delegationCounter( 0 )
{
    for ( auto constraint : problemConstraints )
      constraint->setPhaseStatus( PHASE_NOT_FIXED );
}

bool Checker::check()
{
    return checkNode( _root );
}

bool Checker::checkNode( const UnsatCertificateNode *node )
{
    Vector<double> groundUpperBoundsBackup( _groundUpperBounds );
    Vector<double> groundLowerBoundsBackup( _groundLowerBounds );

    _upperBoundChanges.push( {} );
    _lowerBoundChanges.push( {} );

    // Update ground bounds according to head split
    for ( const auto &tightening : node->getSplit().getBoundTightenings() )
    {
        auto &temp = tightening._type == Tightening::UB ? _groundUpperBounds : _groundLowerBounds;
        temp[tightening._variable] = tightening._value;

        tightening._type == Tightening::UB ? _upperBoundChanges.top().insert( tightening._variable ) : _lowerBoundChanges.top().insert( tightening._variable );
    }

    // Check all PLC bound propagations
    if ( !checkAllPLCExplanations( node, GlobalConfiguration::LEMMA_CERTIFICATION_TOLERANCE ) )
        return false;

    // Save to file if marked
    if ( node->getDelegationStatus() == DelegationStatus::DELEGATE_SAVE )
        writeToFile();

    // Skip if leaf has the SAT solution, or if was marked to delegate
    if ( node->getSATSolutionFlag() || node->getDelegationStatus() != DelegationStatus::DONT_DELEGATE )
        return true;

    // Check if it is a leaf, and if so use contradiction to check
    // return true iff it is certified
    if ( node->isValidLeaf() )
        return checkContradiction( node );

    // If not a valid leaf, skip only if it is leaf that was not visited
    if ( !node->getVisited() && !node->getContradiction() && node->getChildren().empty() )
        return true;

    // Otherwise, should be a valid non-leaf node
    if ( !node->isValidNonLeaf() )
        return false;

    // If so, check all children and return true iff all children are certified
    // Also make sure that they are split correctly (i.e by ReLU constraint or by a single var)
    bool answer = true;
    List<PiecewiseLinearCaseSplit> childrenSplits;

    for ( const auto &child : node->getChildren() )
        childrenSplits.append( child->getSplit() );

    auto *childrenSplitConstraint = getCorrespondingConstraint( childrenSplits );

    if ( !checkSingleVarSplits( childrenSplits ) && !childrenSplitConstraint )
        return false;

    for ( const auto &child : node->getChildren() )
    {
        // Fix the phase of the constraint corresponding to the children
        if ( childrenSplitConstraint && childrenSplitConstraint->getType() == PiecewiseLinearFunctionType::RELU )
        {
            auto tightenings = child->getSplit().getBoundTightenings();
            if ( tightenings.front()._type == Tightening::LB || tightenings.back()._type == Tightening::LB  )
                childrenSplitConstraint->setPhaseStatus( RELU_PHASE_ACTIVE );
            else
                childrenSplitConstraint->setPhaseStatus( RELU_PHASE_INACTIVE );
        }

        else if ( childrenSplitConstraint && childrenSplitConstraint->getType() == PiecewiseLinearFunctionType::SIGN )
        {
            auto tightenings = child->getSplit().getBoundTightenings();
            if ( tightenings.front()._type == Tightening::LB  )
                childrenSplitConstraint->setPhaseStatus( SIGN_PHASE_POSITIVE );
            else
                childrenSplitConstraint->setPhaseStatus( SIGN_PHASE_NEGATIVE );
        }
        else if ( childrenSplitConstraint && childrenSplitConstraint->getType() == PiecewiseLinearFunctionType::ABSOLUTE_VALUE )
        {
            auto tightenings = child->getSplit().getBoundTightenings();
            if ( tightenings.front()._type == Tightening::LB )
                childrenSplitConstraint->setPhaseStatus( ABS_PHASE_POSITIVE );
            else
                childrenSplitConstraint->setPhaseStatus( ABS_PHASE_NEGATIVE );
        }
        else if ( childrenSplitConstraint && childrenSplitConstraint->getType() == PiecewiseLinearFunctionType::DISJUNCTION )
            ( ( DisjunctionConstraint * )childrenSplitConstraint )->removeFeasibleDisjunct( child->getSplit() );

        if ( !checkNode( child ) )
            answer = false;
    }

    // Revert all changes
    if ( childrenSplitConstraint )
        childrenSplitConstraint->setPhaseStatus( PHASE_NOT_FIXED );

    if ( childrenSplitConstraint && childrenSplitConstraint->getType() == PiecewiseLinearFunctionType::DISJUNCTION )
    {
        for ( const auto &child : node->getChildren() )
            ( ( DisjunctionConstraint * )childrenSplitConstraint )->addFeasibleDisjunct( child->getSplit() );
    }

    // Revert only bounds that where changed during checking the current node
    for ( unsigned i : _upperBoundChanges.top() )
        _groundUpperBounds[i] = groundUpperBoundsBackup[i];

    for ( unsigned i : _lowerBoundChanges.top() )
        _groundLowerBounds[i] = groundLowerBoundsBackup[i];

    _upperBoundChanges.pop();
    _lowerBoundChanges.pop();

    return answer;
}

bool Checker::checkContradiction( const UnsatCertificateNode *node ) const
{
    ASSERT( node->isValidLeaf() && !node->getSATSolutionFlag() );
    const double *contradiction = node->getContradiction()->getContradiction();

    if ( contradiction == NULL )
    {
        unsigned infeasibleVar = node->getContradiction()->getVar();
        return FloatUtils::isNegative( _groundUpperBounds[infeasibleVar] - _groundLowerBounds[infeasibleVar] );
    }

    double contradictionUpperBound = UNSATCertificateUtils::computeCombinationUpperBound( contradiction, _initialTableau, _groundUpperBounds.data(), _groundLowerBounds.data(), _proofSize, _groundUpperBounds.size() );

    return FloatUtils::isNegative( contradictionUpperBound );
}

bool Checker::checkAllPLCExplanations( const UnsatCertificateNode *node, double epsilon )
{
    // Create copies of the gb, check for their validity, and pass these changes to all the children
    // Assuming the splits of the children are ok.
    // NOTE, this will change as PLCLemma will
    for ( const auto &plcExplanation : node->getPLCExplanations() )
    {
        PiecewiseLinearConstraint *matchedConstraint = NULL;
        bool tighteningMatched = false;
        unsigned causingVar = plcExplanation->getCausingVar();
        unsigned affectedVar = plcExplanation->getAffectedVar();

        // Make sure propagation was made by a problem constraint
        for ( const auto &constraint : _problemConstraints )
            if ( constraint->getParticipatingVariables().exists( affectedVar ) && constraint->getParticipatingVariables().exists( causingVar ) )
                matchedConstraint = constraint;

        if ( !matchedConstraint )
            return false;

        PiecewiseLinearFunctionType constraintType = matchedConstraint->getType();

        if ( constraintType == RELU )
          tighteningMatched = checkReluLemma( *plcExplanation, *matchedConstraint, epsilon );
        else if ( constraintType == SIGN )
            tighteningMatched = checkSignLemma( *plcExplanation, *matchedConstraint, epsilon );
        else if ( constraintType == ABSOLUTE_VALUE )
            tighteningMatched = checkAbsLemma( *plcExplanation, *matchedConstraint, epsilon );
        else if ( constraintType == MAX )
            tighteningMatched = checkMaxLemma( *plcExplanation, *matchedConstraint, epsilon );
        else
            return false;

        double bound = plcExplanation->getBound();
        // If so, update the ground bounds and continue
        BoundType affectedVarBound = plcExplanation->getAffectedVarBound();

        auto &temp = affectedVarBound == UPPER ? _groundUpperBounds : _groundLowerBounds;
        bool isTighter = affectedVarBound == UPPER ? FloatUtils::lt( bound, temp[affectedVar] ) : FloatUtils::gt( bound, temp[affectedVar] );
        if ( isTighter && tighteningMatched )
        {
            temp[affectedVar] = bound;
            ASSERT( !_upperBoundChanges.empty() && !_lowerBoundChanges.empty() );
            affectedVarBound == UPPER ? _upperBoundChanges.top().insert( affectedVar ) : _lowerBoundChanges.top().insert( affectedVar );
        }
    }
    return true;
}

double Checker::explainBound( unsigned var, bool isUpper, const double *explanation ) const
{
    return UNSATCertificateUtils::computeBound( var, isUpper, explanation, _initialTableau, _groundUpperBounds.data(), _groundLowerBounds.data(), _proofSize, _groundUpperBounds.size() );
}

PiecewiseLinearConstraint *Checker::getCorrespondingConstraint( const List<PiecewiseLinearCaseSplit> &splits )
{
    auto constraint = getCorrespondingReluConstraint( splits );
    if ( !constraint )
        constraint = getCorrespondingSignConstraint( splits );
    if ( !constraint )
        constraint = getCorrespondingAbsConstraint( splits );
    if ( !constraint )
        constraint = getCorrespondingMaxConstraint( splits );
    if ( !constraint )
        constraint = getCorrespondingDisjunctionConstraint( splits );

    return constraint;
}

void Checker::writeToFile()
{
    List<String> leafInstance;

    // Write with SmtLibWriter
    unsigned b, f;
    unsigned m = _proofSize;
    unsigned n = _groundUpperBounds.size();

    SmtLibWriter::addHeader( n, leafInstance );
    SmtLibWriter::addGroundUpperBounds( _groundUpperBounds, leafInstance );
    SmtLibWriter::addGroundLowerBounds( _groundLowerBounds, leafInstance );

    auto tableauRow = SparseUnsortedList();

    for ( unsigned i = 0; i < m; ++i )
    {
        tableauRow = SparseUnsortedList();
        _initialTableau->getRow( i, &tableauRow );

        // Fix correct size
        if ( !tableauRow.getSize() && !tableauRow.empty() )
        for ( auto it = tableauRow.begin(); it != tableauRow.end(); ++it )
            tableauRow.incrementSize();

        SmtLibWriter::addTableauRow( tableauRow , leafInstance );
    }

    for ( auto &constraint : _problemConstraints )
    {
        auto vars = constraint->getParticipatingVariables();
        if ( constraint->getType() == RELU )
        {
            b = vars.front();
            vars.popBack();
            f = vars.back();
            SmtLibWriter::addReLUConstraint( b, f, constraint->getPhaseStatus(), leafInstance );
        }
        else if ( constraint->getType() == SIGN )
        {
            b = vars.front();
            f = vars.back();
            SmtLibWriter::addSignConstraint( b, f, constraint->getPhaseStatus(), leafInstance );
        }
        else if ( constraint->getType() == ABSOLUTE_VALUE )
        {
            b = vars.front();
            f = vars.back();
            SmtLibWriter::addAbsConstraint( b, f, constraint->getPhaseStatus(), leafInstance );
        }
        else if ( constraint->getType() == MAX )
            SmtLibWriter::addMaxConstraint( constraint->getParticipatingVariables().back(), ( ( MaxConstraint * )constraint )->getParticipatingElements(), leafInstance );
        else if ( constraint->getType() == DISJUNCTION )
            SmtLibWriter::addDisjunctionConstraint( ( ( DisjunctionConstraint * )constraint )->getFeasibleDisjuncts(), leafInstance );
    }

    SmtLibWriter::addFooter( leafInstance );
    File file ( "delegated" + std::to_string( _delegationCounter ) + ".smtlib" );
    SmtLibWriter::writeInstanceToFile( file, leafInstance );

    ++_delegationCounter;
}

bool Checker::checkSingleVarSplits( const List<PiecewiseLinearCaseSplit> &splits )
{
    if ( splits.size() != 2 )
        return false;

    // These are singletons to tightenings
    auto &frontSplitTightenings = splits.front().getBoundTightenings();
    auto &backSplitTightenings = splits.back().getBoundTightenings();

    if ( frontSplitTightenings.size() != 1 || backSplitTightenings.size() != 1 )
        return false;

    // These are the elements in the singletons
    auto &frontSplitOnlyTightening = frontSplitTightenings.front();
    auto &backSplitOnlyTightening = backSplitTightenings.front();

    // Check that cases are of the same var and bound, where the for one the bound is UB, and for the other is LB
    if ( frontSplitOnlyTightening._variable != backSplitOnlyTightening._variable )
        return false;

    if ( FloatUtils::areDisequal( frontSplitOnlyTightening._value, backSplitOnlyTightening._value ) )
        return false;

    if ( frontSplitOnlyTightening._type == backSplitOnlyTightening._type )
        return false;

    return true;
}

PiecewiseLinearConstraint *Checker::getCorrespondingReluConstraint( const List<PiecewiseLinearCaseSplit> &splits )
{
    if ( splits.size() != 2 )
        return NULL;

    auto firstSplitTightenings = splits.front().getBoundTightenings();
    auto secondSplitTightenings = splits.back().getBoundTightenings();

    // Find the LB tightening, its var is b
    auto &activeSplit = firstSplitTightenings.front()._type == Tightening::LB ? firstSplitTightenings : secondSplitTightenings;
    auto &inactiveSplit = firstSplitTightenings.front()._type == Tightening::LB ? secondSplitTightenings : firstSplitTightenings;

    unsigned b = activeSplit.front()._variable;
    unsigned aux = activeSplit.back()._variable;
    unsigned f = inactiveSplit.back()._variable;

    // Aux var may or may not be used
    if ( ( activeSplit.size() != 2 && activeSplit.size() != 1 ) || inactiveSplit.size() != 2 )
        return NULL;

    if ( FloatUtils::areDisequal( inactiveSplit.back()._value, 0.0 ) || FloatUtils::areDisequal( inactiveSplit.front()._value, 0.0 ) || FloatUtils::areDisequal( activeSplit.back()._value, 0.0 ) || FloatUtils::areDisequal( activeSplit.front()._value, 0.0 ) )
        return NULL;

    // Check that f = b + aux corresponds to a problem constraints
    PiecewiseLinearConstraint *correspondingConstraint = NULL;
    for ( auto &constraint : _problemConstraints )
    {
        auto constraintVars = constraint->getParticipatingVariables();
        if ( constraint->getType() == PiecewiseLinearFunctionType::RELU && constraintVars.front() == b && constraintVars.exists( f ) && ( activeSplit.size() == 1 || constraintVars.back() == aux ) )
            correspondingConstraint = constraint;
    }

    // Return the constraint for which f=relu(b)
    return correspondingConstraint;
}

PiecewiseLinearConstraint *Checker::getCorrespondingSignConstraint( const List<PiecewiseLinearCaseSplit> &splits )
{
    if ( splits.size() != 2 )
        return NULL;

    auto firstSplitTightenings = splits.front().getBoundTightenings();
    auto secondSplitTightenings = splits.back().getBoundTightenings();

    // Find an LB tightening, it marks the positive split
    auto &positiveSplit = firstSplitTightenings.front()._type == Tightening::LB ? firstSplitTightenings : secondSplitTightenings;
    auto &negativeSplit = firstSplitTightenings.front()._type == Tightening::LB ? secondSplitTightenings : firstSplitTightenings;

    unsigned b = positiveSplit.back()._variable;
    unsigned f = positiveSplit.front()._variable;

    // Check details of both constraints - value and types
    if ( positiveSplit.size() != 2 || negativeSplit.size() != 2 || positiveSplit.back()._type != Tightening::LB || positiveSplit.front()._type != Tightening::LB || negativeSplit.back()._type != Tightening::UB || negativeSplit.front()._type != Tightening::UB )
        return NULL;

    if ( FloatUtils::areDisequal( negativeSplit.back()._value, -1.0 ) || FloatUtils::areDisequal( negativeSplit.front()._value, 0.0 ) ||  FloatUtils::areDisequal(positiveSplit.back()._value, 1.0 ) || FloatUtils::areDisequal( positiveSplit.front()._value, 0.0 ))
        return NULL;

    // Check that f=sign(b) corresponds to a problem constraints
    PiecewiseLinearConstraint *correspondingConstraint = NULL;
    for ( auto &constraint : _problemConstraints )
    {
        auto constraintVars = constraint->getParticipatingVariables();
        if ( constraint->getType() == PiecewiseLinearFunctionType::SIGN && constraintVars.back() == b && constraintVars.front() == f  )
            correspondingConstraint = constraint;
    }

    // Return the constraint for which f=sign(b)
    return correspondingConstraint;
}

PiecewiseLinearConstraint *Checker::getCorrespondingAbsConstraint( const List<PiecewiseLinearCaseSplit> &splits )
{
    if ( splits.size() != 2 )
        return NULL;

    auto firstSplitTightenings = splits.front().getBoundTightenings();
    auto secondSplitTightenings = splits.back().getBoundTightenings();

    // Find an LB tightening, it marks the positive split
    auto &positiveSplit = firstSplitTightenings.front()._type == Tightening::LB ? firstSplitTightenings : secondSplitTightenings;
    auto &negativeSplit = firstSplitTightenings.front()._type == Tightening::LB ? secondSplitTightenings : firstSplitTightenings;

    unsigned b = positiveSplit.front()._variable;
    unsigned posAux = positiveSplit.back()._variable;

    unsigned negAux = negativeSplit.back()._variable;
    // Check details of both constraints - value and types
    if ( positiveSplit.size() != 2 || negativeSplit.size() != 2 || positiveSplit.back()._type != Tightening::UB || positiveSplit.front()._type != Tightening::LB || negativeSplit.back()._type != Tightening::UB || negativeSplit.front()._type != Tightening::UB )
        return NULL;

    if ( FloatUtils::areDisequal( negativeSplit.back()._value, 0.0 ) || FloatUtils::areDisequal( negativeSplit.front()._value, 0.0 ) ||  FloatUtils::areDisequal(positiveSplit.back()._value, 0.0 ) || FloatUtils::areDisequal( positiveSplit.front()._value, 0.0 ))
        return NULL;

    // Check that f=sign(b) corresponds to a problem constraints
    PiecewiseLinearConstraint *correspondingConstraint = NULL;
    for ( auto &constraint : _problemConstraints )
    {
        auto constraintVars = constraint->getParticipatingVariables();
        if ( constraint->getType() == PiecewiseLinearFunctionType::ABSOLUTE_VALUE && constraintVars.front() == b && constraintVars.back() == negAux && constraintVars.exists( posAux )  )
            correspondingConstraint = constraint;
    }

    // Return the constraint for which f=abs(b)
    return correspondingConstraint;
}

PiecewiseLinearConstraint *Checker::getCorrespondingMaxConstraint( const List<PiecewiseLinearCaseSplit> &splits )
{
    bool constraintMatched = true;

    for ( auto &constraint : _problemConstraints )
    {
        if ( constraint->getType() != MAX )
            continue;

        // When checking, it is possible that the problem constraints already eliminated elements that appear in the proof
        List<PiecewiseLinearCaseSplit> constraintSplits = constraint->getCaseSplits();
        for ( auto const &element : ( ( MaxConstraint * ) constraint )->getEliminatedElements() )
        {
            PiecewiseLinearCaseSplit eliminatedElementCaseSplit;
            eliminatedElementCaseSplit.storeBoundTightening( Tightening( element, 0, Tightening::UB ) );
            constraintSplits.append( eliminatedElementCaseSplit );
        }

        for ( const auto &split : splits )
            if ( !constraintSplits.exists( split ) )
                constraintMatched = false;

        if ( constraintMatched )
            return constraint;
    }
    return NULL;
}

PiecewiseLinearConstraint *Checker::getCorrespondingDisjunctionConstraint( const List<PiecewiseLinearCaseSplit> & splits )
{
    bool constraintMatched = true;

    for ( auto &constraint : _problemConstraints )
    {
        if ( constraint->getType() != DISJUNCTION )
            continue;

        // constraintMatched will remain true iff the splits list is the same as the list of disjuncts (up to order)
        for ( const auto &split : constraint->getCaseSplits() )
            if ( !splits.exists( split ) )
                constraintMatched = false;

        for ( const auto &split : splits )
            if ( !constraint->getCaseSplits().exists( split ) )
                constraintMatched = false;

        if ( constraintMatched )
            return constraint;
    }
    return NULL;
}

bool Checker::checkReluLemma(const PLCLemma &expl, PiecewiseLinearConstraint &constraint, double epsilon )
{
    ASSERT( constraint.getType() == RELU && expl.getConstraintType() == RELU && epsilon > 0 );

    unsigned causingVar = expl.getCausingVar();
    unsigned affectedVar = expl.getAffectedVar();
    double bound = expl.getBound();
    const double *explanation = expl.getExplanation();
    BoundType causingVarBound = expl.getCausingVarBound();
    BoundType affectedVarBound = expl.getAffectedVarBound();

    double explainedBound = UNSATCertificateUtils::computeBound( causingVar, causingVarBound == UPPER, explanation, _initialTableau, _groundUpperBounds.data(), _groundLowerBounds.data(), _proofSize, _groundUpperBounds.size() );

    bool tighteningMatched = false;
    List<unsigned> constraintVars = constraint.getParticipatingVariables();
    ASSERT(constraintVars.size() == 3 );

    Vector<unsigned> conVec( constraintVars.begin(), constraintVars.end() );
    unsigned b = conVec[0];
    unsigned f = conVec[1];
    unsigned aux = conVec[2];

    // If explanation is phase fixing, mark it
    if ( ( affectedVarBound == LOWER && affectedVar == f && FloatUtils::isPositive( bound ) ) || ( affectedVarBound == UPPER && affectedVar == aux && FloatUtils::isZero( bound ) ) )
        constraint.setPhaseStatus( RELU_PHASE_ACTIVE );
    else if ( ( affectedVarBound == LOWER && affectedVar == aux && FloatUtils::isPositive( bound ) ) || ( affectedVarBound == UPPER && affectedVar == f && FloatUtils::isZero( bound ) ) )
        constraint.setPhaseStatus( RELU_PHASE_INACTIVE );

    // Make sure the explanation is explained using a ReLU bound tightening. Cases are matching each rule in ReluConstraint.cpp
    // We allow explained bound to be tighter than the ones recorded (since an explanation can explain tighter bounds), and an epsilon sized error is tolerated.

    // If lb of b is non negative, then ub of aux is 0
    if ( causingVar == b && causingVarBound == LOWER && affectedVar == aux && affectedVarBound == UPPER && FloatUtils::isZero( bound ) && !FloatUtils::isNegative( explainedBound + epsilon ) )
        tighteningMatched = true;

    // If lb of f is positive, then ub if aux is 0
    else if ( causingVar == f && causingVarBound == LOWER && affectedVar == aux && affectedVarBound == UPPER && FloatUtils::isZero( bound ) && FloatUtils::isPositive( explainedBound + epsilon ) )
        tighteningMatched = true;

    // If lb of b is positive x, then ub of aux is -x
    else if ( causingVar == b && causingVarBound == LOWER && affectedVar == aux && affectedVarBound == UPPER && FloatUtils::gte( explainedBound, - bound - epsilon ) && bound > 0 )
        tighteningMatched = true;

    // If lb of aux is positive, then ub of f is 0
    else if ( causingVar == aux && causingVarBound ==LOWER && affectedVar == f && affectedVarBound == UPPER && FloatUtils::isZero( bound ) && FloatUtils::isPositive( explainedBound + epsilon ) )
        tighteningMatched = true;

    // If lb of f is negative, then it is 0
    else if ( causingVar == f && causingVarBound == LOWER && affectedVar == f && affectedVarBound == LOWER && FloatUtils::isZero( bound ) && FloatUtils::isNegative( explainedBound - epsilon ) )
        tighteningMatched = true;

    // Propagate ub from f to b
    else if ( causingVar == f && causingVarBound == UPPER && affectedVar == b && affectedVarBound == UPPER && FloatUtils::lte( explainedBound, bound + epsilon ) )
        tighteningMatched = true;

    // If ub of b is non positive, then ub of f is 0
    else if ( causingVar == b && causingVarBound == UPPER && affectedVar == f && affectedVarBound == UPPER && FloatUtils::isZero( bound ) && !FloatUtils::isPositive( explainedBound - epsilon ) )
        tighteningMatched = true;

    // If ub of b is non positive x, then lb of aux is -x
    else if ( causingVar == b && causingVarBound == UPPER && affectedVar == aux && affectedVarBound == LOWER && bound > 0 && !FloatUtils::isPositive( explainedBound - epsilon ) && FloatUtils::lte( explainedBound, - bound + epsilon ) )
        tighteningMatched = true;

    // If ub of b is positive, then propagate to f ( positivity of explained bound is not checked since negative explained ub can always explain positive bound )
    else if ( causingVar == b && causingVarBound == UPPER && affectedVar == f && affectedVarBound == UPPER && FloatUtils::isPositive( bound ) && FloatUtils::lte( explainedBound,  bound + epsilon ) )
        tighteningMatched = true;

    // If ub of aux is x, then lb of b is -x
    else if ( causingVar == aux && causingVarBound == UPPER && affectedVar == b && affectedVarBound == LOWER && FloatUtils::lte( explainedBound, - bound + epsilon ) )
        tighteningMatched = true;

    return tighteningMatched;
}

bool Checker::checkSignLemma(const PLCLemma &expl, PiecewiseLinearConstraint &constraint, double epsilon )
{
    ASSERT( constraint.getType() == SIGN && expl.getConstraintType() == SIGN && epsilon > 0 );

    unsigned causingVar = expl.getCausingVar();
    unsigned affectedVar = expl.getAffectedVar();
    double bound = expl.getBound();
    const double *explanation = expl.getExplanation();
    BoundType causingVarBound = expl.getCausingVarBound();
    BoundType affectedVarBound = expl.getAffectedVarBound();

    double explainedBound = UNSATCertificateUtils::computeBound( causingVar, causingVarBound == UPPER, explanation, _initialTableau, _groundUpperBounds.data(), _groundLowerBounds.data(), _proofSize, _groundUpperBounds.size() );
    List<unsigned> constraintVars = constraint.getParticipatingVariables();
    ASSERT(constraintVars.size() == 2 );

    unsigned b = constraintVars.front();
    unsigned f = constraintVars.back();

    bool tighteningMatched = false;

    // Any explanation is phase fixing
    if ( ( affectedVarBound == LOWER && affectedVar == f && FloatUtils::gt( bound, -1 ) ) || ( affectedVarBound == LOWER && affectedVar == b && !FloatUtils::isNegative( bound ) ) )
        constraint.setPhaseStatus( SIGN_PHASE_POSITIVE );
    else if ( ( affectedVarBound == UPPER && affectedVar == f && FloatUtils::gt( bound, 1 ) ) || ( affectedVarBound == UPPER && affectedVar == b && FloatUtils::isNegative( bound ) ) )
        constraint.setPhaseStatus( SIGN_PHASE_NEGATIVE );

    // Make sure the explanation is explained using a sign bound tightening. Cases are matching each rule in SignConstraint.cpp
    // We allow explained bound to be tighter than the ones recorded (since an explanation can explain tighter bounds), and an epsilon sized error is tolerated.


    // If lb of f is > -1, then lb of f is 1
    if ( causingVar == f && causingVarBound == LOWER && affectedVar == f && affectedVarBound == LOWER && FloatUtils::areEqual( bound, 1 ) && FloatUtils::gte( explainedBound + epsilon, -1 ) )
        tighteningMatched = true;

    // If lb of f is > -1, then lb of b is 0
    else if ( causingVar == f && causingVarBound == LOWER && affectedVar == b && affectedVarBound == LOWER && FloatUtils::isZero( bound ) && FloatUtils::gte( explainedBound + epsilon, -1 ) )
        tighteningMatched = true;

    // If lb of b is non negative, then lb of f is 1
    else if ( causingVar == b && causingVarBound == LOWER && affectedVar == f && affectedVarBound == LOWER && FloatUtils::areEqual( bound, 1 ) && !FloatUtils::isNegative( explainedBound + epsilon ) )
        tighteningMatched = true;

    // If ub of f is < 1, then ub of f is -1
    else if ( causingVar == f && causingVarBound == UPPER && affectedVar == f && affectedVarBound == UPPER && FloatUtils::areEqual( bound, -1 ) && FloatUtils::lte( explainedBound - epsilon, 1 ) )
        tighteningMatched = true;

    // If ub of f is < 1, then ub of b is 0
    else if ( causingVar == f && causingVarBound == UPPER && affectedVar == b && affectedVarBound == UPPER && FloatUtils::isZero( bound ) && FloatUtils::lte( explainedBound - epsilon, 1 ) )
        tighteningMatched = true;

    // If ub of b is negative, then ub of f is -1
    else if ( causingVar == b && causingVarBound == UPPER && affectedVar == f && affectedVarBound == UPPER && FloatUtils::areEqual( bound, -1 ) && FloatUtils::isNegative( explainedBound - epsilon ) )
        tighteningMatched = true;

    return tighteningMatched;
}

bool Checker::checkAbsLemma(const PLCLemma &expl, PiecewiseLinearConstraint &constraint, double epsilon )
{
    ASSERT( constraint.getType() == ABSOLUTE_VALUE && expl.getConstraintType() == ABSOLUTE_VALUE && epsilon > 0 );

    unsigned causingVar = expl.getCausingVar();
    unsigned affectedVar = expl.getAffectedVar();
    double bound = expl.getBound();
    const double *explanation = expl.getExplanation();
    BoundType causingVarBound = expl.getCausingVarBound();
    BoundType affectedVarBound = expl.getAffectedVarBound();

    double explainedUpperBound = UNSATCertificateUtils::computeBound( causingVar,  UPPER, explanation, _initialTableau, _groundUpperBounds.data(), _groundLowerBounds.data(), _proofSize, _groundUpperBounds.size() );
    double explainedLowerBound = UNSATCertificateUtils::computeBound( causingVar, LOWER, explanation, _initialTableau, _groundUpperBounds.data(), _groundLowerBounds.data(), _proofSize, _groundUpperBounds.size() );

    List<unsigned> constraintVars = constraint.getParticipatingVariables();
    ASSERT(constraintVars.size() == 4 );

    Vector<unsigned> conVec( constraintVars.begin(), constraintVars.end() );
    unsigned b = conVec[0] ;
    unsigned f = conVec[1];

    bool tighteningMatched = false;

    // Make sure the explanation is explained using an absolute value bound tightening. Cases are matching each rule in AbsolutValueConstraint.cpp
    // We allow explained bound to be tighter than the ones recorded (since an explanation can explain tighter bounds), and an epsilon sized error is tolerated.

    //f is always the affected var
    if ( affectedVar != f )
        return false;

    // TODO should check that it is indeed the maximal bound -l(b) and u(b)
    // Ub of f can be tightened by both ub and -lb of b
    if ( causingVar == b && affectedVarBound == UPPER && FloatUtils::lte( explainedUpperBound, bound + epsilon ) )
        tighteningMatched = true;
    else if ( causingVar == b && affectedVarBound == UPPER && FloatUtils::lte( -explainedLowerBound,  bound + epsilon ) )
        tighteningMatched = true;

    // If lb of f is < 0, then it is 0
    else if ( causingVar == f && causingVarBound == LOWER && affectedVarBound == LOWER && FloatUtils::isZero( bound ) && FloatUtils::isNegative( explainedLowerBound ) )
        tighteningMatched = true;

    //TODO debug
    if ( !tighteningMatched )
            printf("ABS: var is %d bound type is %d explained upper bound is %.5lf, explained lower bound is %.5lf real bound is %.5lf\n", causingVar, causingVarBound,  explainedUpperBound, explainedLowerBound, bound );

    return tighteningMatched;
}

bool Checker::checkMaxLemma(const PLCLemma &expl, PiecewiseLinearConstraint &constraint, double epsilon )
{
    ASSERT( constraint.getType() == MAX && expl.getConstraintType() == MAX && epsilon > 0);
    MaxConstraint *maxConstraint =  ( MaxConstraint * ) &constraint;

    unsigned causingVar = expl.getCausingVar();
    unsigned affectedVar = expl.getAffectedVar();
    double bound = expl.getBound();
    const double *explanation = expl.getExplanation();
    BoundType causingVarBound = expl.getCausingVarBound();
    BoundType affectedVarBound = expl.getAffectedVarBound();

    double explainedBound = UNSATCertificateUtils::computeBound( causingVar, causingVarBound == UPPER, explanation, _initialTableau, _groundUpperBounds.data(), _groundLowerBounds.data(), _proofSize, _groundUpperBounds.size() );
    List<unsigned> constraintVars = constraint.getParticipatingVariables();

    unsigned f = maxConstraint->getF();
    for ( const auto &element : maxConstraint->getEliminatedElements() )
        constraintVars.append( element );

    if ( !constraintVars.exists( causingVar ) && causingVar != f )
        return false;

    bool tighteningMatched = false;
    // Make sure the explanation is explained using a max bound tightening.
    // Only tightening type is of the form f = element, for some element

    if ( causingVarBound == UPPER && affectedVar == f && causingVar != f && affectedVarBound == UPPER && FloatUtils::lte( explainedBound,  bound + epsilon ) )
        tighteningMatched = true;
    if ( causingVarBound == UPPER && affectedVar == f && causingVar == f && affectedVarBound == UPPER && FloatUtils::lte( maxConstraint->getMaxValueOfEliminatedPhases(),  bound + epsilon ) )
        tighteningMatched = true;
    // TODO should check that it is indeed the maximal upper bound among all other elements


    //TODO debug
    if ( !tighteningMatched )
        printf("MAX: var is %d bound type is %d explained bound is %.5lf real bound is %.5lf\n", causingVar, causingVarBound, explainedBound, bound );

    return tighteningMatched;
}