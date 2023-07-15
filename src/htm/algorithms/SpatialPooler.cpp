/* ---------------------------------------------------------------------
 * HTM Community Edition of NuPIC
 * Copyright (C) 2013, Numenta, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Affero Public License for more details.
 *
 * You should have received a copy of the GNU Affero Public License
 * along with this program.  If not, see http://www.gnu.org/licenses.
 * ---------------------------------------------------------------------- */

/** @file
 * Implementation of SpatialPooler
 */

#include <string>
#include <algorithm>
#include <iterator> //begin()
#include <cmath> //fmod
#include <numeric> //iota

#include <htm/algorithms/SpatialPooler.hpp>
#include <htm/utils/Topology.hpp>
#include <htm/utils/VectorHelpers.hpp>

using namespace std;
using namespace htm;

class CoordinateConverterND {

public:
  CoordinateConverterND(const vector<UInt> &dimensions) {
    NTA_ASSERT(!dimensions.empty());

    dimensions_ = dimensions;
    UInt b = 1u;
    for (Size i = dimensions.size(); i > 0u; i--) {
      bounds_.insert(bounds_.begin(), b);
      b *= dimensions[i-1];
    }
  }

  void toCoord(UInt index, vector<UInt> &coord) const {
    coord.clear();
    for (Size i = 0u; i < bounds_.size(); i++) {
      coord.push_back((index / bounds_[i]) % dimensions_[i]);
    }
  };

  UInt toIndex(vector<UInt> &coord) const {
    UInt index = 0;
    for (Size i = 0; i < coord.size(); i++) {
      index += coord[i] * bounds_[i];
    }
    return index;
  };

private:
  vector<UInt> dimensions_;
  vector<UInt> bounds_;
};

SpatialPooler::SpatialPooler() {
  // The current version number.
  version_ = 3;
}

SpatialPooler::SpatialPooler(
    const vector<UInt> inputDimensions, const vector<UInt> columnDimensions,
    UInt potentialRadius, Real potentialPct, bool globalInhibition,
    Real localAreaDensity, UInt numActiveColumnsPerInhArea,
    UInt stimulusThreshold, Real synPermInactiveDec, Real synPermActiveInc,
    Real synPermConnected, Real minPctOverlapDutyCycles, UInt dutyCyclePeriod,
    Real boostStrength, Int seed, UInt spVerbosity, bool wrapAround)
    : SpatialPooler::SpatialPooler()
{
  // The current version number for serialzation.
  version_ = 3;

  initialize(inputDimensions,
             columnDimensions,
             potentialRadius,
             potentialPct,
             globalInhibition,
             localAreaDensity,
             numActiveColumnsPerInhArea,
             stimulusThreshold,
             synPermInactiveDec,
             synPermActiveInc,
             synPermConnected,
             minPctOverlapDutyCycles,
             dutyCyclePeriod,
             boostStrength,
             seed,
             spVerbosity,
             wrapAround);
}

vector<UInt> SpatialPooler::getColumnDimensions() const {
  return columnDimensions_;
}

vector<UInt> SpatialPooler::getInputDimensions() const {
  return inputDimensions_;
}

UInt SpatialPooler::getNumColumns() const { return numColumns_; }

UInt SpatialPooler::getNumInputs() const { return numInputs_; }

UInt SpatialPooler::getPotentialRadius() const { return potentialRadius_; }

void SpatialPooler::setPotentialRadius(UInt potentialRadius) {
  NTA_CHECK(potentialRadius < numInputs_);
  potentialRadius_ = potentialRadius;
}

Real SpatialPooler::getPotentialPct() const { return potentialPct_; }

void SpatialPooler::setPotentialPct(Real potentialPct) {
  NTA_CHECK(potentialPct > 0.0f && potentialPct <= 1.0f);
  potentialPct_ = potentialPct;
}

bool SpatialPooler::getGlobalInhibition() const { return globalInhibition_; }

void SpatialPooler::setGlobalInhibition(bool globalInhibition) {
  globalInhibition_ = globalInhibition;
}

Int SpatialPooler::getNumActiveColumnsPerInhArea() const {
  return numActiveColumnsPerInhArea_;
}

void SpatialPooler::setNumActiveColumnsPerInhArea(UInt numActiveColumnsPerInhArea) {
  NTA_CHECK( numActiveColumnsPerInhArea > 0 && numActiveColumnsPerInhArea <= numColumns_); //TODO this boundary could be smarter
  numActiveColumnsPerInhArea_ = numActiveColumnsPerInhArea;
  localAreaDensity_ = 0.0f;  //MUTEX with localAreaDensity
}

Real SpatialPooler::getLocalAreaDensity() const { return localAreaDensity_; }

void SpatialPooler::setLocalAreaDensity(const Real localAreaDensity) {
  NTA_CHECK(localAreaDensity > static_cast<Permanence>(0.0) && localAreaDensity <= static_cast<Permanence>(1.0));
  NTA_CHECK(static_cast<UInt>(localAreaDensity * getNumColumns()) > 0) << "Too small density or sp.getNumColumns() -> would have zero active output columns.";
  localAreaDensity_ = localAreaDensity;
  numActiveColumnsPerInhArea_ = 0; //MUTEX with numActiveColumnsPerInhArea
}

UInt SpatialPooler::getStimulusThreshold() const { return stimulusThreshold_; }

void SpatialPooler::setStimulusThreshold(UInt stimulusThreshold) {
  stimulusThreshold_ = stimulusThreshold;
}

