/* ---------------------------------------------------------------------
 * HTM Community Edition of NuPIC
 * Copyright (C) 2014-2016, Numenta, Inc.
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
 * Implementation of Connections
 */

#include <algorithm> // nth_element
#include <climits>
#include <iomanip>
#include <iostream>
#include <set>

#include <htm/algorithms/Connections.hpp>


using std::endl;
using std::string;
using std::vector;
using std::set;
using namespace htm;

//!using Permanence = UInt16; //TODO try this optimization, overrides Permanence(=Real) from Connections.hpp 

Connections::Connections(const CellIdx numCells, 
		         const Permanence connectedThreshold, 
			 const bool timeseries) {
  initialize(numCells, connectedThreshold, timeseries);
}


void Connections::initialize(CellIdx numCells, Permanence connectedThreshold, bool timeseries) {
  cells_ = vector<CellData>(numCells);
  segments_.clear();
  destroyedSegments_.clear();
  synapses_.clear();
  destroyedSynapses_.clear();
  potentialSynapsesForPresynapticCell_.clear();
  connectedSynapsesForPresynapticCell_.clear();
  potentialSegmentsForPresynapticCell_.clear();
  connectedSegmentsForPresynapticCell_.clear();
  eventHandlers_.clear();
  NTA_CHECK(connectedThreshold >= minPermanence);
  NTA_CHECK(connectedThreshold <= maxPermanence);
  connectedThreshold_ = connectedThreshold - htm::Epsilon;
  iteration_ = 0;

  nextEventToken_ = 0;

  timeseries_ = timeseries;
  reset();
}


UInt32 Connections::subscribe(ConnectionsEventHandler *handler) {
  UInt32 token = nextEventToken_++;
  eventHandlers_[token] = handler;
  return token;
}


void Connections::unsubscribe(UInt32 token) {
  delete eventHandlers_.at(token);
  eventHandlers_.erase(token);
}


void Connections::pruneSegment_(const CellIdx& cell) {
  // This uses a simple heuristic to determine how "useful" a segment is.
  // Heuristic = sum(synapse.permanence ^ power for synapse on segment)
  // Where power is a positive integer.
  // This heuristic favors keeping segments which have many strong synapses over
  // segments with fewer or weaker synapses.
  auto leastUsefulSegment   = std::numeric_limits<Segment>::max();
  auto leastUsefulHeuristic = std::numeric_limits<double>::max();
  for (const Segment& segment : segmentsForCell(cell)) {
    auto heuristic = 0.0f;
    for (const Synapse& syn : synapsesForSegment(segment)) {
      const auto p = dataForSynapse(syn).permanence;
      heuristic += p * p;
    }
    if ((heuristic < leastUsefulHeuristic)
        || (heuristic == leastUsefulHeuristic && segment < leastUsefulSegment)) { // Needed for deterministic sort.
      leastUsefulSegment = segment;
      leastUsefulHeuristic = heuristic;
    }
  }
  destroySegment(leastUsefulSegment);
}

Segment Connections::createSegment(const CellIdx cell, 
	                           const SegmentIdx maxSegmentsPerCell) {

  // Limit number of segments per cell. If exceeded, remove the least recently used ones.
  NTA_CHECK(maxSegmentsPerCell > 0);
  NTA_CHECK(cell < numCells());
  while (numSegments(cell) >= maxSegmentsPerCell) {
    pruneSegment_(cell);
  }
  NTA_ASSERT(numSegments(cell) <= maxSegmentsPerCell);

  // Proceed to create a new segment.
  const SegmentData& segmentData = SegmentData(cell);
  Segment segment;
  if (!destroyedSegments_.empty() ) { //reuse old, destroyed segs
    segment = destroyedSegments_.back();
    destroyedSegments_.pop_back();
    segments_[segment] = segmentData;
  } else { //create a new segment
    NTA_CHECK(segments_.size() < std::numeric_limits<Segment>::max()) << "Add segment failed: Range of Segment (data-type) insufficinet size."
	    << (size_t)segments_.size() << " < " << (size_t)std::numeric_limits<Segment>::max();
    segment = static_cast<Segment>(segments_.size());
    segments_.push_back(segmentData);
  }

  CellData &cellData = cells_[cell];
  cellData.segments.push_back(segment); // Assign the new segment to its mother-cell.

  for (auto h : eventHandlers_) {
    h.second->onCreateSegment(segment);
  }

  return segment;
}


