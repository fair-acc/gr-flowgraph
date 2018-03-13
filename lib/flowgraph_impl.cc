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

#include <functional>
#include <memory>
#include <vector>

#include <boost/property_tree/xml_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/type_traits.hpp>

#include "flowgraph_impl.h"

namespace flowgraph {


std::ostream& operator<<(std::ostream& os, const BlockInfo& dt)
{
    os << "id/key: " << dt.id << "/"  << dt.key;

    if (dt.params.size()) {
    	os << ", params:\n";
		for (const auto& kv : dt.params) {
			os << "  " << kv.first << " : " << kv.second << "\n";
		}
    }

    return os;
}

std::ostream& operator<<(std::ostream& os, const ConnectionInfo& dt)
{
    os << dt.src_id << ":" << dt.src_key << " <---> "
       << dt.dst_id << ":" << dt.dst_key;
    return os;
}

void GrcParser::parse()
{
    // populate tree structure
    boost::property_tree::ptree tree;
    boost::property_tree::read_xml(d_is, tree);

    boost::property_tree::ptree flow_graph = tree.get_child("flow_graph");

    BOOST_FOREACH( boost::property_tree::ptree::value_type const& f, flow_graph) {
        if (f.first == "block" ) {
            BlockInfo block;

            // Obtain key and parameters
            boost::property_tree::ptree subtree = (boost::property_tree::ptree) f.second;

            BOOST_FOREACH(boost::property_tree::ptree::value_type &v, subtree) {
                if(v.first == "param") {
                    std::string key = v.second.get<std::string>("key");
                    std::string value = v.second.get<std::string>("value");

                    if (key == "id")
                    {
                        block.id = value;
                    } else {
                        block.params[key] = value;
                    }
                } else if (v.first == "key") {
                    block.key = v.second.get<std::string>("");
                } else {
                    // TODO: log error instead?
                    //throw std::invalid_argument("invalid block entry: " + v.first);
                    std::cout << "invalid block entry: " << v.first << "\n";
                }
            }

            if (block.key == "options") {
                d_top_block = block;
            }
            else if (block.key == "variable") {
                d_variables.push_back(block);
            }
            else {
                d_blocks.push_back(block);
            }
        } else if (f.first == "connection") {
            ConnectionInfo con;

            con.src_id = f.second.get<std::string>("source_block_id");
            con.dst_id = f.second.get<std::string>("sink_block_id");
            con.src_key = f.second.get<int>("source_key");
            con.dst_key = f.second.get<int>("sink_key");

            d_connections.push_back(con);

        } else {
            // TODO: log error instead?
            std::cout << "unknown flowgraph entry: " << f.first << "\n";
        }
    }

    d_parsed = true;
}

void GrcParser::collapse_variables()
{
    // For simplicity make a map
    std::map<std::string, std::string> variable_value_map;
    for (const auto &v: d_variables) {
        variable_value_map[v.id] = v.param_value("value");
    }

    // If orig. value is in the map, replace it with the associated value
    for(auto& b: d_blocks) {
        for (auto& p: b.params) {
            if (variable_value_map.count(p.second)) {
                p.second = variable_value_map[p.second];
            }
        }
    }
}


struct SigSourceMaker : BlockMaker
{
	const std::map<std::string, gr::analog::gr_waveform_t> enum_repr =
	{
		{"analog.GR_CONST_WAVE", gr::analog::GR_CONST_WAVE},
		{"analog.GR_SIN_WAVE", gr::analog::GR_SIN_WAVE},
		{"analog.GR_COS_WAVE", gr::analog::GR_COS_WAVE},
		{"analog.GR_SQR_WAVE", gr::analog::GR_SQR_WAVE},
		{"analog.GR_TRI_WAVE", gr::analog::GR_TRI_WAVE},
		{"analog.GR_SAW_WAVE", gr::analog::GR_SAW_WAVE},
	};

    gr::analog::gr_waveform_t lexical_cast(const std::string & s)
    {
    	return enum_repr.find(s)->second;
    }