UInt SpatialPooler::getInhibitionRadius() const { return inhibitionRadius_; }

void SpatialPooler::setInhibitionRadius(UInt inhibitionRadius) {
  NTA_ASSERT(inhibitionRadius > 0);
  if (inhibitionRadius_ != inhibitionRadius) {
    inhibitionRadius_ = inhibitionRadius;
    neighborMap_ = Neighborhood::updateAllNeighbors(inhibitionRadius_, columnDimensions_, wrapAround_, /*skip_center=*/true);
  }
}

UInt SpatialPooler::getDutyCyclePeriod() const { return dutyCyclePeriod_; }

void SpatialPooler::setDutyCyclePeriod(UInt dutyCyclePeriod) {
  dutyCyclePeriod_ = dutyCyclePeriod;
}

Real SpatialPooler::getBoostStrength() const { return boostStrength_; }

void SpatialPooler::setBoostStrength(Real boostStrength) {
  NTA_CHECK(boostStrength >= 0.0f);
  boostStrength_ = boostStrength;
}

UInt SpatialPooler::getIterationNum() const { return iterationNum_; }

void SpatialPooler::setIterationNum(UInt iterationNum) {
  iterationNum_ = iterationNum;
}

UInt SpatialPooler::getIterationLearnNum() const { return iterationLearnNum_; }

void SpatialPooler::setIterationLearnNum(UInt iterationLearnNum) {
  iterationLearnNum_ = iterationLearnNum;
}

UInt SpatialPooler::getSpVerbosity() const { return spVerbosity_; }

void SpatialPooler::setSpVerbosity(UInt spVerbosity) {
  spVerbosity_ = spVerbosity;
}

bool SpatialPooler::getWrapAround() const { return wrapAround_; }

void SpatialPooler::setWrapAround(bool wrapAround) { wrapAround_ = wrapAround; }

UInt SpatialPooler::getUpdatePeriod() const { return updatePeriod_; }

void SpatialPooler::setUpdatePeriod(UInt updatePeriod) {
  updatePeriod_ = updatePeriod;
}

Real SpatialPooler::getSynPermActiveInc() const { return synPermActiveInc_; }

void SpatialPooler::setSynPermActiveInc(Real synPermActiveInc) {
  NTA_CHECK( synPermActiveInc > minPermanence );
  NTA_CHECK( synPermActiveInc <= maxPermanence );
  synPermActiveInc_ = synPermActiveInc;
}

Real SpatialPooler::getSynPermInactiveDec() const {
  return synPermInactiveDec_;
}

void SpatialPooler::setSynPermInactiveDec(Real synPermInactiveDec) {
  NTA_CHECK( synPermInactiveDec >= minPermanence );
  NTA_CHECK( synPermInactiveDec <= maxPermanence );
  synPermInactiveDec_ = synPermInactiveDec;
}

Real SpatialPooler::getSynPermBelowStimulusInc() const {
  return synPermBelowStimulusInc_;
}

void SpatialPooler::setSynPermBelowStimulusInc(Real synPermBelowStimulusInc) {
  NTA_CHECK( synPermBelowStimulusInc > minPermanence );
  NTA_CHECK( synPermBelowStimulusInc <= maxPermanence );
  synPermBelowStimulusInc_ = synPermBelowStimulusInc;
}

Real SpatialPooler::getSynPermConnected() const { return synPermConnected_; }

Real SpatialPooler::getSynPermMax() const { return maxPermanence; }

Real SpatialPooler::getMinPctOverlapDutyCycles() const {
  return minPctOverlapDutyCycles_;
}

void SpatialPooler::setMinPctOverlapDutyCycles(Real minPctOverlapDutyCycles) {
  NTA_CHECK(minPctOverlapDutyCycles > 0.0f && minPctOverlapDutyCycles <= 1.0f);
  minPctOverlapDutyCycles_ = minPctOverlapDutyCycles;
}

void SpatialPooler::getBoostFactors(Real boostFactors[]) const { //TODO make vector
  copy(boostFactors_.begin(), boostFactors_.end(), boostFactors);
}

void SpatialPooler::setBoostFactors(Real boostFactors[]) {
  boostFactors_.assign(&boostFactors[0], &boostFactors[numColumns_]);
}

void SpatialPooler::getOverlapDutyCycles(Real overlapDutyCycles[]) const {
  copy(overlapDutyCycles_.begin(), overlapDutyCycles_.end(), overlapDutyCycles);
}

void SpatialPooler::setOverlapDutyCycles(const Real overlapDutyCycles[]) {
  overlapDutyCycles_.assign(&overlapDutyCycles[0],
                            &overlapDutyCycles[numColumns_]);
}

void SpatialPooler::getActiveDutyCycles(Real activeDutyCycles[]) const {
  copy(activeDutyCycles_.begin(), activeDutyCycles_.end(), activeDutyCycles);
}

void SpatialPooler::setActiveDutyCycles(const Real activeDutyCycles[]) {
  activeDutyCycles_.assign(&activeDutyCycles[0],
                           &activeDutyCycles[numColumns_]);
}

void SpatialPooler::getMinOverlapDutyCycles(Real minOverlapDutyCycles[]) const {
  copy(minOverlapDutyCycles_.begin(), minOverlapDutyCycles_.end(),
       minOverlapDutyCycles);
}

void SpatialPooler::setMinOverlapDutyCycles(const Real minOverlapDutyCycles[]) {
  minOverlapDutyCycles_.assign(&minOverlapDutyCycles[0],
                               &minOverlapDutyCycles[numColumns_]);
}


