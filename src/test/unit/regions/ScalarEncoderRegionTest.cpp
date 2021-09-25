/* ---------------------------------------------------------------------
 * HTM Community Edition of NuPIC
 * Copyright (C) 2019, Numenta, Inc.
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
 *
 * Author: David Keeney, Nov, 2019
 * --------------------------------------------------------------------- */

 
/*---------------------------------------------------------------------
  * This is a test of the ScalarEncoderRegion module.  It does not check the ScalarEncoder itself
  * but rather just the plug-in mechanisom to call the ScalarEncoder.
  *
  * For those not familiar with GTest:
  *     ASSERT_TRUE(value)   -- Fatal assertion that the value is true.  Test terminates if false.
  *     ASSERT_FALSE(value)   -- Fatal assertion that the value is false. Test terminates if true.
  *     ASSERT_STREQ(str1, str2)   -- Fatal assertion that the strings are equal. Test terminates if false.
  *
  *     EXPECT_TRUE(value)   -- Nonfatal assertion that the value is true.  Test fails but continues if false.
  *     EXPECT_FALSE(value)   -- Nonfatal assertion that the value is false. Test fails but continues if true.
  *     EXPECT_STREQ(str1, str2)   -- Nonfatal assertion that the strings are equal. Test fails but continues if false.
  *
  *     EXPECT_THROW(statement, exception_type) -- nonfatal exception, cought and continues.
  *---------------------------------------------------------------------
  */
#include <htm/regions/ScalarEncoderRegion.hpp>
#include <htm/engine/Network.hpp>
#include <htm/engine/Region.hpp>
#include <htm/engine/Spec.hpp>
#include <htm/engine/Input.hpp>
#include <htm/engine/Output.hpp>
#include <htm/engine/Link.hpp>
#include <htm/engine/RegisteredRegionImpl.hpp>
#include <htm/engine/RegisteredRegionImplCpp.hpp>
#include <htm/ntypes/Array.hpp>
#include <htm/utils/Log.hpp>

#include "gtest/gtest.h"
#include "RegionTestUtilities.hpp"

#define VERBOSE if(verbose)std::cerr << "[          ] "
static bool verbose = false;  // turn this on to print extra stuff for debugging the test.

const UInt EXPECTED_SPEC_COUNT =  13u;  // The number of parameters expected in the ScalarSensor Spec

using namespace htm;
namespace testing 
{

  // Verify that all parameters are working.
  // Assumes that the default value in the Spec is the same as the default 
  // when creating a region with default constructor.
  TEST(ScalarEncoderRegionTest, testSpecAndParameters)
  {
    // create an ScalarSensor region with default parameters
    Network net;

    Spec* ns = ScalarEncoderRegion::createSpec();
    VERBOSE << *ns << std::endl;

    std::shared_ptr<Region> region1 = net.addRegion("region1", "ScalarEncoderRegion", "{n: 100, w: 10}"); 
    std::set<std::string> excluded = {"n", "w", "size", "activeBits", "resolution", "radius", "sparsity"};
    checkGetSetAgainstSpec(region1, EXPECTED_SPEC_COUNT, excluded, verbose);
    checkInputOutputsAgainstSpec(region1, verbose);
  }


	TEST(ScalarEncoderRegionTest, initialization_with_builtin_impl)
	{
	  VERBOSE << "Creating network..." << std::endl;
	  Network net;

	  size_t regionCntBefore = net.getRegions().size();

	  VERBOSE << "Adding a built-in ScalarEncoderRegion..." << std::endl;
	  std::shared_ptr<Region> region1 = net.addRegion("region1", "ScalarEncoderRegion", "{n: 100, w: 10}");
	  size_t regionCntAfter = net.getRegions().size();
	  ASSERT_TRUE(regionCntBefore + 1 == regionCntAfter) << " Expected number of regions to increase by one.  ";
	  ASSERT_TRUE(region1->getType() == "ScalarEncoderRegion") << " Expected type for region1 to be \"ScalarEncoderRegion\" but type is: " << region1->getType();


    EXPECT_THROW(region1->getOutputData("doesnotexist"), std::exception);
    EXPECT_THROW(region1->getInputData("doesnotexist"), std::exception);

    net.initialize();

	  // run and compute() should fail because network has not been initialized
	  net.run(1);
	  region1->compute();
	}