	gr::block_sptr make(const BlockInfo &info, const std::vector<BlockInfo> &variables) override
	{
		assert(info.key == "analog_sig_source_x");

		auto type = info.param_value<>("type");
		if (type != "float") {
			throw std::invalid_argument("the only supported type is float: " + type);
		}

		auto sampling_freq = info.eval_param_value<double>("samp_rate", variables);
		auto wave_freq     = info.eval_param_value<double>("freq", variables);
		auto ampl          = info.eval_param_value<double>("amp", variables);
		auto offset        = info.eval_param_value<float>("offset", variables);
		auto waveform_type = lexical_cast(info.param_value<>("waveform"));

		return gr::analog::sig_source_f::make(sampling_freq, waveform_type, wave_freq, ampl, offset);
	}
};

struct NullSinkMaker : BlockMaker
{
	gr::block_sptr make(const BlockInfo &info, const std::vector<BlockInfo> &variables) override
	{
		assert(info.key == "blocks_null_sink");

        auto type = info.param_value<>("type");
        if (type != "float") {
            throw std::invalid_argument("the only supported type is float: " + type);
        }

		return gr::blocks::null_sink::make(sizeof(float));
	}
};

struct ThrottleMaker : BlockMaker
{
    gr::block_sptr make(const BlockInfo &info, const std::vector<BlockInfo> &variables) override
    {
        assert(info.key == blocks_throttle_key);

        auto type = info.param_value<>("type");
        if (type != "float") {
            throw std::invalid_argument("the only supported type is float: " + type);
        }

        auto samples_per_sec = info.eval_param_value<double>("samples_per_second", variables);
        auto ignore_tags = info.param_value<bool>("ignoretag");

        return gr::blocks::throttle::make(sizeof(float), samples_per_sec, ignore_tags);
    }
};

struct TimeDomainSinkMaker : BlockMaker
{
    gr::block_sptr make(const BlockInfo &info, const std::vector<BlockInfo> &variables) override
    {
        assert(info.key == time_domain_sink_key);

        auto signal_name = info.param_value("signal_name");
        auto signal_unit = info.param_value("signal_unit");
        auto buffer_size = info.eval_param_value<int>("buffer_size", variables);
        auto mode        = info.param_value<int>("acquisition_type");

        auto sink =  gr::digitizers::time_domain_sink::make(signal_name, signal_unit,
                buffer_size, static_cast<gr::digitizers::time_sink_mode_t>(mode));

        return sink;
    }
};

struct PostMortemSinkMaker : BlockMaker
{
    gr::block_sptr make(const BlockInfo &info, const std::vector<BlockInfo> &variables) override
    {
        assert(info.key == post_mortem_sink_key);

        auto signal_name = info.param_value("signal_name");
        auto signal_unit = info.param_value("signal_unit");
        auto buffer_size = info.param_value<int>("buffer_size");

        auto sink =  gr::digitizers::post_mortem_sink::make(signal_name, signal_unit, buffer_size);
        return sink;
    }
};

struct ExtractorMaker : BlockMaker
{
    gr::block_sptr make(const BlockInfo &info, const std::vector<BlockInfo> &variables) override
    {
        assert(info.key == extractor_key);

        auto pre_trigger_window    = info.eval_param_value<float>("pre_trigger_window", variables);
        auto post_trigger_window   = info.eval_param_value<float>("post_trigger_window", variables);

        auto block =  gr::digitizers::extractor::make(post_trigger_window, pre_trigger_window);

        return block;
    }
};

struct TimeRealignmentMaker : BlockMaker
{
    gr::block_sptr make(const BlockInfo &info, const std::vector<BlockInfo> &variables) override
    {
        assert(info.key == time_realignment_key);

        auto user_delay            = info.eval_param_value<float>("user_delay", variables);
        auto timing_available      = info.param_value<bool>("timing_available");
        auto ignore_realignment    = info.param_value<bool>("ignore_realignment");

        auto block =  gr::digitizers::time_realignment_ff::make(user_delay, timing_available, ignore_realignment);
        return block;
    }
};

struct Ps3000aMaker : BlockMaker
{
    gr::block_sptr make(const BlockInfo &info, const std::vector<BlockInfo> &variables) override
    {
        assert(info.key == picoscope_3000a_key);

        auto serial_number         = info.param_value("serial_number");
        auto trigger_once          = info.param_value<bool>("trigger_once");
        auto samp_rate             = info.eval_param_value<float>("samp_rate", variables);
        auto samples               = info.eval_param_value<int>("samples", variables);
        auto pre_samples           = info.eval_param_value<int>("pre_samples", variables);
        auto downsampling_mode     = info.param_value<int>("downsampling_mode");
        auto downsampling_factor   = info.eval_param_value<int>("downsampling_factor", variables);

        auto ps = gr::digitizers::picoscope_3000a::make(serial_number, true);
        ps->set_trigger_once(trigger_once);
        ps->set_samp_rate(samp_rate);
        ps->set_samples(samples, pre_samples);
        ps->set_downsampling(
                static_cast<gr::digitizers::downsampling_mode_t>(downsampling_mode),
                downsampling_factor);

        auto enable_ai_a = info.param_value<bool>("enable_ai_a");
        if (enable_ai_a) {
            auto range_ai_a = info.param_value<double>("range_ai_a");
            auto coupling_ai_a = info.param_value<bool>("coupling_ai_a");
            auto offset_ai_a = info.param_value<double>("offset_ai_a");
            ps->set_aichan("A", enable_ai_a, range_ai_a, coupling_ai_a, offset_ai_a);
        }

        auto enable_ai_b = info.param_value<bool>("enable_ai_b");
        if (enable_ai_b) {
            auto range_ai_b = info.param_value<double>("range_ai_b");
            auto coupling_ai_b = info.param_value<bool>("coupling_ai_b");
            auto offset_ai_b = info.param_value<double>("offset_ai_b");
            ps->set_aichan("B", enable_ai_b, range_ai_b, coupling_ai_b, offset_ai_b);
        }

        auto enable_ai_c = info.param_value<bool>("enable_ai_c");
        if (enable_ai_c) {
            auto range_ai_c = info.param_value<double>("range_ai_c");
            auto coupling_ai_c = info.param_value<bool>("coupling_ai_c");
            auto offset_ai_c = info.param_value<double>("offset_ai_c");
            ps->set_aichan("C", enable_ai_c, range_ai_c, coupling_ai_c, offset_ai_c);
        }

        auto enable_ai_d = info.param_value<bool>("enable_ai_d");
        if (enable_ai_d) {
            auto range_ai_d = info.param_value<double>("range_ai_d");
            auto coupling_ai_d = info.param_value<bool>("coupling_ai_d");
            auto offset_ai_d = info.param_value<double>("offset_ai_d");
            ps->set_aichan("D", enable_ai_d, range_ai_d, coupling_ai_d, offset_ai_d);
        }

        auto enable_di_0 = info.param_value<bool>("enable_di_0");
        auto thresh_di_0 = info.param_value<double>("thresh_di_0");
        ps->set_diport("port0", enable_di_0, thresh_di_0);

        auto enable_di_1 = info.param_value<bool>("enable_di_1");
        auto thresh_di_1 = info.param_value<double>("thresh_di_1");
        ps->set_diport("port1", enable_di_1, thresh_di_1);

        auto trigger_source = info.param_value("trigger_source");

        if (trigger_source != "None") {
            if (trigger_source == "Digital") {
                auto pin_number = info.param_value<uint32_t>("pin_number");
                auto trigger_direction = info.param_value<int>("trigger_direction");
                ps->set_di_trigger(pin_number,
                        static_cast<gr::digitizers::trigger_direction_t>(trigger_direction));
            }
            else {
                auto trigger_direction = info.param_value<int>("trigger_direction");
                auto trigger_threshold = info.param_value<double>("trigger_threshold");
                ps->set_aichan_trigger(
                        trigger_source,
                        static_cast<gr::digitizers::trigger_direction_t>(trigger_direction),
                        trigger_threshold);
            }
        }

        auto acquisition_mode = info.param_value("acquisition_mode");

        if (acquisition_mode == "Streaming") {
            auto buff_size = info.eval_param_value<int>("buff_size", variables);
            auto poll_rate = info.eval_param_value<int>("poll_rate", variables);
            ps->set_buffer_size(buff_size);
            ps->set_streaming(poll_rate);
        }
        else {
            auto nr_waveforms = info.eval_param_value<int>("nr_waveforms", variables);
            ps->set_rapid_block(nr_waveforms);
        }

        return ps;
    }
};

struct EdgeTriggerMaker : BlockMaker
{
    gr::block_sptr make(const BlockInfo &info, const std::vector<BlockInfo> &variables) override
    {
        assert(info.key == edge_trigger_key);

        auto sampling          = info.eval_param_value<float>("sampling", variables);
        auto lo                = info.eval_param_value<float>("lo", variables);
        auto hi                = info.eval_param_value<float>("hi", variables);
        auto initial_satate    = info.eval_param_value<float>("initial_state", variables);
        auto send_udp          = info.param_value<bool>("send_udp");
        auto host_list         = info.param_value("host_list");

        auto block =  gr::digitizers::edge_trigger_ff::make(sampling, lo, hi, initial_satate, send_udp, host_list);
        return block;
    }
};


BlockFactory::BlockFactory()
{
    handlers_b["analog_sig_source_x"] = boost::shared_ptr<BlockMaker>(new SigSourceMaker());
    handlers_b[time_domain_sink_key] = boost::shared_ptr<BlockMaker>(new TimeDomainSinkMaker());
    handlers_b["blocks_null_sink"] = boost::shared_ptr<BlockMaker>(new NullSinkMaker());
    handlers_b[blocks_throttle_key] = boost::shared_ptr<BlockMaker>(new ThrottleMaker());
    handlers_b[post_mortem_sink_key] = boost::shared_ptr<BlockMaker>(new PostMortemSinkMaker());
    handlers_b[extractor_key] = boost::shared_ptr<BlockMaker>(new ExtractorMaker());
    handlers_b[picoscope_3000a_key] = boost::shared_ptr<BlockMaker>(new Ps3000aMaker());
    handlers_b[edge_trigger_key] = boost::shared_ptr<BlockMaker>(new EdgeTriggerMaker());
    handlers_b[time_realignment_key] = boost::shared_ptr<BlockMaker>(new TimeRealignmentMaker());
}


/*!
 * Affinity is not parsed as a vector for now...
 */
void BlockFactory::common_settings(gr::block_sptr block,
        const BlockInfo &info, const std::vector<BlockInfo> &variables)
{
    if (info.is_param_set("affinity")) {
        auto affinity = info.param_value<int>("affinity");
        block->set_processor_affinity(std::vector<int> {affinity});
    }

    if (info.is_param_set("minoutbuf")) {
        auto minoutbuf = info.eval_param_value<int>("minoutbuf", variables);
        if (minoutbuf > 0) {
            block->set_min_output_buffer(minoutbuf);
        }
    }

    if (info.is_param_set("maxoutbuf")) {
        auto maxoutbuf = info.eval_param_value<int>("maxoutbuf", variables);
        if (maxoutbuf > 0) {
            block->set_max_output_buffer(maxoutbuf);
        }
    }
}

gr::block_sptr BlockFactory::make_block(const BlockInfo &info, const std::vector<BlockInfo> &variables)
{
    auto it = handlers_b.find(info.key);

    if (it == handlers_b.end()) {
        throw std::invalid_argument("block type " + info.key + " not supported.");
    }

    boost::shared_ptr<BlockMaker> maker = it->second;
    auto block = maker->make(info, variables);

    // apply common settings
    common_settings(block, info, variables);

    return block;
}

std::unique_ptr<FlowGraph> make_flowgraph(std::istream &input, const std::map<std::string, std::string> &hw_mapping)
{
	// parse input and replace variables
	flowgraph::GrcParser parser(input);
	parser.parse();
	parser.collapse_variables();

	// obtain title if provided
	std::string title = parser.top_block().param_value("title");
	if (!title.length())
	{
		title = "My Flowgraph";
	}

	// make graph, add blocks and connections
	BlockFactory factory;

	std::unique_ptr<FlowGraph> graph(new FlowGraph(title));

	auto variables = parser.variables();
	std::vector<std::string> disabled_blocks;
 	for (auto info : parser.blocks()) {

 	    if (!info.param_value<bool>("_enabled")) {
 	        disabled_blocks.push_back(info.id);
 	        continue;
 	    }

 		if (hw_mapping.count(info.id)) {
 			info.params["serial_number"] = hw_mapping.find(info.id)->second;
 		}

		auto block = factory.make_block(info, variables);
		graph->add(block, info.id, info.key);
	}

	for (const auto info : parser.connections()) {
	    // connect only if both ends are enabled
	    if (std::count(disabled_blocks.begin(), disabled_blocks.end(), info.src_id)
	            || std::count(disabled_blocks.begin(), disabled_blocks.end(), info.dst_id)) {
	        continue;
	    }

	    graph->connect(info.src_id, info.src_key,
	                   info.dst_id, info.dst_key);
    }

	return std::move(graph);
}

}

