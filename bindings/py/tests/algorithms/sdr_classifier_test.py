# ----------------------------------------------------------------------
# HTM Community Edition of NuPIC
# Copyright (C) 2016, Numenta, Inc.
#               2019, David McDougall
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

""" Unit tests for Classifier & Predictor classes. """

import math
import numpy
import pickle
import random
import sys
import tempfile
import unittest
import os

from htm.bindings.sdr import SDR
from htm.bindings.algorithms import Classifier, Predictor


class ClassifierTest(unittest.TestCase):
  """ Unit tests for Classifier & Predictor classes. """


  def testExampleUsage(self):
    # Make a random SDR and associate it with a category.
    inputData  = SDR( 1000 ).randomize( 0.02 )
    categories = { 'A': 0, 'B': 1, 'C': 2, 'D': 3 }
    clsr = Classifier()
    clsr.learn( inputData, categories['B'] )
    assert( numpy.argmax( clsr.infer( inputData ) )  ==  categories['B'] )

    # Estimate a scalar value.  The Classifier only accepts categories, so
    # put real valued inputs into bins (AKA buckets) by subtracting the
    # minimum value and dividing by a resolution.
    scalar     = 567.8
    minimum    = 500
    resolution = 10
    clsr.learn( inputData, int((scalar - minimum) / resolution) )
    assert( numpy.argmax( clsr.infer( inputData ) ) * resolution + minimum  ==  560 )


    # Predict 1 and 2 time steps into the future.

    # Make a sequence of 4 random SDRs, each SDR has 1000 bits and 2% sparsity.
    sequence = [ SDR( 1000 ).randomize( 0.02 ) for i in range(4) ]

    # Make category labels for the sequence.
    labels = [ 4, 5, 6, 7 ]

    # Make a Predictor and train it.
    pred = Predictor([ 1, 2 ])
    pred.learn( 0, sequence[0], labels[0] )
    pred.learn( 1, sequence[1], labels[1] )
    pred.learn( 2, sequence[2], labels[2] )
    pred.learn( 3, sequence[3], labels[3] )

    # Give the predictor partial information, and make predictions
    # about the future.
    pred.reset()
    A = pred.infer( sequence[0] )
    assert( numpy.argmax( A[1] )  ==  labels[1] )
    assert( numpy.argmax( A[2] )  ==  labels[2] )

    B = pred.infer( sequence[1] )
    assert( numpy.argmax( B[1] )  ==  labels[2] )
    assert( numpy.argmax( B[2] )  ==  labels[3] )


  def testInitialization(self):
    Classifier( .1 )
    Predictor( [2,3,4], .1 )


  def testInitInvalidParams(self):
    # Invalid alpha
    self.assertRaises(Exception, Classifier, alpha = -1.0)

    # Invalid steps
    kwargs = {"steps": [0.3], "alpha": 0.1}
    self.assertRaises(Exception, Predictor, **kwargs)
    kwargs = {"steps": [], "alpha": 0.1}
    self.assertRaises(Exception, Predictor, **kwargs)
    kwargs = {"steps": [-1], "alpha": 0.1}
    self.assertRaises(Exception, Predictor, **kwargs)


  def testSingleValue(self):
    """Send same value 10 times and expect high likelihood for prediction."""
    classifier = Classifier( alpha = 0.5 )

    # Enough times to perform inference and learn associations
    inp = SDR(10)
    inp.randomize( .2 )
    for recordNum in range(10):
      classifier.learn(inp, 2)

    retval = classifier.infer( inp )
    self.assertGreater(retval[2], 0.9)


  def testSingleValue0Steps(self):
    """Send same value 10 times and expect high likelihood for prediction
    using 0-step ahead prediction"""
    pred = Predictor( steps=[0], alpha=0.5)

    # Enough times to perform Inference and learn associations
    inp = SDR(10)
    inp.randomize( .2 )
    for recordNum in range(10):
      pred.learn(recordNum, inp, 2)

    retval = pred.infer( inp )
    self.assertGreater(retval[0][2], 0.9)


  def testComputeInferOrLearnOnly(self):
    c = Predictor([1], 1.0)
    inp = SDR(10)
    inp.randomize( .3 )

    # learn only
    prediction = c.infer(pattern=inp)[1]
    self.assertTrue(prediction == []) # not enough training data -> []
    c.learn(recordNum=0, pattern=inp, classification=4)
    self.assertTrue(c.infer(pattern=inp)[1] == []) # not enough training data.
    c.learn(recordNum=2, pattern=inp, classification=4)
    c.learn(recordNum=3, pattern=inp, classification=4)
    self.assertTrue(c.infer(pattern=inp)[1] != []) # Don't crash with enough training data.

    # infer only
    retval1 = c.infer(pattern=inp)
    retval2 = c.infer(pattern=inp)
    self.assertSequenceEqual(list(retval1[1]), list(retval2[1]))


  def testComputeComplex(self):
    c   = Predictor([1], 1.0)
    inp = SDR(100)

    inp.sparse = [1, 5, 9]
    c.learn(recordNum=0, pattern=inp,
              classification=4,)

    inp.sparse = [0, 6, 9, 11]
    c.learn(recordNum=1, pattern=inp,
              classification=5,)

    inp.sparse = [6, 9]
    c.learn(recordNum=2, pattern=inp,
              classification=5,)

    inp.sparse = [1, 5, 9]
    c.learn(recordNum=3, pattern=inp,
              classification=4,)

    inp.sparse = [1, 5, 9]
    result = c.infer(pattern=inp)

    self.assertSetEqual(set(result.keys()), set([1]))
    self.assertEqual(len(result[1]), 6)
    self.assertAlmostEqual(result[1][0], 0.034234, places=5)
    self.assertAlmostEqual(result[1][1], 0.034234, places=5)
    self.assertAlmostEqual(result[1][2], 0.034234, places=5)
    self.assertAlmostEqual(result[1][3], 0.034234, places=5)
    self.assertAlmostEqual(result[1][4], 0.093058, places=5)
    self.assertAlmostEqual(result[1][5], 0.770004, places=5)


  def testOverlapPattern(self):
    classifier = Classifier(alpha=10.0)
    inp = SDR( 10 )
    inp.randomize( .2 )

    classifier.learn(pattern=inp, classification=9)
    classifier.learn(pattern=inp, classification=9)

    inp.addNoise( .5 )
    retval = classifier.infer(pattern=inp)

    # Since overlap - should be previous with high likelihood
    self.assertGreater(retval[9], 0.9)

    classifier.learn(pattern=inp, classification=2)
    classifier.learn(pattern=inp, classification=2)
    # Second example: now new value should be more probable than old

    retval = classifier.infer(pattern=inp)
    self.assertGreater(retval[2], retval[9])


  def testMultistepSingleValue(self):
    classifier = Predictor(steps=[1, 2])
    inp = SDR( 10 )
    inp.randomize( .2 )

    for recordNum in range(10):
      classifier.learn(recordNum, inp, 0)

    retval = classifier.infer(inp)

    # Should have a probability of 100% for that bucket.
    self.assertEqual(retval[1], [1.])
    self.assertEqual(retval[2], [1.])


  def testMultistepSimple(self):
    classifier = Predictor(steps=[1, 2], alpha=10.0)
    inp = SDR( 10 )

    for i in range(100):
      inp.sparse = [i % 10]
      classifier.learn(recordNum=i, pattern=inp, classification=(i % 10))

    retval = classifier.infer(inp)

    self.assertGreater(retval[1][0], 0.99)
    for i in range(1, 10):
      self.assertLess(retval[1][i], 0.01)
    self.assertGreater(retval[2][1], 0.99)
    for i in [0] + list(range(2, 10)):
      self.assertLess(retval[2][i], 0.01)


  def testMissingRecords(self):
    """ Test missing record support.

    Here, we intend the classifier to learn the associations:
      [1,3,5] => bucketIdx 1
      [2,4,6] => bucketIdx 2
      [7,8,9] => don"t care

    If it doesn't pay attention to the recordNums in this test, it will learn the
    wrong associations.
    """

    c = Predictor( steps=[1], alpha=1.0 )
    recordNum = 0
    inp = SDR( 10 )

    inp.sparse = [1, 3, 5]
    c.learn(recordNum=recordNum, pattern=inp, classification=0)
    recordNum += 1

    inp.sparse = [2, 4, 6]
    c.learn(recordNum=recordNum, pattern=inp, classification=1)
    recordNum += 1

    inp.sparse = [1, 3, 5]
    c.learn(recordNum=recordNum, pattern=inp, classification=2)
    recordNum += 1

    inp.sparse = [2, 4, 6]
    c.learn(recordNum=recordNum, pattern=inp, classification=1)
    recordNum += 1

    # -----------------------------------------------------------------------
    # At this point, we should have learned [1,3,5] => bucket 1
    #                                       [2,4,6] => bucket 2
    inp.sparse = [1, 3, 5]
    result = c.infer(pattern=inp)
    c.learn(recordNum=recordNum, pattern=inp, classification=2)
    recordNum += 1
    self.assertLess(result[1][0], 0.1)
    self.assertGreater(result[1][1], 0.9)
    self.assertLess(result[1][2], 0.1)

    inp.sparse = [2, 4, 6]
    result = c.infer(pattern=inp)
    c.learn(recordNum=recordNum, pattern=inp, classification=1)
    recordNum += 1
    self.assertLess(result[1][0], 0.1)
    self.assertLess(result[1][1], 0.1)
    self.assertGreater(result[1][2], 0.9)

    # -----------------------------------------------------------------------
    # Feed in records that skip and make sure they don"t mess up what we
    #  learned
    # If we skip a record, the CLA should NOT learn that [2,4,6] from
    #  the previous learn associates with bucket 0
    recordNum += 1
    inp.sparse = [1, 3, 5]
    result = c.infer(pattern=inp)
    c.learn(recordNum=recordNum, pattern=inp, classification=0)
    recordNum += 1
    self.assertLess(result[1][0], 0.1)
    self.assertGreater(result[1][1], 0.9)
    self.assertLess(result[1][2], 0.1)

    # If we skip a record, the CLA should NOT learn that [1,3,5] from
    #  the previous learn associates with bucket 0
    recordNum += 1
    inp.sparse = [2, 4, 6]
    result = c.infer(pattern=inp)
    c.learn(recordNum=recordNum, pattern=inp, classification=0)
    recordNum += 1
    self.assertLess(result[1][0], 0.1)
    self.assertLess(result[1][1], 0.1)
    self.assertGreater(result[1][2], 0.9)

    # If we skip a record, the CLA should NOT learn that [2,4,6] from
    #  the previous learn associates with bucket 0
    recordNum += 1
    inp.sparse = [1, 3, 5]
    result = c.infer(pattern=inp)
    c.learn(recordNum=recordNum, pattern=inp, classification=0)
    recordNum += 1
    self.assertLess(result[1][0], 0.1)
    self.assertGreater(result[1][1], 0.9)
    self.assertLess(result[1][2], 0.1)


  def testPredictionDistribution(self):
    """ Test the distribution of predictions.

    Here, we intend the classifier to learn the associations:
      [1,3,5] => bucketIdx 0 (30%)
              => bucketIdx 1 (30%)
              => bucketIdx 2 (40%)

      [2,4,6] => bucketIdx 1 (50%)
              => bucketIdx 3 (50%)

    The classifier should get the distribution almost right given enough
    repetitions and a small learning rate
    """

    c = Classifier(alpha = 0.001)

    SDR1 = SDR(10);  SDR1.sparse = [1, 3, 5]
    SDR2 = SDR(10);  SDR2.sparse = [2, 4, 6]

    random.seed(42)
    for _ in range(5000):
      randomNumber = random.random()
      if randomNumber < 0.3:
        bucketIdx = 0
      elif randomNumber < 0.6:
        bucketIdx = 1
      else:
        bucketIdx = 2
      c.learn(pattern=SDR1, classification=bucketIdx)

      randomNumber = random.random()
      if randomNumber < 0.5:
        bucketIdx = 1
      else:
        bucketIdx = 3
      c.learn(pattern=SDR2, classification=bucketIdx)

    result1 = c.infer(pattern=SDR1)
    self.assertAlmostEqual(result1[0], 0.3, places=1)
    self.assertAlmostEqual(result1[1], 0.3, places=1)
    self.assertAlmostEqual(result1[2], 0.4, places=1)

    result2 = c.infer(pattern=SDR2)
    self.assertAlmostEqual(result2[1], 0.5, places=1)
    self.assertAlmostEqual(result2[3], 0.5, places=1)


  def testPredictionDistributionOverlap(self):
    """ Test the distribution of predictions with overlapping input SDRs

    Here, we intend the classifier to learn the associations:
      SDR1    => bucketIdx 0 (30%)
              => bucketIdx 1 (30%)
              => bucketIdx 2 (40%)

      SDR2    => bucketIdx 1 (50%)
              => bucketIdx 3 (50%)

    SDR1 and SDR2 has 10% overlaps (2 bits out of 20)
    The classifier should get the distribution almost right despite the overlap
    """
    c = Classifier( 0.0005 )

    # generate 2 SDRs with 2 shared bits
    SDR1 = SDR( 100 )
    SDR2 = SDR( 100 )
    SDR1.randomize( .20 )
    SDR2.setSDR( SDR1 )
    SDR2.addNoise( .9 )

    random.seed(42)
    for _ in range(5000):
      randomNumber = random.random()
      if randomNumber < 0.3:
        bucketIdx = 0
      elif randomNumber < 0.6:
        bucketIdx = 1
      else:
        bucketIdx = 2
      c.learn(SDR1, bucketIdx)

      randomNumber = random.random()
      if randomNumber < 0.5:
        bucketIdx = 1
      else:
        bucketIdx = 3
      c.learn(SDR2, bucketIdx)

    result1 = c.infer(SDR1)
    self.assertAlmostEqual(result1[0], 0.3, places=1)
    self.assertAlmostEqual(result1[1], 0.3, places=1)
    self.assertAlmostEqual(result1[2], 0.4, places=1)

    result2 = c.infer(SDR2)
    self.assertAlmostEqual(result2[1], 0.5, places=1)
    self.assertAlmostEqual(result2[3], 0.5, places=1)


  def testPredictionMultipleCategories(self):
    """ Test the distribution of predictions.

    Here, we intend the classifier to learn the associations:
      [1,3,5] => bucketIdx 0 & 1
      [2,4,6] => bucketIdx 2 & 3

    The classifier should get the distribution almost right given enough
    repetitions and a small learning rate
    """
    c = Classifier( 0.001 )

    SDR1 = SDR(10);  SDR1.sparse = [1, 3, 5]
    SDR2 = SDR(10);  SDR2.sparse = [2, 4, 6]
    random.seed(42)
    for _ in range(5000):
      c.learn(pattern=SDR1, classification=[0, 1])
      c.learn(pattern=SDR2, classification=[2, 3])

    result1 = c.infer(pattern=SDR1)
    self.assertAlmostEqual(result1[0], 0.5, places=1)
    self.assertAlmostEqual(result1[1], 0.5, places=1)

    result2 = c.infer(pattern=SDR2)
    self.assertAlmostEqual(result2[2], 0.5, places=1)
    self.assertAlmostEqual(result2[3], 0.5, places=1)


  def testPredictionDistributionContinuousLearning(self):
    """ Test continuous learning

    First, we intend the classifier to learn the associations:
      SDR1    => bucketIdx 0 (30%)
              => bucketIdx 1 (30%)
              => bucketIdx 2 (40%)

      SDR2    => bucketIdx 1 (50%)
              => bucketIdx 3 (50%)

    After 20000 iterations, we change the association to
      SDR1    => bucketIdx 0 (30%)
              => bucketIdx 1 (20%)
              => bucketIdx 3 (40%)

      No further training for SDR2

    The classifier should adapt continuously and learn new associations for
    SDR1, but at the same time remember the old association for SDR2
    """
    c    = Classifier( 0.001 )
    SDR1 = SDR(10); SDR1.sparse = [1, 3, 5]
    SDR2 = SDR(10); SDR2.sparse = [2, 4, 6]

    random.seed(42)
    for _ in range(10000):
      randomNumber = random.random()
      if randomNumber < 0.3:
        bucketIdx = 0
      elif randomNumber < 0.6:
        bucketIdx = 1
      else:
        bucketIdx = 2
      c.learn( SDR1, bucketIdx )

      randomNumber = random.random()
      if randomNumber < 0.5:
        bucketIdx = 1
      else:
        bucketIdx = 3
      c.learn( SDR2, bucketIdx )

    result1 = c.infer( SDR1 )
    self.assertAlmostEqual(result1[0], 0.3, places=1)
    self.assertAlmostEqual(result1[1], 0.3, places=1)
    self.assertAlmostEqual(result1[2], 0.4, places=1)

    result2 = c.infer( SDR2 )
    self.assertAlmostEqual(result2[1], 0.5, places=1)
    self.assertAlmostEqual(result2[3], 0.5, places=1)

    for _ in range(20000):
      randomNumber = random.random()
      if randomNumber < 0.3:
        bucketIdx = 0
      elif randomNumber < 0.6:
        bucketIdx = 1
      else:
        bucketIdx = 3
      c.learn( SDR1, bucketIdx )

    result1new = c.infer( SDR1 )
    self.assertAlmostEqual(result1new[0], 0.3, places=1)
    self.assertAlmostEqual(result1new[1], 0.3, places=1)
    self.assertAlmostEqual(result1new[3], 0.4, places=1)

    result2new = c.infer( SDR2 )
    self.assertSequenceEqual(list(result2), list(result2new))


  def testMultiStepPredictions(self):
    """ Test multi-step predictions
    We train the 0-step and the 1-step classifiers simultaneously on
    data stream
    (SDR1, bucketIdx0)
    (SDR2, bucketIdx1)
    (SDR1, bucketIdx0)
    (SDR2, bucketIdx1)
    ...

    We intend the 0-step classifier to learn the associations:
      SDR1    => bucketIdx 0
      SDR2    => bucketIdx 1

    and the 1-step classifier to learn the associations
      SDR1    => bucketIdx 1
      SDR2    => bucketIdx 0
    """

    c = Predictor([0, 1], 1.0)

    SDR1 = SDR(10);  SDR1.sparse = [1, 3, 5]
    SDR2 = SDR(10);  SDR2.sparse = [2, 4, 6]
    recordNum = 0
    for _ in range(100):
      c.learn(recordNum, pattern=SDR1, classification=0)
      recordNum += 1

      c.learn(recordNum, pattern=SDR2, classification=1)
      recordNum += 1

    result1 = c.infer(SDR1)
    result2 = c.infer(SDR2)

    self.assertAlmostEqual(result1[0][0], 1.0, places=1)
    self.assertAlmostEqual(result1[0][1], 0.0, places=1)
    self.assertAlmostEqual(result2[0][0], 0.0, places=1)
    self.assertAlmostEqual(result2[0][1], 1.0, places=1)


  def testSerialization1(self):
    # This test verifies that pickle works for pickle of a Classifier
    SDR1 = SDR(15);  SDR1.sparse = [1, 5, 9]
    SDR2 = SDR(15);  SDR2.sparse = [0, 6, 9, 11]
    SDR3 = SDR(15);  SDR3.sparse = [6, 9]
    SDR4 = SDR(15);  SDR4.sparse = [1, 5, 9]
    c1 = Classifier()
    c1.learn(pattern=SDR1, classification=4)
    c1.learn(pattern=SDR2, classification=5)
    c1.learn(pattern=SDR3, classification=5)
    c1.learn(pattern=SDR4, classification=4)
    c1.learn(pattern=SDR4, classification=4)
    
    serialized = pickle.dumps(c1)
    c2 = pickle.loads(serialized)

    result1 = c1.infer(SDR1)
    result2 = c2.infer(SDR1)
    #print("  testSerialization1 result: %.6f, %.6f, %.6f, %.6f, %.6f, %.6f "%( result1[0], result1[1], result1[2], result1[3], result1[4], result1[5]));
    self.assertEqual(len(result1), 6)
    self.assertAlmostEqual(result1[0], 0.166344, places=5)
    self.assertAlmostEqual(result1[1], 0.166344, places=5)
    self.assertAlmostEqual(result1[2], 0.166344, places=5)
    self.assertAlmostEqual(result1[3], 0.166344, places=5)
    self.assertAlmostEqual(result1[4], 0.167847, places=5)
    self.assertAlmostEqual(result1[5], 0.166777, places=5)
    self.assertEqual(len(result1), len(result2))
    for i in range(len(result1)):
      self.assertAlmostEqual(result1[i], result2[i], places=5)

  def testSerialization2(self):
    # This test verifies that pickle works for pickle of a Predictor
    SDR1 = SDR(15);  SDR1.sparse = [1, 5, 9]
    SDR2 = SDR(15);  SDR2.sparse = [0, 6, 9, 11]
    SDR3 = SDR(15);  SDR3.sparse = [6, 9]
    SDR4 = SDR(15);  SDR4.sparse = [1, 5, 9]
    
    c1 = Predictor( steps=[1], alpha=1.0 )
    c1.learn(1, pattern=SDR1, classification=4)
    c1.learn(2, pattern=SDR2, classification=5)
    c1.learn(3, pattern=SDR3, classification=5)
    c1.learn(4, pattern=SDR4, classification=4)
    c1.learn(5, pattern=SDR4, classification=4)
    
    serialized = pickle.dumps(c1)
    c2 = pickle.loads(serialized)

    result1 = c1.infer(SDR1)
    result2 = c2.infer(SDR1)
    #print("  testSerialization2 result: %.6f, %.6f, %.6f, %.6f, %.6f, %.6f "%( result1[1][0], result1[1][1], result1[1][2], result1[1][3], result1[1][4], result1[1][5]));
    self.assertEqual(len(result1[1]), 6)
    self.assertEqual(len(result1[1]), len(result2[1]))
    for i in range(len(result1[1])):
      self.assertAlmostEqual(result1[1][i], result2[1][i], places=5)

  def testSerialization3(self):
    # This test verifies that saveToFile() and loadFromFile() on Classifier are accessable from Python.
    SDR1 = SDR(15);  SDR1.sparse = [1, 5, 9]
    SDR2 = SDR(15);  SDR2.sparse = [0, 6, 9, 11]
    SDR3 = SDR(15);  SDR3.sparse = [6, 9]
    SDR4 = SDR(15);  SDR4.sparse = [1, 5, 9]
    c1 = Classifier()
    c1.learn(pattern=SDR1, classification=4)
    c1.learn(pattern=SDR2, classification=5)
    c1.learn(pattern=SDR3, classification=5)
    c1.learn(pattern=SDR4, classification=4)

    # The Predictor now has some data in it, try serialization.
    file = "Classifier_test_save.XML"
    c1.saveToFile(file, "XML")
    c2 = Classifier()
    c2.loadFromFile(file, "XML")
    os.remove(file)

    result1 = c1.infer(SDR1)
    result2 = c2.infer(SDR1)
    self.assertEqual(len(result1), len(result2))
    for i in range(len(result1)):
      self.assertAlmostEqual(result1[i], result2[i], places=5)
  
  def testSerialization4(self):
    # This test verifies that saveToFile() and loadFromFile with BINARY on Classifier are accessible from Python.
    inputData  = SDR( 1000 ).randomize( 0.02 )
    categories = { 'A': 0, 'B': 1, 'C': 2, 'D': 3 }
    c1 = Classifier()
    c1.learn(inputData, categories['B'] )
    file = "Classifier_test_save.BINARY"
    c1.saveToFile(file)
    
    c2 = Classifier()
    c2.loadFromFile(file)
    result2 = c2.infer( inputData )
    self.assertTrue(numpy.argmax( result2 )  ==  categories['B'])
    os.remove(file)

  def testSerialization5(self):
    # This test verifies that saveToFile() and loadFromFile with JSON on Predictor are accessible from Python.
    inputData  = SDR( 1000 ).randomize( 0.02 )
    categories = { 'A': 0, 'B': 1, 'C': 2, 'D': 3 }
    c1 = Predictor( steps=[1], alpha=1.0 )
    c1.learn(1, inputData, categories['B'] )
    file = "Predictor_test_save.JSON"
    c1.saveToFile(file, "JSON")

    c2 = Predictor( steps=[1], alpha=1.0 )
    c2.loadFromFile(file, "JSON")
    os.remove(file)

if __name__ == "__main__":
  unittest.main()
