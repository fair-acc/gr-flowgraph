/* -*- c++ -*- */
/*
 * Copyright (C) 2018 GSI Darmstadt, Germany - All Rights Reserved
 *
 * Co-developed with: Cosylab, Ljubljana, Slovenia and CERN, Geneva, Switzerland
 * You may use, distribute and modify this code under the terms of the GPL v.3  license.
 */

#include <iostream>

#include <gnuradio/attributes.h>
#include <cppunit/TestAssert.h>
#include "test_parser.h"

#include <flowgraph/flowgraph.h>
#include <flowgraph/constants.h>
#include "flowgraph_impl.h"
#include "exprtk.hpp"

namespace flowgraph {

void qa_parser::testGetVersion()
{
  auto version = flowgraph::version();
  CPPUNIT_ASSERT(!version.empty());
}

void qa_parser::testOptions()
{
  std::ifstream input("test_parser_options.grc");

  GrcParser parser(input);
  parser.parse();

  auto block = parser.top_block();
  CPPUNIT_ASSERT_EQUAL(std::string("options"), block.key);
  CPPUNIT_ASSERT_EQUAL(std::string("dial_tone"), block.id);
  CPPUNIT_ASSERT_EQUAL(std::string("Dial Tone"), block.param_value("title"));
}

void qa_parser::testCollapseVariables()
{
  std::ifstream input("test_collapse_variables.grc");

  GrcParser parser(input);
  parser.parse();

  auto variables = parser.variables();

  CPPUNIT_ASSERT_EQUAL(1, (int)variables.size());
  CPPUNIT_ASSERT_EQUAL(std::string("samp_rate"), variables[0].id);
  CPPUNIT_ASSERT_EQUAL(std::string("1000"), variables[0].param_value("value"));

  auto blocks = parser.blocks();

  CPPUNIT_ASSERT_EQUAL(1, (int)blocks.size());
  CPPUNIT_ASSERT_EQUAL(std::string("analog_sig_source_x_0"), blocks[0].id);
  CPPUNIT_ASSERT_EQUAL(std::string("samp_rate"), blocks[0].param_value("samp_rate"));

  parser.collapse_variables();

  blocks = parser.blocks();

  CPPUNIT_ASSERT_EQUAL(1, (int)blocks.size());
  CPPUNIT_ASSERT_EQUAL(std::string("analog_sig_source_x_0"), blocks[0].id);
  CPPUNIT_ASSERT_EQUAL(std::string("1000"), blocks[0].param_value("samp_rate"));
}

void qa_parser::testExprtk()
{
    // try to evaluate expression
    typedef exprtk::symbol_table<float> symbol_table_t;
    typedef exprtk::expression<float>     expression_t;
    typedef exprtk::parser<float>             parser_t;

    symbol_table_t symbol_table;
    symbol_table.add_constant("samp_rate", 2);
    expression_t expression;
    expression.register_symbol_table(symbol_table);

    std::string simple_expr_string = "samp_rate";

    parser_t parser;
    parser.compile(simple_expr_string, expression);
    float result = expression.value();

    CPPUNIT_ASSERT_DOUBLES_EQUAL(2.0, result, 1E-8);

    std::string expr_string = "samp_rate * 10 + 2";
    parser.compile(expr_string, expression);
    result = expression.value();

    CPPUNIT_ASSERT_DOUBLES_EQUAL(22.0, result, 1E-8);
}

void qa_parser::testEvaluateExpressions()
{
  std::ifstream input("test_expressions.grc");

  GrcParser parser(input);
  parser.parse();

  auto variables = parser.variables();

  CPPUNIT_ASSERT_EQUAL(1, (int)variables.size());
  CPPUNIT_ASSERT_EQUAL(std::string("samp_rate"), variables[0].id);
  CPPUNIT_ASSERT_EQUAL(1000, variables[0].param_value<int>("value"));

  auto blocks = parser.blocks();

  CPPUNIT_ASSERT_EQUAL(1, (int)blocks.size());
  CPPUNIT_ASSERT_EQUAL(std::string("sig_source"), blocks[0].id);
  CPPUNIT_ASSERT_EQUAL(std::string("samp_rate * 10 / 2"), blocks[0].param_value("samp_rate"));

  parser.collapse_variables();

  blocks = parser.blocks();

  CPPUNIT_ASSERT_EQUAL(1, (int)blocks.size());
  CPPUNIT_ASSERT_EQUAL(std::string("sig_source"), blocks[0].id);
  CPPUNIT_ASSERT_EQUAL(5000, (int)blocks[0].eval_param_value<float>("samp_rate", parser.variables()));
}

}
