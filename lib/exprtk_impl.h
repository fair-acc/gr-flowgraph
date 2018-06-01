/* -*- c++ -*- */
/* Copyright (C) 2018 GSI Darmstadt, Germany - All Rights Reserved
 * co-developed with: Cosylab, Ljubljana, Slovenia and CERN, Geneva, Switzerland
 * You may use, distribute and modify this code under the terms of the GPL v.3  license.
 */

#ifndef _FLOWGRAPH_EXPRTK_IMPL_H_
#define _FLOWGRAPH_EXPRTK_IMPL_H_

#include <string>
#include <vector>
#include <map>

namespace flowgraph {

  namespace detail {

    /*!
     * Implemented in a seperate compilation unit to avoid long compilation times.
     */
    double evaluate_expression(const std::string &expr_string, const std::map<std::string, double> &variables);
  }
}

#endif /* _FLOWGRAPH_EXPRTK_IMPL_H_ */
