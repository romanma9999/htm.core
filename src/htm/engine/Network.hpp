/* ---------------------------------------------------------------------
 * HTM Community Edition of NuPIC
 * Copyright (C) 2013-2017, Numenta, Inc.
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
 * --------------------------------------------------------------------- */

/** @file
 * Interface for the Network class
 */

#ifndef NTA_NETWORK_HPP
#define NTA_NETWORK_HPP

#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <htm/engine/Region.hpp>
#include <htm/engine/Link.hpp>
#include <htm/ntypes/Collection.hpp>

#include <htm/types/Serializable.hpp>
#include <htm/types/Types.hpp>
#include <htm/utils/Log.hpp>

namespace htm {

class Region;
class Dimensions;
class RegisteredRegionImpl;
class Link;

/**
 * Represents an HTM network. A network is a collection of regions.
 *
 * @nosubgrouping
 */
  class Network : public Serializable
{
public:
  /**
   * @name Construction and destruction
   * @{
   */

  /**
   *
   * Create an new Network
   *
   */
  Network();
  Network(const std::string& filename);

  /**
   * Cannot copy or assign a Network object. But can be moved.
   */
  Network(Network &&) noexcept; // move is allowed
  Network(const Network&) = delete;
  void operator=(const Network&) = delete;

  /**
   * Destructor.
   *
   */
  ~Network();

   /**
   * An alternate way to configure the network.
   * Pass in an yaml or JSON string that defines all regions and links.
   * YAML Syntax:
   *      network:
   *         - registerRegion:            (TODO:)
   *             type: <region type>
   *             path: <path to shared lib to link to>
   *             class: <classname to load>
   *
   *         - addRegion:
   *             name: <region name>
   *             type: <region type>
   *             params: <list of parameters>  (optional)
   *             phase:  <phase ID or a list of phases to put this region into> (optional)
   *
   *         - addLink:
   *             src: <Name of the source region "." Output name>
   *             dest: <Name of the destination region "." Input name>
   *             delay: <iterations to delay> (optional, default=0)
   *
   *
   * JSON syntax:
    *   {network: [
   *       {addRegion: {name: <region name>, type: <region type>, params: {<parameters>}, phase: <phase>}},
   *       {addLink:   {src: "<region name>.<output name>", dest: "<region name>.<output name>", delay: <delay>}},
   *    ]}
  *
   * JSON example:
   *   {network: [
   *       {addRegion: {name: "encoder", type: "RDSERegion", params: {size: 1000, sparsity: 0.2, radius: 0.03, seed: 2019, noise: 0.01}}},
   *       {addRegion: {name: "sp", type: "SPRegion", params: {columnCount: 2048, globalInhibition: true}}},
   *       {addRegion: {name: "tm", type: "TMRegion", params: {cellsPerColumn: 8, orColumnOutputs: true}}},
   *       {addLink:   {src: "encoder.encoded", dest: "sp.bottomUpIn"}},
   *       {addLink:   {src: "sp.bottomUpOut", dest: "tm.bottomUpIn"}}
   *    ]}

   *  On errors it throws an exception.
   */
  void configure(const std::string &yaml);
  
  /**
   * Return the Spec for the region type as a JSON string.
   * Note that the region does NOT have to have been previously added with addRegion( ).
   *
   * @param region_type  The name of the region implementation.
   *                     This is the name of one of the built-in Regions (usually class name) 
   *                     or the region type given to a custom region that has been registered.  
   *                     Python implemented regions are registered as the class name with
   *                     'py.' prepended.
   * @returns  A JSON string containing the Spec.
   */
  std::string getSpecJSON(const std::string &region_type);


  /**
   * Initialize all elements of a network so that it can run.
   *
   * @note This can be called after the Network structure has been set and
   * before Network.run(). However, if you don't call it, Network.run() will
   * call it for you. Also sets up various memory buffers etc. once the Network
   *  structure has been finalized.
   */
  void initialize();

