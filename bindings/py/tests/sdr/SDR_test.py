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

"""Unit tests for SDR."""

import pickle
import numpy as np
import unittest
import pytest
import timeit


from htm.bindings.sdr import SDR
from htm.bindings.math import Random

class SdrTest(unittest.TestCase):
    def testExampleUsage(self):
        # Make an SDR with 9 values, arranged in a (3 x 3) grid.
        X = SDR(dimensions = (3, 3))

        # These three statements are equivalent.
        X.dense = [[0, 1, 0],
                   [0, 1, 0],
                   [0, 0, 1]]
        assert( X.dense.tolist() == [[ 0, 1, 0 ], [ 0, 1, 0 ], [ 0, 0, 1 ]] )
        assert( [list(v) for v in X.coordinates] == [[ 0, 1, 2 ], [1, 1, 2 ]] )
        assert( list(X.sparse) == [ 1, 4, 8 ] )
        X.coordinates = [[0, 1, 2], [1, 1, 2]]
        assert( X.dense.tolist() == [[ 0, 1, 0 ], [ 0, 1, 0 ], [ 0, 0, 1 ]] )
        assert( [list(v) for v in X.coordinates] == [[ 0, 1, 2 ], [1, 1, 2 ]] )
        assert( list(X.sparse) == [ 1, 4, 8 ] )
        X.sparse = [ 1, 4, 8 ]

        # Access data in any format, SDR will automatically convert data formats,
        # even if it was not the format used by the most recent assignment to the
        # SDR.
        assert( X.dense.tolist() == [[ 0, 1, 0 ], [ 0, 1, 0 ], [ 0, 0, 1 ]] )
        assert( [list(v) for v in X.coordinates] == [[ 0, 1, 2 ], [1, 1, 2 ]] )
        assert( list(X.sparse) == [ 1, 4, 8 ] )

        # Data format conversions are cached, and when an SDR value changes the
        # cache is cleared.
        X.sparse = [1, 2, 3] # Assign new data to the SDR, clearing the cache.
        X.dense     # This line will convert formats.
        X.dense     # This line will resuse the result of the previous line

        X = SDR((1000, 1000))
        data = X.dense
        data[  0,   4] = 1
        data[444, 444] = 1
        X.dense = data
        assert(list(X.sparse) == [ 4, 444444 ])

    def testConstructor(self):
        A = SDR((103,))
        B = SDR((100, 100, 1))
        assert(tuple(B.dimensions) == (100, 100, 1))
        # Test crazy dimensions, also test keyword arguments.
        C = SDR(dimensions = (2, 4, 5, 1,1,1,1, 3) )
        assert(C.size == 2*4*5*3)

        # Test copy constructor
        D = SDR( sdr = C ) # also test KW-arg
        assert( D.dimensions == C.dimensions )
        C.randomize( .5 )
        assert( D != C )
        D = SDR( C )
        assert( D == C )
        # Test convenience constructor, integer argument instead of list.
        V = SDR( 999 )
        assert( V.size == 999 )

    def testZero(self):
        A = SDR((103,))
        A.sparse = list(range(20))
        B = A.zero()
        assert( np.sum( A.dense ) == 0 )
        assert( A is B )

    def testDense(self):
        A = SDR((103,))
        B = SDR((100, 100, 1))

        A.dense
        # Test is the same buffer every time
        A.dense[0] = 1
        A.dense[99] = 1
        assert(A.dense[0] + A.dense[99] == 2)
        # Test modify in-place
        A.dense = A.dense
        assert(set(A.sparse) == set((0, 99)))
        # Test dense dimensions
        assert(B.dense.shape == (100, 100, 1))
        # No crash with dimensions
        B.dense[0, 0, 0] += 1
        B.dense[66, 2, 0] += 1
        B.dense[99, 99, 0] += 1
        B.dense = B.dense
        # Test wrong dimensions assigned
        C = SDR(( A.size + 1 ))
        C.randomize( .5 )
        test_cases = [
            (SDR(1), SDR(2)),
            (SDR(100),      SDR((100, 1))),
            (SDR((1, 100)), SDR((100, 1))),
        ]
        for left, right in test_cases:
            try:
                left.dense = right.dense
            except RuntimeError:
                pass
            else:
                self.fail()
        # Test assign data.
        A.dense = np.zeros( A.size, dtype=np.int16 )
        A.dense = np.ones(  A.size, dtype=np.uint64 )
        A.dense = np.zeros( A.size, dtype=np.int8 )
        A.dense = [1] * A.size
        B.dense = [[[1]] * 100 for _ in range(100)]

    def testDenseInplace(self):
        # Check that assigning dense data to itself (ie: sdr.dense = sdr.dense)
        # is significantly faster than copying the data on assignment.

        # Also, it should not be *too* much faster because this test-case is
        # tuned to very fast in both situations.
       
       
        
        copy_time = timeit.timeit(stmt='A.dense = B', 
                                  setup='import numpy as np;from htm.bindings.sdr import SDR;A = SDR( 100*1000 ); B = np.copy(A.dense)', 
                                  number=100)
        inplace_time = timeit.timeit(stmt='A.dense = A.dense', 
                                  setup='from htm.bindings.sdr import SDR;A = SDR( 100*1000 )',
                                  number=100)
        assert( inplace_time < copy_time / 3 )

    def testSparse(self):
        A = SDR((103,))
        B = SDR((100, 100, 1))

        A.sparse
        B.sparse = [1,2,3,4]
        assert(all(B.sparse == np.array([1,2,3,4])))

        B.sparse = []
        assert( not B.dense.any() )

        # Test wrong dimensions assigned
        C = SDR( 1000 )
        C.randomize( .98 )
        try:
            A.sparse = C.sparse
        except RuntimeError:
            pass
        else:
            self.fail()

    def testCoordinates(self):
        A = SDR((103,))
        B = SDR((100, 100, 1))
        C = SDR((2, 4, 5, 1,1,1,1, 3))

        A.coordinates
        B.coordinates = [[0, 55, 99], [0, 11, 99], [0, 0, 0]]
        assert(B.dense[0, 0, 0]   == 1)
        assert(B.dense[55, 11, 0] == 1)
        assert(B.dense[99, 99, 0] == 1)
        C.randomize( .5 )
        assert( len(C.coordinates) == len(C.dimensions) )

        # Test wrong dimensions assigned
        C = SDR((2, 4, 5, 1,1,1,1, 3))
        C.randomize( .5 )
        try:
            A.coordinates = C.coordinates
        except RuntimeError:
            pass
        else:
            self.fail()

    def testKeepAlive(self):
        """ If there is a reference to an SDR's data then the SDR must be alive """
        # Test Dense
        A = SDR( 20 ).dense
        assert(( A == [0]*20 ).all())
        # Test Sparse
        B = SDR( 100 )
        B.randomize( .5 )
        B_sparse = B.sparse
        B_copy   = SDR( B )
        del B
        assert(( B_sparse == B_copy.sparse ).all())
        # Test Coordinates
        C = SDR([ 100, 100 ])
        C.randomize( .5 )
        C_data = C.coordinates
        C_copy = SDR( C )
        del C
        assert(( C_data[0] == C_copy.coordinates[0] ).all())
        assert(( C_data[1] == C_copy.coordinates[1] ).all())

    def testSetSDR(self):
        A = SDR((103,))
        B = SDR((103,))
        A.sparse = [66]
        B.setSDR( A )
        assert( B.dense[66] == 1 )
        assert( B.getSum() == 1 )
        B.dense[77] = 1
        B.dense = B.dense
        A.setSDR( B )
        assert( set(A.sparse) == set((66, 77)) )

        # Test wrong dimensions assigned
        C = SDR((2, 4, 5, 1,1,1,1, 3))
        C.randomize( .5 )
        try:
            A.setSDR(C)
        except RuntimeError:
            pass
        else:
            self.fail()
        # Check return value.
        D = A.setSDR( B )
        assert( D is A )

    def testGetSum(self):
        A = SDR((103,))
        assert(A.getSum() == 0)
        A.dense = np.ones( A.size )
        assert(A.getSum() == 103)

    def testGetSparsity(self):
        A = SDR((103,))
        assert(A.getSparsity() == 0)
        A.dense = np.ones( A.size )
        assert(A.getSparsity() == 1)

    def testGetOverlap(self):
        A = SDR((103,))
        B = SDR((103,))
        assert(A.getOverlap(B) == 0)

        A.dense[:10] = 1
        B.dense[:20] = 1
        A.dense = A.dense
        B.dense = B.dense
        assert(A.getOverlap(B) == 10)

        A.dense[:20] = 1
        A.dense = A.dense
        assert(A.getOverlap(B) == 20)

        A.dense[50:60] = 1
        B.dense[0] = 0
        A.dense = A.dense
        B.dense = B.dense
        assert(A.getOverlap(B) == 19)

        # Test wrong dimensions
        C = SDR((1,1,1,1, 103))
        C.randomize( .5 )
        try:
            A.getOverlap(C)
        except RuntimeError:
            pass
        else:
            self.fail()

    def testRandomizeEqNe(self):
        A = SDR((103,))
        B = SDR((103,))
        A.randomize( .1 )
        B.randomize( .1 )
        assert( A != B )
        A.randomize( .1, 0 )
        B.randomize( .1, 0 )
        assert( A != B )
        A.randomize( .1, 42 )
        B.randomize( .1, 42 )
        assert( A == B )

    def testRandomRNG(self):
        x = SDR(1000).randomize(.5, Random(77))
        y = SDR(1000).randomize(.5, Random(99))
        assert( x != y )
        z = SDR(1000).randomize(.5, Random(77))
        assert( x == z )

    def testRandomizeReturn(self):
        X = SDR( 100 )
        Y = X.randomize( .2 )
        assert( X is Y )

    def testAddNoise(self):
        A = SDR((103,))
        B = SDR((103,))
        A.randomize( .1 )
        B.setSDR( A )
        A.addNoise( .5 )
        assert( A.getOverlap(B) == 5 )
        # Check different seed makes different results.
        A.randomize( .3, 42 )
        B.randomize( .3, 42 )
        A.addNoise( .5 )
        B.addNoise( .5 )
        assert( A != B )
        # Check same seed makes same results.
        A.randomize( .3, 42 )
        B.randomize( .3, 42 )
        A.addNoise( .5, 42 )
        B.addNoise( .5, 42 )
        assert( A == B )
        # Check that it returns itself.
        C = A.addNoise( .5 )
        assert( C is A )

    def testKillCells(self):
        A = SDR(1000).randomize(1.00)
        # Test killing zero/no cells.
        A.killCells( .00 ) # Check no seed does not crash.
        assert( A.getSparsity() == 1.00 )
        # Test killing half of the cells, 3 times in a row.
        A.killCells( .50, 123 ) # Check seed as positional argument
        assert( A.getSparsity() == .5 )
        A.killCells( .50, seed=456 )
        assert( A.getSparsity() >= .25 - .05 and A.getSparsity() <= .25 + .05 )
        A.killCells( .50, seed=789 )
        assert( A.getSparsity() >= .125 - .05 and A.getSparsity() <= .125 + .05 )
        # Test killing all cells.
        A.killCells( 1.00, seed=101 )
        assert( A.getSparsity() == 0 )
        # Check that the same seed always kills the same cells.
        B = SDR(1000).randomize(1.00)
        B.killCells(.50, seed=444)
        B.killCells(.50, seed=444) # Kill the same cells twice, so no further change of sparsity.
        assert( B.getSparsity() == .50 )
        # Check different seeds kill different cells.
        D = SDR(1000).randomize(1.00)
        D.killCells(.50, seed=5)
        D.killCells(.50, seed=6)
        assert( D.getSparsity() < .50 )
        # Check return value
        X = SDR(100).randomize(.5)
        Y = X.killCells(.10)
        assert( X is Y )

    def testStr(self):
        A = SDR((103,))
        B = SDR((100, 100, 1))
        A.dense[0] = 1
        A.dense[9] = 1
        A.dense[102] = 1
        A.dense = A.dense
        assert(str(A) == "SDR( 103 ) 0, 9, 102")
        A.zero()
        assert(str(A) == "SDR( 103 )")
        B.dense[0, 0, 0] = 1
        B.dense[99, 99, 0] = 1
        B.dense = B.dense
        assert(str(B) == "SDR( 100, 100, 1 ) 0, 9999")

    def testPickle(self):
        for sparsity in (0, .3, 1):
            A = SDR((103,))
            A.randomize( sparsity )
            P = pickle.dumps( A )
            B = pickle.loads( P )
            assert( A == B )

    def testReshape(self):
        # Convert SDR dimensions from (4 x 4) to (8 x 2)
        A = SDR([ 4, 4 ])
        A.coordinates = ([1, 1, 2], [0, 1, 2])
        B = A.reshape([8, 2])
        assert( (np.array(B.coordinates) == ([2, 2, 5], [0, 1, 0]) ).all() )
        assert( A is B )