Synapse Connections::createSynapse(Segment segment,
                                   CellIdx presynapticCell,
                                   Permanence permanence) {

  // Skip cells that are already synapsed on by this segment
  // Biological motivation (?):
  // There are structural constraints on the shapes of axons & synapses
  // which prevent a large number duplicate of connections.
  //
  // It's important to prevent cells from growing duplicate synapses onto a segment,
  // because otherwise a strong input would be sampled many times and grow many synapses.
  // That would give such input a stronger connection.
  // Synapses are supposed to have binary effects (0 or 1) but duplicate synapses give
  // them (synapses 0/1) varying levels of strength.
  for (const Synapse& syn : synapsesForSegment(segment)) {
    const CellIdx existingPresynapticCell = dataForSynapse(syn).presynapticCell; //TODO 1; add way to get all presynaptic cells for segment (fast)
    if (presynapticCell == existingPresynapticCell) {
      //synapse (connecting to this presyn cell) already exists on the segment; don't create a new one, exit early and return the existing
      NTA_ASSERT(synapseExists_(syn));
      //TODO what is the strategy on creating a new synapse, while the same already exists (on the same segment, presynapticCell) ??
      //1. just keep the older (former default)
      //2. throw an error (ideally, user should not createSynapse() but rather updateSynapsePermanence())
      //3. create a duplicit new synapse -- NO. This is the only choice that is incorrect! HTM works on binary synapses, duplicates would break that.
      //4. update to the max of the permanences (default)

      auto& synData = synapses_[syn];
      if(permanence > synData.permanence) updateSynapsePermanence(syn, permanence);
      return syn;
    }
  } //else: the new synapse is not duplicit, so keep creating it. 

  // Get an index into the synapses_ list, for the new synapse to reside at.
  Synapse synapse;
  if (!destroyedSynapses_.empty() ) {
    synapse = destroyedSynapses_.back();
    destroyedSynapses_.pop_back();
  } else {
    NTA_CHECK(synapses_.size() < std::numeric_limits<Synapse>::max())
      << "Add synapse failed: Range of Synapse (data-type) insufficient size."
	    << synapses_.size() << " < " << (size_t)std::numeric_limits<Synapse>::max();
    synapse = static_cast<Synapse>(synapses_.size());
    synapses_.emplace_back();
  }

  // Fill in the new synapse's data
  SynapseData &synapseData    = synapses_[synapse];
  synapseData.presynapticCell = presynapticCell;
  synapseData.segment         = segment;
  // Start in disconnected state.
  synapseData.permanence           = connectedThreshold_ - static_cast<Permanence>(1.0);
  synapseData.presynapticMapIndex_ = 
    (Synapse)potentialSynapsesForPresynapticCell_[presynapticCell].size();
  potentialSynapsesForPresynapticCell_[presynapticCell].push_back(synapse);
  potentialSegmentsForPresynapticCell_[presynapticCell].push_back(segment);

  SegmentData &segmentData = segments_[segment];
  segmentData.synapses.push_back(synapse);


  for (auto h : eventHandlers_) {
    h.second->onCreateSynapse(synapse);
  }

  updateSynapsePermanence(synapse, permanence);

  return synapse;
}


bool Connections::synapseExists_(const Synapse synapse, bool fast) const {
  if(synapse >= synapses_.size()) return false; //out of bounds. Can happen after serialization, where only existing synapses are stored.

#ifdef NTA_ASSERTIONS_ON
  fast = false; //in Debug, do the proper, slow check always
#endif
  if(!fast) {
  //proper but slow method to check for valid, existing synapse
  const SynapseData &synapseData = synapses_[synapse];
  const vector<Synapse> &synapsesOnSegment =
      segments_[synapseData.segment].synapses;
  const bool found = (std::find(synapsesOnSegment.begin(), synapsesOnSegment.end(), synapse) != synapsesOnSegment.end());
  //validate the fast & slow methods for same result:
#ifdef NTA_ASSERTIONS_ON
  const bool removed = synapses_[synapse].permanence == -1;
  NTA_ASSERT( (removed and not found) or (not removed and found) );
#endif
  return found;

  } else {
  //quick method. Relies on hack in destroySynapse() where we set synapseData.permanence == -1
  return synapses_[synapse].permanence != -1;
  }
}