// potential will have 1 for all inputs connected to column by synapse
void SpatialPooler::getPotential(UInt column, UInt potential[]) const {
  NTA_ASSERT(column < numColumns_);
  std::fill( potential, potential + numInputs_, 0 );
  const auto &synapses = connections_.synapsesForSegment( column );
  for(const auto syn : synapses) {
    const auto &synData = connections_.dataForSynapse( syn );
    potential[synData.presynapticCell] = 1; 
  }
}


// take potential input space, connect it to columns by synapses and initiate with random strength, initConnectedPct_ synapses will have strength above connected threshold
void SpatialPooler::setPotential(UInt column, const UInt potential[]) {
  NTA_ASSERT(column < numColumns_);

  // Remove all existing synapses.
  const auto &synapses = connections_.synapsesForSegment( column );
  while( synapses.size() > 0 )
    connections_.destroySynapse( synapses[0] );

  // Replace with new synapse.
  vector<UInt> potentialDenseVec( potential, potential + numInputs_ );
  const auto &perm = initPermanence_( potentialDenseVec, initConnectedPct_ );
  for(UInt i = 0; i < numInputs_; i++) {
    if( potential[i] )
      connections_.createSynapse( column, i, perm[i] );
  }
}

// get all inputs where permanence is above threshold
vector<Real> SpatialPooler::getPermanence(const UInt column, 
				          const Permanence threshold) const {
  NTA_ASSERT(column < numColumns_);
  const auto &synapses = connections_.synapsesForSegment( column );
  vector<Real> permanences(numInputs_, 0.0f);
  for( const auto syn : synapses ) {
    const auto &synData = connections_.dataForSynapse( syn );
    if( synData.permanence >= threshold) { // there must be >= for default case 0.0 where we want all permanences
      permanences[ synData.presynapticCell ] = synData.permanence;
    }
  }
  return permanences;
}

// update connection permanence of inputs connected to columns
void SpatialPooler::setPermanence(UInt column, const Real permanences[]) {
  NTA_ASSERT(column < numColumns_);

#ifndef NDEBUG // If DEBUG mode ...
  // Keep track of which permanences have been successfully applied to the
  // connections, by zeroing each out after processing.  After all synapses
  // processed check that all permanences are zeroed.
  vector<Real> check_data(permanences, permanences + numInputs_);
#endif

  const auto synapses = connections_.synapsesForSegment( column );
  for(const auto &syn : synapses) {
    const auto &synData = connections_.dataForSynapse( syn );
    const auto &presyn  = synData.presynapticCell;
    connections_.updateSynapsePermanence( syn, permanences[presyn] );

#ifndef NDEBUG
    check_data[presyn] = minPermanence;
#endif
  }

#ifndef NDEBUG
  for(UInt i = 0; i < numInputs_; i++) {
    NTA_ASSERT(check_data[i] == minPermanence)
          << "Can't setPermanence for synapse which is not in potential pool!";
  }
#endif
}


void SpatialPooler::getConnectedCounts(UInt connectedCounts[]) const {
  for(size_t seg = 0; seg < numColumns_; seg++) { //in SP each column = 1 cell with 1 segment only.
    const auto &segment = connections_.dataForSegment( (htm::Segment)seg );
    connectedCounts[ seg ] = segment.numConnected; //TODO numConnected only used here, rm from SegmentData and compute for each segment.synapses?
  }
}


const vector<Real> &SpatialPooler::getBoostedOverlaps() const {
  return boostedOverlaps_;
}