class IntersectionTest(unittest.TestCase):
    def testExampleUsage(self):
        A = SDR( 10 )
        B = SDR( 10 )
        X = SDR( A.dimensions )
        A.sparse = [0, 1, 2, 3]
        B.sparse =       [2, 3, 4, 5]
        X.intersection( A, B )
        assert(set(X.sparse) == set([2, 3]))

    def testInPlace(self):
        A = SDR( 1000 )
        B = SDR( 1000 )
        A.randomize( 1.00 )
        B.randomize(  .50 )
        A.intersection( A, B )
        assert( A.getSparsity() == .5 )
        A.randomize( 1.00 )
        B.randomize(  .50 )
        A.intersection( B, A )
        assert( A.getSparsity() == .5 )

    def testReturn(self):
        A = SDR( 10 ).randomize( .5 )
        B = SDR( 10 ).randomize( .5 )
        X = SDR( A.dimensions )
        Y = X.intersection( A, B )
        assert( X is Y )

    def testSparsity(self):
        test_cases = [
            ( 0.5,  0.5 ),
            ( 0.1,  0.9 ),
            ( 0.25, 0.3 ),
            ( 0.5,  0.5,  0.5 ),
            ( 0.95, 0.95, 0.95 ),
            ( 0.10, 0.10, 0.60 ),
            ( 0.0,  1.0,  1.0 ),
            ( 0.5,  0.5,  0.5, 0.5),
            ( 0.11, 0.25, 0.33, 0.5, 0.98, 0.98, 0.98, 0.98, 0.98, 0.98, 0.98),
        ]
        size = 10000
        seed = 99
        X    = SDR( size )
        for sparsities in test_cases:
            sdrs = []
            for S in sparsities:
                inp = SDR( size )
                inp.randomize( S, seed)
                seed += 1
                sdrs.append( inp )
            X.intersection( sdrs )
            mean_sparsity = np.product( sparsities )
            assert( X.getSparsity() >= (2./3.) * mean_sparsity )
            assert( X.getSparsity() <= (4./3.) * mean_sparsity )