  /**
   * @}
   *
   * @name Internal Serialization methods
   * @{
   */
  /**
   *    saveToFile(path [, fmt])  Open a file and stream to it. 
   *    save(ostream f [, fmt])   Stream to your stream.  
   *    f << net;                 Output human readable text. 
   *
   *    loadFromFile(path [, fmt])
   *    load(istream f [, fmt])
   *
   * @path The filename into which to save/load the streamed serialization.
   * @f    The stream with which to save/load the serialization.
   * @fmt  Format: One of following from enum SerializableFormat
   *   SerializableFormat::BINARY   - A binary format which is the fastest but not portable between platforms (default).
   *   SerializableFormat::PORTABLE - Another Binary format, not quite as fast but is portable between platforms.
   *   SerializableFormat::JSON     - Human readable JSON text format. Slow.
   *   SerializableFormat::XML      - Human readable XML text format. Even slower.
   * or an std::string, one of "BINARY", "PORTABLE", "JSON", "XML".
   *
	 * See Serializable base class for more details (types/Serializable.hpp).
	 */
  CerealAdapter;  // see Serializable.hpp
  // FOR Cereal Serialization
  template<class Archive>
  void save_ar(Archive& ar) const {
    const std::vector<std::shared_ptr<Link>> links = getLinks();
    std::string phases = phasesToString();
    std::string name = "Network";
    ar(cereal::make_nvp("name", name));
    ar(cereal::make_nvp("iteration", iteration_));
    ar(cereal::make_nvp("Regions", regions_));
    ar(cereal::make_nvp("links", links));
    ar(cereal::make_nvp("phases", phases));
  }
  
  // FOR Cereal Deserialization
  template<class Archive>
  void load_ar(Archive& ar) {
    std::vector<std::shared_ptr<Link>> links;
    std::string name, phases;
    ar(cereal::make_nvp("name", name));  // ignore value
    ar(cereal::make_nvp("iteration", iteration_));
    ar(cereal::make_nvp("Regions", regions_));
    ar(cereal::make_nvp("links", links));
    ar(cereal::make_nvp("phases", phases));

    post_load(links);
    phasesFromString(phases);
  }

  /**
   * @}
   *
   * @name Region and Link operations
   *
   * @{
   */

  /**
   * Create a new region in a network.
   *
   * @param name
   *        Name of the region, Must be unique in the network
   * @param nodeType
   *        Type of node in the region, e.g. "FDRNode"
   * @param nodeParams (optional)
   *        A JSON-encoded string specifying writable params
   * @param phase  (optional)
   *        A set of ID's of execution phases to add the region into.
   *        This is useful to group regions that execute together.
   *        If not given, regions are grouped into phase 0.
   *        A region can be a member of any number of phases but only once per phase.
   *
   * @returns A pointer to the newly created Region
   *
   * The optional arguments are implemented as overloads to make it easier
   * to write the Python bindings.
   */
  // An overload to use with phases
  std::shared_ptr<Region> addRegion(const std::string &name,
  					                        const std::string &nodeType,
                                    const std::string &nodeParams,
                                    const std::set<UInt32> &phases);
  // An overload to use with a single phase as an integer (avoid warning of scaler initializer)
  std::shared_ptr<Region> addRegion(const std::string &name, 
                                    const std::string &nodeType, 
                                    const std::string &nodeParams,
                                    UInt32 phase) {
    std::set<UInt32> phases;
    phases.insert(phase);
    return addRegion(name, nodeType, nodeParams, phases);
  }
  // An overload to use without phases (region placed in phase 0)
  std::shared_ptr<Region> addRegion(const std::string &name,
  					                        const std::string &nodeType,
                                    const std::string &nodeParams);
  // An overload to use with phases and nodeParams has already been parsed.
  std::shared_ptr<Region> addRegion(const std::string &name, 
                                    const std::string &nodeType,
                                    ValueMap& vm,
                                    const std::set<UInt32> &phases);
  // An overload to use when restoring an archive with load().
  std::shared_ptr<Region> addRegion(std::shared_ptr<Region>& region);


  /**
   * Removes an existing region from the network.
   *
   * @param name
   *        Name of the Region
   */
  void removeRegion(const std::string &name);

  /**
   * Create a link and add it to the network.
   *
   * @param srcName
   *        Name of the source region
   * @param destName
   *        Name of the destination region
   * @param linkType
   *        Type of the link
   * @param linkParams
   *        Parameters of the link
   * @param srcOutput
   *        Name of the source output
   * @param destInput
   *        Name of the destination input
   * @param propagationDelay
   *            Propagation delay of the link as number of network run
   *            iterations involving the link as input; the delay vectors, if
   *            any, are initially populated with 0's. Defaults to 0=no delay
   */
  std::shared_ptr<Link> link(const std::string &srcName, const std::string &destName,
            const std::string &linkType="", const std::string &linkParams="",
            const std::string &srcOutput = "",
            const std::string &destInput = "",
            const size_t propagationDelay = 0);