void SpatialPooler::initialize(
    const vector<UInt>& inputDimensions, 
    const vector<UInt>& columnDimensions,
    UInt potentialRadius, 
    Real potentialPct, 
    bool globalInhibition,
    Real localAreaDensity,
    UInt numActiveColumnsPerInhArea,
    UInt stimulusThreshold, 
    Real synPermInactiveDec, 
    Real synPermActiveInc,
    Real synPermConnected, 
    Real minPctOverlapDutyCycles, 
    UInt dutyCyclePeriod,
    Real boostStrength, 
    Int seed, 
    UInt spVerbosity, 
    bool wrapAround) {

  numInputs_ = 1u;
  inputDimensions_.clear();
  for (const auto &inputDimension : inputDimensions) {
    NTA_CHECK(inputDimension > 0) << "Input dimensions must be positive integers!";
    numInputs_ *= inputDimension;
    inputDimensions_.push_back(inputDimension);
  }
  numColumns_ = 1u;
  columnDimensions_.clear();
  for (const auto &columnDimension : columnDimensions) {
    NTA_CHECK(columnDimension > 0) << "Column dimensions must be positive integers!";
    numColumns_ *= columnDimension;
    columnDimensions_.push_back(columnDimension);
  }
  NTA_CHECK(numColumns_ > 0);
  NTA_CHECK(numInputs_ > 0);

  // 1D input produces 1D output; 2D => 2D, etc. //TODO allow nD -> mD conversion
  NTA_CHECK(inputDimensions_.size() == columnDimensions_.size()); 

  NTA_CHECK( (numActiveColumnsPerInhArea > 0 && localAreaDensity == 0) || (localAreaDensity > 0 && numActiveColumnsPerInhArea == 0) ) 
  << "SP: Mutex. Only one can be set to >0: localAreaDensity, numActiveColumnsPerInhArea";
  if(numActiveColumnsPerInhArea > 0) {
    setNumActiveColumnsPerInhArea(numActiveColumnsPerInhArea);
  } else {
    setLocalAreaDensity(localAreaDensity); 
  }

  rng_ = Random(seed);

  potentialRadius_ = potentialRadius > numInputs_ ? numInputs_ : potentialRadius;
  NTA_CHECK(potentialPct > 0 && potentialPct <= 1);
  potentialPct_ = potentialPct;
  globalInhibition_ = globalInhibition;
  stimulusThreshold_ = stimulusThreshold;
  synPermInactiveDec_ = synPermInactiveDec;
  synPermActiveInc_ = synPermActiveInc;
  synPermBelowStimulusInc_ = synPermConnected / 10.0f;
  synPermConnected_ = synPermConnected;
  minPctOverlapDutyCycles_ = minPctOverlapDutyCycles;
  dutyCyclePeriod_ = dutyCyclePeriod;
  boostStrength_ = boostStrength;
  spVerbosity_ = spVerbosity;
  wrapAround_ = wrapAround; //TODO consider keeping only wrapping version if results are the same (seems no difference), as wrap=true is much faster now for local inh. 
  updatePeriod_ = 50u;
  initConnectedPct_ = 0.5f; //FIXME make SP's param, and much lower 0.01 https://discourse.numenta.org/t/spatial-pooler-implementation-for-mnist-dataset/2317/25?u=breznak 
  iterationNum_ = 0u;
  iterationLearnNum_ = 0u;

  overlapDutyCycles_.assign(numColumns_, 0); //TODO make all these sparse or rm to reduce footprint
  activeDutyCycles_.assign(numColumns_, 0);
  minOverlapDutyCycles_.assign(numColumns_, 0.0);
  boostFactors_.assign(numColumns_, 1.0); //1 is neutral value for boosting
  boostedOverlaps_.resize(numColumns_);

  inhibitionRadius_ = 0;

  connections_.initialize(numColumns_, synPermConnected_);
  for (Size i = 0; i < numColumns_; ++i) {
    connections_.createSegment( static_cast<CellIdx>(i) , 1 /* max segments per cell is fixed for SP to 1 */);

    // Note: initMapPotential_ & initPermanence_ return dense arrays.
    vector<UInt> potential = initMapPotential_((UInt)i, wrapAround_);
    vector<Permanence> perm = initPermanence_(potential, initConnectedPct_);
    for(size_t presyn = 0; presyn < numInputs_; presyn++) {
      if( potential[presyn] )
        connections_.createSynapse( static_cast<Segment>(i), static_cast<htm::CellIdx>(presyn), perm[presyn] );
    }

    connections_.raisePermanencesToThreshold( (Segment)i, stimulusThreshold_ );
  }

  updateInhibitionRadius_();

  if (spVerbosity_ > 0) {
    printParameters();
    std::cout << "CPP SP seed                 = " << seed << std::endl;
  }
}


const vector<SynapseIdx> SpatialPooler::compute(const SDR &input, const bool learn, SDR &active) {
  input.reshape(  inputDimensions_ );
  active.reshape( columnDimensions_ );
  updateBookeepingVars_(learn);

  // overlaps is vector where for each segment(column) the weight of connected inputs is counted
  const auto& overlaps = connections_.computeActivityWeighted(input.getSparse(),input.getSparseWeights(), learn);

  // dot product vs boost factors. 
  boostOverlaps_(overlaps, boostedOverlaps_);

  // return vector with winning columns indexes
  auto activeVector = inhibitColumns_(boostedOverlaps_);
  // Notify the active SDR that its internal data vector has changed.  Always
  // call SDR's setter methods even if when modifying the SDR's own data
  // inplace.
  sort( activeVector.begin(), activeVector.end() );

  SDR_weight_t active_weights;
  active_weights.assign(activeVector.size(),1);
  size_t weight_idx = 0;
  for (auto column_idx : activeVector)
    active_weights[weight_idx++] = boostedOverlaps_[column_idx];

  active.setSparse( activeVector );
  active.setSparseWeights(active_weights);

  if (learn) {
    adaptSynapses_(input, active);
    updateDutyCycles_(overlaps, active);
    bumpUpWeakColumns_();
    updateBoostFactors_();
    if (isUpdateRound_()) {
      updateInhibitionRadius_();
      updateMinDutyCycles_();
    }
  }

  return overlaps;
}


void SpatialPooler::boostOverlaps_(const vector<SynapseIdx> &overlaps, //TODO use Eigen sparse vector here
                                   vector<Real> &boosted) const {
  if(boostStrength_ < static_cast<Real>(htm::Epsilon)) { //boost ~ 0.0, we can skip these computations, just copy the data
    boosted.assign(overlaps.begin(), overlaps.end());
    return;
  }
  for (size_t i = 0; i < numColumns_; i++) {
    boosted[i] = overlaps[i] * boostFactors_[i];
  }
}


UInt SpatialPooler::initMapColumn_(UInt column) const {
  NTA_ASSERT(column < numColumns_);
  vector<UInt> columnCoords;
  const CoordinateConverterND columnConv(columnDimensions_);
  columnConv.toCoord(column, columnCoords);

  vector<UInt> inputCoords;
  inputCoords.reserve(columnCoords.size());
  for (Size i = 0; i < columnCoords.size(); i++) {
    const Real inputCoord = ((Real)columnCoords[i] + 0.5f) *
                            (inputDimensions_[i] / (Real)columnDimensions_[i]);
    inputCoords.push_back((UInt32)floor(inputCoord));
  }

  const CoordinateConverterND inputConv(inputDimensions_);
  return inputConv.toIndex(inputCoords);
}


