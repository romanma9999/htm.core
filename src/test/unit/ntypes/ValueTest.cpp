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
 * Implementation of Value test
 */

#include <gtest/gtest.h>
#include <htm/ntypes/Array.hpp>
#include <htm/ntypes/BasicType.hpp>
#include <htm/ntypes/Value.hpp>

#include <map>
#include <sstream>
#include <vector>

namespace testing {

using namespace htm;
TEST(ValueTest, toValueNumber) {
  ValueMap vm;

  const char *s1 = "10";
  vm.parse(s1);
  EXPECT_TRUE(vm.isScalar());
  UInt32 u = vm.as<UInt32>();
  EXPECT_EQ(10u, u);

  vm.parse("-1");
  Int32 i = vm.as<Int32>();
  EXPECT_EQ(-1, i);
  UInt32 x = vm.as<UInt32>();
  EXPECT_EQ(4294967295u, x);

  vm.parse("- 1"); // "- " means a sequence element in YAML
  EXPECT_TRUE(vm.isSequence());
  UInt32 u1 = vm[0].as<UInt32>();
  UInt32 u2 = 1u;
  EXPECT_EQ(u1, u2);

  vm.parse("[123]"); // explicit sequence with one element.
  EXPECT_TRUE(vm.isSequence());
  i = vm[0].as<Int32>();
  EXPECT_EQ(123, i);

  EXPECT_ANY_THROW(vm.parse("999999999999999999999999999").as<Int32>());
  EXPECT_ANY_THROW(vm.parse("abc").as<Int32>());
  EXPECT_ANY_THROW(vm.parse("").as<Int32>());

}

TEST(ValueTest, toValueTestReal32) {
  ValueMap vm;
  vm.parse("10.1");
  Real32 x = vm.as<Real32>();
  EXPECT_NEAR(10.1f, x, 0.000001);
}

TEST(ValueTest, toValueString) {
  ValueMap vm;
  std::string s1 = "A: \"this is a string\"";
  std::string s;

  // Positive tests
  vm.parse(s1);
  s = vm.getScalarT<std::string>("A", "x");
  EXPECT_TRUE(s == "this is a string");
  s = vm.getScalarT<std::string>("A");
  EXPECT_TRUE(s == "this is a string");
  s = vm.getString("A", "x");
  EXPECT_TRUE(s == "this is a string");
  s = vm["A"].as<std::string>();
  EXPECT_TRUE(s == "this is a string");
  s = vm["A"].str();
  EXPECT_TRUE(s == "this is a string");

  // Negative tests
  s = vm.getScalarT<std::string>("B", "x");
  EXPECT_TRUE(s == "x");
  EXPECT_ANY_THROW(s = vm.getScalarT<std::string>("B"));
  s = vm.getString("B", "y");
  EXPECT_TRUE(s == "y");
  EXPECT_ANY_THROW(s = vm["B"].as<std::string>());
  EXPECT_ANY_THROW(s = vm["B"].str());
}

TEST(ValueTest, toValueBool) {
  ValueMap vm;

  EXPECT_TRUE(vm.parse("B: true").getScalarT<bool>("B", false));
  EXPECT_TRUE(vm.parse("B: True").getScalarT<bool>("B", false));
  EXPECT_TRUE(vm.parse("B: 1").getScalarT<bool>("B", false));
  EXPECT_TRUE(vm.parse("B: ON").getScalarT<bool>("B", false));
  EXPECT_FALSE(vm.parse("B: false").getScalarT<bool>("B", true));
  EXPECT_FALSE(vm.parse("B: FALSE").getScalarT<bool>("B", true));
  EXPECT_FALSE(vm.parse("B: 0").getScalarT<bool>("B", true));
  EXPECT_FALSE(vm.parse("B: off").getScalarT<bool>("B", true));
  vm.parse("B: false");
  EXPECT_FALSE(vm.getScalarT<bool>("B", true));
  EXPECT_FALSE(vm.parse("B: 0").getScalarT<bool>("B", true));
  EXPECT_ANY_THROW(vm.parse("B: 1234").getScalarT<bool>("B", false));
}

TEST(ValueTest, asArray) {
  ValueMap vm;
  std::vector<UInt32> s2 = {10u, 20u, 30u, 40u, 50u};

  std::string json = "[10,20,30,40,50]";
  vm.parse(json);

  EXPECT_EQ(ValueMap::Sequence, vm.getCategory());
  EXPECT_TRUE(vm.isSequence());
  EXPECT_TRUE(!vm.isMap());
  EXPECT_TRUE(!vm.isScalar());
  EXPECT_TRUE(!vm.isEmpty());

  std::vector<UInt32> s1 = vm.asVector<UInt32>();
  EXPECT_TRUE(s1 == s2);

  EXPECT_EQ(vm[0].as<UInt32>(), 10u);
  EXPECT_STREQ(vm[0].str().c_str(), "10");

  std::vector<UInt32> s3 = {100u, 200u, 300u, 400u, 500u};
  vm[5] = s3; // assign an array to the 6th element.

  std::string t = vm.to_json();
  EXPECT_STREQ(t.c_str(), "[10, 20, 30, 40, 50, [100, 200, 300, 400, 500]]");

  EXPECT_EQ(vm[0].as<UInt32>(), 10u);
  EXPECT_TRUE(vm[5].isSequence());
  EXPECT_TRUE(vm[5][4].isScalar());
  EXPECT_EQ(vm[5][4].as<UInt32>(), 500u);
  EXPECT_ANY_THROW(vm.as<UInt32>());    // not a scaler
  EXPECT_ANY_THROW(vm[5].as<UInt32>()); // not a sequence
}

TEST(ValueTest, asMap) {
  ValueMap vm;
  std::string src = "{\"scalar\": 456, \"array\": [1, 2, 3, 4], \"string\": \"is a scalar.\"}";
  vm.parse(src);

  //std::cout << vm << "\n";
  std::string result = vm.to_json();
  EXPECT_STREQ(result.c_str(), src.c_str());

  std::map<std::string,std::string> m;
  m = vm.asMap<std::string>();  // note, the array will be skipped because it is not a string.
  std::stringstream ss;
  bool first = true;
  for (auto itr = m.begin(); itr != m.end(); itr++) {
    if (!first) ss << ", ";
    first = false;
    ss << itr->first << "=" << itr->second;
  }
  result = ss.str();
  //std::cout << result << "\n";
  EXPECT_STREQ(result.c_str(), "scalar=456, string=is a scalar.");
}


TEST(ValueTest, String) {
  std::string s("hello world");
  Value v;
  v.parse(s);
  EXPECT_TRUE(!v.isSequence());
  EXPECT_TRUE(!v.isMap());
  EXPECT_TRUE(v.isScalar());

  std::string s1 = v.str();
  EXPECT_STREQ("hello world", s1.c_str());

  EXPECT_ANY_THROW(v.asVector<UInt32>());
  EXPECT_ANY_THROW(v.as<UInt32>());

  EXPECT_STREQ("\"hello world\"", v.to_json().c_str());
}



TEST(ValueTest, inserts) {
  ValueMap vm;
  vm[0][0][0] = 1;
  //std::cout << "inserts1: " << vm << "\n";
  EXPECT_STREQ("[[[1]]]", vm.to_json().c_str());

  Value v = vm[1]; // Create a zombie node
  for (int i = 0; i < 3; i++) {
    v[i] = i*100+100; // assign to an array on a zombie which should add it to the tree.
  }

  //std::cout << "inserts2: " << vm << "\n";
  EXPECT_STREQ("[[[1]], [100, 200, 300]]", vm.to_json().c_str());

  EXPECT_ANY_THROW(vm[3]["hello"] = std::string("world"));
}

TEST(ValueTest, insertParsedValue) {
  ValueMap vm;
  std::string tree_src = "{\"param1\": \"first node\", \"param2\": \"second node\", \"param3\": \"third node\"}";
  vm.parse(tree_src);
  // std::cout << "initial tree: " << vm << "\n";
  EXPECT_STREQ(tree_src.c_str(), vm.to_json().c_str());

  // Replace param2 with a sequence.
  std::string insert_seq = "[ 1, 2, 3, 4 ]";
  vm["param2"].parse(insert_seq);
  EXPECT_STREQ("{\"param1\": \"first node\", \"param2\": [1, 2, 3, 4], \"param3\": \"third node\"}",
               vm.to_json().c_str());

  // Add a map to the sequence just added.
  std::string insert_map = "{ a: \"value a\", b: \"value b\"}";
  vm["param2"][4].parse(insert_map);
  EXPECT_STREQ("{\"param1\": \"first node\", \"param2\": [1, 2, 3, 4, {\"a\": \"value a\", \"b\": \"value b\"}], "
               "\"param3\": \"third node\"}",
      vm.to_json().c_str());
}

TEST(ValueTest, ValueMapTest) {
  std::vector<UInt32> a = {1, 2, 3, 4};
  ValueMap vm;
  vm["scalar"] = 123;
  vm["scalar"] = 456; // should replace
  vm["array"] = a;
  vm["string"] = std::string("str");

  EXPECT_TRUE(vm.isMap());
  EXPECT_TRUE(vm.contains("scalar"));
  EXPECT_TRUE(vm.contains("array"));
  EXPECT_TRUE(vm.contains("string"));
  EXPECT_TRUE(!vm.contains("foo"));
  EXPECT_TRUE(!vm.contains("scalar2"));
  EXPECT_TRUE(!vm.contains("xscalar"));

  int s = vm["scalar"].as<int>();
  EXPECT_TRUE(456 == s);

  std::vector<UInt32> a1 = vm["array"].asVector<UInt32>();
  EXPECT_TRUE(a1 == a);

  Int32 x = vm.getScalarT("scalar2", (Int32)20);
  EXPECT_EQ((Int32)20, x);

  std::string expected = "{\"scalar\": 456, \"array\": [1, 2, 3, 4], \"string\": \"str\"}";
  std::string result;
  std::stringstream ss;
  ss << vm;
  result = ss.str();
  EXPECT_STREQ(result.c_str(), expected.c_str());

  result = vm.to_json();
  EXPECT_STREQ(result.c_str(), expected.c_str());

  ValueMap vm2;
  vm2.parse(result);
  //std::cout << "vm=" << vm << "\n";
  //std::cout << "vm2" << vm2 << "\n";
  EXPECT_TRUE(vm == vm2);
}

TEST(ValueTest, Iterations) {
  ValueMap vm;
  std::vector<std::string> results;
  int cnt = 0;

  std::string data = R"(scalar: 123.45
array: 
  - 1
  - 2
  - 3
  - 4
string: this is a string
)";

