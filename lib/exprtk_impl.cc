/* -*- c++ -*- */
/* Copyright (C) 2018 GSI Darmstadt, Germany - All Rights Reserved
 * co-developed with: Cosylab, Ljubljana, Slovenia and CERN, Geneva, Switzerland
 * You may use, distribute and modify this code under the terms of the GPL v.3  license.
 */

#include "exprtk_impl.h"
#include "exprtk.hpp"


namespace flowgraph {
  namespace detail {

    double evaluate_expression(const std::string &expr_string, const std::map<std::string, double> &variables)
    {
        exprtk::symbol_table<double> symbol_table;

        for (const auto& variable : variables) {
            symbol_table.add_constant(variable.first, variable.second);
        }

        exprtk::expression<double> expression;
        expression.register_symbol_table(symbol_table);

        exprtk::parser<double> parser;
        parser.compile(expr_string, expression);

        return expression.value();
    }
  }
}