/**
 * Helper method to remove a synapse from a presynaptic map, by moving the
 * last synapse in the list over this synapse.
 */
void Connections::removeSynapseFromPresynapticMap_(
    const Synapse index,
    vector<Synapse> &preSynapses,
    vector<Segment> &preSegments)
{
  NTA_ASSERT( !preSynapses.empty() );
  NTA_ASSERT( index < preSynapses.size() );
  NTA_ASSERT( preSynapses.size() == preSegments.size() );

  const auto move = preSynapses.back();
  synapses_[move].presynapticMapIndex_ = index;
  preSynapses[index] = move;
  preSynapses.pop_back();

  preSegments[index] = preSegments.back();
  preSegments.pop_back();
}


void Connections::destroySegment(const Segment segment) {

  for (auto h : eventHandlers_) {
    h.second->onDestroySegment(segment);
  }

  SegmentData &segmentData = segments_[segment];

  // Destroy synapses from the end of the list, so that the index-shifting is
  // easier to do.
  while( !segmentData.synapses.empty() )
    destroySynapse(segmentData.synapses.back());

  CellData &cellData = cells_[segmentData.cell];

  const auto segmentOnCell = std::find(cellData.segments.cbegin(), cellData.segments.cend(), segment);
  NTA_ASSERT(segmentOnCell != cellData.segments.cend()) << "Segment to be destroyed not found on the cell!";
  NTA_ASSERT(*segmentOnCell == segment);

  cellData.segments.erase(segmentOnCell);
  destroyedSegments_.push_back(segment);
}


void Connections::destroySynapse(const Synapse synapse) {
  if(not synapseExists_(synapse, true)) return;

  for (auto h : eventHandlers_) {
    h.second->onDestroySynapse(synapse);
  }

  SynapseData& synapseData = synapses_[synapse]; //like dataForSynapse() but here we need writeable access
  SegmentData &segmentData = segments_[synapseData.segment];
  const auto   presynCell  = synapseData.presynapticCell;

  if( synapseData.permanence >= connectedThreshold_ ) {
    segmentData.numConnected--;

    removeSynapseFromPresynapticMap_(
      synapseData.presynapticMapIndex_,
      connectedSynapsesForPresynapticCell_.at( presynCell ),
      connectedSegmentsForPresynapticCell_.at( presynCell ));

    if( connectedSynapsesForPresynapticCell_.at( presynCell ).empty() ){
      connectedSynapsesForPresynapticCell_.erase( presynCell );
      connectedSegmentsForPresynapticCell_.erase( presynCell );
    }
  }
  else {
    removeSynapseFromPresynapticMap_(
      synapseData.presynapticMapIndex_,
      potentialSynapsesForPresynapticCell_.at( presynCell ),
      potentialSegmentsForPresynapticCell_.at( presynCell ));

    if( potentialSynapsesForPresynapticCell_.at( presynCell ).empty() ){
      potentialSynapsesForPresynapticCell_.erase( presynCell );
      potentialSegmentsForPresynapticCell_.erase( presynCell );
    }
  }

  for(auto i = 0u; i < segmentData.synapses.size(); i++) {
    if (segmentData.synapses[i] == synapse) {
      segmentData.synapses[i] = segmentData.synapses.back();
      segmentData.synapses.pop_back();
      break;
    }
  }

  destroyedSynapses_.push_back(synapse);
}