  vm.parse(data);
  //std::cout << vm << "\n";
  EXPECT_TRUE(vm.check());
  for (auto itr = vm.begin(); itr != vm.end(); itr++) {
    std::string key = itr->first;
    if (key == "scalar")
      EXPECT_NEAR(itr->second.as<Real32>(), 123.45f, 0.000001);
    else if (key == "string")
      EXPECT_STREQ(itr->second.str().c_str(), "this is a string");
    else if (key == "array") {
      for (size_t i = 0; i < 4; i++) {
        EXPECT_EQ(i + 1, itr->second[i].as<size_t>());
      }
    } else
      NTA_THROW << "unexpected key";
  }

  // iterate with for range
  cnt = 0;
  for (auto itm : vm) {
    if (itm.second.isScalar())
      cnt++;
    if (itm.second.isSequence())
      cnt++;
  }
  EXPECT_EQ(cnt, 3);

  std::string result = vm.to_yaml();
  //std::cout << result << "\n";
  EXPECT_STREQ(result.c_str(), data.c_str());

}

TEST(ValueTest, deletes) {
  ValueMap vm;
  std::string src = "{scalar: 456, array: [1, 2, 3, 4], string: \"a string\"}";
  vm.parse(src);

  EXPECT_EQ(vm.size(), 3u);
  EXPECT_EQ(vm["array"].size(), 4u);

  //std::cout << "reference: " << vm << "\n";

  vm["scalar"].remove();
  //std::cout << "scalar removed: " << vm << "\n";
  EXPECT_TRUE(vm.check());
  EXPECT_EQ(vm.size(), 2u);
  EXPECT_ANY_THROW(vm["scalar"].str());
  EXPECT_TRUE(vm[0].isSequence());
  EXPECT_TRUE(vm[0][0].isScalar());
  EXPECT_EQ(vm[0][0].as<int>(), 1);
  EXPECT_EQ(vm[0][3].as<int>(), 4);
  EXPECT_EQ(vm[0].size(), 4u);

  vm[0][0].remove();
  //std::cout << "[0][0] removed: " << vm << "\n";
  EXPECT_TRUE(vm.check());
  EXPECT_EQ(vm[0].size(), 3u);
  EXPECT_TRUE(vm[0][0].isScalar());
  EXPECT_EQ(vm[0][0].as<int>(), 2);
  EXPECT_EQ(vm[0][2].as<int>(), 4);
  EXPECT_ANY_THROW(vm[0][3].as<int>());

  vm[0][2].remove();
  // std::cout << "[0][2] removed: " << vm << "\n";
  EXPECT_TRUE(vm.check());
  EXPECT_EQ(vm[0].size(), 2u);
  EXPECT_TRUE(vm[0][1].isScalar());
  EXPECT_EQ(vm[0][0].as<int>(), 2);
  EXPECT_EQ(vm[0][1].as<int>(), 3);
  EXPECT_ANY_THROW(vm[0][2].as<int>());

  vm[0][2] = 6;
  //std::cout << "[0][2] = 6 : " << vm << "\n";
  EXPECT_EQ(vm[0].size(), 3u);
  EXPECT_TRUE(vm[0][1].isScalar());
  EXPECT_EQ(vm[0][0].as<int>(), 2);
  EXPECT_EQ(vm[0][1].as<int>(), 3);
  EXPECT_ANY_THROW(vm[0][3].as<int>());
  std::vector<int> v = vm[0].asVector<int>();

  std::vector<int> expected({2, 3, 6});
  for (size_t i = 0; i < 3u; i++) {
    EXPECT_EQ(expected[i], v[i]);
  }
  EXPECT_TRUE(vm.check());

  vm[0][2].remove();
  //std::cout << "[0][2] removed : " << vm << "\n";
  EXPECT_EQ(vm[0].size(), 2u);
  for (size_t i = 0; i < 2u; i++) {
    EXPECT_EQ(expected[i], v[i]);
  }

  vm[0].remove();
  //std::cout << "[0][2] removed : " << vm << "\n";
  EXPECT_EQ(vm.size(), 1u);
  EXPECT_TRUE(vm[0].isScalar());
  EXPECT_TRUE(vm.contains("string"));

  vm.remove();
  EXPECT_TRUE(vm.isEmpty());
}

