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

#ifndef _FLOWGRAPH_FLOWGRAPH_IMPL_H_
#define _FLOWGRAPH_FLOWGRAPH_IMPL_H_

#include <gnuradio/types.h>
#include <gnuradio/runtime_types.h>
#include <gnuradio/top_block.h>
#include <gnuradio/analog/sig_source_f.h>
#include <gnuradio/blocks/null_sink.h>
#include <gnuradio/blocks/throttle.h>
#include <digitizers/time_domain_sink.h>
#include <digitizers/post_mortem_sink.h>
#include <digitizers/extractor.h>
#include <digitizers/time_realignment_ff.h>
#include <digitizers/picoscope_3000a.h>
#include <digitizers/edge_trigger_ff.h>

#include <flowgraph/flowgraph.h>
#include "exprtk_impl.h" // evaluate_expression

#include <functional>
#include <memory>
#include <vector>
#include <algorithm>
#include <cctype>

#include <boost/property_tree/xml_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/type_traits.hpp>


namespace flowgraph {

  namespace detail {

    template <typename T = std::string>
    inline T convert_to(const std::string& string)
    {
        return boost::lexical_cast<T>(string);
    }

    template <>
    inline bool convert_to(const std::string& string)
    {
        if (boost::iequals(string, "False")) {
            return false;
        } else if (boost::iequals(string, "True")) {
            return true;
        }

        return boost::lexical_cast<bool>(string);
    }

  }



struct BlockInfo
{
	std::string key; // Block type e.g. blocks_null_sink
	std::string id;  // Unique block name e.g. blocks_null_sink_0
	std::map<std::string, std::string> params;

	/*!
	 * Helper function for obtaining parameter values.
	 */
	template <class T = std::string>
	T param_value(const std::string &param_name) const
	{
        auto param = params.find(param_name);
        if (param == params.end()) {
            throw std::runtime_error("can't find parameter " + param_name
                    + " for block " + id);
        }

	    try {
	        return detail::convert_to<T>(param->second);
	    }
	    catch (...) {
	        throw std::runtime_error("failed to parse parameter " + param_name
	                + " for block " + id + ", string value: " + param->second);
        }
	}

	bool is_param_set(const std::string &param_name) const
	{
	    auto param = params.find(param_name);
	    if (param == params.end()) {
	        return false;
	    }

	    auto value = param->second;
	    // remove spaces
	    value.erase(std::remove_if(value.begin(), value.end(),
	            [](unsigned char c){ return std::isspace(c); }), value.end());

	    return !value.empty();
	}

    template< class T, typename boost::enable_if< boost::is_arithmetic< T >, int >::type = 0>
    T eval_param_value(const std::string &param_name, const std::vector<BlockInfo> &variables) const
    {
        std::map<std::string, double> variable_map;

        for (auto &var : variables) {
            variable_map[var.id] = var.param_value<double>("value");
        }

        auto expression = param_value(param_name);

        try {
            return static_cast<T>(detail::evaluate_expression(expression, variable_map));
        }
        catch (...) {
            throw std::runtime_error("failed to evaluate parameter " + param_name
                    + " for block " + id + ", expression: " + expression);
        }
    }
};

std::ostream& operator<<(std::ostream& os, const BlockInfo& dt);

struct ConnectionInfo
{
	std::string src_id;
	std::string dst_id;
	int src_key;
	int dst_key;
};

std::ostream& operator<<(std::ostream& os, const ConnectionInfo& dt);

class GrcParser
{
public:
	GrcParser(std::istream &is) : d_is(is), d_parsed(false)  { }

	/*!
	 * Reads GRC configuration file and produces two vectors:
	 *  - one holding information about blocks, and
	 *  - another holding info about connections
	 *
	 * Block format:
	 *  <block>
	 *   <key>options</key>
	 *   <param>
	 *     <key>_enabled</key>
	 *     <value>True</value>
	 *   </param>
	 *   <param>
	 *     <key>id</key>
	 *     <value>top_block</value>
	 *   </param>
	 *   ...
	 *  </block>
	 *
	 * Connection format:
	 *  <connection>
	 *   <source_block_id>analog_sig_source_x_0</source_block_id>
	 *   <sink_block_id>blocks_null_sink_0</sink_block_id>
	 *   <source_key>0</source_key>
	 *   <sink_key>0</sink_key>
	 *  </connection>
	 *
	 * For now everything is read out as stings and later converted to
	 * appropriate types my individual factory methods.
	 *
	 * Note xml attributes seems not to be used by GRC therefore they
	 * are not supported.
	 */
	void parse();

	/*!
	 *\brief Replace all the variables with real numbers.
	 */
	void collapse_variables();

	std::vector<BlockInfo> blocks() const
	{
		return d_blocks;
	}

	std::vector<BlockInfo> variables() const
	{
		return d_variables;
	}

	std::vector<ConnectionInfo> connections() const
	{
		return d_connections;
	}

	BlockInfo top_block() const
	{
		if (!d_parsed) {
			throw std::runtime_error("run parse first");
		}

		return d_top_block;
	}

private:
	std::istream &d_is;
	std::vector<BlockInfo> d_blocks;
	std::vector<BlockInfo> d_variables;
	std::vector<ConnectionInfo> d_connections;
	BlockInfo d_top_block;
	bool d_parsed;
};

// Needed to work around missing lambda support on some target platforms
struct BlockMaker
{
    virtual gr::block_sptr make(const BlockInfo &info, const std::vector<BlockInfo> &variables) = 0;
    virtual ~BlockMaker() {}
};

class BlockFactory
{
public:
	BlockFactory();

	bool supported_block_type(const std::string &key) const
	{
		return handlers_b.find(key) != handlers_b.end();
	}

	/*!
	 * \brief Apply setting common to all block types.
	 *
	 * Affinity is not properly supported for now...
	 */
	void common_settings(gr::block_sptr block, const BlockInfo &info, const std::vector<BlockInfo> &variables);

	gr::block_sptr make_block(const BlockInfo &info, const std::vector<BlockInfo> &variables=std::vector<BlockInfo>());

private:
	std::map<std::string, boost::shared_ptr<BlockMaker>> handlers_b;
};


}



#endif /* _FLOWGRAPH_FLOWGRAPH_IMPL_H_ */