  /**
   * Removes a link.
   *
   * @param srcName
   *        Name of the source region
   * @param destName
   *        Name of the destination region
   * @param srcOutputName
   *        Name of the source output
   * @param destInputName
   *        Name of the destination input
   */
  void removeLink(const std::string &srcName, const std::string &destName,
                  const std::string &srcOutputName = "",
                  const std::string &destInputName = "");


  /**
   * Set the source data for a Link identified with the source as "INPUT" and <sourceName>.
   */
  virtual void setInputData(const std::string &sourceName, const Array &data);
  virtual void setInputData(const std::string &sourceName, const Value &vm);

  /**
   * @}
   *
   * @name Access to components
   *
   * @{
   */

  /**
   * Get all regions.
   *
   * @returns A Collection of Region objects in the network
   *          Note: this is a copy of the region list.
   */
  const Collection<std::shared_ptr<Region> > getRegions() const;
  std::shared_ptr<Region> getRegion(const std::string& name) const;

  /**
   * Get all links between regions
   *
   * @returns A Collection of Link objects in the network
   */
  std::vector<std::shared_ptr<Link>> getLinks() const;

  /**
   * Set phases for a region.
   *
   * @param name
   *        Name of the region
   * @param phases
   *        A set of phase ID's to insert the region into (must be positive integers)
   *        If the phase does not exist, it is created.
   */
  void setPhases(const std::string &name, const std::set<UInt32> &phases);

  /**
   * Get phases for a region.
   *
   * @param name
   *        Name of the region
   *
   * @returns Set of phases for the region
   */
  std::set<UInt32> getPhases(const std::string &name) const;

  /**
   * Get minimum phase for regions in this network. If no regions, then min = 0.
   * This returns the phaseID for the first non-empty phase.
   *
   * @returns Minimum phase
   */
  UInt32 getMinPhase() const;

  /**
   * Get maximum phase for regions in this network. If no regions, then returns = 0.
   * The next phase to be created should be getMaxPhase() + 1.
   *
   * @returns Maximum phase
   */
  UInt32 getMaxPhase() const;

  /**
   * Set the minimum enabled phase for this network.
   * This is normally 1 if there are phases.
   * If a region is created without specifiying a phase it is put in phase 0.
   * Use this function to reduce the range of phases to be executed.
   *
   * @param minPhase Minimum enabled phase
   */
  void setMinEnabledPhase(UInt32 minPhase);

  /**
   * Set the maximum enabled phase for this network.
   * Use this function to reduce the range of phases to be executed.
   *
   * @param minPhase Maximum enabled phase
   */
  void setMaxEnabledPhase(UInt32 minPhase);

  /**
   * Get the minimum enabled phase for this network.
   *
   * @returns Minimum enabled phase for this network
   */
  UInt32 getMinEnabledPhase() const;

  /**
   * Get the maximum enabled phase for this network.
   *
   * @returns Maximum enabled phase for this network
   */
  UInt32 getMaxEnabledPhase() const;

  /**
   * @}
   *
   * @name Running
   *
   * @{
   */

  /**
   * Run the network for the given number of iterations of compute for each
   * Region in the correct order.
   *
   * For each iteration, the specified phases are executed in the order specified
   * and within each phase the Region.compute() is called for each region in the phase
   * in the order that the regions were placed in the phases.
   * If a region was not assigned to a phase it will be in phase ID = 0.
   * 
   * At the end of each iteration, the callback function is called and the delays are advanced.
   * 
   * Just after execution of a region, its outputs are distributed to inputs via attached links.
   *
   * @param n Number of iterations
   * @param phase The phase ID or a vector of phase ID's of the phases to execute.  
   *              If not specified, execute all phases.
   */
  // execute all phases in phase ID order. Repeat n times.
  void run(int n) {     
    std::vector<UInt32> phases; // an empty phase list
    run(n, phases);
  };
  // execute a specific phase only. Repeat n times
  void run(int n, UInt32 phase) {  
    std::vector<UInt32> phases;
    phases.push_back(phase);
    run(n, phases);
  }
  // execute specific phases in this order. Repeat n times.
  void run(int n, std::vector<UInt32> phases);  

