/*********************                                                        */
/*! \file PrecisionRestorer.cpp
 ** \verbatim
 ** Top contributors (to current version):
 **   Guy Katz
 ** This file is part of the Marabou project.
 ** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved. See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** [[ Add lengthier description here ]]

 **/

#include "Debug.h"
#include "FloatUtils.h"
#include "MalformedBasisException.h"
#include "PrecisionRestorer.h"
#include "MarabouError.h"
#include "SmtCore.h"
#include "UNSATCertificate.h"

void PrecisionRestorer::storeInitialEngineState( const IEngine &engine )
{
    engine.storeState( _initialEngineState, true );
}

void PrecisionRestorer::restorePrecision( IEngine &engine,
                                          ITableau &tableau,
                                          SmtCore &smtCore,
                                          RestoreBasics restoreBasics )
{
    // Store the dimensions, bounds and basic variables in the current tableau, before restoring it
    unsigned targetM = tableau.getM();
    unsigned targetN = tableau.getN();

    Set<unsigned> shouldBeBasic = tableau.getBasicVariables();

    double *lowerBounds = new double[targetN];
    double *upperBounds = new double[targetN];
    memcpy( lowerBounds, tableau.getLowerBounds(), sizeof(double) * targetN );
    memcpy( upperBounds, tableau.getUpperBounds(), sizeof(double) * targetN );

	BoundsExplainer tableauExplanations = BoundsExplainer( targetN, targetM );

	if ( GlobalConfiguration::PROOF_CERTIFICATE )
		tableauExplanations = *tableau.getAllBoundsExplanations();

    try
    {
        EngineState targetEngineState;
        engine.storeState( targetEngineState, false );

        // Store the case splits performed so far
        List<PiecewiseLinearCaseSplit> targetSplits;
        smtCore.allSplitsSoFar( targetSplits );

        // Restore engine and tableau to their original form
        engine.restoreState( _initialEngineState );

        // Re-add all splits, which will restore variables and equations
        for ( const auto &split : targetSplits )
            engine.applySplit( split );

        // At this point, the tableau has the appropriate dimensions. Restore the variable bounds
        // and basic variables.
        // Note that if column merging is enabled, the dimensions may not be precisely those before
        // the resotration, because merging sometimes fails - in which case an equation is added. If
        // we fail to restore the dimensions, we cannot restore the basics.

        bool dimensionsRestored = ( tableau.getN() == targetN ) && ( tableau.getM() == targetM );

        ASSERT( dimensionsRestored || GlobalConfiguration::USE_COLUMN_MERGING_EQUATIONS );

        Set<unsigned> currentBasics = tableau.getBasicVariables();

        if ( dimensionsRestored && restoreBasics == RESTORE_BASICS )
        {
            List<unsigned> shouldBeBasicList;
            for ( const auto &basic : shouldBeBasic )
                shouldBeBasicList.append( basic );

            bool failed = false;
            try
            {
                tableau.initializeTableau( shouldBeBasicList );
            }
            catch ( MalformedBasisException & )
            {
                failed = true;
            }

            if ( failed )
            {
                // The "restoreBasics" set leads to a malformed basis.
                // Try again without this part of the restoration
                shouldBeBasicList.clear();
                for ( const auto &basic : currentBasics )
                    shouldBeBasicList.append( basic );

                try
                {
                    tableau.initializeTableau( shouldBeBasicList );
                }
                catch ( MalformedBasisException & )
                {
                    throw MarabouError( MarabouError::RESTORATION_FAILED_TO_REFACTORIZE_BASIS,
                                         "Precision restoration failed - could not refactorize basis after setting basics" );
                }
            }
        }

		if ( GlobalConfiguration::PROOF_CERTIFICATE )
		{
			tableau.setAllBoundsExplanations( &tableauExplanations );
			engine.removePLCExplanationsFromCurrentCertificateNode();
		}

        // Tighten bounds to be as explained bounds, in order to avoid inaccuracies
        // In case that bounds explanations are not tight enough, reset the explanation to use ground bounds
        for ( unsigned i = 0; i < targetN; ++i )
        {
        	if ( GlobalConfiguration::PROOF_CERTIFICATE )
			{
				if ( FloatUtils::gte( engine.getExplainedBound( i, false ), engine.getGroundLowerBound( i ) ) )
					tableau.tightenLowerBoundNaively( i,  engine.getExplainedBound( i, false ) );
				else
					tableau.resetExplanation( i, false );

				if ( FloatUtils::lte( engine.getExplainedBound( i, true ),  engine.getGroundUpperBound( i ) ) )
					tableau.tightenUpperBoundNaively( i,engine.getExplainedBound( i, true ) );
        		else
        			tableau.resetExplanation( i, true );
			}
			else
			{
				tableau.tightenLowerBound( i, lowerBounds[i] );
				tableau.tightenUpperBound( i, upperBounds[i] );
			}
        }

        // Restore constraint status
        for ( const auto &pair : targetEngineState._plConstraintToState )
            pair.first->setActiveConstraint( pair.second->isActive() );

        engine.setNumPlConstraintsDisabledByValidSplits
            ( targetEngineState._numPlConstraintsDisabledByValidSplits );

        DEBUG({
                // Same dimensions
                ASSERT( GlobalConfiguration::USE_COLUMN_MERGING_EQUATIONS || tableau.getN() == targetN );
                ASSERT( GlobalConfiguration::USE_COLUMN_MERGING_EQUATIONS || tableau.getM() == targetM );

                // Constraints should be in the same state before and after restoration
                for ( const auto &pair : targetEngineState._plConstraintToState )
                {
                    ASSERT( pair.second->isActive() == pair.first->isActive() );
                    ASSERT( pair.second->phaseFixed() == pair.first->phaseFixed() );
                    ASSERT( pair.second->constraintObsolete() == pair.first->constraintObsolete() );
                }

                EngineState currentEngineState;
                engine.storeState( currentEngineState, false );

                ASSERT( currentEngineState._numPlConstraintsDisabledByValidSplits ==
                        targetEngineState._numPlConstraintsDisabledByValidSplits );

                tableau.verifyInvariants();
            });
    }
    catch ( ... )
    {
        delete[] lowerBounds;
        delete[] upperBounds;

        throw;
    }

    delete[] lowerBounds;
    delete[] upperBounds;
}

//
// Local Variables:
// compile-command: "make -C ../.. "
// tags-file-name: "../../TAGS"
// c-basic-offset: 4
// End:
//