vector<UInt> SpatialPooler::initMapPotential_(UInt column, bool wrapAround) {
  NTA_ASSERT(column < numColumns_);
  const UInt centerInput = initMapColumn_(column);

  vector<UInt> columnInputs;
  for (const auto input : Neighborhood(centerInput, potentialRadius_, inputDimensions_, wrapAround, /*skip_center=*/false)) {
      columnInputs.push_back(input);
  }

  const UInt numPotential = static_cast<UInt>(round(columnInputs.size() * potentialPct_));
  const auto selectedInputs = rng_.sample<UInt>(columnInputs, numPotential);
  const vector<UInt> potential = VectorHelpers::sparseToBinary<UInt>(selectedInputs, numInputs_);
  return potential;
}


Permanence SpatialPooler::initPermConnected_() {
  return static_cast<Permanence>(rng_.realRange(synPermConnected_, maxPermanence));
}


Permanence SpatialPooler::initPermNonConnected_() {
  return static_cast<Permanence>(rng_.realRange(minPermanence, synPermConnected_));
}


vector<Permanence> SpatialPooler::initPermanence_(const vector<UInt> &potential, //TODO make potential sparse
                                                  const Real connectedPct) {
  vector<Permanence> perm(numInputs_, 0);
  for (size_t i = 0; i < numInputs_; i++) {
    if (potential[i] < 1) {
      continue;
    }

    if (rng_.getReal64() <= connectedPct) {
      perm[i] = initPermConnected_();
    } else {
      perm[i] = initPermNonConnected_();
    }
  }

  return perm;
}


void SpatialPooler::updateInhibitionRadius_() {
  if (globalInhibition_) {
    setInhibitionRadius( *max_element(columnDimensions_.cbegin(), columnDimensions_.cend()) );
    return;
  }

  Real connectedSpan = 0.0f;
  for (UInt i = 0; i < numColumns_; i++) {
    connectedSpan += avgConnectedSpanForColumnND_(i);
  }
  connectedSpan /= numColumns_;
  const Real columnsPerInput = avgColumnsPerInput_();
  const Real diameter = connectedSpan * columnsPerInput;
  Real radius = (diameter - 1) / 2.0f;
  radius = max((Real)1.0, radius);

  setInhibitionRadius(static_cast<UInt>(round(radius)));
}


void SpatialPooler::updateMinDutyCycles_() {
  if (globalInhibition_ ||
      inhibitionRadius_ >=
          *max_element(columnDimensions_.begin(), columnDimensions_.end())) {
    updateMinDutyCyclesGlobal_();
  } else {
    updateMinDutyCyclesLocal_();
  }
}


void SpatialPooler::updateMinDutyCyclesGlobal_() {
  const Real maxOverlapDutyCycles =
      *max_element(overlapDutyCycles_.begin(), overlapDutyCycles_.end());

  fill(minOverlapDutyCycles_.begin(), minOverlapDutyCycles_.end(),
       minPctOverlapDutyCycles_ * maxOverlapDutyCycles);
}


void SpatialPooler::updateMinDutyCyclesLocal_() {
  for (UInt i = 0; i < numColumns_; i++) {
    Real maxOverlapDuty = overlapDutyCycles_[i]; //start with the center, which is column 'i'
    const auto& hood = neighborMap_[i];
    for(const auto column : hood) {
      maxOverlapDuty = max(maxOverlapDuty, overlapDutyCycles_[column]);
    }
    minOverlapDutyCycles_[i] = maxOverlapDuty * minPctOverlapDutyCycles_;
  }
}


void SpatialPooler::updateDutyCycles_(const vector<SynapseIdx> &overlaps,
                                      SDR &active) {

  // Turn the overlaps array into an SDR. Convert directly to flat-sparse to
  // avoid copies and  type convertions.
  SDR newOverlap({ numColumns_ });
  auto &overlapsSparseVec = newOverlap.getSparse();
  for (UInt i = 0; i < numColumns_; i++) {
    if( overlaps[i] != 0 )
      overlapsSparseVec.push_back( i );
  }
  newOverlap.setSparse( overlapsSparseVec );

  const UInt period = std::min(dutyCyclePeriod_, iterationNum_);

  updateDutyCyclesHelper_(overlapDutyCycles_, newOverlap, period);
  updateDutyCyclesHelper_(activeDutyCycles_, active, period);
}


Real SpatialPooler::avgColumnsPerInput_() const {
  const size_t numDim = max(columnDimensions_.size(), inputDimensions_.size());
  Real columnsPerInput = 0.0f;
  for (size_t i = 0; i < numDim; i++) {
    const Real col = (Real)((i < columnDimensions_.size()) ? columnDimensions_[i] : 1);
    const Real input = (Real)((i < inputDimensions_.size()) ? inputDimensions_[i] : 1);
    columnsPerInput += col / input;
  }
  return columnsPerInput / numDim;
}