class UnionTest(unittest.TestCase):
    def testExampleUsage(self):
        A = SDR( 10 )
        B = SDR( 10 )
        U = SDR( A.dimensions )
        A.sparse = [0, 1, 2, 3]
        B.sparse =       [2, 3, 4, 5]
        U.union( A, B )
        assert(set(U.sparse) == set([0, 1, 2, 3, 4, 5]))

    def testInPlace(self):
        A = SDR( 1000 )
        B = SDR( 1000 )
        A.randomize( .50 )
        B.randomize( .50 )
        A.union( A, B )
        assert( A.getSparsity() >= .75 - .05 and A.getSparsity() <= .75 + .05 )
        A.union( B.randomize( .50 ), A.randomize( .50 ) )
        assert( A.getSparsity() >= .75 - .05 and A.getSparsity() <= .75 + .05 )

    def testReturn(self):
        A = SDR( 10 ).randomize( .5 )
        B = SDR( 10 ).randomize( .5 )
        X = SDR( A.dimensions )
        Y = X.union( A, B )
        assert( X is Y )

    def testSparsity(self):
        test_cases = [
            ( 0.5,  0.5 ),
            ( 0.1,  0.9 ),
            ( 0.25, 0.3 ),
            ( 0.5,  0.5,  0.5 ),
            ( 0.95, 0.95, 0.95 ),
            ( 0.10, 0.10, 0.60 ),
            ( 0.0,  1.0,  1.0 ),
            ( 0.5,  0.5,  0.5, 0.5),
            ( 0.11, 0.20, 0.05, 0.04, 0.03, 0.01, 0.01, 0.02, 0.02, 0.02),
        ]
        size = 10000
        seed = 99
        X    = SDR( size )
        for sparsities in test_cases:
            sdrs = []
            for S in sparsities:
                inp = SDR( size )
                inp.randomize( S, seed)
                seed += 1
                sdrs.append( inp )
            X.union( sdrs )
            mean_sparsity = np.product(list( 1 - s for s in sparsities ))
            assert( X.getSparsity() >= (2./3.) * (1 - mean_sparsity) )
            assert( X.getSparsity() <= (4./3.) * (1 - mean_sparsity) )