// Utility routine to test Array to Value
template <typename T> 
static std::string vectorToJSON(const std::vector<T>& data) {
  Array a(data);
  Value vm;
  std::string typeName = BasicType::getName(a.getType());
  vm["type"] = typeName;
  Value& vm2 = vm["data"];
  T *p = (T *)a.getBuffer();
  for (size_t i = 0; i < a.getCount(); i++) {
    vm2[i] = p[i];  // a numeric index on vm2 creates a sequence.
  }
  return vm.to_json();
}

TEST(ValueTest, from_Array) {
  // What I want to test is converting an Array object to a Value object.
  // I want to do that for each data type.
  // So I am using the templated function above.  This will create a
  // test Array object from a vector.  Then convert it to a VM.
  // To confirm that it worked, I convert the VM to a JSON string and compare
  // with expected results.
  {
    std::vector<Byte> data = {1, 2, 3, 4};
    std::string j = vectorToJSON(data);
    EXPECT_STREQ(j.c_str(), "{\"type\": \"Byte\", \"data\": [1, 2, 3, 4]}");
  }
  {
    std::vector<Int16> data = {1, 2, 3, 4};
    std::string j = vectorToJSON(data);
    EXPECT_STREQ(j.c_str(), "{\"type\": \"Int16\", \"data\": [1, 2, 3, 4]}");
  }
  {
    std::vector<UInt16> data = {1, 2, 3, 4};
    std::string j = vectorToJSON(data);
    EXPECT_STREQ(j.c_str(), "{\"type\": \"UInt16\", \"data\": [1, 2, 3, 4]}");
  }
  {
    std::vector<Int32> data = {1, 2, 3, 4};
    std::string j = vectorToJSON(data);
    EXPECT_STREQ(j.c_str(), "{\"type\": \"Int32\", \"data\": [1, 2, 3, 4]}");
  }
  {
    std::vector<UInt32> data = {1, 2, 3, 4};
    std::string j = vectorToJSON(data);
    EXPECT_STREQ(j.c_str(), "{\"type\": \"UInt32\", \"data\": [1, 2, 3, 4]}");
  }
  {
    std::vector<Int64> data = {1, 2, 3, 4};
    std::string j = vectorToJSON(data);
    EXPECT_STREQ(j.c_str(), "{\"type\": \"Int64\", \"data\": [1, 2, 3, 4]}");
  }
  {
    std::vector<UInt64> data = {1, 2, 3, 4};
    std::string j = vectorToJSON(data);
    EXPECT_STREQ(j.c_str(), "{\"type\": \"UInt64\", \"data\": [1, 2, 3, 4]}");
  }
  {
    std::vector<Real32> data = {1, 2, 3, 4};
    std::string j = vectorToJSON(data);
    EXPECT_STREQ(j.c_str(), "{\"type\": \"Real32\", \"data\": [1.000000, 2.000000, 3.000000, 4.000000]}");
  }
  {
    std::vector<Real64> data = {1, 2, 3, 4};
    std::string j = vectorToJSON(data);
    EXPECT_STREQ(j.c_str(), "{\"type\": \"Real64\", \"data\": [1.000000, 2.000000, 3.000000, 4.000000]}");
  }
  {
    SDR sdr({4});
    sdr.setDense(SDR_dense_t({1, 0, 1, 0}));
    std::string j = vectorToJSON(sdr.getDense());
    EXPECT_STREQ(j.c_str(), "{\"type\": \"Byte\", \"data\": [1, 0, 1, 0]}");
  }
  {
    std::vector<std::string> data = {"A", "B", "C", "D"};
    std::string j = vectorToJSON(data);
    EXPECT_STREQ(j.c_str(), "{\"type\": \"String\", \"data\": [\"A\", \"B\", \"C\", \"D\"]}");
  }
}

} // namespace testing