Real SpatialPooler::avgConnectedSpanForColumnND_(const UInt column) const {
  NTA_ASSERT(column < numColumns_);

  //get connected synapses
  const auto& connectedDense = getPermanence( column, synPermConnected_ + htm::Epsilon );

  const auto numDimensions = inputDimensions_.size();
  vector<UInt> maxCoord(numDimensions, 0);
  vector<UInt> minCoord(numDimensions, *max_element(inputDimensions_.begin(),
                                                    inputDimensions_.end()));
  const CoordinateConverterND conv(inputDimensions_);
  bool all_zero = true;
  for(UInt i = 0; i < numInputs_; i++) {
    if( connectedDense[i] < synPermConnected_ ) // 0.0 for empty == not-conected values
      continue;
    all_zero = false;
    vector<UInt> columnCoord;
    conv.toCoord(i, columnCoord);
    for (size_t j = 0; j < columnCoord.size(); j++) {
      maxCoord[j] = max(maxCoord[j], columnCoord[j]); //FIXME this computation may be flawed
      minCoord[j] = min(minCoord[j], columnCoord[j]);
    }
  }
  if( all_zero ) return 0.0f;

  UInt totalSpan = 0;
  for (size_t j = 0; j < inputDimensions_.size(); j++) {
    totalSpan += maxCoord[j] - minCoord[j] + 1;
  }

  return (Real)totalSpan / inputDimensions_.size();
}


void SpatialPooler::adaptSynapses_(const SDR &input,
                                   const SDR &active) {
  for(const auto &column : active.getSparse()) {
    connections_.adaptSegment(column, input, synPermActiveInc_, synPermInactiveDec_);
    connections_.raisePermanencesToThreshold( column, stimulusThreshold_ );
  }
}


void SpatialPooler::bumpUpWeakColumns_() {
  for (size_t i = 0; i < numColumns_; i++) {
    if (overlapDutyCycles_[i] >= minOverlapDutyCycles_[i]) {
      continue;
    }
    connections_.bumpSegment( static_cast<Segment>(i), synPermBelowStimulusInc_ );
  }
}


void SpatialPooler::updateDutyCyclesHelper_(vector<Real> &dutyCycles,
                                            const SDR &newValues,
                                            const UInt period) {
  NTA_ASSERT(period > 0);
  NTA_ASSERT(dutyCycles.size() == newValues.size) << "duty dims: " << dutyCycles.size() << " SDR dims: " << newValues.size;

  // Duty cycles are exponential moving averages, typically written like:
  //   alpha = 1 / period
  //   DC( time ) = DC( time - 1 ) * (1 - alpha) + value( time ) * alpha
  // However since the values are sparse this equation is split into two loops,
  // and the second loop iterates over only the non-zero values.

  const Real decay = (period - 1) / static_cast<Real>(period);
  for (Size i = 0; i < dutyCycles.size(); i++)
    dutyCycles[i] *= decay;

  const Real increment = 1.0f / period;  // All non-zero values are 1.
  for(const auto idx : newValues.getSparse())
    dutyCycles[idx] += increment;
}


void SpatialPooler::updateBoostFactors_() {
  if (globalInhibition_) {
    updateBoostFactorsGlobal_();
  } else {
    updateBoostFactorsLocal_();
  }
}


void applyBoosting_(const size_t i,
		    const Real targetDensity, 
		    const vector<Real>& actualDensity,
		    const Permanence boost,
	            vector<Real>& output) {
  if(boost < htm::Epsilon) return; //skip for disabled boosting
  output[i] = exp((targetDensity - actualDensity[i]) * boost); //TODO doc this code
}


void SpatialPooler::updateBoostFactorsGlobal_() {
  Permanence targetDensity;
  if (numActiveColumnsPerInhArea_ > 0) {
    UInt inhibitionArea = 1u;
    for(const auto dim : columnDimensions_) {
      inhibitionArea *= min(dim, 2 * inhibitionRadius_ + 1);
    } 
    NTA_ASSERT(inhibitionArea > 0 && inhibitionArea <= numColumns_);
    targetDensity = ((Real)numActiveColumnsPerInhArea_) / inhibitionArea;
    targetDensity = min(targetDensity, (Real)MAX_LOCALAREADENSITY);
  } else {
    targetDensity = localAreaDensity_;
  }
  
  for (size_t i = 0; i < numColumns_; ++i) { 
    applyBoosting_(i, targetDensity, activeDutyCycles_, boostStrength_, boostFactors_);
  }
}


void SpatialPooler::updateBoostFactorsLocal_() {
  for (UInt i = 0; i < numColumns_; ++i) {
    Real localActivityDensity = 0.0f;
    
    const auto& hood = neighborMap_[i]; //hood is vector<> of cached neighborhood values
    //optimization: In wrapAround, number of neighbors to be considered is solely a function of the inhibition radius,
    // the number of dimensions, and of the size of each of those dimenions. 
    // Or in non-wrap, if we use cached hood, we obtain the value the same as hood.size()
    const UInt numNeighbors = static_cast<UInt>(hood.size()) + 1; 
    //start by adding the center ('i') which is not included in the hood
    localActivityDensity += activeDutyCycles_[i]; //include the center, which is 'i' (not included in hood)

    //for(auto neighbor: Neighborhood(i, inhibitionRadius_, columnDimensions_, wrapAround_)) {
    for (const auto neighbor : hood) {
      localActivityDensity += activeDutyCycles_[neighbor];
      //numNeighbors++;
    }
    const Permanence targetDensity = static_cast<Permanence>(localActivityDensity / numNeighbors);
    applyBoosting_(i, targetDensity, activeDutyCycles_, boostStrength_, boostFactors_);
  }
}


void SpatialPooler::updateBookeepingVars_(bool learn) {
  iterationNum_++;
  if (learn) {
    iterationLearnNum_++;
  }
}

