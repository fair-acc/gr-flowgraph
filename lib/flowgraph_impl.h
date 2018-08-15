/* -*- c++ -*- */
/*
 * Copyright (C) 2018 GSI Darmstadt, Germany - All Rights Reserved
 *
 * Co-developed with: Cosylab, Ljubljana, Slovenia and CERN, Geneva, Switzerland
 * You may use, distribute and modify this code under the terms of the GPL v.3  license.
 */

#ifndef _FLOWGRAPH_FLOWGRAPH_IMPL_H_
#define _FLOWGRAPH_FLOWGRAPH_IMPL_H_

#include <gnuradio/types.h>
#include <gnuradio/runtime_types.h>
#include <gnuradio/top_block.h>
#include <gnuradio/analog/sig_source_f.h>
#include <gnuradio/blocks/null_sink.h>
#include <gnuradio/blocks/vector_to_stream.h>
#include <gnuradio/blocks/throttle.h>

#include <digitizers/block_aggregation.h>
#include <digitizers/block_amplitude_and_phase.h>
#include <digitizers/block_complex_to_mag_deg.h>
#include <digitizers/block_demux.h>
#include <digitizers/block_scaling_offset.h>
#include <digitizers/block_spectral_peaks.h>
#include <digitizers/chi_square_fit.h>
#include <digitizers/decimate_and_adjust_timebase.h>
#include <digitizers/edge_trigger_ff.h>
#include <digitizers/demux_ff.h>
#include <digitizers/freq_sink_f.h>
#include <digitizers/function_ff.h>
#include <digitizers/interlock_generation_ff.h>
#include <digitizers/picoscope_3000a.h>
#include <digitizers/picoscope_4000a.h>
#include <digitizers/picoscope_6000.h>
#include <digitizers/post_mortem_sink.h>
#include <digitizers/signal_averager.h>
#include <digitizers/stft_algorithms.h>
#include <digitizers/stft_goertzl_dynamic_decimated.h>
#include <digitizers/time_domain_sink.h>
#include <digitizers/time_realignment_ff.h>


#include <map>
#include <gnuradio/filter/firdes.h>//win_type enum

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
#include <boost/range/algorithm/remove.hpp>
#include <boost/algorithm/string/split.hpp>


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
            std::ostringstream message;
            message << "Exception in " << __FILE__ << ":" << __LINE__ << ": can't find parameter " << param_name << " for block " << id;
            throw std::runtime_error(message.str());
        }

        auto param_value = param->second;

        // get rid of '' or "" quotes both representing string in Python
        if ((param_value.size() > 2)
                && ((param_value.at(0) == '\'' && param_value.at(param_value.size() - 1) == '\'')
                    || (param_value.at(0) == '"' && param_value.at(param_value.size() - 1) == '"'))) {
            param_value.erase(0, 1);
            param_value.erase(param_value.size() - 1, 1);
        }

	    try {
	        return detail::convert_to<T>(param_value);
	    }
	    catch (...) {
            std::ostringstream message;
            message << "Exception in " << __FILE__ << ":" << __LINE__ << ": failed to parse parameter " << param_name << " for block " << id << ", string value: " << param_value;
            throw std::runtime_error(message.str());
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
	        variable_map[var.id] = var.param_value<T>("value");
	    }

	    auto expression = param_value(param_name);

        try {
          return static_cast<T>(detail::evaluate_expression(expression, variable_map));
        }
        catch (...) {
            std::ostringstream message;
            message << "Exception in " << __FILE__ << ":" << __LINE__ << ": failed to evaluate parameter " << param_name << " for block " << id << ", expression: " << expression;
            throw std::runtime_error(message.str());
        }
    }

	template< class T>
	std::vector<T> eval_param_vector(const std::string &param_name, const std::vector<BlockInfo> &variables) const
	{
	    std::vector<T> result;

	    std::map<std::string, double> variable_map;

        for (auto &var : variables) {
            variable_map[var.id] = var.param_value<double>("value");
        }

        auto expression = param_value(param_name);

        // get rid of spaces
        expression.erase(
                std::remove_if(expression.begin(), expression.end(),
                        [](unsigned char c) {return std::isspace(c);}),
                expression.end());

        // get rid of () or []
        if ((expression.size() > 2)
                && ((expression.at(0) == '(' && expression.at(expression.size() - 1) == ')')
                    || (expression.at(0) == '[' && expression.at(expression.size() - 1) == ']'))) {
            expression.erase(0, 1);
            expression.erase(expression.size() - 1, 1);
        }

        // split string vector
        std::vector<std::string> parts;
        boost::algorithm::split(parts, expression,
                [] (char c) {return c == ',';});

        try {
            for (auto &p : parts) {
                result.push_back(
                        static_cast<T>(detail::evaluate_expression(p,
                                variable_map)));
            }
        } catch (...) {
            std::ostringstream message;
            message << "Exception in " << __FILE__ << ":" << __LINE__ << ": failed to evaluate parameter vector " << param_name << " for block " << id << ", expression: " << expression;
            throw std::runtime_error(message.str());
        }

        return result;
    }

  int eval_param_enum(const std::string &param_name) const
  {
    auto expression = param_value(param_name);
    auto enum_type = expression.substr(0, expression.find('.'));
    auto enum_spec = expression.substr(expression.find('.')+1, expression.length());
    if(enum_type == "firdes") {
      std::map<std::string, gr::filter::firdes::win_type> enum_map = {
        {"WIN_NONE", gr::filter::firdes::win_type::WIN_NONE},
        {"WIN_HAMMING", gr::filter::firdes::win_type::WIN_HAMMING},
        {"WIN_HANN", gr::filter::firdes::win_type::WIN_HANN},
        {"WIN_BLACKMAN", gr::filter::firdes::win_type::WIN_BLACKMAN},
        {"WIN_RECTANGULAR", gr::filter::firdes::win_type::WIN_RECTANGULAR},
        {"WIN_KAISER", gr::filter::firdes::win_type::WIN_KAISER},
        {"WIN_BLACKMAN_hARRIS", gr::filter::firdes::win_type::WIN_BLACKMAN_hARRIS},
        {"WIN_BLACKMAN_HARRIS", gr::filter::firdes::win_type::WIN_BLACKMAN_HARRIS},
        {"WIN_BARTLETT", gr::filter::firdes::win_type::WIN_BARTLETT},
        {"WIN_FLATTOP", gr::filter::firdes::win_type::WIN_FLATTOP}};
        return enum_map[enum_spec];
    }
    //other enums can be added here
    return -1;
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
		       std::ostringstream message;
		       message << "Exception in " << __FILE__ << ":" << __LINE__ << ": run parse first !";
		       throw std::runtime_error(message.str());
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
    static int getSizeOfType(std::string type);
    virtual gr::basic_block_sptr make(const BlockInfo &info, const std::vector<BlockInfo> &variables) = 0;
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
	void common_settings(gr::basic_block_sptr block, const BlockInfo &info, const std::vector<BlockInfo> &variables);

	gr::basic_block_sptr make_block(const BlockInfo &info, const std::vector<BlockInfo> &variables=std::vector<BlockInfo>());

private:
	std::map<std::string, boost::shared_ptr<BlockMaker>> handlers_b;
};


}



#endif /* _FLOWGRAPH_FLOWGRAPH_IMPL_H_ */
