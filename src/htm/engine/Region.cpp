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
 * --------------------------------------------------------------------- */

/** @file
Implementation of the Region class

Methods related to parameters are in Region_parameters.cpp
Methods related to inputs and outputs are in Region_io.cpp

*/

#include <iostream>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>

#include <htm/engine/Input.hpp>
#include <htm/engine/Link.hpp>
#include <htm/engine/Output.hpp>
#include <htm/engine/Region.hpp>
#include <htm/engine/RegionImpl.hpp>
#include <htm/engine/RegionImplFactory.hpp>
#include <htm/engine/Spec.hpp>
#include <htm/ntypes/Array.hpp>
#include <htm/ntypes/BasicType.hpp>
#include <htm/types/Sdr.hpp>
#include <htm/utils/Log.hpp>

namespace htm {

class GenericRegisteredRegionImpl;

// Create region from parameter spec
Region::Region(const std::string &name, const std::string &nodeType, const std::string &nodeParams, Network *network)
    : name_(std::move(name)), type_(nodeType), initialized_(false), network_(network), profilingEnabled_(false) {
  ValueMap vm;
  vm.parse(nodeParams);
  // Set region spec and input/outputs before creating the RegionImpl so that the
  // Impl has access to the region info in its constructor.
  RegionImplFactory &factory = RegionImplFactory::getInstance();
  spec_ = factory.getSpec(nodeType);
  createOutputs_();
  createInputs_();
  impl_.reset(factory.createRegionImpl(nodeType, vm, this));
}
Region::Region(const std::string &name, const std::string &nodeType, ValueMap &vm, Network *network) {
  name_ = name;
  type_ = nodeType;
  initialized_ = false;
  network_ = network;
  profilingEnabled_ = false;

  // Set region spec and input/outputs before creating the RegionImpl so that the
  // Impl has access to the region info in its constructor.
  RegionImplFactory &factory = RegionImplFactory::getInstance();
  spec_ = factory.getSpec(nodeType);
  createOutputs_();
  createInputs_();
  impl_.reset(factory.createRegionImpl(nodeType, vm, this));

  //std::cerr << "Region created " << getName() << "=" << nodeType << "\n";
  //auto outputs = getOutputs();
  //for (auto out : outputs) std::cerr << "   " << getName() << "." << out.first << "\n";

}

Region::Region(Network *net) {
  network_ = net;
  initialized_ = false;
  profilingEnabled_ = false;
} // for deserialization of region.
Region::Region() {
  network_ = nullptr;
  initialized_ = false;
  profilingEnabled_ = false;
} // for deserialization of region.


Network *Region::getNetwork() { return network_; }

void Region::createOutputs_() {
  // This is called when a Region is added.
  // Create all the outputs for this region from the Spec.
  // By default outputs are zero size, no dimensions.
  for (size_t i = 0; i < spec_->outputs.getCount(); ++i) {
    const std::pair<std::string, OutputSpec> &p = spec_->outputs.getByIndex(i);
    const std::string& outputName = p.first;
    const OutputSpec &os = p.second;
    std::shared_ptr<Output> output = std::make_shared<Output>(this, outputName, os.dataType);
    outputs_[outputName] = output;
  }
}
void Region::createInputs_() {
  // Create all the inputs for this node type.
  for (size_t i = 0; i < spec_->inputs.getCount(); ++i) {
    const std::pair<std::string, InputSpec> &p = spec_->inputs.getByIndex(i);
    const std::string& inputName = p.first;
    const InputSpec &is = p.second;

    auto input = std::make_shared<Input>(this, inputName, is.dataType);
    inputs_[inputName] = input;
  }
}

bool Region::hasOutgoingLinks() const {
  for (const auto &elem : outputs_) {
    if (elem.second->hasOutgoingLinks()) {
      return true;
    }
  }
  return false;
}

Region::~Region() {
  if (initialized_)
    uninitialize();

  removeAllIncomingLinks();  // Note: link objects are stored on the Input object.
  outputs_.clear();

  clearInputs(); // just in case there are some still around.

  // Note: the impl will be deleted when the region goes out of scope.
}

void Region::clearInputs() {
  for (auto &input : inputs_) {
    auto &links = input.second->getLinks();
    for (auto &link : links) {
      	link->getSrc()->removeLink(link); // remove it from the Output object.
    }
	  links.clear();
  }
  inputs_.clear();
}

void Region::initialize() {

  if (initialized_)
    return;

  // Make sure all unconnected outputs have a buffer.
  for(auto out: outputs_) {
    if (!out.second->getData().has_buffer()) {
      out.second->determineDimensions();
      out.second->initialize();
    }
  }

  impl_->initialize();
  initialized_ = true;
}




std::string Region::executeCommand(const std::vector<std::string> &args) {
  std::string retVal;
  if (args.size() < 1) {
    NTA_THROW << "Invalid empty command specified";
  }

  if (profilingEnabled_)
    executeTimer_.start();

  retVal = impl_->executeCommand(args, (UInt64)(-1));

  if (profilingEnabled_)
    executeTimer_.stop();

  return retVal;
}

void Region::compute() {
  if (!initialized_)
    NTA_THROW << "Region " << getName()
              << " unable to compute because not initialized";

  if (profilingEnabled_)
    computeTimer_.start();

  impl_->compute();

  if (profilingEnabled_)
    computeTimer_.stop();

  return;
}

/**
 * These internal methods are called by Network as
 * part of initialization.
 */

void Region::evaluateLinks() {
  for (auto &elem : inputs_) {
    (elem.second)->initialize();
  }
}

size_t Region::getNodeInputElementCount(const std::string &name) {
  size_t count = impl_->getNodeInputElementCount(name);
  return count;
}
size_t Region::getNodeOutputElementCount(const std::string &name) {
  size_t count = impl_->getNodeOutputElementCount(name);
  return count;
}

// Ask the implementation how dimensions should be set
Dimensions Region::askImplForInputDimensions(const std::string &name) const {
  Dimensions dim;
  try {
    dim = impl_->askImplForInputDimensions(name);
  } catch (Exception &e) {
      NTA_THROW << "Internal error -- the dimensions for the input " << name
                << "is unknown. : " << e.what();
  }
  return dim;
}
Dimensions Region::askImplForOutputDimensions(const std::string &name) const {
  Dimensions dim;
  try {
    dim = impl_->askImplForOutputDimensions(name);
  } catch (Exception &e) {
      NTA_THROW << "Internal error -- the dimensions for the input " << name
                << "is unknown. : " << e.what();
  }
  return dim;
}

Dimensions Region::getInputDimensions(std::string name) const {
  if (name.empty()) {
    name = spec_->getDefaultOutputName();
  }
  std::shared_ptr<Input> in = getInput(name);
  NTA_CHECK(in != nullptr)
    << "Unknown input (" << name << ") requested on " << name_;
  return in->getDimensions();
}
Dimensions Region::getOutputDimensions(std::string name) const {
  if (name.empty()) {
    name = spec_->getDefaultOutputName();
  }
  std::shared_ptr<Output> out = getOutput(name);
  NTA_CHECK(out != nullptr)
    << "Unknown output (" << name << ") requested on " << name_;
  return out->getDimensions();
}

void Region::setInputDimensions(std::string name, const Dimensions& dim) {
  if (name.empty()) {
    name = spec_->getDefaultOutputName();
  }
  std::shared_ptr<Input> in = getInput(name);
  NTA_CHECK(in != nullptr)
    << "Unknown input (" << name << ") requested on " << name_;
  return in->setDimensions(dim);
}
void Region::setOutputDimensions(std::string name, const Dimensions& dim) {
  if (name.empty()) {
    name = spec_->getDefaultOutputName();
  }
  std::shared_ptr<Output> out = getOutput(name);
  NTA_CHECK(out != nullptr)
    << "Unknown output (" << name << ") requested on " << name_;
  return out->setDimensions(dim);
}

// This is for backward compatability with API
// Normally Output dimensions are set by setting parameters known to the implementation.
// This sets a global dimension.
void Region::setDimensions(Dimensions dim) {
  NTA_CHECK(!initialized_) << "Cannot set region dimensions after initialization.";
  impl_->setDimensions(dim);
}
Dimensions Region::getDimensions() const {
  return impl_->getDimensions();
}



void Region::removeAllIncomingLinks() {
  InputMap::const_iterator i = inputs_.begin();
  for (; i != inputs_.end(); i++) {
    auto &links = i->second->getLinks();
    while (links.size() > 0) {
      i->second->removeLink(links[0]);
    }
  }
}

void Region::uninitialize() { initialized_ = false; }
void Region::enableProfiling() { profilingEnabled_ = true; }

void Region::disableProfiling() { profilingEnabled_ = false; }

void Region::resetProfiling() {
  computeTimer_.reset();
  executeTimer_.reset();
}

const Timer &Region::getComputeTimer() const { return computeTimer_; }

const Timer &Region::getExecuteTimer() const { return executeTimer_; }

bool Region::operator==(const Region &o) const {

  if (initialized_ != o.initialized_ || outputs_.size() != o.outputs_.size() ||
      inputs_.size() != o.inputs_.size()) {
    return false;
  }

  if (name_ != o.name_ || type_ != o.type_ || spec_ != o.spec_ ) {
    return false;
  }
  if (getDimensions() != o.getDimensions()) {
    return false;
  }

  // Compare Regions's Input (checking only input buffer names, size, and type)
  static auto compareInput = [](decltype(*inputs_.begin()) a, decltype(*inputs_.begin()) b) {
    if (a.first != b.first) {
      return false;
    }
    auto input_a = a.second;
    auto input_b = b.second;
    if (input_a->getDimensions().getCount() != input_b->getDimensions().getCount())
      return false;
    if (input_a->isInitialized() != input_b->isInitialized()) return false;
    if (input_a->isInitialized()) {
      if (input_a->getData().getType() != input_b->getData().getType() ||
          input_a->getData().getCount() != input_b->getData().getCount())
        return false;
    }
    auto links_a = input_a->getLinks();
    auto links_b = input_b->getLinks();
    if (links_a.size() != links_b.size()) {
      return false;
    }
    for (size_t i = 0; i < links_a.size(); i++) {
      if (*(links_a[i]) != *(links_b[i])) {
        return false;
      }
    }
    return true;
  };
  if (!std::equal(inputs_.begin(), inputs_.end(), o.inputs_.begin(),
                  compareInput)) {
    return false;
  }
  // Compare Regions's Output
  static auto compareOutput = [](decltype(*outputs_.begin()) a, decltype(*outputs_.begin()) b) {
    if (a.first != b.first ) {
      return false;
    }
    auto output_a = a.second;
    auto output_b = b.second;
    if (output_a->getDimensions() != output_b->getDimensions()) return false;
    if (output_a->getData().getType() != output_b->getData().getType() ||
        output_a->getData().getCount() != output_b->getData().getCount())
        return false;
    // compare output buffer contents.
    if (output_a->getData() != output_b->getData()) return false;
    return true;
  };
  if (!std::equal(outputs_.begin(), outputs_.end(), o.outputs_.begin(), compareOutput)) {
    return false;
  }

  if (impl_ && !o.impl_) return false;
  if (!impl_ && o.impl_) return false;
  if (impl_ && *impl_.get() != *o.impl_.get()) return false;

  return true;
}

// Internal methods called by RegionImpl.
bool Region::hasOutput(const std::string &name) const {
  auto out = getOutput(name);
  if (out) return out->hasOutgoingLinks();
  return false;
}
bool Region::hasInput(const std::string &name) const {
  auto in = getInput(name);
  if (in) return in->hasIncomingLinks();
  return false;
}

std::shared_ptr<Output> Region::getOutput(const std::string &name) const {
  auto o = outputs_.find(name);
  if (o == outputs_.end())
    return nullptr;
  return o->second;
}

std::shared_ptr<Input> Region::getInput(const std::string &name) const {
  auto i = inputs_.find(name);
  if (i == inputs_.end())
    return nullptr;
  return i->second;
}

// Called by Network during serialization
const std::map<std::string, std::shared_ptr<Input>> &Region::getInputs() const {
  return inputs_;
}

const std::map<std::string, std::shared_ptr<Output>> &Region::getOutputs() const {
  return outputs_;
}


const Array& Region::getOutputData(const std::string &outputName) const {
  auto oi = outputs_.find(outputName);
  if (oi == outputs_.end())
    NTA_THROW << "getOutputData -- unknown output '" << outputName
              << "' on region " << getName();

  const Array& data = oi->second->getData();
  return data;
}

const Array& Region::getInputData(const std::string &inputName) const {
  auto ii = inputs_.find(inputName);
  if (ii == inputs_.end())
    NTA_THROW << "getInput -- unknown input '" << inputName << "' on region "
              << getName();

  const Array & data = ii->second->getData();
  return data;
}

void Region::prepareInputs() {
  // Ask each input to prepare itself
  for (InputMap::const_iterator i = inputs_.begin(); i != inputs_.end(); i++) {
    i->second->prepare();
  }
}
void Region::pushOutputsOverLinks() {
  // Ask each output to distribute itself
  for (auto i = outputs_.begin(); i != outputs_.end(); i++) {
    i->second->push();
  }
}

// setParameter
void Region::setParameterByte(const std::string &name, Byte value) {
  impl_->setParameterByte(name, (Int64)-1, value);
}

void Region::setParameterInt32(const std::string &name, Int32 value) {
  impl_->setParameterInt32(name, (Int64)-1, value);
}

void Region::setParameterUInt32(const std::string &name, UInt32 value) {
  impl_->setParameterUInt32(name, (Int64)-1, value);
}

void Region::setParameterInt64(const std::string &name, Int64 value) {
  impl_->setParameterInt64(name, (Int64)-1, value);
}

void Region::setParameterUInt64(const std::string &name, UInt64 value) {
  impl_->setParameterUInt64(name, (Int64)-1, value);
}

void Region::setParameterReal32(const std::string &name, Real32 value) {
  impl_->setParameterReal32(name, (Int64)-1, value);
}

void Region::setParameterReal64(const std::string &name, Real64 value) {
  impl_->setParameterReal64(name, (Int64)-1, value);
}

void Region::setParameterBool(const std::string &name, bool value) {
impl_->setParameterBool(name, (Int64)-1, value);
}

void Region::setParameterJSON(const std::string &name, const std::string &value) {
  try {
    Value vm;
    vm.parse(value);

    NTA_BasicType type = spec_->parameters.getByName(name).dataType;
    switch (type) {
    case NTA_BasicType_Byte:
      setParameterByte(name, vm.as<Byte>());
      break;
    case NTA_BasicType_Int32:
      setParameterInt32(name, vm.as<Int32>());
      break;
    case NTA_BasicType_UInt32:
      setParameterUInt32(name, vm.as<UInt32>());
      break;
    case NTA_BasicType_Int64:
      setParameterInt64(name, vm.as<Int64>());
      break;
    case NTA_BasicType_UInt64:
      setParameterUInt64(name, vm.as<UInt64>());
      break;
    case NTA_BasicType_Real32:
      setParameterReal32(name, vm.as<Real32>());
      break;
    case NTA_BasicType_Real64:
      setParameterReal64(name, vm.as<Real64>());
      break;
    case NTA_BasicType_Bool:
      setParameterBool(name, vm.as<bool>());
      break;
    case NTA_BasicType_Str:
      setParameterString(name, vm.str());
      break;

    default:
      NTA_THROW << "Unknow parameter type '" + std::string(BasicType::getName(type)) + "'";
      break;
    }
  } catch (Exception &e) {
    NTA_THROW << "Error setting parameter "+ getName() + "." + name+ "; " +e.getMessage();
  }
}


// getParameters
std::string Region::getParameters() const {
   //std::cout << "getParameters() on " << getName() << "\n";

  std::string json = "{\n";
  for (size_t i = 0; i < spec_->parameters.getCount(); ++i) {
    const std::pair<std::string, ParameterSpec> &item = spec_->parameters.getByIndex(i);
    //std::cout << "getParameterJSON(" + getName() + '" << item.first << "')\n";
    if(i!=0)
    	json += ",\n"; // appending comma and newline each time, excluding first line
    json += "  \"" + item.first + "\": "+getParameterJSON(item.first);
  }
  json += "\n}";
  return json;
}

// getParameter
Byte Region::getParameterByte(const std::string &name) const { return impl_->getParameterByte(name, (Int64)-1); }

Int32 Region::getParameterInt32(const std::string &name) const { return impl_->getParameterInt32(name, (Int64)-1); }

Int64 Region::getParameterInt64(const std::string &name) const { return impl_->getParameterInt64(name, (Int64)-1); }

UInt32 Region::getParameterUInt32(const std::string &name) const { return impl_->getParameterUInt32(name, (Int64)-1); }

UInt64 Region::getParameterUInt64(const std::string &name) const { return impl_->getParameterUInt64(name, (Int64)-1); }

Real32 Region::getParameterReal32(const std::string &name) const { return impl_->getParameterReal32(name, (Int64)-1); }

Real64 Region::getParameterReal64(const std::string &name) const { return impl_->getParameterReal64(name, (Int64)-1); }

bool Region::getParameterBool(const std::string &name) const { return impl_->getParameterBool(name, (Int64)-1); }

std::string Region::getParameterJSON(const std::string &name, bool withType) const {
  // NOTE: if withType is not given or false, it just returns the JSON encoded value.
  //       if withType IS given, it returns "{"value": <value>, "type": <type>}
  NTA_BasicType type = NTA_BasicType_Last; // initialize to an invalid type.
  Value vm;
  //std::cout << "getParameterJSON(" << name << ")\n";
  try {
    auto p = spec_->parameters.getByName(name);
    type = p.dataType;
    size_t len = p.count;
    if (len == 1) {
      // This is a scalar value, not an array.

      switch (type) {
      case NTA_BasicType_Byte:
        vm = getParameterByte(name);
        break;
      case NTA_BasicType_Int32:
        vm = getParameterInt32(name);
        break;
      case NTA_BasicType_UInt32:
        vm = getParameterUInt32(name);
        break;
      case NTA_BasicType_Int64:
        vm = getParameterInt64(name);
        break;
      case NTA_BasicType_UInt64:
        vm = getParameterUInt64(name);
        break;
      case NTA_BasicType_Real32:
        vm = getParameterReal32(name);
        break;
      case NTA_BasicType_Real64:
        vm = getParameterReal64(name);
        break;
      case NTA_BasicType_Bool:
        vm = getParameterBool(name);
        break;
      case NTA_BasicType_Str:
        vm = getParameterString(name);
        break;

      default:
        NTA_THROW << "Unknow parameter type '" + std::string(BasicType::getName(type)) + "'";
        break;
      }
      if (!withType)
        return vm.to_json();
      else
        return "{\"value\": " + vm.to_json() + ", \"type\": \"" + std::string(BasicType::getName(type)) + "\"}";

    } else {
      // This is an array, not a scalar.
      if (len == 0)
        len = getParameterArrayCount(name);
      // Pre-allocate the buffer in the Array object.
      Array a(type);
      a.allocateBuffer(len);
      getParameterArray(name, a);
      std::string data = a.toJSON();
      if (!withType)
        return data;

      std::string dimStr;
      type = a.getType();
      if (type == NTA_BasicType_SDR) {
        const SDR& sdr = a.getSDR();
        auto d = sdr.dimensions;
        dimStr = "[";
        bool first = true;
        for (UInt item : d) {
          if (!first)
            dimStr.append(", ");
          first = false;
          dimStr.append(std::to_string(item));
        }
        dimStr.append("]");
      }
      else
        dimStr = "[" + std::to_string(a.getCount()) + "]";

      return "{\"value\": " + data +
              ", \"type\": \"" + std::string(BasicType::getName(type)) +
              ", \"dim\": " + dimStr + "}";

    }
  } catch (Exception &e) {
    NTA_THROW << "Error getting parameter " + getName() + "." + name + "; " + e.getMessage();
  } catch (...) {
    NTA_THROW << "Error getting parameter " + getName() + "." + name + "; ";
  }
}

// array parameters

void Region::getParameterArray(const std::string &name, Array &array) const {
  impl_->getParameterArray(name, (Int64)-1, array);
}

void Region::setParameterArray(const std::string &name, const Array &array) {
  impl_->setParameterArray(name, (Int64)-1, array);
}

size_t Region::getParameterArrayCount(const std::string &name) const {
  return impl_->getParameterArrayCount(name, (Int64)-1);
}

void Region::setParameterString(const std::string &name, const std::string &s) {
  impl_->setParameterString(name, (Int64)-1, s);
}

std::string Region::getParameterString(const std::string &name) const {
  return impl_->getParameterString(name, (Int64)-1);
}

bool Region::isParameter(const std::string &name) const {
  return (spec_->parameters.contains(name));
}

// Some functions used to prevent symbles from being in Region.hpp
void Region::getDims_(std::map<std::string,Dimensions>& outDims,
                      std::map<std::string,Dimensions>& inDims) const {
  for(auto out: outputs_) {
    Dimensions& dim = out.second->getDimensions();
    outDims[out.first] = dim;
  }
  for(auto in: inputs_) {
    Dimensions& dim = in.second->getDimensions();
    inDims[in.first] = dim;
  }
}
void Region::loadDims_(std::map<std::string,Dimensions>& outDims,
                     std::map<std::string,Dimensions>& inDims) const {
  for(auto out: outDims) {
      auto itr = outputs_.find(out.first);
      if (itr != outputs_.end()) {
        itr->second->setDimensions(out.second);
      }
  }
  for(auto in: inDims) {
      auto itr = inputs_.find(in.first);
      if (itr != inputs_.end()) {
        itr->second->setDimensions(in.second);
      }
  }
}

void Region::getOutputBuffers_(std::map<std::string, Array>& buffers) const {
	for (auto iter : outputs_) {
    buffers[iter.first] = iter.second->getData();
	}
}

void Region::restoreOutputBuffers_(const std::map<std::string, Array>& buffers) {
  // This is called when a Region is being restored from archive.
  // Recreate all the outputs for this region from the archived output buffer.
  for (auto const &it : buffers) {
    std::string outputName = it.first;
    NTA_BasicType dataType = it.second.getType();
    std::shared_ptr<Output> output = std::make_shared<Output>(this, outputName, dataType);
    Array& outputBuffer = output->getData();
    outputBuffer = it.second;
    outputs_[outputName] = output;
  }

  RegionImplFactory &factory = RegionImplFactory::getInstance();
  spec_ = factory.getSpec(type_);
  createInputs_(); // we use the spec for input name and type
}


void Region::serializeImpl(ArWrapper& arw) const{
    impl_->cereal_adapter_save(arw);
}
void Region::deserializeImpl(ArWrapper& arw) {
    RegionImplFactory &factory = RegionImplFactory::getInstance();
    impl_.reset(factory.deserializeRegionImpl(type_, arw, this));
}

std::ostream &operator<<(std::ostream &f, const Region &r) {
  f << "Region: {\n";
  f << "name: " << r.name_ << "\n";
  f << "nodeType: " << r.type_ << "\n";
  f << "outputs: [\n";
  for(auto out: r.outputs_) {
    f << out.first << " " << out.second->getDimensions() << "\n";
  }
  f << "]\n";
  f << "inputs: [\n";
  for(auto in: r.inputs_) {
    f << in.first << " " << in.second->getDimensions() << "\n";
  }
  f << "]\n";
	// TODO: add region impl...maybe
  //f << "RegionImpl:\n";
  // Now serialize the RegionImpl plugin.

  f << "}\n";
  return f;
}

} // namespace htm