/**
 *  helper function to compute area (ie for inhibition) in nD. 
 *  This is typically a "hyper-cube" but takes into account that
 *  dimensions must not be a cube.
 *
 *  @return area (=num columns) within the hyper-cube in nD with radius.
 *  #TODO for nD, support also nD radius (not just scalar, but vector with radii for each dim)
 *  #TODO or switch also radius to percentage of the dim (that would solve the above)
 **/
UInt getAreaND_(const vector<UInt>& dimensions, const Real radius) {
  NTA_ASSERT(radius > 0);
  NTA_ASSERT(not dimensions.empty());

  Real area = 1;
  for(const auto dim: dimensions) {
    area *= min(static_cast<Real>(dim), (2* radius + 1));
  }
  
  NTA_ASSERT(area >= 1);
  return static_cast<UInt>(area);
}

vector<CellIdx> SpatialPooler::inhibitColumns_(const vector<Real> &overlaps) const {
  Real density = localAreaDensity_; //option 1: used localAreaDensity
  if (numActiveColumnsPerInhArea_ > 0) { //option 2: used numActiveColumnsPerInhArea in constructor
    const UInt inhibitionArea = getAreaND_(columnDimensions_, static_cast<Real>(inhibitionRadius_)); 
    NTA_ASSERT(inhibitionArea <= numColumns_);
    density = ((Real)numActiveColumnsPerInhArea_) / inhibitionArea;
    density = min(density, (Real)MAX_LOCALAREADENSITY);
  }
  NTA_ASSERT(density > 0.0f and density < 1.0f);

  if (globalInhibition_ ||
      inhibitionRadius_ > *max_element(columnDimensions_.begin(), columnDimensions_.end())) {
    return inhibitColumnsGlobal_(overlaps, density);
  } else {
    return inhibitColumnsLocal_(overlaps, density);
  }
}


vector<CellIdx> SpatialPooler::inhibitColumnsGlobal_(const vector<Real> &overlaps,
                                          const Real density) const {
  const UInt numDesired = static_cast<UInt>((density * numColumns_));
  NTA_CHECK(numDesired > 0) << "Not enough columns (" << numColumns_ << ") "
                            << "for desired density (" << density << ").";
  // Sort the columns by the amount of overlap.  First make a list of all of the
  // column indexes.
  vector<CellIdx> activeColumns(numColumns_);
  std::iota(activeColumns.begin(), activeColumns.end(), 0); //fill with sequence 0,1,..N

  // Compare the column indexes by their overlap.
  auto compare = [&overlaps](const UInt &a, const UInt &b) -> bool
    {return (overlaps[a] == overlaps[b]) ? (a > b) : (overlaps[a] > overlaps[b]) ;};  //for determinism if overlaps match (tieBreaker does not solve that),
  //otherwise we'd return just `return overlaps[a] > overlaps[b]`. 

  // Do a partial sort to divide the winners from the losers.  This sort is
  // faster than a regular sort because it stops after it partitions the
  // elements about the Nth element, with all elements on their correct side of
  // the Nth element.
  std::nth_element(
    activeColumns.begin(),
    activeColumns.begin() + numDesired,
    activeColumns.end(),
    compare);
  // Remove the columns which lost the competition.
  activeColumns.resize(numDesired);
  // Finish sorting the winner columns by their overlap.
  std::sort(activeColumns.begin(), activeColumns.end(), compare);
  // Remove sub-threshold winners
  while( !activeColumns.empty() &&
         overlaps[activeColumns.back()] < stimulusThreshold_) {
      activeColumns.pop_back();
  }
  
  activeColumns.shrink_to_fit();
  return activeColumns;
}


vector<CellIdx> SpatialPooler::inhibitColumnsLocal_(const vector<Real> &overlaps,
                                                    const Real density) const {
  NTA_ASSERT(overlaps.size() == numColumns_);
  vector<CellIdx> activeColumns;
  //optimization: reserve for numDesired approximation
  const UInt approxNumDesired = static_cast<UInt>(density * numColumns_); //note: this is just a heuristic, not precise number. It can be used for global inh, 
  //..but here the density is requested for inhibition radius. The guess is it correlates to the global somehow. 
  activeColumns.reserve(approxNumDesired); 

  // Tie-breaking: when overlaps are equal, columns that have already been
  // selected are treated as "bigger".
  vector<bool> alreadyUsedColumn(numColumns_, false); // in tie we prefer already used columns

  for (UInt column = 0; column < numColumns_; column++) {
    if (overlaps[column] < stimulusThreshold_) { //TODO make connections.computeActivity() already drop sub-threshold columns
      continue;
    }

    UInt otherBigger = 0; //how many neighbor columns are bigger/better than this column 'column'. 
    //..aka. how many times this column lost. 

    const auto& hood = neighborMap_.at(column);
    // Optimization: In wrapAround, number of neighbors to be considered is solely a function of the inhibition radius, 
    // the number of dimensions, and of the size of each of those dimenion
    const UInt numNeighbors = static_cast<UInt>(hood.size());
    //const UInt numDesiredLocalActive = static_cast<UInt>(ceil(density * (numNeighbors + 1)));
    const UInt numDesiredLocalActive = static_cast<UInt>(0.5f + (density * (numNeighbors + 1)));
    NTA_ASSERT(numDesiredLocalActive > 0);
    
    //for(auto neighbor: Neighborhood(column, inhibitionRadius_,columnDimensions_, wrapAround_, false /*skip center*/)) { 
    for (const auto neighbor: hood) {
      NTA_ASSERT(neighbor != column);

      if (overlaps[neighbor] > overlaps[column] || ( (overlaps[neighbor] == overlaps[column]) && alreadyUsedColumn[neighbor])) { //this column lost to a neighbor
        otherBigger++;
	if (otherBigger >= numDesiredLocalActive) { break; }
      }
    } 

    if (otherBigger < numDesiredLocalActive) { //successful column, add it
      activeColumns.push_back(column);
      alreadyUsedColumn[column] = true;
    }
  }
  //activeColumns.shrink_to_fit();
  return activeColumns;
}