  TEST(ScalarEncoderRegionTest, initialization_with_custom_impl)
  {
    VERBOSE << "Creating network..." << std::endl;
    Network net;

    size_t regionCntBefore = net.getRegions().size();

    // make sure the custom region registration works for CPP.
    // We will just use the same SPRegion class but it could be a subclass or some different custom class.
    // While we are at it, make sure we can initialize the dimensions and parameters from here too.
    // The parameter names and data types must match those of the spec.
    // Explicit parameters:  (Yaml format...but since YAML is a superset of JSON, you can use JSON format as well)
    std::string nodeParams = "{n: 2048, w: 40}";

    VERBOSE << "Adding a custom-built ScalarEncoderRegion..." << std::endl;
    net.registerRegion("ScalarSensorCustom", new RegisteredRegionImplCpp<ScalarEncoderRegion>());
    std::shared_ptr<Region> region2 = net.addRegion("region2", "ScalarSensorCustom", nodeParams);
    size_t regionCntAfter = net.getRegions().size();
    ASSERT_TRUE(regionCntBefore + 1 == regionCntAfter) 
      << "  Expected number of regions to increase by one.  ";
    ASSERT_TRUE(region2->getType() == "ScalarSensorCustom") 
      << " Expected type for region2 to be \"ScalarSensorCustom\" but type is: " 
      << region2->getType();

    ASSERT_DOUBLE_EQ(region2->getParameterReal64("radius"), 0.039840637450199202);

    net.run(1);
    region2->compute();
  }




	TEST(ScalarEncoderRegionTest, testLinking)
	{
    // This is a minimal end-to-end test containing a ScalarEncoderRegion.
    // To make sure we can feed data from some other region to our ScalarEncoderRegion,
    // this test will hook up the FileInputRegion to our ScalarEncoderRegion and then
    // connect that to an SPRegion and then on to a FileOutputRegion to capture the results.
    //
    std::string test_input_file = "TestOutputDir/ScalarEncoderRegionTestInput.csv";
    std::string test_output_file = "TestOutputDir/ScalarEncoderRegionTestOutput.csv";


    // make a place to put test data.
    if (!Directory::exists("TestOutputDir")) Directory::create("TestOutputDir", false, true); 
    if (Path::exists(test_input_file)) Path::remove(test_input_file);
    if (Path::exists(test_output_file)) Path::remove(test_output_file);

    // Create a csv data file to use as input.
    // The data we will feed it will be a sin wave over 365 degrees in one degree increments.
    size_t dataRows = 360;
    std::ofstream f(test_input_file.c_str());
    for (size_t i = 0; i < dataRows; i++) {
      f << sin(i * (3.1415 / 180)) << std::endl;
    }
    f.close();

    VERBOSE << "Setup Network; add 4 regions and 3 links." << std::endl;
	  Network net;

    // Explicit parameters:  (Yaml format...but since YAML is a superset of JSON, 
    // you can use JSON format as well)

    std::shared_ptr<Region> region1 = net.addRegion("region1", "FileInputRegion", "{activeOutputCount: 1}");
    std::shared_ptr<Region> region2 = net.addRegion("region2", "ScalarEncoderRegion", "{n: 100, w: 4}");
    std::shared_ptr<Region> region3 = net.addRegion("region3", "SPRegion", "{columnCount: 200}");
    std::shared_ptr<Region> region4 = net.addRegion("region4", "FileOutputRegion", "{outputFile: '" + test_output_file + "'}");


    net.link("region1", "region2", "", "", "dataOut", "values");
    net.link("region2", "region3", "", "", "encoded", "bottomUpIn");
    net.link("region3", "region4", "", "", "bottomUpOut", "dataIn");

    VERBOSE << "Load Data." << std::endl;
    region1->executeCommand({ "loadFile", test_input_file });


    VERBOSE << "Initialize." << std::endl;
    net.initialize();



	  // check actual dimensions
    ASSERT_EQ(region2->getParameterUInt32("n"), 100u);

    VERBOSE << "Execute once." << std::endl;
    net.run(1);

	  VERBOSE << "Checking data after first iteration..." << std::endl;
    Array r1OutputArray = region1->getOutputData("dataOut");
    VERBOSE << "  FileInputRegion Output" << r1OutputArray << std::endl;
    EXPECT_TRUE(r1OutputArray.getType() == NTA_BasicType_Real64)
            << "actual type is " << BasicType::getName(r1OutputArray.getType());
    VERBOSE << "  " << std::endl;

    Array r2InputArray = region2->getInputData("values");
    VERBOSE << "  ScalarSensor input" << r2InputArray << std::endl;
    ASSERT_TRUE(r1OutputArray.getCount() == r2InputArray.getCount()) 
		<< "Buffer length different. Output from FileInputRegion is " << r1OutputArray.getCount() << ", input to ScalarEncoderRegion is " << r2InputArray.getCount();
    
    Array r2OutputArray = region2->getOutputData("encoded");
    VERBOSE << "  ScalarEncoderRegion output" << r2OutputArray << std::endl;
    EXPECT_TRUE(r2OutputArray.getType() == NTA_BasicType_SDR) 
      << "actual type is " << BasicType::getName(r2OutputArray.getType());

	  // execute SPRegion several more times and check that it has output.
    VERBOSE << "Execute 9 times." << std::endl;
    net.run(9);

    VERBOSE << "  FileOutputRegion input" << std::endl;
    Array r4InputArray = region4->getInputData("dataIn");
    ASSERT_TRUE(r4InputArray.getType() == NTA_BasicType_Real64)
      << "actual type is " << BasicType::getName(r4InputArray.getType());

    // cleanup
    region4->executeCommand({ "closeFile" });
    Directory::removeTree("TestOutputDir", true);
  }


