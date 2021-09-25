# ----------------------------------------------------------------------
# HTM Community Edition of NuPIC
# Copyright (C) 2019, David McDougall
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

import unittest
import pytest
import sys
import tempfile
import os

from htm.bindings.sdr import SDR
from htm.algorithms import SpatialPooler as SP
import numpy as np

import pickle

class SpatialPoolerTest(unittest.TestCase):

  def testCompute(self):
    """ Check that there are no errors in call to compute. """
    inputs = SDR( 100 ).randomize( .05 )
    active = SDR( 100 )
    sp = SP( inputs.dimensions, active.dimensions, stimulusThreshold = 1 )
    sp.compute( inputs, True, active )
    assert( active.getSum() > 0 )


  def _runGetPermanenceTrial(self, float_type):
    """ 
    Check that getPermanence() returns values for a given float_type. 
    These tests are sensitive to the data type. This because if you pass a 
    numpy array of the type matching the C++ argument then PyBind11 does not
    convert the data, and the C++ code can modify the data in-place
    If you pass the wrong data type, then PyBind11 does a conversion so 
    your C++ function only gets a converted copy of a numpy array, and any changes 
    are lost after returning
    """
    inputs = SDR( 100 ).randomize( .05 )
    active = SDR( 100 )
    sp = SP( inputs.dimensions, active.dimensions, stimulusThreshold = 1 )

    # Make sure that the perms start off zero.
    perms_in = np.zeros(sp.getNumInputs(), dtype=float_type)
    sp.setPermanence(0, perms_in)
    perms = np.zeros(sp.getNumInputs(), dtype=float_type)
    sp.getPermanence(0, perms)
    assert( perms.sum() == 0.0 )
    
    for i in range(10):
      sp.compute( inputs, True, active )
    
    # There should be at least one perm none zero
    total = np.zeros(sp.getNumInputs(), dtype=float_type)
    for i in range(100):
      perms = np.zeros(sp.getNumInputs(), dtype=float_type)
      sp.getPermanence(i, perms)
      total = total + perms
    assert( total.sum() > 0.0 )
    
  def testGetPermanenceFloat64(self):
    """ Check that getPermanence() returns values. """
    try:
      # This is when NTA_DOUBLE_PRECISION is true
      self._runGetPermanenceTrial(np.float64)
      
    except ValueError:
      # This has caught wrong precision error
      print("Successfully caught incorrect float numpy data length")
      pass     

  def testGetPermanenceFloat32(self):
    """ Check that getPermanence() returns values. """
    try:
      self._runGetPermanenceTrial(np.float32)
      
    except ValueError:
      print("Successfully caught incorrect float numpy data length")
      # This has correctly caught wrong precision error
      pass     

  def _runGetConnectedSynapses(self, float_type):
    """ Check that getConnectedSynapses() returns values. """
    inputs = SDR( 100 ).randomize( .05 )
    active = SDR( 100 )
    sp = SP( inputs.dimensions, active.dimensions, stimulusThreshold = 1 )

    for i in range(10):
      sp.compute( inputs, True, active )
    
    # There should be at least one connected none zero
    total = np.zeros(sp.getNumInputs(), dtype=float_type)
    for i in range(100):
      connected = np.zeros(sp.getNumInputs(), dtype=float_type)
      sp.getPermanence(i, connected, sp.connections.connectedThreshold)
      total = total + connected
    assert( total.sum() > 0 )

  def testGetConnectedSynapsesUint64(self):
    """ Check that getConnectedSynapses() returns values. """
    try:
      # This is when NTA_DOUBLE_PRECISION is true
      self._runGetConnectedSynapses(np.uint64)
      
    except ValueError:
      # This has correctly caught wrong precision error
      print("Successfully caught incorrect uint numpy data length")
      pass     

  def testGetConnectedSynapsesUint32(self):
    """ Check that getConnectedSynapses() returns values. """
    try:
      # This is when NTA_DOUBLE_PRECISION is true
      self._runGetConnectedSynapses(np.float32)
      
    except ValueError:
      # This has correctly caught wrong precision error
      print("Successfully caught incorrect uint numpy data length")
      pass     

  def _runGetConnectedCounts(self, uint_type):
    """ Check that getConnectedCounts() returns values. """
    inputs = SDR( 100 ).randomize( .05 )
    active = SDR( 100 )
    sp = SP( inputs.dimensions, active.dimensions, stimulusThreshold = 1 )

    for _ in range(10):
      sp.compute( inputs, True, active )
    
    # There should be at least one connected none zero
    connected = np.zeros(sp.getNumColumns(), dtype=uint_type)
    sp.getConnectedCounts(connected)
    assert( connected.sum() > 0 )

  def testGetConnectedCountsUint64(self):
    """ Check that getConnectedCounts() returns values. """
    try:
      # This is when NTA_DOUBLE_PRECISION is true
      self._runGetConnectedCounts(np.uint64)
      
    except ValueError:
      # This has correctly caught wrong precision error
      print("Successfully caught incorrect uint numpy data length")
      pass     

  def testGetConnectedCountsUint32(self):
    """ Check that getConnectedCounts() returns values. """
    try:
      # This is when NTA_DOUBLE_PRECISION is true
      self._runGetConnectedCounts(np.uint32)
      
    except ValueError:
      # This has correctly caught wrong precision error
      print("Successfully caught incorrect uint numpy data length")
      pass     

  @pytest.mark.skipif(sys.version_info < (3, 6), reason="Fails for python2 with segmentation fault")
  def testNupicSpatialPoolerPickling(self):
    """Test pickling / unpickling of HTM SpatialPooler."""
    inputs = SDR( 100 ).randomize( .05 )
    active = SDR( 100 )
    sp = SP( inputs.dimensions, active.dimensions, stimulusThreshold = 1 )

    for _ in range(10):
      sp.compute( inputs, True, active )
      
    if sys.version_info[0] >= 3:
      proto = 3
    else:
      proto = 2

    # Simple test: make sure that dumping / loading works...
    pickledSp = pickle.dumps(sp, proto)
    sp2 = pickle.loads(pickledSp)
    self.assertEqual(str(sp), str(sp2),  "Simple SpatialPooler pickle/unpickle failed.")
    
    # Test unpickled functionality
    sp.compute( inputs, False, active )
    result =  np.array(active.sparse).tolist()

    sp2.compute( inputs, False, active )
    result2 = np.array(active.sparse).tolist()
    self.assertEqual(result, result2, "Simple SpatialPooler pickle/unpickle failed functional test.")
            
    # or using File I/O
    f = tempfile.TemporaryFile()  # simulates opening a file ('wb')
    pickle.dump(sp,f, proto)
    f.seek(0)
    sp3 = pickle.load(f)
    #print(str(sp3))
    f.close();
    self.assertEqual(str(sp), str(sp3),  "File I/O SpatialPooler pickle/unpickle failed.")

    

  def testNupicSpatialPoolerSavingToString(self):
    """Test writing to and reading from NuPIC SpatialPooler."""
    inputs = SDR( 100 ).randomize( .05 )
    active = SDR( 100 )
    sp = SP( inputs.dimensions, active.dimensions, stimulusThreshold = 1 )

    for _ in range(10):
      sp.compute( inputs, True, active )

    # Simple test: make sure that writing/reading works...
    s = sp.writeToString()

    sp2 = SP(columnDimensions=[32, 32])
    sp2.loadFromString(s)

    self.assertEqual(sp.getNumColumns(), sp2.getNumColumns(),
                     "NuPIC SpatialPooler write to/read from string failed.")
    self.assertEqual(str(sp), str(sp2),
                     "HTM SpatialPooler write to/read from string failed.")

  def testSpatialPoolerSerialization(self):
     # Test serializing with saveToFile() and loadFromFile()
     inputs = SDR( 100 ).randomize( .05 )
     active = SDR( 100 )
     sp = SP( inputs.dimensions, active.dimensions, stimulusThreshold = 1 )

     for _ in range(10):
       sp.compute( inputs, True, active )
      
     #print(str(sp))
     
     # The SP now has some data in it, try serialization.  
     file = "spatial_pooler_test_save2.bin"
     sp.saveToFile(file, "PORTABLE")
     sp3 = SP()
     sp3.loadFromFile(file, "PORTABLE")
     self.assertEqual(str(sp), str(sp3), "HTM SpatialPooler serialization (using saveToFile/loadFromFile) failed.")
     os.remove(file)


if __name__ == "__main__":
  unittest.main()
