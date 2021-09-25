/* ---------------------------------------------------------------------
 * HTM Community Edition of NuPIC
 * Copyright (C) 2016, Numenta, Inc.
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
 * Implementation of the ScalarEncoderRegion
 *      (was ScalarSensor)
 */

#include <htm/regions/ScalarEncoderRegion.hpp>

#include <htm/engine/Input.hpp>
#include <htm/engine/Output.hpp>
#include <htm/engine/Region.hpp>
#include <htm/engine/Spec.hpp>
#include <htm/ntypes/Array.hpp>
#include <htm/utils/Log.hpp>

namespace htm {

ScalarEncoderRegion::ScalarEncoderRegion(const ValueMap &params, Region *region)
    : RegionImpl(region) {
  params_.minimum = params.getScalarT<Real64>("minValue", -1.0);
  params_.maximum = params.getScalarT<Real64>("maxValue", +1.0);
  params_.clipInput = params.getScalarT<bool>("clipInput", false);
  params_.periodic = params.getScalarT<bool>("periodic", false);
  params_.category = params.getScalarT<bool>("category", false);
  params_.activeBits = params.getScalarT<UInt32>("activeBits", params.getScalarT<UInt32>("w", 0));
  params_.sparsity = params.getScalarT<Real32>("sparsity", 0.0);
  params_.size = params.getScalarT<UInt32>("size", params.getScalarT<UInt32>("n", 0));
  params_.radius = params.getScalarT<Real64>("radius", 0.0);
  params_.resolution = params.getScalarT<Real64>("resolution", 0.0);

  encoder_ = std::make_shared<ScalarEncoder>( params_ );


  sensedValue_ = params.getScalarT<Real64>("sensedValue", -1.0);
}

ScalarEncoderRegion::ScalarEncoderRegion(ArWrapper &wrapper, Region *region):RegionImpl(region) {
  cereal_adapter_load(wrapper);
}


void ScalarEncoderRegion::initialize() {
  // Normally a region will create the algorithm here, at the point
  // when the dimensions and parameters are known. But in this case
  // it is the encoder that determines the dimensions so it must be
  // allocated in the constructor.  
  // If parameters are changed after the encoder is instantiated 
  // then the encoder's initialization function must be called again 
  // to reset the dimensions in the BaseEncoder.  This is done in
  // askImplForOutputDimensions()
}


Dimensions ScalarEncoderRegion::askImplForOutputDimensions(const std::string &name) {

  if (name == "encoded") {
    // just in case parameters changed since instantiation, we call the
    // encoder's initialize() again. Note that if dimensions have been manually set, 
    // use those if same number of elements, else the dimensions are determined 
    // only by the encoder's algorithm.
    encoder_->initialize(params_); 

    // get the dimensions determined by the encoder.
    Dimensions encDim(encoder_->dimensions); // get dimensions from encoder
    Dimensions regionDim = getDimensions();  // get the region level dimensions.
    if (regionDim.isSpecified()) {
      // region level dimensions were explicitly specified.
      NTA_CHECK(regionDim.getCount() == encDim.getCount()) 
        << "Manually set dimensions are incompatible with encoder parameters; region: " 
        << regionDim << "  encoder: " << encDim;
      encDim = regionDim;
    }
    setDimensions(encDim);  // This output is 'isRegionLevel' so set region level dimensions.
    return encDim;
  } 
  else if (name == "bucket") {
    return 1;
  }
  // for any other output name, let RegionImpl handle it.
  return RegionImpl::askImplForOutputDimensions(name);
}

std::string ScalarEncoderRegion::executeCommand(const std::vector<std::string> &args,
                                         Int64 index) {
  NTA_THROW << "ScalarEncoderRegion::executeCommand -- commands not supported";
}

void ScalarEncoderRegion::compute()
{
  if (hasInput("values")) {
    Array &a = getInput("values")->getData();
    sensedValue_ = ((Real64 *)(a.getBuffer()))[0];
  }
  SDR &output = getOutput("encoded")->getData().getSDR();
  encoder_->encode((Real64)sensedValue_, output);

  // create the quantized sample or bucket. This becomes the title in the ClassifierRegion.
  Real64 *quantizedSample = (Real64*)getOutput("bucket")->getData().getBuffer();
  quantizedSample[0] = sensedValue_ - std::fmod(sensedValue_, encoder_->parameters.radius);

  // trace facility
  NTA_DEBUG << "compute " << getOutput("encoded") << std::endl;
}

ScalarEncoderRegion::~ScalarEncoderRegion() {}

/* static */ Spec *ScalarEncoderRegion::createSpec() {
  auto ns = new Spec;
  ns->name = "ScalarEncoderRegion";
  ns->singleNodeOnly = true;

  /* ----- parameters ----- */
  ns->parameters.add("sensedValue",
                     ParameterSpec("Scalar input", NTA_BasicType_Real64,
                                   1,    // elementCount
                                   "",   // constraints
                                   "-1", // defaultValue
                                   ParameterSpec::ReadWriteAccess));

  ns->parameters.add("size", 
                     ParameterSpec("The length of the encoding. Size of buffer. "
                                   "Use one of: 'size', 'radius', 'resolution', or 'category'.",
                                        NTA_BasicType_UInt32,
                                        1,   // elementCount
                                        "",  // constraints
                                        "0", // defaultValue
                                        ParameterSpec::CreateAccess));
  ns->parameters.add("n", 
                     ParameterSpec("Old name for the 'size' parameter.", NTA_BasicType_UInt32,
                                        1,   // elementCount
                                        "",  // constraints
                                        "0", // defaultValue
                                        ParameterSpec::CreateAccess));

  ns->parameters.add("activeBits", 
                     ParameterSpec("The number of active bits in the encoding. i.e. how sparse is it."
                                   "Use one of: 'activeBits' or 'sparsity'.",
                                   NTA_BasicType_UInt32,
                                   1,   // elementCount
                                   "",  // constraints
                                   "0", // defaultValue
                                   ParameterSpec::CreateAccess));
  ns->parameters.add("w", 
                     ParameterSpec("Old name for the 'activeBits' parameter", NTA_BasicType_UInt32,
                                   1,   // elementCount
                                   "",  // constraints
                                   "0", // defaultValue
                                   ParameterSpec::CreateAccess));

  ns->parameters.add("resolution",
                     ParameterSpec("The resolution for the encoder "
                                   "Use one of: 'size', 'radius', 'resolution', or 'category'.",
                                   NTA_BasicType_Real64,
                                   1,   // elementCount
                                   "",  // constraints
                                   "0", // defaultValue
                                   ParameterSpec::CreateAccess));

  ns->parameters.add("radius", ParameterSpec("The radius for the encoder. "
                                   "Use one of: 'size', 'radius', 'resolution', or 'category'.",
                                  NTA_BasicType_Real64,
                                  1,   // elementCount
                                  "",  // constraints
                                  "0", // defaultValue
                                  ParameterSpec::CreateAccess));

  ns->parameters.add("minValue",
                     ParameterSpec("The minimum value for the input",
                                   NTA_BasicType_Real64,
                                   1,    // elementCount
                                   "",   // constraints
                                   "-1.0", // defaultValue
                                   ParameterSpec::CreateAccess));

  ns->parameters.add("maxValue",
                     ParameterSpec("The maximum value for the input",
                                   NTA_BasicType_Real64,
                                   1,    // elementCount
                                   "",   // constraints
                                   "+1.0", // defaultValue
                                   ParameterSpec::CreateAccess));

  ns->parameters.add("periodic",
                     ParameterSpec("Whether the encoder is periodic",
                                   NTA_BasicType_Bool,
                                   1,       // elementCount
                                   "",      // constraints
                                   "false", // defaultValue
                                   ParameterSpec::CreateAccess));

  ns->parameters.add("clipInput",
                    ParameterSpec(
                                  "Whether to clip inputs if they're outside [minValue, maxValue]",
                                  NTA_BasicType_Bool,
                                  1,       // elementCount
                                  "",      // constraints
                                  "false", // defaultValue
                                  ParameterSpec::CreateAccess));
  ns->parameters.add("sparsity",
                    ParameterSpec(
                                  "Sparsity is the number of active bits divided by the total number of bits. "
                                  "Use one of: 'activeBits' or 'sparsity'.",
                                  NTA_BasicType_Real32,
                                  1,       // elementCount
                                  "",      // constraints
                                  "false", // defaultValue
                                  ParameterSpec::CreateAccess));
  ns->parameters.add("category",
                     ParameterSpec("Whether the encoder parameter is a category. "
                                   "Use one of: 'size', 'radius', 'resolution', or 'category'.",
                                   NTA_BasicType_Bool,
                                   1,       // elementCount
                                   "",      // constraints
                                   "false", // defaultValue
                                   ParameterSpec::CreateAccess));

   /* ----- inputs ------- */
  ns->inputs.add("values",
                 InputSpec("The input values to be encoded.", // description
                           NTA_BasicType_Real64,   // type
                           1,                   // count.
                           false,                // required?
                           false,               // isRegionLevel,
                           true                 // isDefaultInput
                           ));

  /* ----- outputs ----- */

  ns->outputs.add("encoded", OutputSpec("Encoded value", NTA_BasicType_SDR,
                                        0,    // elementCount
                                        true, // isRegionLevel
                                        true  // isDefaultOutput
                                        ));

  ns->outputs.add("bucket", OutputSpec("Quantized sensedValue for this iteration.  Becomes the title in ClassifierRegion.",
                                       NTA_BasicType_Real64,
                                       1,    // elementCount
                                       false, // isRegionLevel
                                       false // isDefaultOutput
                                       ));

  return ns;
}

Real64 ScalarEncoderRegion::getParameterReal64(const std::string &name, Int64 index) const {
  if (name == "sensedValue") {
    return sensedValue_;
  } else if (name == "resolution") 
    return encoder_->parameters.resolution;
  else if (name == "radius")
    return encoder_->parameters.radius;
  else if (name == "minValue")
    return encoder_->parameters.minimum;
  else if (name == "maxValue")
    return encoder_->parameters.maximum;
  else {
    return RegionImpl::getParameterReal64(name, index);
  }
}

Real32 ScalarEncoderRegion::getParameterReal32(const std::string &name, Int64 index) const {
  if (name == "sparsity") 
    return encoder_->parameters.sparsity;
  else {
    return RegionImpl::getParameterReal32(name, index);
  }
}

bool ScalarEncoderRegion::getParameterBool(const std::string& name, Int64 index) const {
  if (name == "periodic") 
    return encoder_->parameters.periodic;
  if (name == "clipInput")
    return encoder_->parameters.clipInput;
  if (name == "category")
    return encoder_->parameters.category;
  else {
    return RegionImpl::getParameterBool(name, index);
  }
}

UInt32 ScalarEncoderRegion::getParameterUInt32(const std::string &name, Int64 index) const {
  if (name == "n" || name == "size") {
    return (UInt32)encoder_->size;
  } else if (name == "w" || name == "activeBits") {
    return encoder_->parameters.activeBits;
  } else {
    return RegionImpl::getParameterUInt32(name, index);
  }
}


void ScalarEncoderRegion::setParameterReal64(const std::string &name, Int64 index, Real64 value) {
  if (name == "sensedValue") {
    sensedValue_ = value;
  } else {
	  RegionImpl::setParameterReal64(name, index, value);
  }
}

bool ScalarEncoderRegion::operator==(const RegionImpl &o) const {
  if (o.getType() != "ScalarEncoderRegion") return false;
  ScalarEncoderRegion &other = (ScalarEncoderRegion &)o;
  if (params_.minimum != other.params_.minimum) return false;
  if (params_.maximum != other.params_.maximum) return false;
  if (params_.clipInput != other.params_.clipInput) return false;
  if (params_.periodic != other.params_.periodic) return false;
  if (params_.activeBits != other.params_.activeBits) return false;
  if (params_.sparsity != other.params_.sparsity) return false;
  if (params_.category != other.params_.category) return false;
  if (params_.size != other.params_.size) return false;
  if (params_.radius != other.params_.radius) return false;
  if (params_.resolution != other.params_.resolution) return false;
  if (sensedValue_ != other.sensedValue_) return false;

  return true;
}


} // namespace htm