void Connections::updateSynapsePermanence(const Synapse synapse,
                                          Permanence permanence) {
  permanence = std::min(permanence, maxPermanence );
  permanence = std::max(permanence, minPermanence );

  auto &synData = synapses_[synapse];
  
  const bool before = synData.permanence >= connectedThreshold_;
  const bool after  = permanence         >= connectedThreshold_;

  // update the permanence
  synData.permanence = permanence;

  if( before == after ) { //no change in dis/connected status
      return;
  }
    const auto &presyn    = synData.presynapticCell;
    auto &potentialPresyn = potentialSynapsesForPresynapticCell_[presyn];
    auto &potentialPreseg = potentialSegmentsForPresynapticCell_[presyn];
    auto &connectedPresyn = connectedSynapsesForPresynapticCell_[presyn];
    auto &connectedPreseg = connectedSegmentsForPresynapticCell_[presyn];
    const auto &segment   = synData.segment;
    auto &segmentData     = segments_[segment];
    
    if( after ) { //connect
      segmentData.numConnected++;

      // Remove this synapse from presynaptic potential synapses.
      removeSynapseFromPresynapticMap_( synData.presynapticMapIndex_,
                                        potentialPresyn, potentialPreseg );

      // Add this synapse to the presynaptic connected synapses.
      synData.presynapticMapIndex_ = (Synapse)connectedPresyn.size();
      connectedPresyn.push_back( synapse );
      connectedPreseg.push_back( segment );
    }
    else { //disconnected
      segmentData.numConnected--;

      // Remove this synapse from presynaptic connected synapses.
      removeSynapseFromPresynapticMap_( synData.presynapticMapIndex_,
                                        connectedPresyn, connectedPreseg );

      // Add this synapse to the presynaptic connected synapses.
      synData.presynapticMapIndex_ = (Synapse)potentialPresyn.size();
      potentialPresyn.push_back( synapse );
      potentialPreseg.push_back( segment );
    }

    for (auto h : eventHandlers_) { //TODO handle callbacks in performance-critical method only in Debug?
      h.second->onUpdateSynapsePermanence(synapse, permanence);
    }
}


SegmentIdx Connections::idxOnCellForSegment(const Segment segment) const {
  const vector<Segment> &segments = segmentsForCell(cellForSegment(segment));
  const auto it = std::find(segments.begin(), segments.end(), segment);
  NTA_ASSERT(it != segments.end());
  return (SegmentIdx)std::distance(segments.begin(), it);
}


bool Connections::compareSegments(const Segment a, const Segment b) const {
  const SegmentData &aData = segments_[a];
  const SegmentData &bData = segments_[b];
  // default sort by cell
  if (aData.cell == bData.cell)
    // fallback to segment index
    return a < b;
  else return aData.cell < bData.cell;
}


vector<Synapse> Connections::synapsesForPresynapticCell(const CellIdx presynapticCell) const {
  vector<Synapse> all;

  if (potentialSynapsesForPresynapticCell_.count(presynapticCell)) {
    const auto& potential = potentialSynapsesForPresynapticCell_.at(presynapticCell);
    all.assign(potential.cbegin(), potential.cend());
  }

  if (connectedSynapsesForPresynapticCell_.count(presynapticCell)) {
    const auto& connected = connectedSynapsesForPresynapticCell_.at(presynapticCell);
    all.insert( all.cend(), connected.cbegin(), connected.cend());
  }

  return all;
}


void Connections::reset() noexcept
{
  if( not timeseries_ ) {
    NTA_WARN << "Connections::reset() called with timeseries=false.";
  }
  previousUpdates_.clear();
  currentUpdates_.clear();
}

vector<SynapseIdx> Connections::computeActivity(const vector<CellIdx> &activePresynapticCells, const bool learn) {

  vector<SynapseIdx> numActiveConnectedSynapsesForSegment(segments_.size(), 0);
  if(learn) iteration_++;

  if( timeseries_ ) {
    // Before each cycle of computation move the currentUpdates to the previous
    // updates, and zero the currentUpdates in preparation for learning.
    previousUpdates_.swap( currentUpdates_ );
    currentUpdates_.clear();
  }

  // Iterate through all connected synapses.
  for (const auto& cell : activePresynapticCells) {
    if (connectedSegmentsForPresynapticCell_.count(cell)) {
      for(const auto& segment : connectedSegmentsForPresynapticCell_.at(cell)) {
        ++numActiveConnectedSynapsesForSegment[segment];
      }
    }
  }
  return numActiveConnectedSynapsesForSegment;
}