class ConcatenationTest(unittest.TestCase):
    def testExampleUsage(self):
        A = SDR( 10 )
        B = SDR( 10 )
        C = SDR( 20 )
        A.sparse = [0, 1, 2]
        B.sparse = [0, 1, 2]
        C.concatenate( A, B )
        assert( set(C.sparse) == set([0, 1, 2, 10, 11, 12]) )

    def testConstructorErrors(self):
        A  = SDR( 100 )
        B  = SDR(( 100, 2 ))
        AB = SDR( 300 )
        C  = SDR([ 3, 3 ])
        D  = SDR([ 3, 4 ])
        CD = SDR([ 3, 7 ])
        # Test bad argument dimensions
        with pytest.raises(TypeError):
            AB.concatenate( A )             # Not enough inputs!
        with pytest.raises(RuntimeError):
            AB.concatenate( A, B )          # Different numbers of dimensions!
        with pytest.raises(RuntimeError):
            AB.concatenate( B, C )          # All dims except axis must match!
        with pytest.raises(RuntimeError):
            CD.concatenate( C, D )          # All dims except axis must match!
        with pytest.raises(RuntimeError):
            CD.concatenate(C, D, axis = 2)  # Invalid axis!
        with pytest.raises(TypeError):
            CD.concatenate(C, D, axis = -1) # Invalid axis!

        # Test KeyWord Arguments.  These should all work.
        CD.concatenate(C, D, 1)
        CD.concatenate(C, D, axis=1)
        CD.concatenate([C, D], axis=1)
        CD.concatenate(inputs=[C, D], axis=1)

    def testReturn(self):
        """
        Check that the following short hand will work, and will not incur extra copying:
        >>> C = SDR( A.size + B.size ).concatenate( A.flatten(), B.flatten() )
        """
        A = SDR(( 10, 10 )).randomize( .5 )
        B = SDR(( 10, 10 )).randomize( .5 )
        C = SDR( A.size + B.size )
        D = C.concatenate( A.flatten(), B.flatten() )
        assert( C is D )

    def testMirroring(self):
        A = SDR( 100 )
        A.randomize( .05 )
        Ax10 = SDR( 100 * 10 )
        Ax10.concatenate( [A] * 10 )
        assert( Ax10.getSum() == 100 * 10 * .05 )

    def testVersusNumpy(self):
        # Each testcase is a pair of lists of SDR dimensions and axis
        # dimensions.
        test_cases = [
            ([(9, 30, 40),  (2, 30, 40)],          0),
            ([(2, 30, 40),  (2, 99, 40)],          1),
            ([(2, 30, 40),  (2, 30, 99)],          2),
            ([(100,), (10), (30)],                 0),
            ([(100,2), (10,2), (30,2)],            0),
            ([(1,77), (1,99), (1,88)],             1),
            ([(1,77,2), (1,99,2), (1,88,2)],       1),
        ]
        for sdr_dims, axis in test_cases:
            sdrs = [SDR(dims) for dims in sdr_dims]
            [sdr.randomize(.50) for sdr in sdrs]
            cat_dims       = sdrs[0].dimensions
            cat_dims[axis] = sum(sdr.dimensions[axis] for sdr in sdrs)
            cat            = SDR( cat_dims )
            cat.concatenate( sdrs, axis )
            np_cat = np.concatenate([sdr.dense for sdr in sdrs], axis=axis)
            assert((cat.dense == np_cat).all())