  /**
   * The type of run callback function.
   *
   * You can attach a callback function to a network, and the callback function
   *  is called after every iteration of run().
   *
   * To attach a callback, just get a reference to the callback
   * collection with getCallbacks() , and add a callback.
   */
  typedef void (*runCallbackFunction)(Network *, UInt64 iteration, void *);

  /**
   * Type definition for a callback item, combines a @c runCallbackFunction and
   * a `void*` pointer to the associated data.
   */
  typedef std::pair<runCallbackFunction, void *> callbackItem;

  /**
   * Get reference to callback Collection.
   *
   * @returns Reference to callback Collection
   */
  Collection<callbackItem> &getCallbacks();

  /**
   * @}
   *
   * @name Profiling
   *
   * @{
   */

  /**
   * Start profiling for all regions of this network.
   */
  void enableProfiling();

  /**
   * Stop profiling for all regions of this network.
   */
  void disableProfiling();

  /**
   * Reset profiling timers for all regions of this network.
   */
  void resetProfiling();
	
  /**
   * Set one of the debug levels: LogLevel_None = 0, LogLevel_Minimal, LogLevel_Normal, LogLevel_Verbose
   */
  static LogLevel setLogLevel(LogLevel level) {
    LogLevel prev = NTA_LOG_LEVEL;
    NTA_LOG_LEVEL = level;
    return prev;
  }


  /**
   * @}
   */

  /*
   * Adds a region implementation to the RegionImplFactory's list of packages
   *
   * NOTE: Built-in C++ regions are automatically registered by the factory
   *       so this function does not need to be called.
   *
   * NOTE: How does C++ register a custom C++ implemented region?
   *       Allocate a templated wrapper RegisteredRegionImplCpp class
   *       and pass it to this function with the name of the region type.
   *       Network::registerRegion("MyRegion", new RegisteredRegionImplCpp<MyRegion>());
   *   
   * NOTE: How does Python register a .py implemented region?
   *       Python code should call Network.registerPyRegion(module, className).
   *       The python bindings will actually call the static function
   *       htm::RegisteredRegionImplPy::registerPyRegion(module, className);
   *       which will register the C++ class PyBindRegion as the stand-in for the 
   *       python implementation.
   */
  static void registerRegion(const std::string name, RegisteredRegionImpl *wrapper);
  /*
   * Removes a region implementation from the RegionImplFactory's list of packages
   */
  static void unregisterRegion(const std::string name);
  
  /*
   * Returns a JSON string containing a list of registered region types.
   *
   */
  static std::string getRegistrations();

  /*
   * Removes all region registrations in RegionImplFactory.
   * Used in unit tests to setup for next test.
   */
  static void cleanup();

  bool operator==(const Network &other) const;
  inline bool operator!=(const Network &other) const {
    return !operator==(other);
  }

  friend std::ostream &operator<<(std::ostream &, const Network &);

  // Returns a text version of the phase structure.
  std::string phasesToString() const;

  // Returns a text version of order of execution
  std::string getExecutionMap();


private:
  // Both constructors use this common initialization method
  void commonInit();


  // perform actions after serialization load
  void post_load();
  void post_load(std::vector<std::shared_ptr<Link>>& links);

  // internal method using region pointer instead of name
  void setPhases_(std::shared_ptr<Region> &r, const std::set<UInt32> &phases);

  // whenever we modify a network or change phase
  // information, we set enabled phases to min/max for
  // the network
  void resetEnabledPhases_();
  void phasesFromString(const std::string& phaseString);

  bool initialized_;
	
	/**
	 * The list of regions registered with the Network.
	 * Internally this is a map so it is easy to serialize
	 * but externally this is a Collection object so it
	 * retains API compatability.
	 */
  std::map<std::string, std::shared_ptr<Region>> regions_;

  UInt32 minEnabledPhase_;
  UInt32 maxEnabledPhase_;

  // This is main data structure used to choreograph
  // network computation
  std::vector<std::vector<std::shared_ptr<Region>> > phaseInfo_;

  // we invoke these callbacks at every iteration
  Collection<callbackItem> callbacks_;

  // number of elapsed iterations
  UInt64 iteration_;
};

} // namespace htm

#endif // NTA_NETWORK_HPP