vector<SynapseIdx> Connections::computeActivity(
    vector<SynapseIdx> &numActivePotentialSynapsesForSegment,
    const vector<CellIdx> &activePresynapticCells,
    const bool learn) {
  NTA_ASSERT(numActivePotentialSynapsesForSegment.size() == segments_.size());

  // Iterate through all connected synapses.
  const vector<SynapseIdx>& numActiveConnectedSynapsesForSegment = computeActivity( activePresynapticCells, learn );
  NTA_ASSERT(numActiveConnectedSynapsesForSegment.size() == segments_.size());

  // Iterate through all potential synapses.
  std::copy( numActiveConnectedSynapsesForSegment.begin(),
             numActiveConnectedSynapsesForSegment.end(),
             numActivePotentialSynapsesForSegment.begin());

  for (const auto& cell : activePresynapticCells) {
    if (potentialSegmentsForPresynapticCell_.count(cell)) {
      for(const auto& segment : potentialSegmentsForPresynapticCell_.at(cell)) {
        ++numActivePotentialSynapsesForSegment[segment];
      }
    }
  }
  return numActiveConnectedSynapsesForSegment;
}


void Connections::adaptSegment(const Segment segment, 
                               const SDR &inputs,
                               const Permanence increment,
                               const Permanence decrement, 
			       const bool pruneZeroSynapses, 
			       const UInt segmentThreshold)
{
  const auto &inputArray = inputs.getDense();

  if( timeseries_ ) {
    previousUpdates_.resize( synapses_.size(), minPermanence );
    currentUpdates_.resize(  synapses_.size(), minPermanence );
  }

  vector<Synapse> destroyLater;
  for(const auto synapse: synapsesForSegment(segment)) {
      const SynapseData &synapseData = dataForSynapse(synapse);

      Permanence update;
      if( inputArray[synapseData.presynapticCell] ) {
        update = increment;
      } else {
        update = -decrement;
      }

    //prune permanences that reached zero
    if (pruneZeroSynapses and 
        synapseData.permanence + update < htm::minPermanence + htm::Epsilon) { //new value will disconnect the synapse
      destroyLater.push_back(synapse);
      prunedSyns_++; //for statistics
      continue;
    }

    //update synapse, but for TS only if changed
    if(timeseries_) {
      if( update != previousUpdates_[synapse] ) {
        updateSynapsePermanence(synapse, synapseData.permanence + update);
      }
      currentUpdates_[ synapse ] = update;
    } else {
      updateSynapsePermanence(synapse, synapseData.permanence + update);
    }
  }

  //destroy synapses accumulated for pruning
  for(const auto pruneSyn : destroyLater) {
    destroySynapse(pruneSyn);
  }

  //destroy segment if it has too few synapses left -> will never be able to connect again
  #ifdef NTA_ASSERTIONS_ON
  if(segmentThreshold > 0) {
    NTA_ASSERT(pruneZeroSynapses) << "Setting segmentThreshold only makes sense when pruneZeroSynapses is allowed.";
  }
  #endif
  if(pruneZeroSynapses and synapsesForSegment(segment).size() < segmentThreshold) { 
    destroySegment(segment);
    prunedSegs_++; //statistics
  }
}


/**
 * Called for under-performing Segments (can have synapses pruned, etc.). After
 * the call, Segment will have at least segmentThreshold synapses connected, so
 * the Segment could be active next time.
 */
void Connections::raisePermanencesToThreshold(
                  const Segment    segment,
                  const UInt       segmentThreshold)
{
  if( segmentThreshold == 0 ) // No synapses requested to be connected, done.
    return;

  NTA_ASSERT(segment < segments_.size()) << "Accessing segment out of bounds.";
  auto &segData = segments_[segment];
  if( segData.numConnected >= segmentThreshold )
    return;   // The segment already satisfies the requirement, done.

  vector<Synapse> &synapses = segData.synapses;
  if( synapses.empty())
    return;   // No synapses to raise permanences to, no work to do.

  // Prune empty segment? No. 
  // The SP calls this method, but the SP does not do any pruning. 
  // The TM already has code to do pruning, but it doesn't ever call this method.

  // There can be situations when synapses are pruned so the segment has too few
  // synapses to ever activate, so we cannot satisfy the >= segmentThreshold
  // connected.  In this case the method should do the next best thing and
  // connect as many synapses as it can.

  // Keep segmentThreshold within synapses range.
  const auto threshold = std::min((size_t)segmentThreshold, synapses.size());

  // Sort the potential pool by permanence values, and look for the synapse with
  // the N'th greatest permanence, where N is the desired minimum number of
  // connected synapses.  Then calculate how much to increase the N'th synapses
  // permance by such that it becomes a connected synapse.  After that there
  // will be at least N synapses connected.

  // Threshold is ensured to be >=1 by condition at very beginning if(thresh == 0)... 
  auto minPermSynPtr = synapses.begin() + threshold - 1;

  const auto permanencesGreater = [&](const Synapse &A, const Synapse &B)
    { return synapses_[A].permanence > synapses_[B].permanence; };
  // Do a partial sort, it's faster than a full sort.
  std::nth_element(synapses.begin(), minPermSynPtr, synapses.end(), permanencesGreater);

  const auto increment = connectedThreshold_ - synapses_[ *minPermSynPtr ].permanence;
  if( increment <= static_cast<Permanence>(0.0) ) // If minPermSynPtr is already connected then ...
    return;            // Enough synapses are already connected.

  // Raise the permanence of all synapses in the potential pool uniformly.
  bumpSegment(segment, increment);
}