  TEST(ScalarEncoderRegionTest, testSerialization) {
    // NOTE: this test does end-to-end serialize and deserialize with the following modules:
    //   Network, Region, Array, ScalarEncoderRegion, SPRegion, SpatialPooler, Connections, Random, Links
    //
	  // use default parameters
	  Network net1;
	  Network net2;
	  Network net3;

	  VERBOSE << "Setup first network and save it" << std::endl;
    std::shared_ptr<Region> n1region1 = net1.addRegion("region1", "ScalarEncoderRegion", "{n: 100, w: 4}");
    std::shared_ptr<Region> n1region2 = net1.addRegion("region2", "SPRegion", "{columnCount: 200}");
    net1.link("region1", "region2", "", "", "encoded", "bottomUpIn");
    net1.initialize();

    n1region1->setParameterReal64("sensedValue", 0.5);
		net1.run(1);

    // take a snapshot of everything in ScalarEncoderRegion at this point
    std::map<std::string, std::string> parameterMap;
    EXPECT_TRUE(captureParameters(n1region1, parameterMap)) << "Capturing parameters before save.";

    Directory::removeTree("TestOutputDir", true);
    VERBOSE << "Writing stream to " << Path::makeAbsolute("TestOutputDir/ScalarEncoderRegionTest.stream") << "\n";
	  net1.saveToFile("TestOutputDir/ScalarEncoderRegionTest.stream", SerializableFormat::JSON);

    VERBOSE << "Restore from " << Path::makeAbsolute("TestOutputDir/ScalarEncoderRegionTest.stream") 
            << " into a second network and compare." << std::endl;
    net2.loadFromFile("TestOutputDir/ScalarEncoderRegionTest.stream", SerializableFormat::JSON);

	  std::shared_ptr<Region> n2region1 = net2.getRegion("region1");
	  std::shared_ptr<Region> n2region2 = net2.getRegion("region2");

	  ASSERT_TRUE (n2region1->getType() == "ScalarEncoderRegion") 
	    << " Restored ScalarEncoderRegion does not have the right type.  Expected ScalarEncoderRegion, found " << n2region1->getType();

    EXPECT_TRUE(compareParameters(n2region1, parameterMap)) 
      << "Conflict when comparing ScalarSensor parameters after restore with before save.";
      
    EXPECT_TRUE(compareParameterArrays(n1region2, n2region2, "spatialPoolerOutput", NTA_BasicType_UInt32))
        << " comparing Output arrays after restore with before save.";
    EXPECT_TRUE(compareParameterArrays(n1region2, n2region2, "spOutputNonZeros", NTA_BasicType_UInt32))
        << " comparing NZ out arrays after restore with before save.";


	  // can we continue with execution?  See if we get any exceptions.
    n2region1->setParameterReal64("sensedValue", 0.5);
    net2.run(2);

    // cleanup
    Directory::removeTree("TestOutputDir", true);
	}
  
  
  TEST(ScalarEncoderRegionTest, getSpecJSON) {
     std::string expected = R"({"spec": "ScalarEncoderRegion",
  "parameters": {
    "sensedValue": {
      "description": "Scalar input",
      "type": "Real64",
      "count": 1,
      "access": "ReadWrite",
      "defaultValue": "-1"
    },
    "size": {
      "description": "The length of the encoding. Size of buffer. Use one of: 'size', 'radius', 'resolution', or 'category'.",
      "type": "UInt32",
      "count": 1,
      "access": "Create",
      "defaultValue": "0"
    },
    "n": {
      "description": "Old name for the 'size' parameter.",
      "type": "UInt32",
      "count": 1,
      "access": "Create",
      "defaultValue": "0"
    },
    "activeBits": {
      "description": "The number of active bits in the encoding. i.e. how sparse is it.Use one of: 'activeBits' or 'sparsity'.",
      "type": "UInt32",
      "count": 1,
      "access": "Create",
      "defaultValue": "0"
    },
    "w": {
      "description": "Old name for the 'activeBits' parameter",
      "type": "UInt32",
      "count": 1,
      "access": "Create",
      "defaultValue": "0"
    },
    "resolution": {
      "description": "The resolution for the encoder Use one of: 'size', 'radius', 'resolution', or 'category'.",
      "type": "Real64",
      "count": 1,
      "access": "Create",
      "defaultValue": "0"
    },
    "radius": {
      "description": "The radius for the encoder. Use one of: 'size', 'radius', 'resolution', or 'category'.",
      "type": "Real64",
      "count": 1,
      "access": "Create",
      "defaultValue": "0"
    },
    "minValue": {
      "description": "The minimum value for the input",
      "type": "Real64",
      "count": 1,
      "access": "Create",
      "defaultValue": "-1.0"
    },
    "maxValue": {
      "description": "The maximum value for the input",
      "type": "Real64",
      "count": 1,
      "access": "Create",
      "defaultValue": "+1.0"
    },
    "periodic": {
      "description": "Whether the encoder is periodic",
      "type": "Bool",
      "count": 1,
      "access": "Create",
      "defaultValue": "false"
    },
    "clipInput": {
      "description": "Whether to clip inputs if they're outside [minValue, maxValue]",
      "type": "Bool",
      "count": 1,
      "access": "Create",
      "defaultValue": "false"
    },
    "sparsity": {
      "description": "Sparsity is the number of active bits divided by the total number of bits. Use one of: 'activeBits' or 'sparsity'.",
      "type": "Real32",
      "count": 1,
      "access": "Create",
      "defaultValue": "false"
    },
    "category": {
      "description": "Whether the encoder parameter is a category. Use one of: 'size', 'radius', 'resolution', or 'category'.",
      "type": "Bool",
      "count": 1,
      "access": "Create",
      "defaultValue": "false"
    }
  },
  "inputs": {
    "values": {
      "description": "The input values to be encoded.",
      "type": "Real64",
      "count": 1,
      "required": 0,
      "regionLevel": 0,
      "isDefaultInput": 1
    }
  },
  "outputs": {
    "encoded": {
      "description": "Encoded value",
      "type": "SDR",
      "count": 0,
      "regionLevel": 1,
      "isDefaultOutput": 1
    },
    "bucket": {
      "description": "Quantized sensedValue for this iteration.  Becomes the title in ClassifierRegion.",
      "type": "Real64",
      "count": 1,
      "regionLevel": 0,
      "isDefaultOutput": 0
    }
  }
})";

       
    Spec *spec = ScalarEncoderRegion::createSpec();
    std::string json = spec->toString();
    EXPECT_STREQ(json.c_str(), expected.c_str());
  }

  TEST(ScalarEncoderRegionTest, getParameters) {
    std::string expected = R"({
  "sensedValue": -1.000000,
  "size": 100,
  "n": 100,
  "activeBits": 4,
  "w": 4,
  "resolution": 0.020833,
  "radius": 0.083333,
  "minValue": -1.000000,
  "maxValue": 1.000000,
  "periodic": false,
  "clipInput": false,
  "sparsity": 0.040000,
  "category": false
})";

    Network net1;
    std::shared_ptr<Region> region1 = net1.addRegion("region1", "ScalarEncoderRegion", "{n: 100, w: 4}");
    std::string json = region1->getParameters();
    EXPECT_STREQ(json.c_str(), expected.c_str());
  }


} // namespace
