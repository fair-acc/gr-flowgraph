/* -*- c++ -*- */
/*
 * Copyright 2017 <+YOU OR YOUR COMPANY+>.
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
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