bool SpatialPooler::isUpdateRound_() const {
  return (iterationNum_ % updatePeriod_) == 0;
}

namespace htm {
std::ostream& operator<< (std::ostream& stream, const SpatialPooler& self)
{
  stream << "Spatial Pooler " << self.connections;
  return stream;
}
}


//----------------------------------------------------------------------
// Debugging helpers
//----------------------------------------------------------------------

// Print the main SP creation parameters
void SpatialPooler::printParameters(std::ostream& out) const {
  out << "------------CPP SpatialPooler Parameters ------------------\n";
  out << "iterationNum                = " << getIterationNum() << std::endl
      << "iterationLearnNum           = " << getIterationLearnNum() << std::endl
      << "numInputs                   = " << getNumInputs() << std::endl
      << "numColumns                  = " << getNumColumns() << std::endl
      << "numActiveColumnsPerInhArea  = " << getNumActiveColumnsPerInhArea()
      << std::endl
      << "potentialPct                = " << getPotentialPct() << std::endl
      << "globalInhibition            = " << getGlobalInhibition() << std::endl
      << "localAreaDensity            = " << getLocalAreaDensity() << std::endl
      << "stimulusThreshold           = " << getStimulusThreshold() << std::endl
      << "synPermActiveInc            = " << getSynPermActiveInc() << std::endl
      << "synPermInactiveDec          = " << getSynPermInactiveDec()
      << std::endl
      << "synPermConnected            = " << getSynPermConnected() << std::endl
      << "minPctOverlapDutyCycles     = " << getMinPctOverlapDutyCycles()
      << std::endl
      << "dutyCyclePeriod             = " << getDutyCyclePeriod() << std::endl
      << "boostStrength               = " << getBoostStrength() << std::endl
      << "spVerbosity                 = " << getSpVerbosity() << std::endl
      << "wrapAround                  = " << getWrapAround() << std::endl
      << "version                     = " << version() << std::endl;
}

void SpatialPooler::printState(const vector<UInt> &state, std::ostream& out) const {
  out << "[  ";
  for (UInt i = 0; i != state.size(); ++i) {
    if (i > 0 && i % 10 == 0) {
      out << "\n   ";
    }
    out << state[i] << " ";
  }
  out << "]\n";
}

void SpatialPooler::printState(const vector<Real> &state, std::ostream& out) const {
  out << "[  ";
  for (UInt i = 0; i != state.size(); ++i) {
    if (i > 0 && i % 10 == 0) {
      out << "\n   ";
    }
    out << state[i];
  }
  out << "]\n";
}


/** equals implementation based on text serialization */
bool SpatialPooler::operator==(const SpatialPooler& o) const{
  // Store the simple variables first.
  if (numInputs_ != o.numInputs_) return false;
  if (numColumns_ != o.numColumns_) return false;
  if (potentialRadius_ != o.potentialRadius_) return false;
  if (potentialPct_ != o.potentialPct_) return false;
  if (initConnectedPct_ != o.initConnectedPct_) return false;
  if (globalInhibition_ != o.globalInhibition_) return false;
  if (numActiveColumnsPerInhArea_ != o.numActiveColumnsPerInhArea_) return false;
  if (localAreaDensity_ != o.localAreaDensity_) return false;
  if (stimulusThreshold_ != o.stimulusThreshold_) return false;
  if (inhibitionRadius_ != o.inhibitionRadius_) return false;
  if (dutyCyclePeriod_ != o.dutyCyclePeriod_) return false;
  if (boostStrength_ != o.boostStrength_) return false;
  if (iterationNum_ != o.iterationNum_) return false;
  if (iterationLearnNum_ != o.iterationLearnNum_) return false;
  if (spVerbosity_ != o.spVerbosity_) return false;
  if (updatePeriod_ != o.updatePeriod_) return false;
  if (synPermInactiveDec_ != o.synPermInactiveDec_) return false;
  if (synPermActiveInc_ != o.synPermActiveInc_) return false;
  if (synPermBelowStimulusInc_ != o.synPermBelowStimulusInc_) return false;
  if (synPermConnected_ != o.synPermConnected_) return false;
  if (minPctOverlapDutyCycles_ != o.minPctOverlapDutyCycles_) return false;
  if (wrapAround_ != o.wrapAround_) return false;

  // compare vectors.
  if (inputDimensions_      != o.inputDimensions_) return false;
  if (columnDimensions_     != o.columnDimensions_) return false;
  if (boostFactors_         != o.boostFactors_) return false;
  if (overlapDutyCycles_    != o.overlapDutyCycles_) return false;
  if (activeDutyCycles_     != o.activeDutyCycles_) return false;
  if (minOverlapDutyCycles_ != o.minOverlapDutyCycles_) return false;

  // compare connections
  if (connections_ != o.connections_) return false;

  //Random
  if (rng_ != o.rng_) return false;
  return true;

}