void Connections::synapseCompetition(
                    const Segment    segment,
                    const SynapseIdx minimumSynapses,
                    const SynapseIdx maximumSynapses)
{
  NTA_ASSERT( minimumSynapses <= maximumSynapses);
  NTA_ASSERT( maximumSynapses > 0 );

  const auto &segData = dataForSegment( segment );

  if( segData.synapses.empty())
    return;   // No synapses to work with, no work to do.

  // Sort the potential pool by permanence values, and look for the synapse with
  // the N'th greatest permanence, where N is the desired number of connected
  // synapses.  Then calculate how much to change the N'th synapses permance by
  // such that it becomes a connected synapse.  After that there will be exactly
  // N synapses connected.
  SynapseIdx desiredConnected;
  if( segData.numConnected < minimumSynapses ) {
    desiredConnected = minimumSynapses;
  }
  else if( segData.numConnected > maximumSynapses ) {
    desiredConnected = maximumSynapses;
  }
  else {
    return;  // The segment already satisfies the requirements, done.
  }
  // Can't connect more synapses than there are in the potential pool.
  desiredConnected = std::min( (SynapseIdx) segData.synapses.size(), desiredConnected);
  // The N'th synapse is at index N-1
  if( desiredConnected != 0 ) {
    desiredConnected--;
  }
  // else {
  //   Corner case: there are no synapses on this segment.
  // }

  vector<Permanence> permanences; permanences.reserve( segData.synapses.size() );
  for( Synapse syn : segData.synapses )
    permanences.push_back( synapses_[syn].permanence );

  // Do a partial sort, it's faster than a full sort.
  auto minPermPtr = permanences.begin() + (segData.synapses.size() - 1 - desiredConnected);
  std::nth_element(permanences.begin(), minPermPtr, permanences.end());

  Permanence delta = (connectedThreshold_ + htm::Epsilon) - *minPermPtr;

  // Change the permance of all synapses in the potential pool uniformly.
  bumpSegment( segment, delta ) ;
}


void Connections::bumpSegment(const Segment segment, const Permanence delta) {
  // TODO: vectorize?
  for( const auto syn : synapsesForSegment(segment) ) {
    updateSynapsePermanence(syn, synapses_[syn].permanence + delta);
  }
}


vector<CellIdx> Connections::presynapticCellsForSegment(const Segment segment) const { //TODO optimize by storing the vector in SegmentData?
  set<CellIdx> presynCells;
  for(const auto synapse: synapsesForSegment(segment)) {
    const auto presynapticCell = dataForSynapse(synapse).presynapticCell;
    presynCells.insert(presynapticCell);
  }
  return vector<CellIdx>(std::begin(presynCells), std::end(presynCells));
}


void Connections::destroyMinPermanenceSynapses(
                              const Segment segment, 
			      const size_t nDestroy,
                              const vector<CellIdx> &excludeCells)
{
  // Don't destroy any cells that are in excludeCells.
  vector<Synapse> destroyCandidates;
  for( Synapse synapse : synapsesForSegment(segment)) {
    const CellIdx presynapticCell = dataForSynapse(synapse).presynapticCell;

    if( not std::binary_search(excludeCells.cbegin(), excludeCells.cend(), presynapticCell)) {
      destroyCandidates.push_back(synapse);
    }
  }

  const auto comparePermanences = [&](const Synapse A, const Synapse B) {
    const Permanence A_perm = dataForSynapse(A).permanence;
    const Permanence B_perm = dataForSynapse(B).permanence;
    if( A_perm == B_perm ) {
      return A < B;
    }
    else {
      return A_perm < B_perm;
    }
  };
  std::sort(destroyCandidates.begin(), destroyCandidates.end(), comparePermanences);

  const size_t destroy = std::min( nDestroy, destroyCandidates.size() );
  for(size_t i = 0; i < destroy; i++) {
    destroySynapse( destroyCandidates[i] );
  }
}



