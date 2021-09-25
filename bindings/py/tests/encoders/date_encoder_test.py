# ----------------------------------------------------------------------
# HTM Community Edition of NuPIC
# Copyright (C) 2020, Numenta, Inc.
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

""" Unit tests for C++ implementation of the date encoder. """

import datetime
import numpy
import unittest
import os

from htm.bindings.encoders import DateEncoder, DateEncoderParameters
from htm.bindings.sdr import SDR, Metrics

"""
def __init__(self,
season=0,
dayOfWeek=0,
weekend=0,
holiday=0,
timeOfDay=0,
customDays=0,
holidays=((12, 25),)):
"""

class DateEncoder_Test(unittest.TestCase):
  """ Unit tests for DateEncoder class. """

  def testDateEncoder(self):
    """ Creating date encoder instance. """
    # 3 bits for season(4seasons), 1 bit for day of week, 1 for weekend, 5 for time of day
    p = DateEncoderParameters()
    p.season_width = 3
    p.dayOfWeek_width = 1
    p.weekend_width = 1
    p.timeOfDay_width = 5
    p.verbose = False;
    enc = DateEncoder(p)
    # In the middle of fall, Thursday, not a weekend, afternoon - 4th Nov,
    # 2010, 14:55
    d = datetime.datetime(2010, 11, 4, 14, 55)
    # d = datetime.datetime(2010, 11, 1, 8, 55) # DEBUG
    bits = enc.encode(d)
    # Season is aaabbbcccddd (1 bit/month)
    seasonExpected = [1,0,0,0,0,0,0,0,0,0,1,1]

    # Week is MTWTFSS contrary to localtime documentation, Monday = 0 (for
    # python datetime.datetime.timetuple()
    dayOfWeekExpected = [0,0,0,1,0,0,0]

    # Not a weekend, so it should be "False"
    weekendExpected = [1, 0]

    # Time of day has radius of 4 hours and w of 5 so each bit = 240/5
    # min = 48min 14:55 is minute 14*60 + 55 = 895; 895/48 = bit 18.6
    # should be 30 bits total (30 * 48 minutes = 24 hours)
    timeOfDayExpected = (
      [0,0,0,0,0,0,0,0,0,0,
       0,0,0,0,0,0,0,0,0,1,
       1,1,1,1,0,0,0,0,0,0])
    expected = seasonExpected + dayOfWeekExpected + weekendExpected + timeOfDayExpected

    self.assertEqual(bits.size, 51)
    self.assertEqual(expected, bits.dense.tolist())



  def testDayOfWeek(self):
    """ Creating date encoder instance. """
    # 1 bit for days in a week (x7 days -> 7 bits), no other fields encoded
    p = DateEncoderParameters()
    p.dayOfWeek_width = 1
    p.verbose = False
    enc = DateEncoder(p)
    # In the middle of fall, Thursday, not a weekend, afternoon - 4th Nov,
    # 2010, 14:55
    d = datetime.datetime(2010, 11, 4, 14, 55)
    bits = enc.encode(d)

    # Week is MTWTFSS, 
    # Monday = 0 (for python datetime.datetime.timetuple())
    dayOfWeekExpected = [0,0,0,1,0,0,0] #Thu

    expected = dayOfWeekExpected
    self.assertEqual(bits.size, 7)
    self.assertEqual(expected, bits.dense.tolist())

    # check a day is encoded consistently during most of its hours.
    p = DateEncoderParameters()
    p.dayOfWeek_width = 40
    p.verbose = False;
    enc = DateEncoder(p)
    dMorn = datetime.datetime(2010, 11, 4,  8,    00) # 8 AM to
    dEve  = datetime.datetime(2010, 11, 4,  8+12, 00) # 8 PM

    bits1 = enc.encode(dMorn)
    bits2 = enc.encode(dEve)
    assert(bits1 .getOverlap( bits2 ) > 40 * .25 )

    # Check the long term statistics of the encoder.
    p = DateEncoderParameters()
    p.dayOfWeek_width = 300
    p.verbose = False;
    enc = DateEncoder(p)
    sdr = SDR( enc.dimensions )
    test_period = 1000
    metrics = Metrics( sdr, test_period )
    now = datetime.datetime.now()
    inc = datetime.timedelta( hours=1 )
    for i in range( test_period ):
        enc.encode(now, sdr)
        now += inc

    #print( metrics )

    assert( metrics.sparsity.min() >= .05 )
    assert( metrics.sparsity.max() <= .20 )
    assert( metrics.activationFrequency.min() >= .05 )
    assert( metrics.activationFrequency.max() <= .20 )
    #assert( metrics.overlap.max() <= .99 )
    #assert( metrics.overlap.min() >= .90 )


  def testSeason(self):
    """ Creating date encoder instance. """
    # 3 bits for season (x4 seasons -> 12 bits), no other fields encoded
    p = DateEncoderParameters()
    p.season_width = 3
    p.verbose = False
    enc = DateEncoder(p)
    # In the middle of fall, Thursday, not a weekend, afternoon - 4th Nov,
    # 2010, 14:55
    d = datetime.datetime(2010, 11, 4, 14, 55)
    bits = enc.encode(d)

    # Season is aaabbbcccddd (1 bit/month)
    seasonExpected = [1,0,0,0,0,0,0,0,0,0,1,1]

    expected = seasonExpected
    self.assertEqual(bits.size, 12)
    self.assertEqual(expected, bits.dense.tolist())


  def testWeekend(self):
    """ Creating date encoder instance. """
    # 1 bit for weekend (x2 possible values (True=is weekend/False=is not)-> 2 bits), no other fields encoded
    p = DateEncoderParameters()
    p.weekend_width = 1
    p.verbose = False
    enc = DateEncoder(p)
    # In the middle of fall, Thursday, not a weekend, afternoon - 4th Nov,
    # 2010, 14:55
    d = datetime.datetime(2010, 11, 4, 14, 55)
    # d = datetime.datetime(2010, 11, 1, 8, 55) # DEBUG
    bits = enc.encode(d)
 
    # Not a weekend, so it should be "False"
    weekendExpected = [1, 0] # "[1,0] = False (category1), [0,1] = True (category2)

    expected = weekendExpected
    self.assertEqual(bits.size, 2)
    self.assertEqual(expected, bits.dense.tolist())


  def testTimeOfDay(self):
    """ Creating date encoder instance. """
    # 3 bits for season (x4 seasons -> 12 bits), no other fields encoded
    p = DateEncoderParameters()
    p.timeOfDay_width = 5
    p.verbose = False
    enc = DateEncoder(p)
    # In the middle of fall, Thursday, not a weekend, afternoon - 4th Nov,
    # 2010, 14:55
    d = datetime.datetime(2010, 11, 4, 14, 55)
    # d = datetime.datetime(2010, 11, 1, 8, 55) # DEBUG
    bits = enc.encode(d)

    # Time of day has radius of 4 hours and w of 5,
    # so each bit = 240/5min = 48min. 
    # 14:55 is minute 14*60 + 55 = 895; 895/48 = cetral bit= bit 18.6 ->18 #FIXME we should round this, so becomes bit19
    # We activate our 5(=w)bits around the central bit -> thus [18-(5-1)/2, 18+(5-1)/2] = [16, 20]
    # Encoder should be 30 bits total (30 * 48 minutes = 24 hours)
    timeOfDayExpected = (
      [0,0,0,0,0,0,0,0,0,0,
       0,0,0,0,0,0,0,0,0,1,
       1,1,1,1,0,0,0,0,0,0])

    expected = timeOfDayExpected
    self.assertEqual(bits.size, 30)
    self.assertEqual(expected, bits.dense.tolist())




  def testHoliday(self):
    """ Look at holiday more carefully because of the smooth transition. """
    p = DateEncoderParameters()
    p.holiday_width = 5
    p.verbose = False
    e = DateEncoder(p)
    notholiday = [1,1,1,1,1,0,0,0,0,0]  # Not a holiday
    holiday1   = [0,0,0,1,1,1,1,1,0,0]  # day before holiday
    holiday    = [0,0,0,0,0,1,1,1,1,1]  # day of holiday
    holiday2   = [1,1,0,0,0,0,0,1,1,1]  # day after holiday


    d = datetime.datetime(2011, 12, 24, 16, 00) #day before holiday, approaching
    assert(all( e.encode(d).dense == holiday1 ))
    
    d = datetime.datetime(2010, 12, 25, 4, 55) #Christmas day 25th Dec, a default holiday
    assert(all( e.encode(d).dense == holiday ))

    d = datetime.datetime(1999, 12, 26, 8, 00) #day after holiday, approaching
    assert(all( e.encode(d).dense == holiday2 ))
    
    d = datetime.datetime(2008, 12, 27, 4, 55) #12/27 is not a holiday
    assert(all( e.encode(d).dense == notholiday ))



  def testHolidayMultiple(self):
    """ Look at holiday more carefully because of the smooth transition. """
    p = DateEncoderParameters()
    p.holiday_width = 5
    p.holiday_dates = [[12, 25], [2018, 4, 1], [2017, 4, 16]]
    p.verbose = False
    e = DateEncoder(p)
    holiday    = [0, 0, 0, 0, 0, 1, 1, 1, 1, 1]
    notholiday = [1, 1, 1, 1, 1, 0, 0, 0, 0, 0]

    d = datetime.datetime(2011, 12, 25, 4, 55)
    assert(all( e.encode(d).dense == holiday ))

    d = datetime.datetime(2007, 12, 2, 4, 55)
    assert(all( e.encode(d).dense == notholiday ))

    d = datetime.datetime(2018, 4, 1, 16, 10)
    assert(all( e.encode(d).dense == holiday ))

    d = datetime.datetime(2017, 4, 16, 16, 10)
    assert(all( e.encode(d).dense == holiday ))


  def testWeekend(self):
    """ Test weekend encoder. """
    p = DateEncoderParameters()
    p.custom_width = 21
    p.custom_days = ["sat,sun,fri"]
    p.verbose = False
    e = DateEncoder(p)
    
    p = DateEncoderParameters()
    p.weekend_width = 21
    e2 = DateEncoder(p)

    d = datetime.datetime(1988, 5, 29, 20, 00)
    #print(d)
    self.assertEqual( e.encode(d), e2.encode(d) )

    for _ in range(300):
      d = d+datetime.timedelta(days=1)
      self.assertEqual( e.encode(d), e2.encode(d) )

  def testJSONSerialization(self):
      """
      This test is to insure that Python can access the C++ serialization functions.
      Serialization is tested more completely in C++ unit tests. Just checking 
      that Python can access it.
      """
      p = DateEncoderParameters()
      p.custom_width = 21
      p.custom_days = ["sat,sun,fri"]
      p.verbose = False
      e1 = DateEncoder(p)

      # serialize
      filename = 'DateEncoder_serialize.json'
      e1.saveToFile(filename, 'JSON')
        
      e2 = DateEncoder()
      e2.loadFromFile(filename, 'JSON')
        
      d = datetime.datetime(1988, 5, 29, 20, 00)
      #print(d)
      self.assertEqual( e1.encode(d), e2.encode(d) )
      os.remove(filename)
  
 
if __name__ == "__main__":
  unittest.main()
