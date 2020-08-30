/*********************                                                        */
/*! \file NetworkLevelReasoner.cpp
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

#include "AbsoluteValueConstraint.h"
#include "Debug.h"
#include "FloatUtils.h"
#include "InputQuery.h"
#include "LPFormulator.h"
#include "MILPFormulator.h"
#include "MStringf.h"
#include "MarabouError.h"
#include "NLRError.h"
#include "NetworkLevelReasoner.h"
#include "ReluConstraint.h"
#include "SignConstraint.h"
#include <cstring>

namespace NLR {

NetworkLevelReasoner::NetworkLevelReasoner()
    : _tableau( NULL )
{
}

NetworkLevelReasoner::~NetworkLevelReasoner()
{
    freeMemoryIfNeeded();
}

bool NetworkLevelReasoner::functionTypeSupported( PiecewiseLinearFunctionType type )
{
    if ( type == PiecewiseLinearFunctionType::RELU )
        return true;

    if ( type == PiecewiseLinearFunctionType::ABSOLUTE_VALUE )
        return true;

    if ( type == PiecewiseLinearFunctionType::SIGN )
        return true;

    return false;
}

void NetworkLevelReasoner::addLayer( unsigned layerIndex, Layer::Type type, unsigned layerSize )
{
    Layer *layer = new Layer( layerIndex, type, layerSize, this );
    _layerIndexToLayer[layerIndex] = layer;
}

void NetworkLevelReasoner::addLayerDependency( unsigned sourceLayer, unsigned targetLayer )
{
    _layerIndexToLayer[targetLayer]->addSourceLayer( sourceLayer, _layerIndexToLayer[sourceLayer]->getSize() );
}

void NetworkLevelReasoner::setWeight( unsigned sourceLayer,
                                      unsigned sourceNeuron,
                                      unsigned targetLayer,
                                      unsigned targetNeuron,
                                      double weight )
{
    _layerIndexToLayer[targetLayer]->setWeight
        ( sourceLayer, sourceNeuron, targetNeuron, weight );
}

void NetworkLevelReasoner::setBias( unsigned layer, unsigned neuron, double bias )
{
    _layerIndexToLayer[layer]->setBias( neuron, bias );
}

void NetworkLevelReasoner::addActivationSource( unsigned sourceLayer,
                                                unsigned sourceNeuron,
                                                unsigned targetLayer,
                                                unsigned targetNeuron )
{
    _layerIndexToLayer[targetLayer]->addActivationSource( sourceLayer, sourceNeuron, targetNeuron );
}

const Layer *NetworkLevelReasoner::getLayer( unsigned index ) const
{
    return _layerIndexToLayer[index];
}

void NetworkLevelReasoner::evaluate( double *input, double *output )
{
    _layerIndexToLayer[0]->setAssignment( input );
    for ( unsigned i = 1; i < _layerIndexToLayer.size(); ++i )
        _layerIndexToLayer[i]->computeAssignment();

    const Layer *outputLayer = _layerIndexToLayer[_layerIndexToLayer.size() - 1];
    memcpy( output,
            outputLayer->getAssignment(),
            sizeof(double) * outputLayer->getSize() );
}

void NetworkLevelReasoner::setNeuronVariable( NeuronIndex index, unsigned variable )
{
    _layerIndexToLayer[index._layer]->setNeuronVariable( index._neuron, variable );
}

void NetworkLevelReasoner::receiveTighterBound( Tightening tightening )
{
    _boundTightenings.append( tightening );
}

void NetworkLevelReasoner::getConstraintTightenings( List<Tightening> &tightenings )
{
    tightenings = _boundTightenings;
    _boundTightenings.clear();
}

void NetworkLevelReasoner::symbolicBoundPropagation()
{
    for ( unsigned i = 0; i < _layerIndexToLayer.size(); ++i )
        _layerIndexToLayer[i]->computeSymbolicBounds();
}

void NetworkLevelReasoner::lpRelaxationPropagation()
{
    LPFormulator lpFormulator( this );
    lpFormulator.setCutoff( 0 );

    if ( GlobalConfiguration::MILP_SOLVER_BOUND_TIGHTENING_TYPE ==
         GlobalConfiguration::LP_RELAXATION )
        lpFormulator.optimizeBoundsWithLpRelaxation( _layerIndexToLayer );
    else if ( GlobalConfiguration::MILP_SOLVER_BOUND_TIGHTENING_TYPE ==
              GlobalConfiguration::LP_RELAXATION_INCREMENTAL )
        lpFormulator.optimizeBoundsWithIncrementalLpRelaxation( _layerIndexToLayer );
}

void NetworkLevelReasoner::MILPPropagation()
{
    MILPFormulator milpFormulator( this );
    milpFormulator.setCutoff( 0 );

    if ( GlobalConfiguration::MILP_SOLVER_BOUND_TIGHTENING_TYPE ==
         GlobalConfiguration::MILP_ENCODING )
        milpFormulator.optimizeBoundsWithMILPEncoding( _layerIndexToLayer );
    else if ( GlobalConfiguration::MILP_SOLVER_BOUND_TIGHTENING_TYPE ==
              GlobalConfiguration::MILP_ENCODING_INCREMENTAL )
        milpFormulator.optimizeBoundsWithIncrementalMILPEncoding( _layerIndexToLayer );
}

void NetworkLevelReasoner::intervalArithmeticBoundPropagation()
{
    for ( unsigned i = 1; i < _layerIndexToLayer.size(); ++i )
        _layerIndexToLayer[i]->computeIntervalArithmeticBounds();
}

void NetworkLevelReasoner::freeMemoryIfNeeded()
{
    for ( const auto &layer : _layerIndexToLayer )
        delete layer.second;
    _layerIndexToLayer.clear();
}

void NetworkLevelReasoner::storeIntoOther( NetworkLevelReasoner &other ) const
{
    other.freeMemoryIfNeeded();

    for ( const auto &layer : _layerIndexToLayer )
    {
        const Layer *thisLayer = layer.second;
        Layer *newLayer = new Layer( thisLayer );
        newLayer->setLayerOwner( &other );
        other._layerIndexToLayer[newLayer->getLayerIndex()] = newLayer;
    }
}

void NetworkLevelReasoner::updateVariableIndices( const Map<unsigned, unsigned> &oldIndexToNewIndex,
                                                  const Map<unsigned, unsigned> &mergedVariables )
{
    for ( auto &layer : _layerIndexToLayer )
        layer.second->updateVariableIndices( oldIndexToNewIndex, mergedVariables );
}

void NetworkLevelReasoner::obtainCurrentBounds()
{
    ASSERT( _tableau );
    for ( const auto &layer : _layerIndexToLayer )
        layer.second->obtainCurrentBounds();
}

void NetworkLevelReasoner::setTableau( const ITableau *tableau )
{
    _tableau = tableau;
}

const ITableau *NetworkLevelReasoner::getTableau() const
{
    return _tableau;
}

void NetworkLevelReasoner::eliminateVariable( unsigned variable, double value )
{
    for ( auto &layer : _layerIndexToLayer )
        layer.second->eliminateVariable( variable, value );
}


void NetworkLevelReasoner::dumpTopology() const
{
    printf( "Number of layers: %u. Sizes:\n", _layerIndexToLayer.size() );
    for ( unsigned i = 0; i < _layerIndexToLayer.size(); ++i )
        printf( "\tLayer %u: %u \t[%s]\n", i, _layerIndexToLayer[i]->getSize(), Layer::typeToString( _layerIndexToLayer[i]->getLayerType() ).ascii() );

    for ( const auto &layer : _layerIndexToLayer )
        layer.second->dump();
}

unsigned NetworkLevelReasoner::getNumberOfLayers() const
{
    return _layerIndexToLayer.size();
}

List<PiecewiseLinearConstraint *> NetworkLevelReasoner::getConstraintsInTopologicalOrder()
{
    return _constraintsInTopologicalOrder;
}

void NetworkLevelReasoner::addConstraintInTopologicalOrder( PiecewiseLinearConstraint *constraint )
{
    _constraintsInTopologicalOrder.append( constraint );
}

void NetworkLevelReasoner::removeConstraintFromTopologicalOrder( PiecewiseLinearConstraint *constraint )
{
    if ( _constraintsInTopologicalOrder.exists( constraint ) )
        _constraintsInTopologicalOrder.erase( constraint );
}

InputQuery NetworkLevelReasoner::generateInputQuery()
{
    InputQuery result;

    // Number of variables
    unsigned numberOfVariables = 0;
    for ( const auto &it : _layerIndexToLayer )
        numberOfVariables += it.second->getSize();
    result.setNumberOfVariables( numberOfVariables );

    // Handle the various layers
    for ( const auto &it : _layerIndexToLayer )
        generateInputQueryForLayer( result, *it.second );

    // Mark the input variables
    const Layer *inputLayer = _layerIndexToLayer[0];
    for ( unsigned i = 0; i < inputLayer->getSize(); ++i )
        result.markInputVariable( inputLayer->neuronToVariable( i ), i );

    // Mark the output variables
    const Layer *outputLayer = _layerIndexToLayer[_layerIndexToLayer.size() - 1];
    for ( unsigned i = 0; i < outputLayer->getSize(); ++i )
        result.markOutputVariable( outputLayer->neuronToVariable( i ), i );

    // Store any known bounds of the input layer
    for ( unsigned i = 0; i < inputLayer->getSize(); ++i )
    {
        unsigned variable = inputLayer->neuronToVariable( i );
        result.setLowerBound( variable, inputLayer->getLb( i ) );
        result.setUpperBound( variable, inputLayer->getUb( i ) );
    }

    return result;
}

void NetworkLevelReasoner::generateInputQueryForLayer( InputQuery &inputQuery,
                                                       const Layer &layer )
{
    switch ( layer.getLayerType() )
    {
    case Layer::INPUT:
        break;

    case Layer::WEIGHTED_SUM:
        generateInputQueryForWeightedSumLayer( inputQuery, layer );
        break;

    case Layer::RELU:
        generateInputQueryForReluLayer( inputQuery, layer );
        break;

    case Layer::SIGN:
        generateInputQueryForSignLayer( inputQuery, layer );
        break;

    case Layer::ABSOLUTE_VALUE:
        generateInputQueryForAbsoluteValueLayer( inputQuery, layer );
        break;

    default:
        throw NLRError( NLRError::LAYER_TYPE_NOT_SUPPORTED,
                        Stringf( "Layer %u not yet supported", layer.getLayerType() ).ascii() );
        break;
    }
}

void NetworkLevelReasoner::generateInputQueryForReluLayer( InputQuery &inputQuery, const Layer &layer )
{
    for ( unsigned i = 0; i < layer.getSize(); ++i )
    {
        NeuronIndex sourceIndex = *layer.getActivationSources( i ).begin();
        const Layer *sourceLayer = _layerIndexToLayer[sourceIndex._layer];
        ReluConstraint *relu = new ReluConstraint( sourceLayer->neuronToVariable( sourceIndex._neuron ), layer.neuronToVariable( i ) );
        inputQuery.addPiecewiseLinearConstraint( relu );
    }
}

void NetworkLevelReasoner::generateInputQueryForSignLayer( InputQuery &inputQuery, const Layer &layer )
{
    for ( unsigned i = 0; i < layer.getSize(); ++i )
    {
        NeuronIndex sourceIndex = *layer.getActivationSources( i ).begin();
        const Layer *sourceLayer = _layerIndexToLayer[sourceIndex._layer];
        SignConstraint *sign = new SignConstraint( sourceLayer->neuronToVariable( sourceIndex._neuron ), layer.neuronToVariable( i ) );
        inputQuery.addPiecewiseLinearConstraint( sign );
    }
}

void NetworkLevelReasoner::generateInputQueryForAbsoluteValueLayer( InputQuery &inputQuery, const Layer &layer )
{
    for ( unsigned i = 0; i < layer.getSize(); ++i )
    {
        NeuronIndex sourceIndex = *layer.getActivationSources( i ).begin();
        const Layer *sourceLayer = _layerIndexToLayer[sourceIndex._layer];
        AbsoluteValueConstraint *absoluteValue = new AbsoluteValueConstraint( sourceLayer->neuronToVariable( sourceIndex._neuron ), layer.neuronToVariable( i ) );
        inputQuery.addPiecewiseLinearConstraint( absoluteValue );
    }
}

void NetworkLevelReasoner::generateInputQueryForWeightedSumLayer( InputQuery &inputQuery, const Layer &layer )
{
    for ( unsigned i = 0; i < layer.getSize(); ++i )
    {
        Equation eq;
        eq.setScalar( -layer.getBias( i ) );
        eq.addAddend( -1, layer.neuronToVariable( i ) );

        for ( const auto &it : layer.getSourceLayers() )
        {
            const Layer *sourceLayer = _layerIndexToLayer[it.first];

            for ( unsigned j = 0; j < sourceLayer->getSize(); ++j )
            {
                double coefficient = layer.getWeight( sourceLayer->getLayerIndex(), j, i );
                eq.addAddend( coefficient, sourceLayer->neuronToVariable( j ) );
            }
        }

        inputQuery.addEquation( eq );
    }
}

// todo new code - START

void NetworkLevelReasoner::mergeWSLayers( )
{
    // Iterate over all layers
    for ( unsigned i = 0; i < getNumberOfLayers() - 1; ++i )
    {

        unsigned firstLayerIndex = i;
        unsigned secondLayerIndex = i + 1;

        const Layer *firstLayer = _layerIndexToLayer[firstLayerIndex];
        const Layer *secondLayer = _layerIndexToLayer[secondLayerIndex];

        // If two subsequent layers are WS
        if ( firstLayer->getLayerType() != Layer::WEIGHTED_SUM )
        {
            continue;
        }
        if ( secondLayer->getLayerType() != Layer::WEIGHTED_SUM )
        {
            continue;
        }
        if ( ! secondLayer->getSourceLayers().exists( firstLayerIndex ) )
        {
            continue;
        }

        // If the first layer's input is the second layer

        if ( ! isReductionPossible( firstLayerIndex, secondLayerIndex ) )
        {
            continue;
        }

        // Remove both layers and insert *new* first layer
        // todo continue - call mergeSubsequentLayers()
        mergeSubsequentLayers( firstLayerIndex, secondLayerIndex );

        // Reduce -1 from all indexes
        reduceLayerIndex( secondLayerIndex );

    }
}



// Change matrix of first layer,
void NetworkLevelReasoner::mergeSubsequentLayers ( unsigned firstLayerIndex,  unsigned secondLayerIndex)
{
    // todo make sure I don't start with INPUT LAYER
    Layer *inputLayerToFirst = _layerIndexToLayer[firstLayerIndex - 1];
    Layer *firstLayer = _layerIndexToLayer[firstLayerIndex];
    Layer *secondLayer = _layerIndexToLayer[secondLayerIndex];


    unsigned inputDimension = inputLayerToFirst->getSize();
    unsigned middleDimension = firstLayer->getSize();
    unsigned outputDimension = secondLayer->getSize();

    // Multiply both layers - all weights
    double *firstLayerMatrix = firstLayer->getWeightsMap()[firstLayerIndex - 1];
    double *secondLayerMatrix = secondLayer->getWeightsMap()[firstLayerIndex];
    double *newWeightsMatrix = multiplyWeights( firstLayerMatrix, secondLayerMatrix, inputDimension, middleDimension, outputDimension );

    auto weightsOfFirstAfterPop = firstLayer->popLayerWeights( firstLayer->getWeightsMap(), firstLayerIndex - 1 );
    auto weightsOfFirstAfterUpdate = firstLayer->addLayerWeights( weightsOfFirstAfterPop, firstLayerIndex - 1, newWeightsMatrix );
    firstLayer->setWeightsMap( weightsOfFirstAfterUpdate );

    auto weightsOfSecondAfterPop = secondLayer->popLayerWeights( secondLayer->getWeightsMap(), firstLayerIndex );
    secondLayer->setWeightsMap( weightsOfSecondAfterPop );

    // Multiply both matrices - positive weights
    double *firstLayerPositiveMatrix = firstLayer->getPositiveWeights()[firstLayerIndex - 1];
    double *secondLayerPositiveMatrix = secondLayer->getPositiveWeights()[firstLayerIndex];
    double *newWeightsPositiveMatrix = multiplyWeights( firstLayerPositiveMatrix, secondLayerPositiveMatrix, inputDimension, middleDimension, outputDimension );

    auto posWeightsOfFirstAfterPop = firstLayer->popLayerWeights( firstLayer->getPositiveWeights(), firstLayerIndex - 1 );
    auto posWeightsOfFirstAfterUpdate = firstLayer->addLayerWeights( posWeightsOfFirstAfterPop, firstLayerIndex - 1, newWeightsPositiveMatrix );
    firstLayer->setPositiveWeights( posWeightsOfFirstAfterUpdate );

    auto posWeightsOfSecondAfterPop = secondLayer->popLayerWeights( secondLayer->getPositiveWeights(), firstLayerIndex );
    secondLayer->setPositiveWeights( posWeightsOfSecondAfterPop );

    // Multiply both matrices - negative weights
    double *firstLayerNegativeMatrix = firstLayer->getNegativeWeights()[firstLayerIndex - 1];
    double *secondLayerNegativeMatrix = secondLayer->getNegativeWeights()[firstLayerIndex];
    double *newWeightsNegativeMatrix = multiplyWeights( firstLayerNegativeMatrix, secondLayerNegativeMatrix, inputDimension, middleDimension, outputDimension );

    auto negWeightsOfFirstAfterPop = firstLayer->popLayerWeights( firstLayer->getNegativeWeights(), firstLayerIndex - 1 );
    auto negWeightsOfFirstAfterUpdate = firstLayer->addLayerWeights( negWeightsOfFirstAfterPop, firstLayerIndex - 1, newWeightsNegativeMatrix );
    firstLayer->setNegativeWeights( negWeightsOfFirstAfterPop );

    auto negWeightsOfSecondAfterPop = secondLayer->popLayerWeights( secondLayer->getNegativeWeights(), firstLayerIndex );
    secondLayer->setNegativeWeights( negWeightsOfSecondAfterPop );
}


// TODO check thoroughly!!!!
double *NetworkLevelReasoner:: multiplyWeights ( double *firstMatrix, double *secondMatrix, unsigned inputDimension,unsigned middleDimension, unsigned outputDimension ) {

        double *newMatrix = new double[inputDimension * outputDimension];
        unsigned indexInNewMatrix = 0;

        for (unsigned row = 0; row < inputDimension; ++row)
        {
            for (unsigned column = 0; column < outputDimension; ++column)
            {
                // Calculate [i, j]
                double sum = 0;
                for (unsigned index = 0; index < middleDimension; ++index) {
                    sum += firstMatrix[(row * middleDimension) + index] *
                           secondMatrix[(index * outputDimension) + column];
                }
                newMatrix[indexInNewMatrix] = sum;
                ++indexInNewMatrix;
            }
        }
        return newMatrix;
}


// Assume we get two subsequent WS layers with WS -> WS
bool NetworkLevelReasoner::isReductionPossible( unsigned firstLayerIndex, unsigned secondLayerIndex )
{
    for ( unsigned i = 0; i < getNumberOfLayers(); ++i ) // here without '-1' an end
    {
        // Found a third layer which the first layer is a source to
        if ( ( i != secondLayerIndex ) && ( _layerIndexToLayer[i]->getSourceLayers().exists( firstLayerIndex ) ) )
        {
            return false;
        }

        // Found a third layer which the second layer is a sink to
        if ( ( i != firstLayerIndex ) && ( _layerIndexToLayer[secondLayerIndex]->getSourceLayers().exists( i ) ) )
        {
            return false;
        }
    }

    // The 2nd input index is the only one for which the source is the later at the 1st input index
    return true;
}

// Reduce -1 from all layer indexes starting from (including) the given input index
void NetworkLevelReasoner::reduceLayerIndex ( unsigned indexToStart )
{
    for ( unsigned layerIndex = 0; layerIndex < getNumberOfLayers(); ++layerIndex )
    {
        Layer *layer = _layerIndexToLayer[layerIndex];

        auto layerSourceMap = layer->getSourceLayers();
        auto newSourceMap = reduceLayerIndexHelper( indexToStart , layerSourceMap );
        layer->setSourceLayers( newSourceMap );

        auto layerWeightsMap = layer->getWeightsMap();
        auto newWeightsMap = reduceLayerIndexHelper( indexToStart , layerWeightsMap );
        layer->setWeightsMap ( newWeightsMap );

        auto layerPositiveWeightMap = layer->getPositiveWeights();
        auto newPositiveWeightsMap = reduceLayerIndexHelper( indexToStart , layerPositiveWeightMap );
        layer->setPositiveWeights( newPositiveWeightsMap );

        auto layerNegativeWeightMap = layer->getNegativeWeights();
        auto newNegativeWeightsMap = reduceLayerIndexHelper( indexToStart , layerNegativeWeightMap );
        layer->setNegativeWeights( newNegativeWeightsMap );

        auto newIdxToLayerMap = reduceLayerIndexHelper( indexToStart , _layerIndexToLayer );
        _layerIndexToLayer = newIdxToLayerMap;

    }
}


template <typename T>
Map <unsigned, T> NetworkLevelReasoner::reduceLayerIndexHelper ( unsigned indexToStart , Map <unsigned, T> layerMap )
{

    Map <unsigned, T> newMap = Map <unsigned, T>();

    for (  auto pair = layerMap.begin() ; pair != layerMap.end(); ++pair )
    {

        auto layerKey = pair->first;
        auto layerValue = pair->second;


        if  ( layerKey < indexToStart )
        {
            newMap.insert( layerKey , layerValue );
        }

        // layerKey >= indexToStart
        else
        {
            newMap.insert( layerKey - 1 , layerValue );
        }
    }

    return newMap;

}


// todo new code - END


} // namespace NLR
