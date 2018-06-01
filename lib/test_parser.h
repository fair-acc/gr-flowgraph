/* -*- c++ -*- */
/*
 * Copyright (C) 2018 GSI Darmstadt, Germany - All Rights Reserved
 *
 * Co-developed with: Cosylab, Ljubljana, Slovenia and CERN, Geneva, Switzerland
 * You may use, distribute and modify this code under the terms of the GPL v.3  license.
 */


#ifndef _TEST_PARSER_H_
#define _TEST_PARSER_H_

#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/TestCase.h>

namespace flowgraph {

class qa_parser : public CppUnit::TestCase
{
public:
  CPPUNIT_TEST_SUITE(qa_parser);
  CPPUNIT_TEST(testGetVersion);
  CPPUNIT_TEST(testOptions);
  CPPUNIT_TEST(testCollapseVariables);
  CPPUNIT_TEST(testExprtk);
  CPPUNIT_TEST(testEvaluateExpressions);
  CPPUNIT_TEST_SUITE_END();
private:
  void testGetVersion();
  void testOptions();
  void testCollapseVariables();
  void testExprtk();
  void testEvaluateExpressions();
};


}
#endif
