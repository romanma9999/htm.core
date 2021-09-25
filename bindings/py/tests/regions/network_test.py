# ----------------------------------------------------------------------
# HTM Community Edition of NuPIC
# Copyright (C) 2017, Numenta, Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero Public License version 3 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU Affero Public License for more details.
#
# You should have received a copy of the GNU Affero Public License
# along with this program.  If not, see http://www.gnu.org/licenses.
# ----------------------------------------------------------------------

import json
import unittest
import pytest
import numpy as np
import sys
import os
import pickle

    
from htm.bindings.regions.PyRegion import PyRegion
from htm.bindings.sdr import SDR
import htm.bindings.engine_internal as engine
from htm.bindings.tools.serialization_test_py_region import \
     SerializationTestPyRegion

TEST_DATA = [0,1,2,3,4]
EXPECTED_RESULT1 = [  4, 5 ]
EXPECTED_RESULT2 = [ 16, 32, 44, 81, 104, 109, 114, 166, 197, 198 ]
EXPECTED_RESULT3 = [ 512, 1024, 1408, 2592, 3328, 3488, 3648, 5312, 6304, 6336 ]


class LinkRegion(PyRegion):
  """
  Test region used to test link validation
  """
  def __init__(self): pass
  def initialize(self): pass
  def compute(self, inputs, outputs): 
    # This will pass its inuts on to the outputs.
    for key in inputs:
      outputs[key][:] = inputs[key]
      
    
  def getOutputElementCount(self, name): 
    return 5
  
  def HelloWorld(self, *args):
    return 'Hello World says: arg1='+args[0]+' arg2='+args[1]
  
  @classmethod
  def getSpec(cls):
    return {
      "description": LinkRegion.__doc__,
      "inputs": {
        "UInt32": {
          "description": "UInt32 Data",
          "dataType": "UInt32",
          "isDefaultInput": True,
          "required": False,
          "count": 0
        },
        "Real32": {
          "description": "Real32 Data",
          "dataType": "Real32",
          "isDefaultInput": False,
          "required": False,
          "count": 0
        },
      },
      "outputs": {
        "UInt32": {
          "description": "UInt32 Data",
          "dataType": "UInt32",
          "isDefaultOutput": True,
          "required": False,
          "count": 0
        },
        "Real32": {
          "description": "Real32 Data",
          "dataType": "Real32",
          "isDefaultOutput": False,
          "required": False,
          "count": 0
        },
      },
      "parameters": { }
    }