void Connections::growSynapses(const Segment segment, 
		                          const vector<Synapse>& growthCandidates, 
					  const Permanence initialPermanence,
					  Random& rng,
					  const size_t maxNew,
					  const size_t maxSynapsesPerSegment) {

  //0. copy input vector - candidate cells on input
  vector<CellIdx> candidates(growthCandidates.begin(), growthCandidates.end());

  //1. figure the number of new synapses to grow
  size_t nActual = std::min(maxNew, candidates.size());
  if(maxNew == 0) nActual = candidates.size(); //grow all, unlimited

  if(maxSynapsesPerSegment > 0) { // ..Check if we're going to surpass the maximum number of synapses.
    NTA_ASSERT(numSynapses(segment) <= maxSynapsesPerSegment) << "Illegal state, shouldn't be here to begin with.";
    const Int overrun = static_cast<Int>(numSynapses(segment) + nActual - maxSynapsesPerSegment);
    if (overrun > 0) { //..too many synapses, make space for new ones
      destroyMinPermanenceSynapses(segment, static_cast<Int>(overrun), candidates);
    }
    //Recalculate in case we weren't able to destroy as many synapses as needed.
    nActual = std::min(nActual, static_cast<size_t>(maxSynapsesPerSegment) - numSynapses(segment));
  }
  if(nActual == 0) return;

  //2. Pick nActual cells randomly.
  if(maxNew > 0 and maxNew < candidates.size()) {
    rng.shuffle(candidates.begin(), candidates.end());
  }
  const size_t nDesired = numSynapses(segment) + nActual; //num synapses on seg after this function (+-), see #COND
  for (const auto syn : candidates) {
    // #COND: this loop finishes two folds: a) we ran out of candidates (above), b) we grew the desired number of new synapses (below)
    if(numSynapses(segment) == nDesired) break;
    createSynapse(segment, syn, initialPermanence); //TODO createSynapse consider creating a vector of new synapses at once?
  }
}


namespace htm {
/**
 * print statistics in human readable form
 */ 
std::ostream& operator<< (std::ostream& stream, const Connections& self)
{
  stream << "Connections:" << std::endl;
  const auto numPresyns = self.potentialSynapsesForPresynapticCell_.size();
  stream << "    Inputs (" << numPresyns
         << ") ~> Outputs (" << self.cells_.size()
         << ") via Segments (" << self.numSegments() << ")" << std::endl;

  UInt        segmentsMin   = -1;
  Real        segmentsMean  = 0.0f;
  UInt        segmentsMax   = 0u;
  UInt        potentialMin  = -1;
  Real        potentialMean = 0.0f;
  UInt        potentialMax  = 0;
  SynapseIdx  connectedMin  = -1;
  Real        connectedMean = 0.0f;
  SynapseIdx  connectedMax  = 0;
  UInt        synapsesDead      = 0;
  UInt        synapsesSaturated = 0;
  for( const auto &cellData : self.cells_ )
  {
    const UInt numSegments = (UInt) cellData.segments.size();
    segmentsMin   = std::min( segmentsMin, numSegments );
    segmentsMax   = std::max( segmentsMax, numSegments );
    segmentsMean += numSegments;

    for( const auto seg : cellData.segments ) {
      const auto &segData = self.dataForSegment( seg );

      const UInt numPotential = (UInt) segData.synapses.size();
      potentialMin   = std::min( potentialMin, numPotential );
      potentialMax   = std::max( potentialMax, numPotential );
      potentialMean += numPotential;

      connectedMin   = std::min( connectedMin, segData.numConnected );
      connectedMax   = std::max( connectedMax, segData.numConnected );
      connectedMean += segData.numConnected;

      for( const auto syn : segData.synapses ) {
        const auto &synData = self.dataForSynapse( syn );
        if( synData.permanence <= minPermanence + Epsilon )
          { synapsesDead++; }
        else if( synData.permanence >= maxPermanence - Epsilon )
          { synapsesSaturated++; }
      }
    }
  }
  segmentsMean  = segmentsMean  / self.numCells();
  potentialMean = potentialMean / self.numSegments();
  connectedMean = connectedMean / self.numSegments();

  stream << "    Segments on Cell Min/Mean/Max " //TODO print std dev too
         << segmentsMin << " / " << segmentsMean << " / " << segmentsMax << std::endl;
  stream << "    Potential Synapses on Segment Min/Mean/Max "
         << potentialMin << " / " << potentialMean << " / " << potentialMax << std::endl;
  stream << "    Connected Synapses on Segment Min/Mean/Max "
         << connectedMin << " / " << connectedMean << " / " << connectedMax << std::endl;

  stream << "    Synapses Dead (" << (Real) synapsesDead / self.numSynapses()
         << "%) Saturated (" <<   (Real) synapsesSaturated / self.numSynapses() << "%)" << std::endl;
  stream << "    Synapses pruned (" << (Real) self.prunedSyns_ / self.numSynapses() 
	 << "%) Segments pruned (" << (Real) self.prunedSegs_ / self.numSegments() << "%)" << std::endl;
  stream << "    Buffer for destroyed synapses: " << self.destroyedSynapses_.size() 
	 << "    Buffer for destroyed segments: " << self.destroyedSegments_.size() << std::endl;

  return stream;
}
}



bool Connections::operator==(const Connections &o) const {
  try {
  NTA_CHECK (cells_.size() == o.cells_.size()) << "Connections equals: cells_" << cells_.size() << " vs. " << o.cells_.size();
  NTA_CHECK (cells_ == o.cells_) << "Connections equals: cells_" << cells_.size() << " vs. " << o.cells_.size();

  NTA_CHECK (segments_ == o.segments_ ) << "Connections equals: segments_";
  NTA_CHECK (destroyedSegments_ == o.destroyedSegments_ ) << "Connections equals: destroyedSegments_";

  NTA_CHECK (synapses_ == o.synapses_ ) << "Connections equals: synapses_";
  NTA_CHECK (destroyedSynapses_ == o.destroyedSynapses_ ) << "Connections equals: destroyedSynapses_";


  //also check underlying datastructures (segments, and subsequently synapses). Can be time consuming.
  //1.cells:
  for(const auto &cellD : cells_) {
    //2.segments:
    const auto& segments = cellD.segments;
    for(const auto seg : segments) {
      NTA_CHECK( dataForSegment(seg) == o.dataForSegment(seg) ) << "CellData equals: segmentData";
      //3.synapses: 
      const auto& synapses = dataForSegment(seg).synapses;
      for(const auto syn : synapses) {
        NTA_CHECK(dataForSynapse(syn) == o.dataForSynapse(syn) ) << "SegmentData equals: synapseData";
      }
    }
  }


  NTA_CHECK (connectedThreshold_ == o.connectedThreshold_ ) << "Connections equals: connectedThreshold_";
  NTA_CHECK (iteration_ == o.iteration_ ) << "Connections equals: iteration_"; 

  NTA_CHECK(potentialSynapsesForPresynapticCell_ == o.potentialSynapsesForPresynapticCell_);
  NTA_CHECK(connectedSynapsesForPresynapticCell_ == o.connectedSynapsesForPresynapticCell_);
  NTA_CHECK(potentialSegmentsForPresynapticCell_ == o.potentialSegmentsForPresynapticCell_);
  NTA_CHECK(connectedSegmentsForPresynapticCell_ == o.connectedSegmentsForPresynapticCell_);

  NTA_CHECK (timeseries_ == o.timeseries_ ) << "Connections equals: timeseries_";
  NTA_CHECK (previousUpdates_ == o.previousUpdates_ ) << "Connections equals: previousUpdates_";
  NTA_CHECK (currentUpdates_ == o.currentUpdates_ ) << "Connections equals: currentUpdates_";

  NTA_CHECK (prunedSyns_ == o.prunedSyns_ ) << "Connections equals: prunedSyns_";
  NTA_CHECK (prunedSegs_ == o.prunedSegs_ ) << "Connections equals: prunedSegs_";

  } catch(const htm::Exception& ex) {
	  std::cout << "Connection equals: differ! " << ex.what();
    return false;
  }
  return true;
}