class NetworkTest(unittest.TestCase):

  def setUp(self):
    """Register test region"""
    engine.Network.cleanup()
    engine.Network.registerPyRegion(LinkRegion.__module__, LinkRegion.__name__)

  def testSerializationWithPyRegion(self):
    """Test  (de)serialization of network containing a python region"""
    engine.Network.registerPyRegion(__name__,
                                    SerializationTestPyRegion.__name__)
                                    
    file_path = "SerializationTest.stream"
    try:
      srcNet = engine.Network()
      srcNet.addRegion(SerializationTestPyRegion.__name__,
                       "py." + SerializationTestPyRegion.__name__,
                       json.dumps({
                         "dataWidth": 128,
                         "randomSeed": 99,
                       }))

      # Serialize
      # Note: saveToFile() will do the following:
      #    - Call network.saveToFile(), in C++. this opens the file.
      #    - that calls network.save(stream) 
      #    - that will use Cereal to serialize the Network object.
      #    - that will serialize the Region object.
      #    - that will serialize PyBindRegion object because this is a python Region.
      #    - that will use pickle to serialize SerializationTestPyRegion in
      #                serialization_test_py_region.py into Base64.
      #
      #      loadFromFile() will do the following:
      #    - Call network.loadFromFile() in C++.  This opens the file
      #    - that calls network.load(stream)
      #    - that will recursivly deserialize and load all of the objects under network.

      srcNet.saveToFile(file_path, "BINARY")
      destNet = engine.Network()
      destNet.loadFromFile(file_path, "BINARY")

      # confirm that it works
      destRegion = destNet.getRegion(SerializationTestPyRegion.__name__)

      self.assertEqual(destRegion.getParameterUInt32("dataWidth"), 128)
      self.assertEqual(destRegion.getParameterUInt32("randomSeed"), 99)

    finally:
      engine.Network.unregisterPyRegion(SerializationTestPyRegion.__name__)
      if os.path.isfile(file_path):
        os.unlink("SerializationTest.stream")


  def testSimpleTwoRegionNetworkIntrospection(self):
    # Create Network instance
    network = engine.Network()

    # Add two TestNode regions to network
    network.addRegion("region1", "TestNode", "")
    network.addRegion("region2", "TestNode", "")

    # Set dimensions on first region
    region1 = network.getRegion("region1")
    region1.setDimensions(engine.Dimensions([1, 1]))

    # Link region1 and region2
    network.link("region1", "region2")

    # Initialize network
    network.initialize()

    for link in network.getLinks():
      # Compare Link API to what we know about the network
      self.assertEqual(link.getDestRegionName(), "region2")
      self.assertEqual(link.getSrcRegionName(), "region1")
      self.assertEqual(link.getDestInputName(), "bottomUpIn")
      self.assertEqual(link.getSrcOutputName(), "bottomUpOut")
      break
    else:
      self.fail("Unable to iterate network links.")


  def testNetworkLinkTypeValidation(self):
    """
    This tests whether the links source and destination dtypes match
    """
    network = engine.Network()
    r_from = network.addRegion("from", "py.LinkRegion", "")
    r_to = network.addRegion("to", "py.LinkRegion", "")
    cnt = r_from.getOutputElementCount("UInt32")
    self.assertEqual(5, cnt)


    # Check for valid links
    network.link("from", "to", "", "", "UInt32", "UInt32")
    network.link("from", "to", "", "", "Real32", "Real32")
    network.link("from", "to", "", "", "Real32", "UInt32")
    network.link("from", "to", "", "", "UInt32", "Real32")
	

  def testParameters(self):

    n = engine.Network()
    l1 = n.addRegion("l1", "TestNode", "")
    scalars = [
      ("int32Param", l1.getParameterInt32, l1.setParameterInt32, 32, int, 35),
      ("uint32Param", l1.getParameterUInt32, l1.setParameterUInt32, 33, int, 36),
      ("int64Param", l1.getParameterInt64, l1.setParameterInt64, 64, int, 74),
      ("uint64Param", l1.getParameterUInt64, l1.setParameterUInt64, 65, int, 75),
      ("real32Param", l1.getParameterReal32, l1.setParameterReal32, 32.1, float, 33.1),
      ("real64Param", l1.getParameterReal64, l1.setParameterReal64, 64.1, float, 65.1),
      ("stringParam", l1.getParameterString, l1.setParameterString, "nodespec value", str, "new value")]

    for paramName, paramGetFunc, paramSetFunc, initval, paramtype, newval in scalars:
      # Check the initial value for each parameter.
      x = paramGetFunc(paramName)
      self.assertEqual(type(x), paramtype, paramName)
      if initval is None:
        continue
      if type(x) == float:
        self.assertTrue(abs(x - initval) < 0.00001, paramName)
      else:
        self.assertEqual(x, initval, paramName)

      # Now set the value, and check to make sure the value is updated
      paramSetFunc(paramName, newval)
      x = paramGetFunc(paramName)
      self.assertEqual(type(x), paramtype)
      if type(x) == float:
        self.assertTrue(abs(x - newval) < 0.00001)
      else:
        self.assertEqual(x, newval)
        
  def testParameterArray(self):
    """
    Tests the setParameterArray( ) and getParameterArray( )
    The TestNode contains 'int64ArrayParam' and 'real32ArrayParam' parameters which are vectors.
    This test will write to each and read from each.
    """
    
    network = engine.Network()
    r1 = network.addRegion("region1", "TestNode", "")
    
    orig = np.array([1,2,3,4, 5,6,7,8], dtype=np.int64)
    r1.setParameterArray("int64ArrayParam", engine.Array(orig, True))
    self.assertEqual(r1.getParameterArrayCount("int64ArrayParam"), 8)
    a = engine.Array()
    r1.getParameterArray("int64ArrayParam", a)
    result = np.array(a)
    self.assertTrue( np.array_equal(orig, result))
    
    orig = np.array([1,2,3,4, 5,6,7,8], dtype=np.float32)
    r1.setParameterArray("real32ArrayParam", engine.Array(orig, True))
    self.assertEqual(r1.getParameterArrayCount("real32ArrayParam"), 8)
    a = engine.Array()
    r1.getParameterArray("real32ArrayParam", a)
    result = np.array(a)
    self.assertTrue( np.array_equal(orig, result))
    
  def testGetInputArray(self):
    """
    This tests whether the input to r_to is accessible and matches the output from the r_from region
    """
    engine.Network.registerPyRegion(LinkRegion.__module__, LinkRegion.__name__)
    
    network = engine.Network()
    r_from = network.addRegion("from", "py.LinkRegion", "")
    r_to = network.addRegion("to", "py.LinkRegion", "")
    network.link("from", "to", "", "", "UInt32", "UInt32")
    network.link("INPUT", "from", "", "{dim: [5]}", "UInt32_source", "UInt32")
    network.initialize()
    
    # Populate the input data
    network.setInputData("UInt32_source", np.array(TEST_DATA))
        
    network.run(1)
    
    to_input = np.array(r_to.getInputArray("UInt32"))
    self.assertTrue(np.array_equal(to_input, TEST_DATA))

  def testGetOutputArray(self):
    """
    This tests whether the final output of the network is accessible
    """
    engine.Network.registerPyRegion(LinkRegion.__module__, LinkRegion.__name__)
    
    network = engine.Network()
    r_from = network.addRegion("from", "py.LinkRegion", "")
    r_to = network.addRegion("to", "py.LinkRegion", "")
    network.link("from", "to", "", "", "UInt32", "UInt32")
    network.link("INPUT", "from", "", "{dim: [5]}", "UInt32_source", "UInt32")
    network.initialize()
    network.setInputData("UInt32_source", np.array(TEST_DATA))
    network.run(1)
    output = r_to.getOutputArray("UInt32")
    self.assertTrue(np.array_equal(output, TEST_DATA))

    

  def testBuiltInRegions(self):
    """
    This sets up a network with built-in regions.
    """
    import htm    
    net = engine.Network()
    #net.setLogLevel(htm.bindings.engine_internal.LogLevel.Verbose)     # Verbose shows data inputs and outputs while executing.

    encoder = net.addRegion("encoder", "ScalarSensor", "{n: 6, w: 2}");
    sp = net.addRegion("sp", "SPRegion", "{columnCount: 200}");
    tm = net.addRegion("tm", "TMRegion", "");
    net.link("encoder", "sp"); 
    net.link("sp", "tm"); 
    net.initialize();

    encoder.setParameterReal64("sensedValue", 0.8);  #Note: default range setting is -1.0 to +1.0
    net.run(1)
    
    sp_input = sp.getInputArray("bottomUpIn")
    sdr = sp_input.getSDR()
    self.assertTrue(np.array_equal(sdr.sparse, EXPECTED_RESULT1))
    
    sp_output = sp.getOutputArray("bottomUpOut")
    sdr = sp_output.getSDR()
    #print(sdr)
    self.assertTrue(np.array_equal(sdr.sparse, EXPECTED_RESULT2))
    
    tm_output = tm.getOutputArray("predictedActiveCells")
    sdr = tm_output.getSDR()
    #print(sdr)
    #print(EXPECTED_RESULT3)
    self.assertTrue(np.array_equal(sdr.sparse, EXPECTED_RESULT3))

  def testExecuteCommand1(self):
    """
    Check to confirm that the ExecuteCommand( ) funtion works.
    From Python calling a C++ region.
    """
    net = engine.Network()
    r = net.addRegion("test", "TestNode", "")
    
    lst = ["list arg", 86]
    result = r.executeCommand("HelloWorld", 42, lst)
    self.assertTrue(result == "Hello World says: arg1=42 arg2=['list arg', 86]")
    
  def testExecuteCommand2(self):
    """
    Check to confirm that the ExecuteCommand( ) funtion works.
    From Python calling a Python region.
    """
    engine.Network.cleanup()
    engine.Network.registerPyRegion(LinkRegion.__module__, LinkRegion.__name__)
    
    net = engine.Network()
    r = net.addRegion("test", "py.LinkRegion", "")
    
    lst = ["list arg", 86]
    result = r.executeCommand("HelloWorld", 42, lst)
    self.assertTrue(result == "Hello World says: arg1=42 arg2=['list arg', 86]")

  def testNetworkPickle(self):
    """
    Test region pickling/unpickling.
    """
    network = engine.Network()
    r_from = network.addRegion("from", "py.LinkRegion", "")
    r_to = network.addRegion("to", "py.LinkRegion", "")
    cnt = r_from.getOutputElementCount("UInt32")
    self.assertEqual(5, cnt)

    network.link("from", "to", "", "", "UInt32", "UInt32")
    network.link("from", "to", "", "", "Real32", "Real32")
    network.link("from", "to", "", "", "Real32", "UInt32")
    network.link("from", "to", "", "", "UInt32", "Real32")
    network.initialize()
    
    if sys.version_info[0] >= 3:
      proto = 3
    else:
      proto = 2

    # Simple test: make sure that dumping / loading works...
    pickledNetwork = pickle.dumps(network, proto)
    network2 = pickle.loads(pickledNetwork)
    
    s1 = network.getRegion("to").executeCommand("HelloWorld", "26", "64");
    s2 = network2.getRegion("to").executeCommand("HelloWorld", "26", "64");

    self.assertEqual(s1,"Hello World says: arg1=26 arg2=64")
    self.assertEqual(s1, s2,  "Simple Network pickle/unpickle failed.")

  def testNetworkSerialization(self):
    """
    Test JSON text serialization for the network.
    The C++ unit tests does a more complete job of checking serialization.
    All we really care about here is being able to call it from Python.
    """
    network  = engine.Network()
    r_from = network.addRegion("from", "py.LinkRegion", "")
    r_to = network.addRegion("to", "py.LinkRegion", "")
    cnt = r_from.getOutputElementCount("UInt32")
    self.assertEqual(5, cnt)

    network.link("from", "to", "", "", "UInt32", "UInt32")
    network.link("from", "to", "", "", "Real32", "Real32")
    network.link("from", "to", "", "", "Real32", "UInt32")
    network.link("from", "to", "", "", "UInt32", "Real32")
    network.initialize()
    
    filename = "Tests/NetworkSerialized.json"
    network.saveToFile(filename, "JSON");
    network2 = engine.Network()
    network2.loadFromFile(filename, "JSON")
    
    s1 = network.getRegion("to").executeCommand("HelloWorld", "26", "64");
    s2 = network2.getRegion("to").executeCommand("HelloWorld", "26", "64");

    self.assertEqual(s1,"Hello World says: arg1=26 arg2=64")
    self.assertEqual(s1, s2,  "Simple Network pickle/unpickle failed.")

